// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>

module clepsydre.control.drive;

import clepsydre.control.infusion;

namespace clepsydre::control {

namespace {

// Pas pour un débit en µL/h pendant un intervalle en ms, avec une section en nL/mm et une avance
// en nm : débit × ms × pas par tour × 2 500 / (9 × section × avance). Le volume de l'intervalle
// vaut débit × ms / 3 600 nL, et un pas chasse section × avance / (10⁶ × pas par tour) nL.
constexpr std::uint64_t numeratorFactor = 2'500;
constexpr std::uint64_t divisorFactor = 9;

// Plages admises. Avec elles, débit × ms × numérateur reste sous 3,3 × 10¹⁵ et le diviseur sous
// 9 × 10¹³ : aucun calcul ne déborde 64 bits.
constexpr std::uint32_t maxScrewLead = 10'000'000; // 10 mm par tour
constexpr std::uint32_t maxStepsPerRevolution = 65'536;
constexpr std::uint32_t maxStrokeVolume = 1'000'000; // 1 000 mm² : seringue de 35 mm de diamètre
constexpr std::uint32_t maxMotorStepRate = 100'000;
constexpr std::uint32_t maxTakeUpTravel = 10'000; // 10 mm
constexpr std::uint64_t millisecondsPerSecond = 1'000;
constexpr std::uint64_t nanometresPerMicrometre = 1'000;

[[nodiscard]] std::uint64_t stepNumerator(const DriveMechanism& mechanism) noexcept {
    return std::uint64_t{mechanism.stepsPerRevolution} * numeratorFactor;
}

[[nodiscard]] std::uint64_t stepDivisor(const DriveMechanism& mechanism,
                                        const SyringeDrive& syringe) noexcept {
    return divisorFactor * syringe.strokeVolume * mechanism.screwLeadNanometres;
}

[[nodiscard]] std::uint64_t periodMilliseconds(const DriveMechanism& mechanism) noexcept {
    return static_cast<std::uint64_t>(mechanism.period.count());
}

[[nodiscard]] std::uint64_t takeUpStepsPerPeriod(const DriveMechanism& mechanism) noexcept {
    return mechanism.takeUpStepRate * periodMilliseconds(mechanism) / millisecondsPerSecond;
}

[[nodiscard]] std::uint64_t maxTakeUpSteps(const DriveMechanism& mechanism,
                                           const SyringeDrive& syringe) noexcept {
    return std::uint64_t{syringe.takeUpTravel} * nanometresPerMicrometre *
           mechanism.stepsPerRevolution / mechanism.screwLeadNanometres;
}

[[nodiscard]] bool validMechanism(const DriveMechanism& mechanism) noexcept {
    return mechanism.screwLeadNanometres >= 1 && mechanism.screwLeadNanometres <= maxScrewLead &&
           mechanism.stepsPerRevolution >= 1 &&
           mechanism.stepsPerRevolution <= maxStepsPerRevolution && mechanism.maxStepRate >= 1 &&
           mechanism.maxStepRate <= maxMotorStepRate && mechanism.takeUpStepRate >= 1 &&
           mechanism.takeUpStepRate <= mechanism.maxStepRate &&
           mechanism.period >= std::chrono::milliseconds{1} && mechanism.period <= Drive::maxStep &&
           takeUpStepsPerPeriod(mechanism) >= 1;
}

[[nodiscard]] bool validSyringe(const DriveMechanism& mechanism,
                                const SyringeDrive& syringe) noexcept {
    return syringe.strokeVolume >= 1 && syringe.strokeVolume <= maxStrokeVolume &&
           syringe.takeUpForce.millinewtons > 0 && syringe.takeUpTravel >= 1 &&
           syringe.takeUpTravel <= maxTakeUpTravel && maxTakeUpSteps(mechanism, syringe) >= 1;
}

// Pas qu'exige le débit maximal de la pompe pendant duration, arrondis par excès.
[[nodiscard]] std::uint64_t maximumRateSteps(const DriveMechanism& mechanism,
                                             const SyringeDrive& syringe,
                                             std::chrono::milliseconds duration) noexcept {
    const std::uint64_t numerator = std::uint64_t{pumpMaxRate.microlitresPerHour} *
                                    static_cast<std::uint64_t>(duration.count()) *
                                    stepNumerator(mechanism);
    const std::uint64_t divisor = stepDivisor(mechanism, syringe);
    return (numerator + divisor - 1) / divisor;
}

// Une commande, plafonnée aux pas du débit maximal de la pompe sur une période, peut être émise
// pendant cette période sans dépasser la vitesse maximale du moteur.
[[nodiscard]] bool maximumRateReachable(const DriveMechanism& mechanism,
                                        const SyringeDrive& syringe) noexcept {
    return maximumRateSteps(mechanism, syringe, mechanism.period) <=
           std::uint64_t{mechanism.maxStepRate} * periodMilliseconds(mechanism) /
               millisecondsPerSecond;
}

} // namespace

bool validDriveParameters(const DriveMechanism& mechanism, const SyringeDrive& syringe) noexcept {
    return validMechanism(mechanism) && validSyringe(mechanism, syringe) &&
           maximumRateReachable(mechanism, syringe);
}

Drive::Drive(const DriveMechanism& mechanism, const SyringeDrive& syringe,
             std::int64_t stepCount) noexcept
    : mechanism_{mechanism}, syringe_{syringe}, start_{stepCount}, requested_{stepCount},
      counted_{stepCount} {
    if (!validDriveParameters(mechanism, syringe)) {
        phase_ = DrivePhase::Faulted;
        fault_ = DriveFault::InvalidParameters;
        return;
    }
    stepNumerator_ = stepNumerator(mechanism);
    stepDivisor_ = stepDivisor(mechanism, syringe);
    maxTakeUpSteps_ = static_cast<std::int64_t>(maxTakeUpSteps(mechanism, syringe));
    takeUpStepsPerPeriod_ = static_cast<std::uint32_t>(takeUpStepsPerPeriod(mechanism));
    maxStepsPerPeriod_ = maximumRateSteps(mechanism, syringe, mechanism.period);
    maxBacklog_ = maximumRateSteps(mechanism, syringe, maxStep);
}

DriveCommand Drive::update(std::chrono::milliseconds elapsed, Rate setpoint, std::int64_t stepCount,
                           std::optional<Force> force) noexcept {
    if (phase_ == DrivePhase::Faulted) {
        return {.steps = 0, .fault = fault_};
    }
    if (elapsed < std::chrono::milliseconds{0} || elapsed > maxStep) {
        return fail(DriveFault::InvalidStep);
    }
    // Le générateur émet au plus les pas demandés ; ceux d'avant la dernière commande sont émis, à
    // un pas près pour un générateur qui reporte sa fraction de pas.
    if (stepCount < counted_ || stepCount > requested_) {
        return fail(DriveFault::CounterInconsistent);
    }
    if (requested_ - stepCount > std::int64_t{lastSteps_} + 1) {
        return fail(DriveFault::StepsNotEmitted);
    }
    counted_ = stepCount;
    if (phase_ == DrivePhase::TakingUp) {
        return takeUp(force);
    }
    if (setpoint > pumpMaxRate) {
        return fail(DriveFault::SetpointAboveMaximum);
    }
    return track(elapsed, setpoint);
}

DriveCommand Drive::fail(DriveFault fault) noexcept {
    phase_ = DrivePhase::Faulted;
    fault_ = fault;
    lastSteps_ = 0;
    return {.steps = 0, .fault = fault};
}

DriveCommand Drive::takeUp(std::optional<Force> force) noexcept {
    if (!force) {
        return fail(DriveFault::ForceUnavailable);
    }
    if (force->millinewtons >= syringe_.takeUpForce.millinewtons) {
        // Jeu fermé et joint précontraint : la perfusion commence à la période suivante.
        phase_ = DrivePhase::Tracking;
        lastSteps_ = 0;
        return {};
    }
    const std::int64_t travel = requested_ - start_;
    if (travel >= maxTakeUpSteps_) {
        // Toute la course est demandée : la force n'est jugée qu'une fois la dernière commande
        // émise, au pas de tolérance près. L'attente dure une période : le contrôle du compteur
        // exige ensuite cette émission.
        if (requested_ - counted_ > 1) {
            lastSteps_ = 0;
            return {};
        }
        return fail(DriveFault::TakeUpTravelExceeded);
    }
    const auto steps = static_cast<std::uint32_t>(
        std::min(std::int64_t{takeUpStepsPerPeriod_}, maxTakeUpSteps_ - travel));
    requested_ += steps;
    lastSteps_ = steps;
    return {.steps = steps, .fault = std::nullopt};
}

DriveCommand Drive::track(std::chrono::milliseconds elapsed, Rate setpoint) noexcept {
    // Intégrale exacte de la consigne : le reste de la division est reporté, aucune erreur
    // d'arrondi ne se cumule.
    remainder_ += std::uint64_t{setpoint.microlitresPerHour} *
                  static_cast<std::uint64_t>(elapsed.count()) * stepNumerator_;
    backlog_ += remainder_ / stepDivisor_;
    remainder_ %= stepDivisor_;
    // Les pas sont émis pendant la période suivante : la commande est plafonnée à ce que le débit
    // maximal de la pompe y demande. Le reste, après un intervalle plus long que la période, est
    // un retard rattrapé aux périodes suivantes, et borné.
    const std::uint64_t steps = std::min(backlog_, maxStepsPerPeriod_);
    backlog_ -= steps;
    if (backlog_ > maxBacklog_) {
        return fail(DriveFault::TrackingLag);
    }
    tracked_ += steps;
    requested_ += static_cast<std::int64_t>(steps);
    lastSteps_ = static_cast<std::uint32_t>(steps);
    return {.steps = lastSteps_, .fault = std::nullopt};
}

} // namespace clepsydre::control
