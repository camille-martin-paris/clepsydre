// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <span>
#include <utility>
#include <vector>

module clepsydre.sim.processor;

namespace clepsydre::sim {

Processor::Processor(const MonotonicClock& clock, ByteChannel& tx, ByteChannel& rx,
                     Firmware firmware)
    : clock_{clock}, tx_{&tx}, rx_{&rx}, firmware_{std::move(firmware)} {}

void Processor::step(TrueTime now) {
    now_ = now;
    switch (state_) {
    case State::resetting:
        if (now_ < resumeAt_) {
            return;
        }
        boot();
        break;
    case State::hung:
    case State::off:
        return;
    case State::running:
        break;
    }
    if (firmware_.run) {
        firmware_.run(*this);
    }
}

std::chrono::nanoseconds Processor::monotonicNow() const noexcept {
    // Un oscillateur arrêté fige le compteur ; après un redémarrage, il reste à zéro.
    const TrueTime counted = frozenAt_ ? std::max(bootAt_, std::min(now_, *frozenAt_)) : now_;
    return clock_.read(counted) - clock_.read(bootAt_);
}

void Processor::send(std::span<const std::byte> bytes) {
    tx_->send(now_, bytes);
}

std::vector<std::byte> Processor::receive() {
    return rx_->receive(now_);
}

void Processor::hang() noexcept {
    state_ = State::hung;
}

void Processor::reset(std::chrono::nanoseconds downtime) noexcept {
    state_ = State::resetting;
    resumeAt_ = now_ + downtime;
}

void Processor::powerOff() noexcept {
    state_ = State::off;
}

void Processor::powerOn(std::chrono::nanoseconds bootTime) noexcept {
    if (state_ == State::off) {
        reset(bootTime);
    }
}

void Processor::freezeClock() noexcept {
    if (!frozenAt_) {
        frozenAt_ = now_;
    }
}

void Processor::boot() {
    ++bootNumber_;
    bootAt_ = now_;
    state_ = State::running;
    // Le récepteur repart vide : les octets arrivés pendant l'arrêt sont perdus.
    static_cast<void>(rx_->receive(now_));
    if (firmware_.boot) {
        firmware_.boot(*this);
    }
}

} // namespace clepsydre::sim
