// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

// Tests des processeurs simulés et de leurs défauts (#21, ADR 0003, ADR 0006) : blocage,
// réinitialisation, arrêt de l'oscillateur, et défauts de la liaison vus de l'autre processeur.
// Les logiciels sont des bouchons de test qui n'échangent que des signaux de vie : ces tests
// vérifient le simulateur, aucune exigence de la pompe.

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expect.hpp>
#include <format>
#include <vector>

import clepsydre.sim.link_faults;
import clepsydre.sim.processor;
import clepsydre.sim.simulation;

namespace {

using namespace std::chrono_literals;
using clepsydre::sim::FaultyChannel;
using clepsydre::sim::FrameFault;
using clepsydre::sim::MonotonicClock;
using clepsydre::sim::Processor;
using clepsydre::sim::SerialChannel;
using clepsydre::sim::Simulation;
using clepsydre::sim::TrueTime;
using clepsydre::test::Expectations;

constexpr std::int64_t baud = 115'200;
constexpr std::chrono::nanoseconds latency = 10us;
constexpr std::chrono::nanoseconds beatPeriod = 50ms; // ADR 0006 § Règles
constexpr std::size_t frameSize = 15;

void lifecycle(Expectations& t) {
    SerialChannel tx{baud, latency};
    SerialChannel rx{baud, latency};
    int boots = 0;
    int runs = 0;
    Processor processor{MonotonicClock{3s, 0},
                        tx,
                        rx,
                        {.boot = [&](Processor&) { ++boots; }, .run = [&](Processor&) { ++runs; }}};
    processor.step(1ms);
    t.expect(boots == 1 && runs == 1 && processor.bootNumber() == 1, "démarrage au premier pas");
    processor.step(11ms);
    t.expect(processor.monotonicNow() == 10ms, "compteur à zéro au démarrage");

    processor.hang();
    processor.step(20ms);
    t.expect(runs == 2 && processor.state() == Processor::State::hung, "bloqué : plus d'exécution");
    t.expect(processor.monotonicNow() == 19ms, "le compteur avance pendant le blocage");

    // Octets arrivés pendant l'arrêt : perdus au redémarrage.
    const std::vector<std::byte> lost(4, std::byte{1});
    rx.send(20ms, lost);
    processor.reset(5ms);
    processor.step(22ms);
    t.expect(runs == 2, "pas d'exécution pendant la réinitialisation");
    processor.step(25ms);
    t.expect(boots == 2 && runs == 3 && processor.bootNumber() == 2,
             "redémarrage avec un nouveau numéro de démarrage");
    t.expect(processor.monotonicNow() == 0ns, "le compteur repart de zéro");
    t.expect(processor.receive().empty(), "octets reçus pendant l'arrêt perdus");
}

void frozenClock(Expectations& t) {
    SerialChannel tx{baud, latency};
    SerialChannel rx{baud, latency};
    Processor processor{MonotonicClock{0ns, 0}, tx, rx, {}};
    processor.step(1ms);
    processor.step(10ms);
    processor.freezeClock();
    processor.step(50ms);
    t.expect(processor.monotonicNow() == 9ms, "compteur figé par l'arrêt de l'oscillateur");
    processor.reset(1ms);
    processor.step(60ms);
    processor.step(70ms);
    t.expect(processor.monotonicNow() == 0ns, "toujours figé après un redémarrage");
}

// Chiffres en base 255 décalés de 1 : aucun octet nul dans le contenu d'une trame.
std::byte digit(std::uint32_t value, std::size_t position) {
    std::uint32_t rest = value;
    for (std::size_t i = 0; i < position; ++i) {
        rest /= 255;
    }
    return static_cast<std::byte>((rest % 255) + 1);
}

std::uint32_t number(const std::vector<std::byte>& frame) {
    std::uint32_t value = 0;
    for (std::size_t position = 3; position > 0; --position) {
        value = (value * 255) + (std::to_integer<std::uint32_t>(frame.at(position)) - 1);
    }
    return value;
}

// Ce qu'un processeur observe des signaux de vie de l'autre, sur son propre compteur.
// L'observation survit aux redémarrages : c'est un instrument de test, pas le logiciel.
struct Observation {
    std::vector<std::byte> partial;
    bool heard = false;
    std::uint32_t lastBoot = 0;
    std::uint32_t lastSequence = 0;
    std::size_t frames = 0;
    std::size_t missing = 0;
    std::size_t repeats = 0;
    std::size_t bootChanges = 0;
    std::size_t malformed = 0;
    std::chrono::nanoseconds lastHeard{};
    std::chrono::nanoseconds maxGap{};
};

// Bouchon de logiciel : un signal de vie toutes les 50 ms de son compteur, avec son numéro de
// démarrage et un numéro de trame qui repart de zéro à chaque démarrage.
class Heartbeat {
public:
    Processor::Firmware firmware() {
        return {.boot =
                    [this](Processor&) {
                        nextBeat_ = 0ns;
                        sequence_ = 0;
                    },
                .run = [this](Processor& processor) { run(processor); }};
    }

