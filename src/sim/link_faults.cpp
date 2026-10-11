// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <span>
#include <utility>
#include <vector>

module clepsydre.sim.link_faults;

namespace clepsydre::sim {

FaultyChannel::FaultyChannel(ByteChannel& line, std::uint64_t seed) : line_{&line}, random_{seed} {}

void FaultyChannel::add(const FrameFaultRule& rule) {
    rules_.push_back(rule);
}

void FaultyChannel::send(TrueTime now, std::span<const std::byte> bytes) {
    release(now);
    for (const std::byte value : bytes) {
        partial_.push_back(value);
        if (value == delimiter) {
            process(now, std::exchange(partial_, {}));
        }
    }
}

std::vector<std::byte> FaultyChannel::receive(TrueTime now) {
    release(now);
    return line_->receive(now);
}

std::size_t FaultyChannel::injected(FrameFault fault) const noexcept {
    return injected_.at(static_cast<std::size_t>(fault));
}

void FaultyChannel::process(TrueTime now, std::vector<std::byte> frame) {
    bool duplicate = false;
    bool reorder = false;
    std::optional<std::chrono::nanoseconds> delay;
    for (const FrameFaultRule& rule : rules_) {
        if (!hits(rule, now)) {
            continue;
        }
        ++injected_.at(static_cast<std::size_t>(rule.fault));
        switch (rule.fault) {
        case FrameFault::drop:
            return;
        case FrameFault::duplicate:
            duplicate = true;
            break;
        case FrameFault::corrupt:
            corrupt(frame);
            break;
        case FrameFault::truncate:
            if (frame.size() > 2) {
                frame.erase(frame.begin() + static_cast<std::ptrdiff_t>(frame.size() / 2),
                            std::prev(frame.end()));
            }
            break;
        case FrameFault::delay:
            delay = rule.delay;
            break;
        case FrameFault::reorder:
            reorder = true;
            break;
        }
    }

    std::vector<std::byte> bytes = frame;
    if (duplicate) {
        bytes.insert(bytes.end(), frame.begin(), frame.end());
    }
    pending_.push_back({.bytes = std::move(bytes),
                        .due = now + delay.value_or(std::chrono::nanoseconds::zero()),
                        .afterNext = reorder});
    release(now);
}

void FaultyChannel::release(TrueTime now) {
    // À chaque tour, la trame prête au plus tôt part ; à instant égal, la première remise.
    while (true) {
        std::optional<std::size_t> next;
        TrueTime nextAt{};
        for (std::size_t index = 0; index < pending_.size(); ++index) {
            const auto at = readyAt(index, now);
            if (at && (!next || *at < nextAt)) {
                next = index;
                nextAt = *at;
            }
        }
        if (!next) {
            break;
        }
        Pending& entry = pending_[*next];
        line_->send(nextAt, entry.bytes);
        entry.departed = true;
        entry.departedAt = nextAt;
    }
    // Une trame partie n'est retirée qu'avec celles qui la précèdent : la trame retenue qui la
    // précède doit encore voir son départ.
    while (!pending_.empty() && pending_.front().departed) {
        pending_.pop_front();
    }
}

std::optional<TrueTime> FaultyChannel::readyAt(std::size_t index, TrueTime now) const {
    const Pending& entry = pending_[index];
    if (entry.departed || entry.due > now) {
        return std::nullopt;
    }
    if (!entry.afterNext) {
        return entry.due;
    }
    if (index + 1 >= pending_.size() || !pending_[index + 1].departed) {
        return std::nullopt;
    }
    return std::max(entry.due, pending_[index + 1].departedAt);
}

bool FaultyChannel::hits(const FrameFaultRule& rule, TrueTime now) {
    if (now < rule.from || now >= rule.until) {
        return false;
    }
    // Tirage seulement pour une règle active : la suite reste déterministe.
    return random_() % 1'000'000 < rule.perMillion;
}

void FaultyChannel::corrupt(std::vector<std::byte>& frame) {
    // Un bit d'un octet de contenu ; le délimiteur final reste en place.
    if (frame.size() < 2) {
        return;
    }
    const std::size_t index = random_() % (frame.size() - 1);
    const auto bit = static_cast<unsigned>(random_() % 8);
    frame[index] ^= std::byte{static_cast<unsigned char>(1U << bit)};
}

} // namespace clepsydre::sim
