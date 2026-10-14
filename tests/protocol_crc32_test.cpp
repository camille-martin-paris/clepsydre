// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

// CRC-32C du protocole de la liaison (#125) : valeur de contrôle du catalogue des CRC et
// vecteurs de la RFC 3720 (annexe B.4).

#include <array>
#include <cstddef>
#include <cstdint>
#include <expect.hpp>
#include <span>
#include <string_view>

import clepsydre.protocol.crc32;

namespace {

using clepsydre::protocol::crc32c;

std::uint32_t crcOf(std::string_view text) {
    return crc32c(std::as_bytes(std::span{text}));
}

std::array<std::byte, 32> filled(auto value) {
    std::array<std::byte, 32> bytes{};
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        bytes.at(index) = std::byte{static_cast<std::uint8_t>(value(index))};
    }
    return bytes;
}

} // namespace

int main() {
    return clepsydre::test::Expectations{}.run([](clepsydre::test::Expectations& t) {
        t.expect(crcOf("123456789") == 0xE306'9283U, "valeur de contrôle de CRC-32C");
        t.expect(crcOf("") == 0U, "suite vide");
        t.expect(crc32c(filled([](std::size_t) { return 0x00; })) == 0x8A91'36AAU,
                 "RFC 3720 : 32 octets nuls");
        t.expect(crc32c(filled([](std::size_t) { return 0xFF; })) == 0x62A8'AB43U,
                 "RFC 3720 : 32 octets 0xFF");
        t.expect(crc32c(filled([](std::size_t index) { return index; })) == 0x46DD'794EU,
                 "RFC 3720 : 32 octets croissants");
        t.expect(crc32c(filled([](std::size_t index) { return 31 - index; })) == 0x113F'DB5CU,
                 "RFC 3720 : 32 octets décroissants");
    });
}