    [[nodiscard]] const Observation& seen() const noexcept { return seen_; }

private:
    void run(Processor& processor) {
        const auto now = processor.monotonicNow();
        if (now >= nextBeat_) {
            std::vector<std::byte> frame(frameSize, std::byte{0xa5});
            frame[0] = static_cast<std::byte>(processor.bootNumber());
            for (std::size_t position = 0; position < 3; ++position) {
                frame.at(1 + position) = digit(sequence_, position);
            }
            frame.back() = FaultyChannel::delimiter;
            processor.send(frame);
            ++sequence_;
            nextBeat_ += beatPeriod;
        }
        for (const std::byte value : processor.receive()) {
            seen_.partial.push_back(value);
            if (value == FaultyChannel::delimiter) {
                account(seen_.partial, now);
                seen_.partial.clear();
            }
        }
    }

    void account(const std::vector<std::byte>& frame, std::chrono::nanoseconds now) {
        if (frame.size() != frameSize) {
            ++seen_.malformed;
            return;
        }
        const auto boot = std::to_integer<std::uint32_t>(frame[0]);
        const std::uint32_t sequenceNumber = number(frame);
        if (seen_.heard) {
            seen_.maxGap = std::max(seen_.maxGap, now - seen_.lastHeard);
            if (boot != seen_.lastBoot) {
                ++seen_.bootChanges;
            } else if (sequenceNumber == seen_.lastSequence) {
                ++seen_.repeats;
            } else if (sequenceNumber > seen_.lastSequence) {
                seen_.missing += sequenceNumber - seen_.lastSequence - 1;
            }
        }
        seen_.heard = true;
        seen_.lastHeard = now;
        seen_.lastBoot = boot;
        seen_.lastSequence = sequenceNumber;
        ++seen_.frames;
    }

