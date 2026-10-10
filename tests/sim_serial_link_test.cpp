// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

// Tests de la liaison série simulée (#21, ADR 0006). Ils vérifient le modèle de la liaison ;
// ils ne vérifient aucune exigence de la pompe.

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expect.hpp>
#include <format>
#include <vector>

import clepsydre.sim.serial_link;

namespace {

using namespace std::chrono_literals;
using clepsydre::sim::SerialChannel;
using clepsydre::sim::SerialLink;
using clepsydre::test::Expectations;

constexpr std::int64_t baud = 115'200;
constexpr std::chrono::nanoseconds latency = 10us;

std::vector<std::byte> sequence(std::size_t count) {
    std::vector<std::byte> bytes(count);
    for (std::size_t i = 0; i < count; ++i) {
        bytes[i] = static_cast<std::byte>(i);
    }
    return bytes;
}

void byteTime(Expectations& t) {
    const SerialChannel channel{baud, latency};
    // 10 bits à 115 200 bit/s : 86,806 µs, arrondis à la nanoseconde supérieure.
    t.expect(channel.byteTime() == 86'806ns,
             std::format("durée d'un octet ({})", channel.byteTime()));
}

void singleByte(Expectations& t) {
    SerialChannel channel{baud, latency};
    const std::array<std::byte, 1> one{std::byte{0x5a}};
    channel.send(1ms, one);
    const auto arrival = 1ms + channel.byteTime() + latency;
    t.expect(channel.receive(arrival - 1ns).empty(), "rien avant la fin de l'octet et la latence");
    const auto received = channel.receive(arrival);
    t.expect(received.size() == 1 && received[0] == std::byte{0x5a}, "octet reçu à l'heure");
    t.expect(channel.inFlight() == 0, "plus rien sur la ligne");
}

void burstAtLineRate(Expectations& t) {
    SerialChannel channel{baud, latency};
    channel.send(0ns, sequence(100));
    t.expect(channel.inFlight() == 100, "cent octets en cours d'émission");
    const auto half = channel.receive((channel.byteTime() * 50) + latency);
    t.expect(half.size() == 50, std::format("débit de la ligne respecté ({} octets)", half.size()));
    const auto rest = channel.receive((channel.byteTime() * 100) + latency);
    t.expect(rest.size() == 50, "le reste arrive à la fin de l'émission");
    bool ordered = true;
    for (std::size_t i = 0; i < rest.size(); ++i) {
        ordered = ordered && rest[i] == static_cast<std::byte>(50 + i);
    }
    t.expect(ordered && half[0] == std::byte{0}, "ordre d'émission conservé");
}

void busyLine(Expectations& t) {
    SerialChannel channel{baud, latency};
    channel.send(0ns, sequence(10));
    // Un octet confié pendant l'émission attend que la ligne se libère.
    const std::array<std::byte, 1> late{std::byte{0xff}};
    channel.send(channel.byteTime(), late);
    t.expect(channel.receive((channel.byteTime() * 11) + latency - 1ns).size() == 10,
             "l'octet suivant attend la ligne");
    t.expect(channel.receive((channel.byteTime() * 11) + latency).size() == 1,
             "puis part à la suite");
    // Ligne libre : l'émission commence aussitôt.
    channel.send(1s, late);
    t.expect(channel.receive(1s + channel.byteTime() + latency).size() == 1,
             "ligne libre : pas d'attente");
}

void independentDirections(Expectations& t) {
    SerialLink link{.controlToSafety = SerialChannel{baud, latency},
                    .safetyToControl = SerialChannel{baud, latency}};
    link.controlToSafety.send(0ns, sequence(3));
    t.expect(link.safetyToControl.receive(1s).empty(), "un sens n'alimente pas l'autre");
    t.expect(link.controlToSafety.receive(1s).size() == 3, "le sens émis reçoit tout");
}

} // namespace

int main() {
    return Expectations{}.run([](Expectations& t) {
        byteTime(t);
        singleByte(t);
        burstAtLineRate(t);
        busyLine(t);
        independentDirections(t);
    });
}
