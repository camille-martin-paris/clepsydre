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
    elapsed_ += dt;
    const double seconds = std::chrono::duration<double>{dt}.count();
    const Current load =
        parameters_.idleCurrent + (motorRunning ? parameters_.motorCurrent : Current{});

    // Bascule matérielle : le secteur alimente la pompe et charge la batterie ; sinon la
    // batterie débite, tant que son circuit de protection ne l'a pas déconnectée.
    batteryCurrent_ = Current{};
    if (mains_) {
        if (batteryConnected_) {
            charge_ = std::min(
                capacity_,
                charge_ + Charge::fromSi(parameters_.battery.chargeCurrent.si() * seconds));
        }
    } else if (batteryAvailable()) {
        batteryCurrent_ = load;
        charge_ = std::max(Charge{}, charge_ - Charge::fromSi(load.si() * seconds));
    }

    if (sourceAvailable()) {
        lostAt_.reset();
    } else if (!lostAt_) {
        lostAt_ = elapsed_;
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

bool PowerSupply::batteryAvailable() const noexcept {
    return batteryConnected_ && batteryVoltage() > parameters_.battery.protectionCutoff;
}

bool PowerSupply::sourceAvailable() const noexcept {
    return mains_ || batteryAvailable();
}

} // namespace clepsydre::sim
