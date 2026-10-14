// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

module clepsydre.protocol.crc32;

namespace clepsydre::protocol {

namespace {

constexpr std::uint32_t reflectedPolynomial = 0x82F6'3B78U;

// Table d'un octet, calculée à la compilation.
constexpr std::array<std::uint32_t, 256> table = [] {
    std::array<std::uint32_t, 256> result{};
    for (std::uint32_t index = 0; index < result.size(); ++index) {
        std::uint32_t value = index;
        for (int bit = 0; bit < 8; ++bit) {
            value = (value >> 1U) ^ ((value & 1U) != 0U ? reflectedPolynomial : 0U);
        }
        result.at(index) = value;
    }
    return result;
}();

} // namespace

std::uint32_t crc32c(std::span<const std::byte> bytes) noexcept {
    std::uint32_t crc = 0xFFFF'FFFFU;
    const std::span<const std::uint32_t, 256> entries{table};
    for (const std::byte byte : bytes) {
        // Index masqué sur 8 bits : toujours dans la table.
        crc = (crc >> 8U) ^ entries[(crc ^ std::to_integer<std::uint32_t>(byte)) & 0xFFU];
    }
    return crc ^ 0xFFFF'FFFFU;
}

} // namespace clepsydre::protocol
