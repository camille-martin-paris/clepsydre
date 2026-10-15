// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <expected>
#include <optional>

module clepsydre.control.occlusion;

import clepsydre.control.drive;

namespace clepsydre::control {

namespace {

// Plages admises. Avec elles, force × course par newton reste sous 2,2 × 10¹⁵ et course × pas par
// tour sous 6,6 × 10¹¹ : aucun calcul ne déborde 64 bits.
constexpr std::int32_t maxOcclusionForce = 200'000; // 200 N : étendue du capteur visée
constexpr std::chrono::milliseconds maxConfirmation{1'000};
constexpr std::uint32_t maxBackoffPerNewton = 1'000'000; // 1 mm par newton
constexpr std::uint32_t maxBackoffTravel = 10'000;       // 10 mm
constexpr std::uint64_t millisecondsPerSecond = 1'000;
constexpr std::uint64_t nanometresPerMicrometre = 1'000;
constexpr std::uint64_t millinewtonsPerNewton = 1'000;

// Au moins un pas : la validation de l'entraînement l'exige déjà du rattrapage, dont la vitesse
// ne dépasse pas la vitesse maximale du moteur.
[[nodiscard]] std::uint64_t backoffStepsPerPeriod(const DriveMechanism& mechanism) noexcept {
    return std::uint64_t{mechanism.maxStepRate} *
           static_cast<std::uint64_t>(mechanism.period.count()) / millisecondsPerSecond;
}

[[nodiscard]] std::uint64_t travelSteps(const DriveMechanism& mechanism,
                                        std::uint64_t nanometres) noexcept {
    return nanometres * mechanism.stepsPerRevolution / mechanism.screwLeadNanometres;
}

// Course de recul pour la force à la détection, tronquée au pas : un recul trop court laisse un
// peu de pression, un recul trop long aspire sur un piston capturé.
[[nodiscard]] std::uint64_t backoffSteps(const DriveMechanism& mechanism,
                                         const OcclusionSettings& settings, Force force) noexcept {
    // La force à la détection atteint le seuil, au-dessus de la force de fin du rattrapage : elle
    // est positive.
    const auto millinewtons = static_cast<std::uint64_t>(force.millinewtons);
    const std::uint64_t nanometres =
        std::min(millinewtons * settings.backoffPerNewton / millinewtonsPerNewton,
                 std::uint64_t{settings.maxBackoffTravel} * nanometresPerMicrometre);
    return travelSteps(mechanism, nanometres);
}

[[nodiscard]] bool validThreshold(Force threshold, const OcclusionSettings& settings) noexcept {
    return threshold.millinewtons >= settings.minThreshold.millinewtons &&
           threshold.millinewtons <= settings.maxThreshold.millinewtons;
}

} // namespace

bool validOcclusionSettings(const DriveMechanism& mechanism, const SyringeDrive& syringe,
                            const OcclusionSettings& settings) noexcept {
    return validDriveParameters(mechanism, syringe) &&
           settings.minThreshold.millinewtons > syringe.takeUpForce.millinewtons &&
           validThreshold(settings.defaultThreshold, settings) &&
           settings.maxThreshold.millinewtons <= maxOcclusionForce &&
           settings.confirmation >= std::chrono::milliseconds{0} &&
           settings.confirmation <= maxConfirmation && settings.backoffPerNewton >= 1 &&
           settings.backoffPerNewton <= maxBackoffPerNewton && settings.maxBackoffTravel >= 1 &&
           settings.maxBackoffTravel <= maxBackoffTravel &&
           travelSteps(mechanism,
                       std::uint64_t{settings.maxBackoffTravel} * nanometresPerMicrometre) >= 1;
}

std::expected<Force, ThresholdRefusal> selectThreshold(Force requested,
                                                       const DriveMechanism& mechanism,
                                                       const SyringeDrive& syringe,
                                                       const OcclusionSettings& settings) noexcept {
    if (!validOcclusionSettings(mechanism, syringe, settings)) {
        return std::unexpected{ThresholdRefusal::InvalidSettings};
    }
    if (requested.millinewtons < settings.minThreshold.millinewtons) {
        return std::unexpected{ThresholdRefusal::BelowMinimum};
    }
    if (requested.millinewtons > settings.maxThreshold.millinewtons) {
        return std::unexpected{ThresholdRefusal::AboveMaximum};
    }
    return requested;
}

OcclusionMonitor::OcclusionMonitor(const DriveMechanism& mechanism, const SyringeDrive& syringe,
                                   const OcclusionSettings& settings, Force threshold) noexcept
    : mechanism_{mechanism}, settings_{settings}, threshold_{threshold} {
    if (!selectThreshold(threshold, mechanism, syringe, settings)) {
        phase_ = OcclusionPhase::Faulted;
        fault_ = OcclusionFault::InvalidParameters;
        return;
    }
    stepsPerPeriod_ = static_cast<std::uint32_t>(backoffStepsPerPeriod(mechanism));
}

OcclusionCommand OcclusionMonitor::update(std::chrono::milliseconds elapsed, std::int64_t stepCount,
                                          std::optional<Force> force) noexcept {
    if (phase_ == OcclusionPhase::Faulted) {
        return {.occlusion = detection_.has_value(), .backoffSteps = 0, .fault = fault_};
    }
    if (elapsed < std::chrono::milliseconds{0} || elapsed > maxStep) {
        return fail(OcclusionFault::InvalidStep);
    }
    switch (phase_) {
    case OcclusionPhase::Watching:
        return watch(elapsed, force);
    case OcclusionPhase::Stopping:
        return startBackoff(stepCount);
    default:
        break;
    }
    // Recul ou recul achevé : le générateur n'émet que les pas de recul demandés, et ceux d'avant
    // la dernière commande sont émis, à un pas près.
    if (stepCount > counted_ || stepCount < requested_) {
        return fail(OcclusionFault::CounterInconsistent);
    }
    if (stepCount - requested_ > std::int64_t{lastSteps_} + 1) {
        return fail(OcclusionFault::StepsNotEmitted);
    }
    counted_ = stepCount;
    return backOff();
}

OcclusionCommand OcclusionMonitor::fail(OcclusionFault fault) noexcept {
    phase_ = OcclusionPhase::Faulted;
    fault_ = fault;
    lastSteps_ = 0;
    return {.occlusion = detection_.has_value(), .backoffSteps = 0, .fault = fault};
}

OcclusionCommand OcclusionMonitor::watch(std::chrono::milliseconds elapsed,
                                         std::optional<Force> force) noexcept {
    if (!force) {
        // Sans mesure, une occlusion passerait inaperçue (RISK-005).
        return fail(OcclusionFault::ForceUnavailable);
    }
    if (force->millinewtons < threshold_.millinewtons) {
        above_.reset();
        return {};
    }
    // La durée au seuil se compte à partir du premier relevé qui l'atteint.
    above_ = above_ ? *above_ + elapsed : std::chrono::milliseconds{0};
    if (*above_ < settings_.confirmation) {
        return {};
    }
    detection_ = force;
    target_ = static_cast<std::uint32_t>(backoffSteps(mechanism_, settings_, *force));
    phase_ = OcclusionPhase::Stopping;
    return {.occlusion = true, .backoffSteps = 0, .fault = std::nullopt};
}

OcclusionCommand OcclusionMonitor::startBackoff(std::int64_t stepCount) noexcept {
    // L'entraînement a cessé d'avancer à la détection : sa dernière commande est émise, et le
    // compteur lu sert d'origine au recul.
    counted_ = stepCount;
    requested_ = stepCount;
    phase_ = OcclusionPhase::BackingOff;
    return backOff();
}

OcclusionCommand OcclusionMonitor::backOff() noexcept {
    if (phase_ == OcclusionPhase::BackingOff && backedOff_ >= target_) {
        // Toute la course est demandée : le recul est achevé une fois la dernière commande émise,
        // au pas de tolérance près. Le contrôle du compteur exige ensuite cette émission.
        if (counted_ - requested_ <= 1) {
            phase_ = OcclusionPhase::Decompressed;
        }
    }
    if (phase_ == OcclusionPhase::Decompressed || backedOff_ >= target_) {
        lastSteps_ = 0;
        return {.occlusion = true, .backoffSteps = 0, .fault = std::nullopt};
    }
    const std::uint32_t steps = std::min(stepsPerPeriod_, target_ - backedOff_);
    backedOff_ += steps;
    requested_ -= steps;
    lastSteps_ = steps;
    return {.occlusion = true, .backoffSteps = steps, .fault = std::nullopt};
}

} // namespace clepsydre::control