    std::chrono::nanoseconds nextBeat_{};
    std::uint32_t sequence_ = 0;
    Observation seen_;
};

// Deux processeurs reliés par une liaison munie d'injecteurs, dans un monde simulé.
struct Bench {
    SerialChannel controlLine{baud, latency};
    SerialChannel safetyLine{baud, latency};
    FaultyChannel controlToSafety{controlLine, 7};
    FaultyChannel safetyToControl{safetyLine, 11};
    Heartbeat controlSoftware;
    Heartbeat safetySoftware;
    Processor control{MonotonicClock{0ns, 50'000}, controlToSafety, safetyToControl,
                      controlSoftware.firmware()};
    Processor safety{MonotonicClock{2s, -50'000}, safetyToControl, controlToSafety,
                     safetySoftware.firmware()};
    Simulation world;
};

// Inscrit les deux processeurs dans le monde simulé.
void connect(Bench& bench) {
    bench.world.add([&bench](TrueTime now, std::chrono::nanoseconds) { bench.control.step(now); });
    bench.world.add([&bench](TrueTime now, std::chrono::nanoseconds) { bench.safety.step(now); });
}

// Silence de la sécurité mesuré par la commande, à l'instant présent.
std::chrono::nanoseconds safetySilence(const Bench& bench) {
    return bench.control.monotonicNow() - bench.controlSoftware.seen().lastHeard;
}

void linkFaultsAccounted(Expectations& t) {
    // Pertes et doublons à 1 % de la commande vers la sécurité : la sécurité retrouve chacun
    // d'eux dans les numéros de trame, aucun n'est silencieux.
    Bench bench;
    connect(bench);
    bench.controlToSafety.add({.fault = FrameFault::drop, .perMillion = 10'000});
    bench.controlToSafety.add({.fault = FrameFault::duplicate, .perMillion = 10'000});
    bench.world.run(2min);
    const Observation& seen = bench.safetySoftware.seen();
    const std::size_t drops = bench.controlToSafety.injected(FrameFault::drop);
    const std::size_t duplicates = bench.controlToSafety.injected(FrameFault::duplicate);
    t.expect(drops > 0 && duplicates > 0,
             std::format("défauts injectés ({} pertes, {} doublons)", drops, duplicates));
    t.expect(seen.missing == drops,
             std::format("pertes retrouvées ({} pour {})", seen.missing, drops));
    t.expect(seen.repeats == duplicates,
             std::format("doublons retrouvés ({} pour {})", seen.repeats, duplicates));
    t.expect(bench.controlSoftware.seen().missing == 0, "l'autre sens n'est pas touché");
}

void truncationVisible(Expectations& t) {
    // Trames tronquées : elles n'ont plus la longueur attendue.
    Bench bench;
    connect(bench);
    bench.safetyToControl.add({.fault = FrameFault::truncate, .perMillion = 50'000});
    bench.world.run(1min);
    const std::size_t truncated = bench.safetyToControl.injected(FrameFault::truncate);
    t.expect(truncated > 0 && bench.controlSoftware.seen().malformed == truncated,
             std::format("trames tronquées retrouvées ({} pour {})",
                         bench.controlSoftware.seen().malformed, truncated));
}

void resetSeenByTheOtherSide(Expectations& t) {
    Bench bench;
    connect(bench);
    bench.world.run(1min);
    bench.safety.reset(300ms);
    bench.world.run(1min);
    const Observation& seen = bench.controlSoftware.seen();
    t.expect(bench.safety.bootNumber() == 2, "la sécurité a redémarré");
    t.expect(seen.bootChanges == 1, "la commande voit changer le numéro de démarrage");
    t.expect(seen.maxGap > 200ms,
             std::format("silence de {} pendant la réinitialisation", seen.maxGap));
}

void hangSeenByTheOtherSide(Expectations& t) {
    Bench bench;
    connect(bench);
    bench.world.run(1min);
    bench.safety.hang();
    bench.world.run(10s);
    t.expect(safetySilence(bench) > 9s,
             std::format("silence de {} depuis le blocage", safetySilence(bench)));
}

void frozenClockSeenByTheOtherSide(Expectations& t) {
    // Oscillateur de la sécurité arrêté : son logiciel tourne, mais son compteur n'avance plus
    // et aucun signal de vie ne part. La commande le voit sur sa propre horloge.
    Bench bench;
    connect(bench);
    bench.world.run(1min);
    bench.safety.freezeClock();
    bench.world.run(10s);
    t.expect(safetySilence(bench) > 9s,
             std::format("silence de {} depuis l'arrêt de l'oscillateur", safetySilence(bench)));
}

void deterministic(Expectations& t) {
    auto scenario = [] {
        Bench bench;
        connect(bench);
        connect(bench);
        bench.controlToSafety.add({.fault = FrameFault::drop, .perMillion = 10'000});
        bench.controlToSafety.add({.fault = FrameFault::reorder, .perMillion = 10'000});
        bench.world.run(1min);
        return std::vector<std::size_t>{bench.safetySoftware.seen().frames,
                                        bench.safetySoftware.seen().missing,
                                        bench.controlToSafety.injected(FrameFault::reorder)};
    };
    t.expect(scenario() == scenario(), "deux exécutions identiques donnent le même résultat");
}

} // namespace

int main() {
    return Expectations{}.run([](Expectations& t) {
        lifecycle(t);
        frozenClock(t);
        linkFaultsAccounted(t);
        truncationVisible(t);
        resetSeenByTheOtherSide(t);
        hangSeenByTheOtherSide(t);
        frozenClockSeenByTheOtherSide(t);
        deterministic(t);
    });
}
