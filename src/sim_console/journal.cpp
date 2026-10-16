// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <chrono>
#include <ostream>
#include <print>
#include <string_view>

module clepsydre.sim.journal;

namespace clepsydre::sim {

void Journal::record(std::chrono::nanoseconds now, Severity severity, std::string_view component,
                     std::string_view message) {
    if (out_ == nullptr) {
        return;
    }
    const auto time = std::chrono::hh_mm_ss{std::chrono::floor<std::chrono::milliseconds>(now)};
    std::println(*out_, "{:02}:{:02}:{:02}.{:03} {} [{}] {}", time.hours().count(),
                 time.minutes().count(), time.seconds().count(), time.subseconds().count(),
                 severity == Severity::alert ? "ALERTE" : "INFO", component, message);
}

} // namespace clepsydre::sim
