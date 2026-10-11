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
    if (delay) {
        const Delayed entry{.due = now + *delay, .frame = std::move(bytes)};
        // File triée par échéance ; à échéance égale, l'ordre d'arrivée est conservé.
        const auto position = std::ranges::upper_bound(delayed_, entry.due, {}, &Delayed::due);
        delayed_.insert(position, entry);
        return;
    }
    if (reorder && !held_) {
        held_ = std::move(bytes);
        return;
    }
    line_->send(now, bytes);
    if (held_) {
        line_->send(now, *held_);
        held_.reset();
    }
}

void FaultyChannel::release(TrueTime now) {
    while (!delayed_.empty() && delayed_.front().due <= now) {
        line_->send(delayed_.front().due, delayed_.front().frame);
        delayed_.pop_front();
    }
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
