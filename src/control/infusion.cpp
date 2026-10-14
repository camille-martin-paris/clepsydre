// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <expected>
#include <optional>

module clepsydre.control.infusion;

import clepsydre.control.state_machine;

namespace clepsydre::control {

namespace {

// Unité interne de volume : 1 µL délivré à 1 µL/h dure 3 600 000 ms.
constexpr std::uint64_t unitsPerMicrolitre = 3'600'000;
constexpr std::uint64_t secondsPerHour = 3'600;
constexpr std::uint64_t minutesPerHour = 60;

[[nodiscard]] bool aligned(Rate rate) noexcept {
    return rate.microlitresPerHour % rateResolution.microlitresPerHour == 0;
}

[[nodiscard]] bool aligned(Volume volume) noexcept {
    return volume.microlitres % volumeResolution.microlitres == 0;
}

// Débit programmable : dans la plage de la pompe, à la résolution.
[[nodiscard]] bool programmable(Rate rate) noexcept {
    return aligned(rate) && rate >= pumpMinRate && rate <= pumpMaxRate;
}

// Limites dures dans la plage de la pompe, limites souples entre elles.
[[nodiscard]] bool validRates(const DrugLimits& limits) noexcept {
    return programmable(limits.hardMinRate) && programmable(limits.hardMaxRate) &&
           aligned(limits.softMinRate) && aligned(limits.softMaxRate) &&
           limits.hardMinRate <= limits.softMinRate && limits.softMinRate <= limits.softMaxRate &&
           limits.softMaxRate <= limits.hardMaxRate;
}

[[nodiscard]] bool validEndOfInfusion(const DrugLimits& limits) noexcept {
    switch (limits.endOfInfusion) {
    case EndOfInfusion::Stop:
        return true;
    case EndOfInfusion::Kvo:
        return programmable(limits.kvoRate) && limits.kvoRate <= maxKvoRate;
    default:
        return false;
    }
}

[[nodiscard]] bool validBolusLimits(const DrugLimits& limits) noexcept {
    if (limits.hardMaxBolusVolume.microlitres == 0) {
        return true; // bolus interdit pour ce médicament
    }
    return aligned(limits.hardMaxBolusVolume) && aligned(limits.softMaxBolusVolume) &&
           limits.softMaxBolusVolume >= volumeResolution &&
           limits.softMaxBolusVolume <= limits.hardMaxBolusVolume &&
           programmable(limits.hardMaxBolusRate);
}

[[nodiscard]] std::optional<Refusal> checkVolume(Volume volume, const Syringe& syringe) noexcept {
    if (!aligned(volume)) {
        return Refusal::VolumeResolution;
    }
    if (volume < volumeResolution) {
        return Refusal::VolumeBelowMinimum;
    }
    if (volume > syringe.usableVolume) {
        return Refusal::VolumeAboveSyringe;
    }
    return std::nullopt;
}

// Plage de la pompe et de la seringue ; la résolution est vérifiée à part.
[[nodiscard]] std::optional<Refusal> checkRate(Rate rate, const Syringe& syringe) noexcept {
    if (rate < pumpMinRate) {
        return Refusal::RateBelowPumpMinimum;
    }
    if (rate > pumpMaxRate) {
        return Refusal::RateAbovePumpMaximum;
    }
    if (rate > syringe.maxRate) {
        return Refusal::RateAboveSyringeMaximum;
    }
    return std::nullopt;
}

[[nodiscard]] SoftLimit softLimit(Rate rate, const DrugLimits& limits) noexcept {
    if (rate < limits.softMinRate) {
        return SoftLimit::BelowMinimum;
    }
    if (rate > limits.softMaxRate) {
        return SoftLimit::AboveMaximum;
    }
    return SoftLimit::Within;
}

// Durée de la perfusion de volume au débit rate, arrondie par excès à la seconde.
[[nodiscard]] std::chrono::seconds durationAt(Volume volume, Rate rate) noexcept {
    const std::uint64_t numerator = std::uint64_t{volume.microlitres} * secondsPerHour;
    const std::uint64_t perHour = rate.microlitresPerHour;
    return std::chrono::seconds{
        static_cast<std::chrono::seconds::rep>((numerator + perHour - 1) / perHour)};
}

[[nodiscard]] std::uint64_t units(Volume volume) noexcept {
    return std::uint64_t{volume.microlitres} * unitsPerMicrolitre;
}

[[nodiscard]] Volume volumeOf(std::uint64_t amount) noexcept {
    return {.microlitres = static_cast<std::uint32_t>(amount / unitsPerMicrolitre)};
}

} // namespace

bool validLimits(const DrugLimits& limits, const Syringe& syringe) noexcept {
    return validRates(limits) && validEndOfInfusion(limits) && validBolusLimits(limits) &&
           syringe.usableVolume >= volumeResolution && syringe.maxRate >= pumpMinRate;
}

std::expected<Program, Refusal> validateProgram(const ProgramRequest& request,
                                                const DrugLimits& limits,
                                                const Syringe& syringe) noexcept {
    if (!validLimits(limits, syringe)) {
        return std::unexpected{Refusal::InvalidLimits};
    }
    if (request.mode != Mode::ContinuousRate && request.mode != Mode::VolumeOverDuration) {
        return std::unexpected{Refusal::InvalidMode};
    }
    if (const auto refusal = checkVolume(request.volume, syringe)) {
        return std::unexpected{*refusal};
    }
    Rate rate = request.rate;
    bool rounded = false;
    if (request.mode == Mode::ContinuousRate) {
        if (!aligned(rate)) {
            return std::unexpected{Refusal::RateResolution};
        }
    } else {
        // Volume sur durée : débit calculé, arrondi par défaut à la résolution, jamais supérieur
        // au débit prescrit. L'arrondi est signalé au récapitulatif.
        if (request.duration.count() <= 0) {
            return std::unexpected{Refusal::DurationMissing};
        }
        const std::uint64_t volumeMinutes =
            std::uint64_t{request.volume.microlitres} * minutesPerHour;
        const auto minutes = static_cast<std::uint64_t>(request.duration.count());
        const std::uint64_t exact = volumeMinutes / minutes;
        if (exact > pumpMaxRate.microlitresPerHour) {
            return std::unexpected{Refusal::RateAbovePumpMaximum};
        }
        const std::uint64_t step = rateResolution.microlitresPerHour;
        const std::uint64_t truncated = exact / step * step;
        rounded = truncated * minutes != volumeMinutes;
        rate = {.microlitresPerHour = static_cast<std::uint32_t>(truncated)};
    }
    if (const auto refusal = checkRate(rate, syringe)) {
        return std::unexpected{*refusal};
    }
    if (rate < limits.hardMinRate) {
        return std::unexpected{Refusal::RateBelowHardLimit};
    }
    if (rate > limits.hardMaxRate) {
        return std::unexpected{Refusal::RateAboveHardLimit};
    }
    // SYS-REQ-032 : le maintien de veine ouverte ne dépasse jamais le débit programmé.
    const Rate kvo =
        limits.endOfInfusion == EndOfInfusion::Kvo ? std::min(limits.kvoRate, rate) : Rate{};
    return Program{.mode = request.mode,
                   .rate = rate,
                   .volume = request.volume,
                   .duration = durationAt(request.volume, rate),
                   .rateRoundedDown = rounded,
                   .rateSoftLimit = softLimit(rate, limits),
                   .endOfInfusion = limits.endOfInfusion,
                   .kvoRate = kvo};
}

std::expected<Bolus, Refusal> validateBolus(const BolusRequest& request, const Program& program,
                                            const DrugLimits& limits,
                                            const Syringe& syringe) noexcept {
    if (!validLimits(limits, syringe)) {
        return std::unexpected{Refusal::InvalidLimits};
    }
    // SYS-REQ-029 : le bolus n'existe qu'en débit continu.
    if (program.mode != Mode::ContinuousRate || limits.hardMaxBolusVolume.microlitres == 0) {
        return std::unexpected{Refusal::BolusNotAllowed};
    }
    if (const auto refusal = checkVolume(request.volume, syringe)) {
        return std::unexpected{*refusal};
    }
    if (request.volume > limits.hardMaxBolusVolume) {
        return std::unexpected{Refusal::BolusVolumeAboveHardLimit};
    }
    if (!aligned(request.rate)) {
        return std::unexpected{Refusal::RateResolution};
    }
    if (const auto refusal = checkRate(request.rate, syringe)) {
        return std::unexpected{*refusal};
    }
    if (request.rate > limits.hardMaxBolusRate) {
        return std::unexpected{Refusal::BolusRateAboveHardLimit};
    }
    return Bolus{.volume = request.volume,
                 .rate = request.rate,
                 .volumeAboveSoftLimit = request.volume > limits.softMaxBolusVolume};
}

Rate setpointBound(ActuatorMode mode, const Program& program, const DrugLimits& limits) noexcept {
    // Chaque borne est plafonnée par une limite du médicament, indépendante de la consigne : une
    // programmation altérée après sa validation reste bornée (remarque de relecture de #129).
    Rate bound{};
    switch (mode) {
    case ActuatorMode::Infusion:
        bound = std::min(program.rate, limits.hardMaxRate);
        break;
    case ActuatorMode::Bolus:
        bound = limits.hardMaxBolusRate;
        break;
    case ActuatorMode::Kvo:
        // SYS-REQ-032 : ni le débit programmé, ni le débit de la bibliothèque, ni 5 mL/h ; nulle
        // si la bibliothèque prévoit l'arrêt en fin de perfusion.
        if (limits.endOfInfusion == EndOfInfusion::Kvo) {
            bound = std::min({program.rate, limits.kvoRate, maxKvoRate});
        }
        break;
    default: // aucune consigne, purge (borne à venir avec son unité), valeur invalide
        break;
    }
    return std::min(bound, pumpMaxRate);
}

Delivery::Delivery(const Program& program, const DrugLimits& limits) noexcept
    : program_{program}, limits_{limits} {}

void Delivery::startBolus(const Bolus& bolus) noexcept {
    bolus_ = bolus;
    bolusDelivered_ = 0;
}

Rate Delivery::setpoint(ActuatorMode mode) const noexcept {
    const bool volumeLeft = infused_ < units(program_.volume);
    switch (mode) {
    case ActuatorMode::Infusion:
        return volumeLeft ? program_.rate : Rate{};
    case ActuatorMode::Bolus:
        return bolus_ && volumeLeft && bolusDelivered_ < units(bolus_->volume) ? bolus_->rate
                                                                               : Rate{};
    case ActuatorMode::Kvo:
        return program_.kvoRate;
    default:
        return Rate{};
    }
}

std::optional<Event> Delivery::advance(ActuatorMode mode,
                                       std::chrono::milliseconds elapsed) noexcept {
    if (elapsed < std::chrono::milliseconds{0} || elapsed > maxStep) {
        return Event::DangerDetected;
    }
    // SW-REQ-006 : la consigne est vérifiée contre la borne du mode avant d'être appliquée.
    const Rate rate = setpoint(mode);
    if (rate > setpointBound(mode, program_, limits_)) {
        return Event::DangerDetected;
    }
    const std::uint64_t amount =
        std::uint64_t{rate.microlitresPerHour} * static_cast<std::uint64_t>(elapsed.count());
    const std::uint64_t target = units(program_.volume);
    const Event endOfVolume = program_.endOfInfusion == EndOfInfusion::Kvo ? Event::VolumeReachedKvo
                                                                           : Event::VolumeReached;
    switch (mode) {
    case ActuatorMode::Infusion:
        bolus_.reset();
        infused_ += std::min(amount, target - infused_);
        if (infused_ == target) {
            return endOfVolume;
        }
        return std::nullopt;
    case ActuatorMode::Bolus: {
        if (!bolus_) {
            return Event::DangerDetected; // mode bolus sans bolus validé
        }
        const std::uint64_t bolusTarget = units(bolus_->volume);
        const std::uint64_t delivered =
            std::min({amount, target - infused_, bolusTarget - bolusDelivered_});
        infused_ += delivered;
        bolusDelivered_ += delivered;
        // Le volume à perfuser atteint pendant le bolus l'emporte sur la fin du bolus.
        if (infused_ == target) {
            return endOfVolume;
        }
        if (bolusDelivered_ == bolusTarget) {
            return Event::BolusEnded;
        }
        return std::nullopt;
    }
    case ActuatorMode::Kvo:
        bolus_.reset();
        kvoDelivered_ += amount;
        return std::nullopt;
    case ActuatorMode::None:
    case ActuatorMode::Purge:
        // Un bolus interrompu, par une pause notamment, n'est pas repris.
        bolus_.reset();
        return std::nullopt;
    default:
        return Event::DangerDetected;
    }
}

Volume Delivery::infused() const noexcept {
    return volumeOf(infused_);
}

Volume Delivery::remaining() const noexcept {
    return {.microlitres = program_.volume.microlitres - infused().microlitres};
}

Volume Delivery::bolusDelivered() const noexcept {
    return volumeOf(bolusDelivered_);
}

Volume Delivery::kvoDelivered() const noexcept {
    return volumeOf(kvoDelivered_);
}

} // namespace clepsydre::control
