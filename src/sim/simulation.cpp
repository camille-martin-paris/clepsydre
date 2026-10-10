// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <algorithm>
#include <chrono>
#include <utility>

module clepsydre.sim.simulation;

namespace clepsydre::sim {

Simulation::Simulation(std::chrono::nanoseconds quantum) noexcept : quantum_{quantum} {}

void Simulation::add(Participant participant) {
    participants_.push_back(std::move(participant));
}

void Simulation::run(std::chrono::nanoseconds duration) {
    if (quantum_ <= std::chrono::nanoseconds::zero()) {
        return;
    }
    while (duration > std::chrono::nanoseconds::zero()) {
        const auto dt = std::min(duration, quantum_);
        now_ += dt;
        duration -= dt;
        for (const Participant& participant : participants_) {
            participant(now_, dt);
        }
    }
}

} // namespace clepsydre::sim
