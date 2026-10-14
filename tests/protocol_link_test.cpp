// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

// Surveillance de la liaison entre les processeurs (#125, SYS-REQ-053) : signal de vie,
// numérotation à l'émission, et chaque transition du moniteur de réception.

#include <chrono>
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

import clepsydre.protocol.frame;
import clepsydre.protocol.link;

namespace {

using namespace std::chrono_literals;
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

Received frame(std::uint32_t transport, std::uint32_t boot = 7, Sender sender = Sender::Control) {
    return Frame{.header = {.sender = sender,
                            .bootNumber = boot,
                            .transportNumber = transport,
                            .type = MessageType::Heartbeat},
                 .payload = {}};
}

Received rejected(FrameError error) {
    return std::unexpected{error};
}

// Reconnaît une trame encodée, octet par octet.
std::optional<Frame> parse(const EncodedFrame& encoded) {
    FrameReceiver receiver;
    std::optional<Frame> result;
    for (const std::byte byte : encoded.bytes()) {
        if (auto received = receiver.push(byte); received && received->has_value()) {
            result = **received;
        }
    }
    return result;
}

void heartbeats(Expectations& t) {
    for (const std::optional<std::uint32_t> last :
         {std::optional<std::uint32_t>{}, std::optional<std::uint32_t>{0},
          std::optional<std::uint32_t>{0xFFFF'FFFFU}}) {
        const Heartbeat beat{.state = 0xA5, .lastReceived = last};
        const std::optional<Heartbeat> decoded = decodeHeartbeat(encodeHeartbeat(beat).bytes());
        t.expect(decoded.has_value() && decoded->state == 0xA5 && decoded->lastReceived == last,
                 "signal de vie : aller-retour");
    }
    t.expect(std::ranges::equal(encodeHeartbeat({.state = 3, .lastReceived = 0x0102'0304U}).bytes(),
                                bytes({3, 1, 4, 3, 2, 1})),
             "signal de vie : format");
    t.expect(!decodeHeartbeat(bytes({3, 0, 0, 0, 0})), "taille trop courte refusée");
    t.expect(!decodeHeartbeat(bytes({3, 0, 0, 0, 0, 0, 0})), "taille trop longue refusée");
    t.expect(!decodeHeartbeat(bytes({3, 2, 0, 0, 0, 0})), "indicateur inconnu refusé");
    t.expect(!decodeHeartbeat(bytes({3, 0, 1, 0, 0, 0})), "numéro sans réception refusé");
}

void transmitter(Expectations& t) {
    LinkTransmitter link{Sender::Safety, 42};
    t.expect(link.heartbeatDue(0ms), "signal de vie dû au démarrage");
    const std::optional<Frame> first = parse(link.heartbeat(0ms, {.state = 1, .lastReceived = {}}));
    t.expect(first && first->header.sender == Sender::Safety && first->header.bootNumber == 42 &&
                 first->header.transportNumber == 0 && first->header.type == MessageType::Heartbeat,
             "premier signal de vie : numéro de transport 0");
    t.expect(first && decodeHeartbeat(first->payload.bytes()).has_value(),
             "charge utile décodable");
    t.expect(!link.heartbeatDue(49ms) && link.heartbeatDue(50ms), "période de 50 ms");
    const std::optional<Frame> other = parse(link.encode(MessageType::Echo, {}));
    t.expect(other && other->header.transportNumber == 1 && other->header.type == MessageType::Echo,
             "toute trame reçoit le numéro suivant");
    t.expect(link.heartbeatDue(50ms), "une autre trame ne remplace pas le signal de vie");
    t.expect(link.nextTransportNumber() == 2, "numéro suivant");
}

void establishment(Expectations& t) {
    LinkMonitor monitor{Sender::Control, 1s};
    t.expect(monitor.state() == LinkState::Waiting && monitor.fault() == LinkFault::None &&
                 !monitor.peerBootNumber(),
             "attente à la mise en route");
    monitor.onReceived(1010ms, rejected(FrameError::BadCrc));
    monitor.onReceived(1020ms, frame(0, 7, Sender::Safety));
    t.expect(monitor.state() == LinkState::Waiting && monitor.losses() == 0,
             "avant établissement : rejets et autre émetteur ignorés");
    monitor.poll(1199ms);
    t.expect(monitor.state() == LinkState::Waiting, "délai non écoulé");
    monitor.poll(1200ms);
    t.expect(monitor.state() == LinkState::Lost && monitor.fault() == LinkFault::Timeout &&
                 monitor.losses() == 1,
             "aucune trame en 200 ms après la mise en route : perte");

    LinkMonitor fresh{Sender::Control, 0ms};
    fresh.onReceived(10ms, frame(5));
    t.expect(fresh.state() == LinkState::Up && fresh.peerBootNumber() == 7, "lien établi");
    fresh.onReceived(60ms, frame(6));
    fresh.onReceived(110ms, frame(7));
    fresh.poll(309ms);
    t.expect(fresh.state() == LinkState::Up && fresh.losses() == 0,
             "trames en séquence : le délai repart de la dernière");
    fresh.poll(310ms);
    t.expect(fresh.state() == LinkState::Lost && fresh.fault() == LinkFault::Timeout,
             "200 ms sans trame valide : perte");
}

// Établit un lien sur la trame transport, puis lui soumet received.
LinkMonitor after(std::uint32_t transport, const Received& received) {
    LinkMonitor monitor{Sender::Control, 0ms};
    monitor.onReceived(10ms, frame(transport));
    monitor.onReceived(20ms, received);
    return monitor;
}

bool lostBy(const LinkMonitor& monitor, LinkFault fault) {
    return monitor.state() == LinkState::Lost && monitor.fault() == fault && monitor.losses() == 1;
}

void anomalies(Expectations& t) {
    const LinkMonitor rejectedFrame = after(5, rejected(FrameError::BadCrc));
    t.expect(lostBy(rejectedFrame, LinkFault::Rejected) &&
                 rejectedFrame.rejection() == FrameError::BadCrc,
             "trame rejetée : perte, motif conservé");
    t.expect(lostBy(after(5, frame(6, 7, Sender::Safety)), LinkFault::WrongSender),
             "émetteur inattendu");
    t.expect(lostBy(after(5, frame(6, 8)), LinkFault::BootChanged), "numéro de démarrage changé");
    t.expect(lostBy(after(5, frame(5)), LinkFault::TransportRepeated), "numéro répété");
    t.expect(lostBy(after(5, frame(4)), LinkFault::TransportBackward), "numéro en arrière");
    t.expect(lostBy(after(5, frame(7)), LinkFault::TransportGap), "numéro manquant");
    t.expect(lostBy(after(0, frame(0x8000'0000U)), LinkFault::TransportBackward),
             "écart d'une demi-plage : en arrière");
    t.expect(lostBy(after(0, frame(0x7FFF'FFFFU)), LinkFault::TransportGap),
             "écart juste inférieur à une demi-plage : en avance");
    t.expect(after(0xFFFF'FFFFU, frame(0)).state() == LinkState::Up, "passage de 2^32 - 1 à 0");
    t.expect(lostBy(after(0xFFFF'FFFFU, frame(1)), LinkFault::TransportGap),
             "trame manquante au passage par 0");
    t.expect(lostBy(after(0, frame(0xFFFF'FFFFU)), LinkFault::TransportBackward),
             "retour en arrière au passage par 0");
    t.expect(after(5, frame(6)).rejection() == std::nullopt, "aucun motif sans rejet");
}

void recovery(Expectations& t) {
    LinkMonitor monitor = after(5, frame(9));
    monitor.onReceived(30ms, rejected(FrameError::TooShort));
    monitor.onReceived(40ms, frame(10, 7, Sender::Safety));
    monitor.poll(1s);
    t.expect(lostBy(monitor, LinkFault::TransportGap),
             "après une perte : rejets, autre émetteur et délai sans effet");
    monitor.onReceived(1010ms, frame(0, 8));
    t.expect(monitor.state() == LinkState::Up && monitor.peerBootNumber() == 8 &&
                 monitor.fault() == LinkFault::TransportGap && monitor.losses() == 1,
             "rétabli par une trame valide, nouveau démarrage de l'autre processeur accepté");
    monitor.onReceived(1020ms, frame(1, 8));
    t.expect(monitor.state() == LinkState::Up, "séquence reprise depuis le rétablissement");

    // Trame reçue après un silence : la perte est constatée avant que la trame ne rétablisse.
    LinkMonitor late{Sender::Control, 0ms};
    late.onReceived(10ms, frame(1));
    late.onReceived(500ms, frame(2));
    t.expect(late.state() == LinkState::Up && late.fault() == LinkFault::Timeout &&
                 late.losses() == 1,
             "silence constaté à la réception suivante");
}

void versions(Expectations& t) {
    for (const int setup : {0, 1, 2}) {
        LinkMonitor monitor{Sender::Control, 0ms};
        if (setup >= 1) {
            monitor.onReceived(10ms, frame(1)); // établi
        }
        if (setup == 2) {
            monitor.onReceived(20ms, frame(3)); // perdu
        }
        monitor.onReceived(30ms, rejected(FrameError::VersionMismatch));
        t.expect(monitor.state() == LinkState::OutOfService &&
                     monitor.fault() == LinkFault::VersionMismatch,
                 "autre version : hors service, depuis l'état " + std::to_string(setup));
        const std::uint32_t losses = monitor.losses();
        monitor.onReceived(40ms, frame(4));
        monitor.onReceived(50ms, rejected(FrameError::BadCrc));
        monitor.poll(10s);
        t.expect(monitor.state() == LinkState::OutOfService &&
                     monitor.fault() == LinkFault::VersionMismatch && monitor.losses() == losses,
                 "hors service définitif");
    }
}

} // namespace

int main() {
    return Expectations{}.run([](Expectations& t) {
        heartbeats(t);
        transmitter(t);
        establishment(t);
        anomalies(t);
        recovery(t);
        versions(t);
    });
}
