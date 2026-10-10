// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

module clepsydre.sim.serial_link;

namespace clepsydre::sim {

namespace {

constexpr std::int64_t bitsPerByte = 10; // 8N1 : bit de départ, 8 bits de données, bit d'arrêt

} // namespace

SerialChannel::SerialChannel(std::int64_t bitsPerSecond, std::chrono::nanoseconds latency) noexcept
    : byteTime_{((bitsPerByte * 1'000'000'000) + bitsPerSecond - 1) / bitsPerSecond},
      latency_{latency} {}

void SerialChannel::send(TrueTime now, std::span<const std::byte> bytes) {
    for (const std::byte value : bytes) {
        const TrueTime start = std::max(now, lineFree_);
        lineFree_ = start + byteTime_;
        pending_.push_back({.at = lineFree_ + latency_, .value = value});
    }
}

std::vector<std::byte> SerialChannel::receive(TrueTime now) {
    std::vector<std::byte> received;
    // Les arrivées sont dans l'ordre d'émission, donc croissantes.
    while (!pending_.empty() && pending_.front().at <= now) {
        received.push_back(pending_.front().value);
        pending_.pop_front();
    }
    return received;
}

} // namespace clepsydre::sim
