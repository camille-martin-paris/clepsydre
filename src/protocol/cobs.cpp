// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

module clepsydre.protocol.cobs;

namespace clepsydre::protocol {

namespace {

constexpr std::byte zero{0};
constexpr std::uint8_t fullBlockCode = 0xFF; // 254 octets non nuls, sans octet nul implicite

} // namespace

std::size_t cobsEncode(std::span<const std::byte> input, std::span<std::byte> output) noexcept {
    if (output.size() < cobsMaxEncodedSize(input.size())) {
        return 0;
    }
    // codeIndex : place réservée au code du bloc en cours ; code : 1 + octets du bloc.
    std::size_t codeIndex = 0;
    std::size_t written = 1;
    std::uint8_t code = 1;
    for (std::size_t index = 0; index < input.size(); ++index) {
        const std::byte byte = input[index];
        if (byte != zero) {
            output[written++] = byte;
            ++code;
        }
        if (byte == zero || code == fullBlockCode) {
            output[codeIndex] = std::byte{code};
            code = 1;
            // Un bloc plein qui termine les données n'est suivi d'aucun code (forme canonique).
            if (byte != zero && index + 1 == input.size()) {
                return written;
            }
            codeIndex = written++;
        }
    }
    output[codeIndex] = std::byte{code};
    return written;
}

std::optional<std::size_t> cobsDecode(std::span<const std::byte> input,
                                      std::span<std::byte> output) noexcept {
    std::size_t read = 0;
    std::size_t written = 0;
    while (read < input.size()) {
        const auto code = std::to_integer<std::uint8_t>(input[read++]);
        const std::size_t blockSize = code - 1U; // code nul : blockSize déborde et est refusé
        if (code == 0 || blockSize > input.size() - read || blockSize > output.size() - written) {
            return std::nullopt;
        }
        for (std::size_t index = 0; index < blockSize; ++index) {
            const std::byte byte = input[read++];
            if (byte == zero) {
                return std::nullopt;
            }
            output[written++] = byte;
        }
        // Un bloc plus court que 254 octets est suivi d'un octet nul, sauf le dernier.
        if (code != fullBlockCode && read < input.size()) {
            if (written == output.size()) {
                return std::nullopt;
            }
            output[written++] = zero;
        }
    }
    return written;
}

} // namespace clepsydre::protocol
