// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

// Encodage COBS du protocole de la liaison (#125) : exemples de référence de l'algorithme
// (Cheshire et Baker, « Consistent Overhead Byte Stuffing », 1999, forme canonique), blocs
// pleins, refus des encodages invalides et des tampons trop petits.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expect.hpp>
#include <initializer_list>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <vector>

import clepsydre.protocol.cobs;

namespace {

using clepsydre::protocol::cobsDecode;
using clepsydre::protocol::cobsEncode;
using clepsydre::protocol::cobsMaxEncodedSize;
using clepsydre::test::Expectations;

std::vector<std::byte> bytes(std::initializer_list<int> values) {
    std::vector<std::byte> result;
    for (const int value : values) {
        result.push_back(std::byte{static_cast<std::uint8_t>(value)});
    }
    return result;
}

std::vector<std::byte> range(int first, int last) {
    std::vector<std::byte> result;
    for (int value = first; value <= last; ++value) {
        result.push_back(std::byte{static_cast<std::uint8_t>(value)});
    }
    return result;
}

std::vector<std::byte> concat(std::initializer_list<std::vector<std::byte>> parts) {
    std::vector<std::byte> result;
    for (const auto& part : parts) {
        result.insert(result.end(), part.begin(), part.end());
    }
    return result;
}

std::vector<std::byte> encode(const std::vector<std::byte>& input) {
    std::vector<std::byte> output(cobsMaxEncodedSize(input.size()));
    output.resize(cobsEncode(input, output));
    return output;
}

std::optional<std::vector<std::byte>> decode(const std::vector<std::byte>& input,
                                             std::size_t capacity = 512) {
    std::vector<std::byte> output(capacity);
    const std::optional<std::size_t> size = cobsDecode(input, output);
    if (!size) {
        return std::nullopt;
    }
    output.resize(*size);
    return output;
}

void example(Expectations& t, const std::vector<std::byte>& raw,
             const std::vector<std::byte>& coded, const std::string& name) {
    t.expect(encode(raw) == coded, "encodage : " + name);
    t.expect(decode(coded) == raw, "décodage : " + name);
    t.expect(std::ranges::find(coded, std::byte{0}) == coded.end(), "aucun octet nul : " + name);
    t.expect(coded.size() <= cobsMaxEncodedSize(raw.size()), "taille bornée : " + name);
}

void references(Expectations& t) {
    example(t, bytes({}), bytes({0x01}), "suite vide");
    example(t, bytes({0x00}), bytes({0x01, 0x01}), "un octet nul");
    example(t, bytes({0x00, 0x00}), bytes({0x01, 0x01, 0x01}), "deux octets nuls");
    example(t, bytes({0x00, 0x11, 0x00}), bytes({0x01, 0x02, 0x11, 0x01}), "nul encadrant");
    example(t, bytes({0x11, 0x22, 0x00, 0x33}), bytes({0x03, 0x11, 0x22, 0x02, 0x33}),
            "nul au milieu");
    example(t, bytes({0x11, 0x22, 0x33, 0x44}), bytes({0x05, 0x11, 0x22, 0x33, 0x44}), "sans nul");
    example(t, bytes({0x11, 0x00, 0x00, 0x00}), bytes({0x02, 0x11, 0x01, 0x01, 0x01}),
            "nuls finaux");
    // Blocs pleins : 254 octets non nuls forment un bloc de code 0xFF, sans octet nul implicite.
    example(t, range(0x01, 0xFE), concat({bytes({0xFF}), range(0x01, 0xFE)}),
            "bloc plein final, sans code supplémentaire");
    example(t, concat({bytes({0x00}), range(0x01, 0xFE)}),
            concat({bytes({0x01, 0xFF}), range(0x01, 0xFE)}), "nul puis bloc plein");
    example(t, range(0x01, 0xFF), concat({bytes({0xFF}), range(0x01, 0xFE), bytes({0x02, 0xFF})}),
            "bloc plein suivi d'un octet");
    example(t, concat({range(0x02, 0xFF), bytes({0x00})}),
            concat({bytes({0xFF}), range(0x02, 0xFF), bytes({0x01, 0x01})}), "bloc plein puis nul");
    example(t, concat({range(0x03, 0xFF), bytes({0x00, 0x01})}),
            concat({bytes({0xFE}), range(0x03, 0xFF), bytes({0x02, 0x01})}), "bloc presque plein");
}

void invalid(Expectations& t) {
    t.expect(decode(bytes({0x00})) == std::nullopt, "code nul refusé");
    t.expect(decode(bytes({0x03, 0x11})) == std::nullopt, "bloc au-delà de la fin refusé");
    t.expect(decode(bytes({0x03, 0x11, 0x00})) == std::nullopt, "octet nul dans un bloc refusé");
    t.expect(decode(bytes({0x02, 0x11, 0x00, 0x01})) == std::nullopt, "octet nul en code refusé");
    t.expect(decode(bytes({})) == std::vector<std::byte>{}, "suite vide décodée en suite vide");
}

void capacity(Expectations& t) {
    const std::vector<std::byte> raw = bytes({0x11, 0x00, 0x22});
    std::array<std::byte, 3> small{};
    t.expect(cobsEncode(raw, small) == 0, "encodage : tampon trop petit refusé");
    t.expect(std::ranges::all_of(small, [](std::byte b) { return b == std::byte{0}; }),
             "encodage refusé : rien n'est écrit");
    const std::vector<std::byte> coded = encode(raw);
    t.expect(decode(coded, 3) == raw, "décodage : tampon exact");
    t.expect(decode(coded, 2) == std::nullopt, "décodage : tampon trop petit pour un bloc");
    t.expect(decode(coded, 1) == std::nullopt, "décodage : tampon trop petit pour l'octet nul");
    t.expect(cobsMaxEncodedSize(0) == 1 && cobsMaxEncodedSize(254) == 256 &&
                 cobsMaxEncodedSize(508) == 511,
             "borne de l'encodage");
}

} // namespace

int main() {
    return Expectations{}.run([](Expectations& t) {
        references(t);
        invalid(t);
        capacity(t);
    });
}
