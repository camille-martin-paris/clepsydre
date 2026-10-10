// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <chrono>
#include <cstdint>

module clepsydre.sim.clock;

namespace clepsydre::sim {

namespace {

constexpr std::int64_t billion = 1'000'000'000;

// Arrondi vers moins l'infini au multiple de step.
[[nodiscard]] std::chrono::nanoseconds floorTo(std::chrono::nanoseconds value,
                                               std::chrono::nanoseconds step) noexcept {
    auto quotient = value / step;
    if (value % step != std::chrono::nanoseconds::zero() && value < std::chrono::nanoseconds{}) {
        --quotient;
    }
    return step * quotient;
}

} // namespace

std::chrono::nanoseconds driftOver(std::chrono::nanoseconds elapsed,
                                   std::int64_t driftPpb) noexcept {
    // Secondes entières et reste séparés : le produit reste loin des limites de 64 bits.
    const std::int64_t ticks = elapsed.count();
    return std::chrono::nanoseconds{((ticks / billion) * driftPpb) +
                                    ((ticks % billion) * driftPpb / billion)};
}

MonotonicClock::MonotonicClock(std::chrono::nanoseconds offset, std::int64_t driftPpb,
                               std::chrono::nanoseconds tick) noexcept
    : offset_{offset}, driftPpb_{driftPpb}, tick_{tick} {}

std::chrono::nanoseconds MonotonicClock::read(TrueTime now) const noexcept {
    return floorTo(offset_ + now + driftOver(now, driftPpb_), tick_);
}

CivilClock::CivilClock(TrueTime now, CivilTime initial, std::int64_t driftPpb) noexcept
    : anchor_{now}, anchorValue_{initial}, driftPpb_{driftPpb} {}

CivilReading CivilClock::read(TrueTime now) const noexcept {
    return {.time = std::chrono::floor<std::chrono::seconds>(valueAt(now)),
            .oscillatorStopped = stopped_};
}

void CivilClock::set(TrueTime now, CivilTime time) noexcept {
    anchor_ = now;
    anchorValue_ = time;
    stopped_ = false;
}

void CivilClock::jump(TrueTime now, std::chrono::seconds delta) noexcept {
    anchorValue_ = valueAt(now) + delta;
    anchor_ = now;
}

void CivilClock::stopOscillator(TrueTime now) noexcept {
    anchorValue_ = valueAt(now);
    anchor_ = now;
    stopped_ = true;
}

std::chrono::sys_time<std::chrono::nanoseconds> CivilClock::valueAt(TrueTime now) const noexcept {
    if (stopped_) {
        return anchorValue_;
    }
    const auto elapsed = now - anchor_;
    return anchorValue_ + elapsed + driftOver(elapsed, driftPpb_);
}

} // namespace clepsydre::sim
