// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

// Tests de l'injection de défauts sur la liaison (#21, ADR 0006). Ils vérifient que chaque
// défaut est injecté comme annoncé ; ils ne vérifient aucune exigence de la pompe.

#include <algorithm>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expect.hpp>
#include <format>
#include <vector>

import clepsydre.sim.link_faults;

namespace {

using namespace std::chrono_literals;
using clepsydre::sim::FaultyChannel;
using clepsydre::sim::FrameFault;
using clepsydre::sim::SerialChannel;
using clepsydre::test::Expectations;

constexpr std::int64_t baud = 115'200;
constexpr std::chrono::nanoseconds latency = 10us;

// Trame de size octets : contenu non nul marqué par tag, puis le délimiteur.
std::vector<std::byte> frame(std::size_t size, std::uint8_t tag) {
    std::vector<std::byte> bytes(size, std::byte{tag});
    bytes.back() = FaultyChannel::delimiter;
    return bytes;
}

void transparent(Expectations& t) {
    // Sans règle, l'injecteur ne change ni les octets ni leurs instants d'arrivée.
    SerialChannel line{baud, latency};
    FaultyChannel channel{line, 1};
    SerialChannel reference{baud, latency};
    for (std::uint8_t i = 1; i <= 3; ++i) {
        channel.send(std::chrono::milliseconds{i}, frame(8, i));
        reference.send(std::chrono::milliseconds{i}, frame(8, i));
    }
    bool same = true;
    for (auto now = 0us; now <= 10ms; now += 10us) {
        same = same && channel.receive(now) == reference.receive(now);
    }
    t.expect(same, "injecteur sans règle transparent, instants compris");
}

void dropWindow(Expectations& t) {
    SerialChannel line{baud, latency};
    FaultyChannel channel{line, 1};
    channel.add({.fault = FrameFault::drop, .from = 1s, .until = 2s});
    channel.send(500ms, frame(4, 1));
    channel.send(1500ms, frame(4, 2));
    channel.send(2500ms, frame(4, 3));
    const auto received = channel.receive(3s);
    t.expect(received.size() == 8 && received[0] == std::byte{1} && received[4] == std::byte{3},
             "seule la trame de la fenêtre est perdue");
    t.expect(channel.injected(FrameFault::drop) == 1, "une perte injectée");
}

void duplicate(Expectations& t) {
    SerialChannel line{baud, latency};
    FaultyChannel channel{line, 1};
    channel.add({.fault = FrameFault::duplicate});
    channel.send(0ns, frame(5, 7));
    const auto received = channel.receive(1s);
    std::vector<std::byte> expected = frame(5, 7);
    expected.insert(expected.end(), expected.begin(), expected.end());
    t.expect(received == expected, "trame émise deux fois");
}

void corrupt(Expectations& t) {
    SerialChannel line{baud, latency};
    FaultyChannel channel{line, 1};
    channel.add({.fault = FrameFault::corrupt});
    const auto sent = frame(12, 0x5a);
    channel.send(0ns, sent);
    const auto received = channel.receive(1s);
    int flipped = 0;
    for (std::size_t i = 0; i < sent.size() && i < received.size(); ++i) {
        flipped += std::popcount(std::to_integer<unsigned>(sent[i] ^ received[i]));
    }
    t.expect(received.size() == sent.size(), "même longueur");
    t.expect(flipped == 1, std::format("un seul bit inversé ({})", flipped));
    t.expect(received.back() == FaultyChannel::delimiter, "délimiteur conservé");
}

void truncate(Expectations& t) {
    SerialChannel line{baud, latency};
    FaultyChannel channel{line, 1};
    channel.add({.fault = FrameFault::truncate});
    channel.send(0ns, frame(10, 3));
    const auto received = channel.receive(1s);
    t.expect(received.size() == 6 && received.back() == FaultyChannel::delimiter,
             std::format("trame tronquée, délimiteur conservé ({} octets)", received.size()));
}

void delay(Expectations& t) {
    SerialChannel line{baud, latency};
    FaultyChannel channel{line, 1};
    // Seule la première trame est retardée de 100 ms ; la suivante la double.
    channel.add({.fault = FrameFault::delay, .from = 0ns, .until = 1ns, .delay = 100ms});
    channel.send(0ns, frame(4, 1));
    channel.send(10ms, frame(4, 2));
    const auto early = channel.receive(99ms);
    t.expect(early.size() == 4 && early[0] == std::byte{2}, "la trame suivante arrive d'abord");
    const auto late = channel.receive(200ms);
    t.expect(late.size() == 4 && late[0] == std::byte{1}, "la trame retardée arrive ensuite");
}

void reorder(Expectations& t) {
    SerialChannel line{baud, latency};
    FaultyChannel channel{line, 1};
    channel.add({.fault = FrameFault::reorder, .from = 0ns, .until = 1ns});
    channel.send(0ns, frame(4, 1));
    channel.send(10ms, frame(4, 2));
    const auto received = channel.receive(1s);
    t.expect(received.size() == 8 && received[0] == std::byte{2} && received[4] == std::byte{1},
             "deux trames permutées");
}

std::size_t dropsWithSeed(std::uint64_t seed, std::size_t& receivedFrames) {
    SerialChannel line{baud, latency};
    FaultyChannel channel{line, seed};
    channel.add({.fault = FrameFault::drop, .perMillion = 100'000});
    receivedFrames = 0;
    for (int i = 0; i < 10'000; ++i) {
        channel.send(std::chrono::milliseconds{i}, frame(4, 1));
        const auto bytes = channel.receive(std::chrono::milliseconds{i + 1});
        receivedFrames += static_cast<std::size_t>(std::ranges::count(bytes, std::byte{0}));
    }
    return channel.injected(FrameFault::drop);
}

void probabilityAndDeterminism(Expectations& t) {
    std::size_t received = 0;
    const std::size_t drops = dropsWithSeed(42, received);
    t.expect(drops > 900 && drops < 1'100,
             std::format("10 % de pertes sur 10 000 trames ({})", drops));
    t.expect(received + drops == 10'000, "chaque trame est reçue ou comptée comme perdue");
    std::size_t again = 0;
    t.expect(dropsWithSeed(42, again) == drops && again == received, "même graine, mêmes défauts");
}

} // namespace

int main() {
    return Expectations{}.run([](Expectations& t) {
        transparent(t);
        dropWindow(t);
        duplicate(t);
        corrupt(t);
        truncate(t);
        delay(t);
        reorder(t);
        probabilityAndDeterminism(t);
    });
}
