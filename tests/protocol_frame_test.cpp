// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

// Trames du protocole de la liaison (#125) : format de l'en-tête, aller-retour par l'encodage et
// la réception octet par octet, bornes, et chaque motif de rejet.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expect.hpp>
#include <expected>
#include <initializer_list>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <vector>

import clepsydre.protocol.cobs;
import clepsydre.protocol.crc32;
import clepsydre.protocol.frame;

namespace {

using namespace clepsydre::protocol;
using clepsydre::test::Expectations;
using Received = std::expected<Frame, FrameError>;

std::vector<std::byte> bytes(std::initializer_list<int> values) {
    std::vector<std::byte> result;
    for (const int value : values) {
        result.push_back(std::byte{static_cast<std::uint8_t>(value)});
    }
    return result;
}

Payload payloadOf(const std::vector<std::byte>& content) {
    return Payload::from(content).value_or(Payload{});
}

// Pousse des octets dans le récepteur et renvoie les résultats obtenus aux délimiteurs.
std::vector<Received> receive(FrameReceiver& receiver, std::span<const std::byte> stream) {
    std::vector<Received> results;
    for (const std::byte byte : stream) {
        if (auto result = receiver.push(byte)) {
            results.push_back(*result);
        }
    }
    return results;
}

bool sameFrame(const Received& received, const Frame& expected) {
    return received.has_value() && received->header.sender == expected.header.sender &&
           received->header.bootNumber == expected.header.bootNumber &&
           received->header.transportNumber == expected.header.transportNumber &&
           received->header.type == expected.header.type &&
           std::ranges::equal(received->payload.bytes(), expected.payload.bytes());
}

bool rejected(const std::vector<Received>& results, FrameError error) {
    return results.size() == 1 && !results.front().has_value() && results.front().error() == error;
}

// Trame décodée construite champ par champ, avec un CRC juste.
std::vector<std::byte> decodedFrame(int version, int sender, int type, int length,
                                    const std::vector<std::byte>& payload) {
    std::vector<std::byte> frame =
        bytes({version, sender, 0x04, 0x03, 0x02, 0x01, 0x08, 0x07, 0x06, 0x05, type, length});
    frame.insert(frame.end(), payload.begin(), payload.end());
    const std::uint32_t crc = crc32c(frame);
    for (int shift = 0; shift < 32; shift += 8) {
        frame.push_back(std::byte{static_cast<std::uint8_t>(crc >> shift)});
    }
    return frame;
}

std::vector<std::byte> encoded(const std::vector<std::byte>& decoded) {
    std::vector<std::byte> stream(cobsMaxEncodedSize(decoded.size()) + 1);
    stream.resize(cobsEncode(decoded, stream));
    stream.push_back(std::byte{0});
    return stream;
}

void layout(Expectations& t) {
    const Frame frame{.header = {.sender = Sender::Safety,
                                 .bootNumber = 0x0102'0304U,
                                 .transportNumber = 0x0506'0708U,
                                 .type = MessageType::Event},
                      .payload = payloadOf(bytes({0xAA, 0x00, 0xBB}))};
    const EncodedFrame encodedFrame{frame};
    const std::span<const std::byte> stream = encodedFrame.bytes();
    t.expect(stream.back() == std::byte{0}, "trame terminée par le délimiteur");
    t.expect(std::ranges::count(stream, std::byte{0}) == 1, "aucun autre octet nul");
    std::array<std::byte, maxFrameSize> decoded{};
    const std::optional<std::size_t> size = cobsDecode(stream.first(stream.size() - 1), decoded);
    // Format fixé par docs/design/link-protocol.md : version, émetteur, démarrage et transport en
    // petit-boutiste, type, longueur, charge utile, CRC-32C en petit-boutiste.
    const std::vector<std::byte> expected = decodedFrame(1, 2, 6, 3, bytes({0xAA, 0x00, 0xBB}));
    t.expect(size.has_value() &&
                 std::ranges::equal(std::span{decoded}.first(size.value_or(0)), expected),
             "octets de la trame conformes au format");
}

void roundTrips(Expectations& t) {
    FrameReceiver receiver;
    std::vector<std::byte> longest(maxPayloadSize);
    for (std::size_t index = 0; index < longest.size(); ++index) {
        longest.at(index) = std::byte{static_cast<std::uint8_t>(index)}; // octets nuls compris
    }
    const std::array frames{
        Frame{.header = {.sender = Sender::Control,
                         .bootNumber = 7,
                         .transportNumber = 0,
                         .type = MessageType::Heartbeat},
              .payload = {}},
        Frame{.header = {.sender = Sender::Safety,
                         .bootNumber = 0xFFFF'FFFFU,
                         .transportNumber = 0xFFFF'FFFFU,
                         .type = MessageType::Test},
              .payload = payloadOf(longest)},
        Frame{.header = {.sender = Sender::Control,
                         .bootNumber = 1,
                         .transportNumber = 2,
                         .type = MessageType::Programming},
              .payload = payloadOf(bytes({0x00, 0x00, 0x00}))},
    };
    std::vector<std::byte> stream;
    for (const Frame& frame : frames) {
        const EncodedFrame encodedFrame{frame};
        t.expect(encodedFrame.bytes().size() <= maxEncodedFrameSize, "taille encodée bornée");
        stream.insert(stream.end(), encodedFrame.bytes().begin(), encodedFrame.bytes().end());
    }
    const std::vector<Received> results = receive(receiver, stream);
    t.expect(results.size() == frames.size(), "une trame par délimiteur");
    for (std::size_t index = 0; index < frames.size() && index < results.size(); ++index) {
        t.expect(sameFrame(results.at(index), frames.at(index)),
                 "trame reconnue à l'identique : " + std::to_string(index));
    }
    t.expect(Payload::from(longest).has_value(), "charge utile maximale admise");
    longest.push_back(std::byte{1});
    t.expect(!Payload::from(longest).has_value(), "charge utile trop longue refusée");
}

void everyType(Expectations& t) {
    FrameReceiver receiver;
    for (int type = 1; type <= 9; ++type) {
        const std::vector<Received> results =
            receive(receiver, encoded(decodedFrame(1, 1, type, 0, {})));
        t.expect(results.size() == 1 && results.front().has_value() &&
                     static_cast<int>(results.front()->header.type) == type,
                 "type reconnu : " + std::to_string(type));
    }
}

void rejections(Expectations& t) {
    FrameReceiver receiver;
    const std::vector<std::byte> payload = bytes({0x11, 0x22});
    t.expect(rejected(receive(receiver, bytes({0x00})), FrameError::TooShort),
             "délimiteurs consécutifs : trame trop courte");
    std::vector<std::byte> shortFrame = decodedFrame(1, 1, 1, 0, {});
    shortFrame.pop_back();
    t.expect(rejected(receive(receiver, encoded(shortFrame)), FrameError::TooShort),
             "trame tronquée : trop courte");
    t.expect(rejected(receive(receiver, bytes({0x05, 0x11, 0x00})), FrameError::InvalidCobs),
             "encodage COBS invalide");

    std::vector<std::byte> altered = encoded(decodedFrame(1, 1, 1, 2, payload));
    altered.at(altered.size() - 7) = std::byte{0x33}; // octet de charge utile, non nul
    t.expect(rejected(receive(receiver, altered), FrameError::BadCrc), "trame altérée : CRC faux");

    t.expect(rejected(receive(receiver, encoded(decodedFrame(2, 1, 1, 2, payload))),
                      FrameError::VersionMismatch),
             "autre version");
    t.expect(rejected(receive(receiver, encoded(decodedFrame(1, 1, 1, 3, payload))),
                      FrameError::LengthMismatch),
             "longueur incohérente");
    for (const int sender : {0, 3}) {
        t.expect(rejected(receive(receiver, encoded(decodedFrame(1, sender, 1, 2, payload))),
                          FrameError::UnknownSender),
                 "émetteur inconnu : " + std::to_string(sender));
    }
    for (const int type : {0, 10}) {
        t.expect(rejected(receive(receiver, encoded(decodedFrame(1, 1, type, 2, payload))),
                          FrameError::UnknownType),
                 "type inconnu : " + std::to_string(type));
    }
    const std::vector<Received> after =
        receive(receiver, encoded(decodedFrame(1, 2, 1, 2, payload)));
    t.expect(after.size() == 1 && after.front().has_value(),
             "le récepteur reste utilisable après les rejets");
}

void bounds(Expectations& t) {
    FrameReceiver receiver;
    // Plus d'octets que la plus longue trame encodée : rejet au délimiteur, puis reprise.
    std::vector<std::byte> flood(maxEncodedFrameSize + 10, std::byte{0x42});
    flood.push_back(std::byte{0});
    t.expect(rejected(receive(receiver, flood), FrameError::TooLong), "flot trop long rejeté");
    const std::vector<Received> next = receive(receiver, encoded(decodedFrame(1, 1, 1, 0, {})));
    t.expect(next.size() == 1 && next.front().has_value(), "reprise après un flot trop long");

    // Exactement la capacité du récepteur, mais décodée au-delà d'une trame : refus du décodage.
    std::vector<std::byte> full(maxEncodedFrameSize - 1, std::byte{0x01});
    full.push_back(std::byte{0});
    t.expect(rejected(receive(receiver, full), FrameError::InvalidCobs),
             "trame décodée trop longue refusée");

    // Octets reçus avant le premier délimiteur : trame incomplète rejetée, la suivante reconnue.
    FrameReceiver late;
    std::vector<std::byte> stream = bytes({0x07, 0x42});
    const std::vector<std::byte> frame = encoded(decodedFrame(1, 1, 1, 0, {}));
    stream.insert(stream.end(), frame.begin(), frame.end());
    const std::vector<Received> results = receive(late, stream);
    t.expect(results.size() == 1 && !results.front().has_value(),
             "début de flux incomplet rejeté sans perdre la synchronisation");
    const std::vector<Received> after = receive(late, frame);
    t.expect(after.size() == 1 && after.front().has_value(), "trame suivante reconnue");

    const std::vector<std::byte> tooLong(maxFrameSize + 1, std::byte{0x01});
    t.expect(!parseFrame(tooLong).has_value() && parseFrame(tooLong).error() == FrameError::TooLong,
             "parseFrame : trame décodée trop longue");
}

} // namespace

int main() {
    return Expectations{}.run([](Expectations& t) {
        layout(t);
        roundTrips(t);
        everyType(t);
        rejections(t);
        bounds(t);
    });
}
