// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <chrono>
#include <cstdint>
#include <expected>
#include <format>
#include <ostream>
#include <print>
#include <string_view>

module clepsydre.sim.journal;

namespace clepsydre::sim {

namespace {

// Opération et corrélation communes à tous les composants : une seule session de simulation.
constexpr std::string_view operation = "simulation";

void line(std::ostream& out, std::chrono::nanoseconds now, mddlog::core::LogLevel level,
          std::string_view component, std::string_view message, bool truncated) {
    const auto time = std::chrono::hh_mm_ss{std::chrono::floor<std::chrono::milliseconds>(now)};
    std::println(out, "{:02}:{:02}:{:02}.{:03} {} [{}] {}{}", time.hours().count(),
                 time.minutes().count(), time.seconds().count(), time.subseconds().count(),
                 level >= mddlog::core::LogLevel::Warn ? "ALERTE" : "INFO", component, message,
                 truncated ? " […]" : "");
}

} // namespace

mddlog::core::RawTime simulatedTime(std::chrono::nanoseconds now) noexcept {
    return mddlog::core::RawTime::available(std::chrono::sys_time<std::chrono::nanoseconds>{now});
}

std::expected<JournalBinding, mddlog::core::Refusal> Journal::bind(std::string_view component) {
    const auto context = mddlog::core::DiagnosticContext::create(
        {.component = component, .operationId = operation, .correlationId = {}});
    if (!context) {
        return std::unexpected{context.error()};
    }
    return JournalBinding{ring_, *context};
}

void Journal::flush(std::chrono::nanoseconds now) {
    const auto view = ring_.drain();
    if (out_ != nullptr) {
        for (const auto part : {view.first(), view.second()}) {
            for (const mddlog::core::GovernedRecord& record : part) {
                line(*out_, record.time().value().time_since_epoch(), record.level(),
                     record.component(), record.message(), record.truncated().message);
            }
        }
    }
    static_cast<void>(ring_.acknowledge(view, view.size()));
    const std::uint64_t refusals = ring_.refusalCount();
    if (out_ != nullptr && refusals != reportedRefusals_) {
        line(*out_, now, mddlog::core::LogLevel::Warn, "journal",
             std::format("{} enregistrement(s) refusé(s) : anneau plein",
                         refusals - reportedRefusals_),
             false);
    }
    reportedRefusals_ = refusals;
}

} // namespace clepsydre::sim
