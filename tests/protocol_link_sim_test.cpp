// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

// Intégration du protocole de la liaison avec la liaison et les processeurs simulés (#125,
// ADR 0006, SYS-REQ-053) : le processeur de commande émet ses signaux de vie, le processeur de
// sécurité les surveille. Chaque défaut de trame injecté par le simulateur, et chaque défaut du
// processeur émetteur, doit être détecté comme perte de lien en moins de linkTimeout +
// heartbeatPeriod, avec la bonne cause ; après un défaut transitoire, le lien se rétablit.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expect.hpp>
#include <expected>
#include <functional>
#include <optional>
#include <string>

import clepsydre.protocol.frame;
import clepsydre.protocol.link;
import clepsydre.sim.clock;
import clepsydre.sim.link_faults;
import clepsydre.sim.processor;
import clepsydre.sim.serial_link;
import clepsydre.sim.simulation;

namespace {

using namespace std::chrono_literals;
using namespace clepsydre::protocol;
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
constexpr TrueTime faultAt = 2s;
// Délai de détection attendu : délai de perte de lien, plus une période de signal de vie.
constexpr std::chrono::nanoseconds detectionBound = linkTimeout + heartbeatPeriod;

// Deux processeurs reliés : la commande émet sur une ligne précédée de l'injecteur de défauts,
// la sécurité reçoit et surveille.
class Bench {
public:
    Bench()
        : control_{MonotonicClock{0ns, 0}, faulty_, safetyLine_, controlFirmware()},
          safety_{MonotonicClock{0ns, 0}, safetyLine_, faulty_, safetyFirmware()} {
        simulation_.add([this](TrueTime now, std::chrono::nanoseconds) {
            control_.step(now);
            safety_.step(now);
            observe(now);
        });
    }

    void run(std::chrono::nanoseconds duration) { simulation_.run(duration); }

    FaultyChannel& line() { return faulty_; }

    Processor& control() { return control_; }

    [[nodiscard]] const LinkMonitor& monitor() const { return monitor_; }

    // Instant et cause de la première perte de lien.
    [[nodiscard]] std::optional<TrueTime> firstLossAt() const { return firstLossAt_; }

    [[nodiscard]] LinkFault firstFault() const { return firstFault_; }

private:
    Processor::Firmware controlFirmware() {
        return {.boot =
                    [this](Processor& p) {
                        transmitter_ = LinkTransmitter{Sender::Control, p.bootNumber()};
                    },
                .run =
                    [this](Processor& p) {
                        if (transmitter_.heartbeatDue(p.monotonicNow())) {
                            p.send(transmitter_.heartbeat(p.monotonicNow(), {}).bytes());
                        }
                    }};
    }

    Processor::Firmware safetyFirmware() {
        return {.boot =
                    [this](Processor& p) {
                        // Le démarrage repart d'un récepteur et d'un moniteur neufs.
                        receiver_ = FrameReceiver{};
                        monitor_ = LinkMonitor{Sender::Control, p.monotonicNow()};
                    },
                .run =
                    [this](Processor& p) {
                        for (const std::byte byte : p.receive()) {
                            if (auto received = receiver_.push(byte)) {
                                monitor_.onReceived(p.monotonicNow(), *received);
                            }
                        }
                        monitor_.poll(p.monotonicNow());
                    }};
    }

    void observe(TrueTime now) {
        if (!firstLossAt_ && monitor_.losses() > 0) {
            firstLossAt_ = now;
            firstFault_ = monitor_.fault();
        }
    }

    SerialChannel controlLine_{baud, latency};
    SerialChannel safetyLine_{baud, latency};
    FaultyChannel faulty_{controlLine_, 2026};
    LinkTransmitter transmitter_{Sender::Control, 0};
    FrameReceiver receiver_;
    LinkMonitor monitor_{Sender::Control, 0ns};
    Processor control_;
    Processor safety_;
    Simulation simulation_;
    std::optional<TrueTime> firstLossAt_;
    LinkFault firstFault_ = LinkFault::None;
};

bool detectedInTime(const Bench& bench, TrueTime from) {
    const std::optional<TrueTime> at = bench.firstLossAt();
    return at && *at >= from && *at - from <= detectionBound;
}

void healthy(Expectations& t) {
    Bench bench;
    bench.run(10s);
    t.expect(bench.monitor().state() == LinkState::Up && bench.monitor().losses() == 0,
             "sans défaut : aucune perte de lien en 10 s");
    t.expect(bench.monitor().peerBootNumber() == 1, "numéro de démarrage de la commande");
}

void frameFault(Expectations& t, FrameFault fault, const std::function<bool(LinkFault)>& expected,
                const std::string& name) {
    Bench bench;
    // Fenêtre d'une période de signal de vie : une seule trame touchée.
    bench.line().add({.fault = fault,
                      .from = faultAt,
                      .until = faultAt + heartbeatPeriod,
                      .perMillion = 1'000'000,
                      .delay = 300ms});
    bench.run(4s);
    t.expect(bench.line().injected(fault) == 1, name + " : une trame touchée");
    t.expect(detectedInTime(bench, faultAt), name + " : perte de lien détectée à temps");
    t.expect(expected(bench.firstFault()), name + " : cause attendue");
    t.expect(bench.monitor().state() == LinkState::Up, name + " : lien rétabli ensuite");
}

void frameFaults(Expectations& t) {
    const auto is = [](LinkFault wanted) { return [wanted](LinkFault f) { return f == wanted; }; };
    frameFault(t, FrameFault::drop, is(LinkFault::TransportGap), "perte");
    frameFault(t, FrameFault::duplicate, is(LinkFault::TransportRepeated), "doublon");
    frameFault(t, FrameFault::corrupt, is(LinkFault::Rejected), "altération");
    frameFault(t, FrameFault::truncate, is(LinkFault::Rejected), "troncature");
    frameFault(t, FrameFault::delay, is(LinkFault::TransportGap), "retard");
    frameFault(t, FrameFault::reorder, is(LinkFault::TransportGap), "réordonnancement");
}

void processorFaults(Expectations& t) {
    {
        Bench bench;
        bench.run(faultAt);
        bench.control().hang();
        bench.run(2s);
        t.expect(detectedInTime(bench, faultAt) && bench.firstFault() == LinkFault::Timeout,
                 "commande bloquée : délai dépassé");
        t.expect(bench.monitor().state() == LinkState::Lost, "commande bloquée : lien perdu");
    }
    {
        Bench bench;
        bench.run(faultAt);
        bench.control().reset(10ms);
        bench.run(2s);
        t.expect(detectedInTime(bench, faultAt) && bench.firstFault() == LinkFault::BootChanged,
                 "commande redémarrée : numéro de démarrage changé");
        t.expect(bench.monitor().state() == LinkState::Up && bench.monitor().peerBootNumber() == 2,
                 "commande redémarrée : lien rétabli avec le nouveau démarrage");
    }
    {
        Bench bench;
        bench.run(faultAt);
        bench.control().freezeClock();
        bench.run(2s);
        t.expect(detectedInTime(bench, faultAt) && bench.firstFault() == LinkFault::Timeout,
                 "oscillateur de la commande arrêté : plus de signal de vie, délai dépassé");
    }
    {
        Bench bench;
        bench.run(faultAt);
        bench.control().powerOff();
        bench.run(2s);
        t.expect(detectedInTime(bench, faultAt) && bench.firstFault() == LinkFault::Timeout,
                 "commande hors tension : délai dépassé");
    }
}

} // namespace

int main() {
    return Expectations{}.run([](Expectations& t) {
        healthy(t);
        frameFaults(t);
        processorFaults(t);
    });
}
