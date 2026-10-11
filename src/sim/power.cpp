// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <algorithm>
#include <chrono>
#include <optional>

module clepsydre.sim.power;

namespace clepsydre::sim {

PowerSupply::PowerSupply(const PowerParameters& parameters, double stateOfCharge) noexcept
    : parameters_{parameters}, capacity_{parameters.battery.capacity},
      charge_{parameters.battery.capacity * std::clamp(stateOfCharge, 0.0, 1.0)} {}

void PowerSupply::step(std::chrono::nanoseconds dt, bool motorRunning) noexcept {
    if (dt <= std::chrono::nanoseconds::zero()) {
        return;
    }
    const std::chrono::nanoseconds start = elapsed_;
    elapsed_ += dt;
    const double seconds = std::chrono::duration<double>{dt}.count();
    const Current load =
        parameters_.idleCurrent + (motorRunning ? parameters_.motorCurrent : Current{});
    const bool availableAtStart = sourceAvailable();
    const Charge before = charge_;

    // Bascule matérielle : le secteur alimente la pompe et charge la batterie ; sinon la
    // batterie débite jusqu'à ce que son circuit de protection la déconnecte.
    std::optional<std::chrono::nanoseconds> cutoffAt;
    if (mains_) {
        if (batteryConnected_) {
            charge_ = std::min(
                capacity_,
                charge_ + Charge::fromSi(parameters_.battery.chargeCurrent.si() * seconds));
        }
    } else if (batteryAvailable()) {
        const Charge drawn = Charge::fromSi(load.si() * seconds);
        if (charge_ - drawn > cutoffCharge()) {
            charge_ -= drawn;
        } else {
            // La protection déconnecte la batterie en cours de pas : la décharge s'arrête là.
            const double untilCutoff = (charge_ - cutoffCharge()).si() / load.si();
            cutoffAt = start + std::chrono::duration_cast<std::chrono::nanoseconds>(
                                   std::chrono::duration<double>{untilCutoff});
            charge_ = cutoffCharge();
        }
    }
    // Courant moyen du pas, positif en décharge, négatif en charge : son intégrale suit
    // exactement la charge de la batterie.
    batteryCurrent_ = Current::fromSi((before - charge_).si() / seconds);

    if (sourceAvailable()) {
        lostAt_.reset();
    } else if (!lostAt_) {
        // Sources perdues avant le pas, ou en cours de pas par la protection de la batterie.
        lostAt_ = availableAtStart ? cutoffAt.value_or(elapsed_) : start;
    }
}

void PowerSupply::setCapacityFactor(double factor) noexcept {
    capacity_ = parameters_.battery.capacity * std::clamp(factor, 0.0, 1.0);
    charge_ = std::min(charge_, capacity_);
}

std::optional<Voltage> PowerSupply::measuredBatteryVoltage() const noexcept {
    if (!batteryConnected_) {
        return std::nullopt;
    }
    return batteryVoltage();
}

bool PowerSupply::powerFailImminent() const noexcept {
    return !sourceAvailable() && powered();
}

bool PowerSupply::powered() const noexcept {
    return !lostAt_ || elapsed_ - *lostAt_ < parameters_.holdUp;
}

bool PowerSupply::onBattery() const noexcept {
    return !mains_ && batteryAvailable();
}

Voltage PowerSupply::batteryVoltage() const noexcept {
    const Battery& battery = parameters_.battery;
    return battery.empty + ((battery.full - battery.empty) * stateOfCharge());
}

Charge PowerSupply::cutoffCharge() const noexcept {
    const Battery& battery = parameters_.battery;
    return capacity_ *
           ((battery.protectionCutoff - battery.empty) / (battery.full - battery.empty));
}

bool PowerSupply::batteryAvailable() const noexcept {
    // Comparaison sur la charge, et non sur la tension recalculée : la batterie arrêtée
    // exactement au seuil reste déconnectée.
    return batteryConnected_ && charge_ > cutoffCharge();
}

bool PowerSupply::sourceAvailable() const noexcept {
    return mains_ || batteryAvailable();
}

} // namespace clepsydre::sim
