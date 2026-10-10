// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

// Tests de l'ordonnanceur du monde simulé (#21, ADR 0006) : deux processeurs simulés, chacun
// sur sa propre horloge, échangent des signaux de vie par la liaison série pendant que le modèle
// physique avance. Les processeurs sont des bouchons de test, pas les logiciels de la pompe :
// ces tests vérifient le simulateur, aucune exigence de la pompe.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expect.hpp>
#include <format>
#include <numbers>
#include <vector>

import clepsydre.sim.clock;
import clepsydre.sim.serial_link;
import clepsydre.sim.simulation;
import clepsydre.sim.syringe_pump;

namespace {

using namespace std::chrono_literals;
using clepsydre::sim::MonotonicClock;
using clepsydre::sim::SerialChannel;
using clepsydre::sim::SerialLink;
using clepsydre::sim::Simulation;
using clepsydre::sim::TrueTime;
using clepsydre::test::Expectations;
namespace sim = clepsydre::sim;

constexpr std::int64_t baud = 115'200;
constexpr std::chrono::nanoseconds latency = 10us;
constexpr std::size_t frameSize = 16;                 // trame de signal de vie, ordre de grandeur
constexpr std::chrono::nanoseconds beatPeriod = 50ms; // ADR 0006 § Règles

// Bouchon de processeur : émet une trame de signal de vie toutes les 50 ms de SON horloge,
// et mesure, sur cette même horloge, le plus long silence de l'autre processeur.
class HeartbeatNode {
public:
    HeartbeatNode(const MonotonicClock& clock, SerialChannel& tx, SerialChannel& rx)
        : clock_{clock}, tx_{&tx}, rx_{&rx}, nextBeat_{clock.read(0ns)}, lastHeard_{nextBeat_} {}

    void step(TrueTime now) {
        const auto local = clock_.read(now);
        if (local >= nextBeat_) {
            tx_->send(now, std::vector<std::byte>(frameSize, std::byte{0xa5}));
            nextBeat_ += beatPeriod;
        }
        const std::size_t framesBefore = bytes_ / frameSize;
        bytes_ += rx_->receive(now).size();
        if (bytes_ / frameSize > framesBefore) {
            maxGap_ = std::max(maxGap_, local - lastHeard_);
            lastHeard_ = local;
        }
    }

    [[nodiscard]] std::size_t frames() const noexcept { return bytes_ / frameSize; }

    [[nodiscard]] std::chrono::nanoseconds maxGap() const noexcept { return maxGap_; }

private:
    MonotonicClock clock_;
    SerialChannel* tx_;
    SerialChannel* rx_;
    std::chrono::nanoseconds nextBeat_;
    std::chrono::nanoseconds lastHeard_;
    std::chrono::nanoseconds maxGap_{};
    std::size_t bytes_ = 0;
};

struct Outcome {
    std::size_t controlFrames;
    std::size_t safetyFrames;
    std::chrono::nanoseconds controlMaxGap;
    std::chrono::nanoseconds safetyMaxGap;
    double deliveredMillilitres;
};

bool identical(const Outcome& a, const Outcome& b) {
    return a.controlFrames == b.controlFrames && a.safetyFrames == b.safetyFrames &&
           a.controlMaxGap == b.controlMaxGap && a.safetyMaxGap == b.safetyMaxGap &&
           a.deliveredMillilitres == b.deliveredMillilitres;
}

// Dix minutes de perfusion à 10 mL/h, les deux processeurs échangeant leurs signaux de vie.
Outcome heartbeatScenario(std::int64_t safetyDriftPpb) {
    Simulation world;
    SerialLink link{.controlToSafety = SerialChannel{baud, latency},
                    .safetyToControl = SerialChannel{baud, latency}};
    HeartbeatNode control{MonotonicClock{0ns, 50'000}, link.controlToSafety, link.safetyToControl};
    HeartbeatNode safety{MonotonicClock{2s, safetyDriftPpb}, link.safetyToControl,
                         link.controlToSafety};
    const auto& syringe = sim::exampleSyringes[2];
    sim::SyringePump pump{sim::exampleMechanism, syringe, sim::exampleLine};
    const double area = std::numbers::pi * std::pow(syringe.innerDiameter.si() / 2.0, 2.0);
    const double perStep = sim::exampleMechanism.screwLead.si() /
                           static_cast<double>(sim::exampleMechanism.stepsPerRevolution);
    pump.setStepRate(sim::stepsPerSecond(sim::millilitresPerHour(10.0).si() / area / perStep));

    world.add([&](TrueTime, std::chrono::nanoseconds dt) { pump.step(dt); });
    world.add([&](TrueTime now, std::chrono::nanoseconds) { control.step(now); });
    world.add([&](TrueTime now, std::chrono::nanoseconds) { safety.step(now); });
    world.run(10min);
    return {.controlFrames = control.frames(),
            .safetyFrames = safety.frames(),
            .controlMaxGap = control.maxGap(),
            .safetyMaxGap = safety.maxGap(),
            .deliveredMillilitres = sim::inMillilitres(pump.deliveredVolume())};
}

void quantumStepping(Expectations& t) {
    Simulation world{1ms};
    std::vector<std::size_t> calls;
    std::chrono::nanoseconds total{};
    world.add([&](TrueTime, std::chrono::nanoseconds dt) {
        calls.push_back(1U);
        total += dt;
    });
    world.add([&](TrueTime, std::chrono::nanoseconds) { calls.push_back(2U); });
    world.run(10500us);
    t.expect(world.now() == 10500us, "le temps simulé avance de la durée demandée");
    t.expect(total == 10500us, "les pas couvrent la durée, dernier pas partiel compris");
    t.expect(calls.size() == 22U, std::format("onze pas, deux participants ({})", calls.size()));
    bool ordered = true;
    for (std::size_t i = 0; i < calls.size(); ++i) {
        ordered = ordered && calls[i] == (i % 2) + 1;
    }
    t.expect(ordered, "participants appelés dans l'ordre d'inscription");
}

void heartbeatsOverIndependentClocks(Expectations& t) {
    // Horloges à +50 ppm et -50 ppm : chaque côté entend l'autre toutes les 50 ms environ,
    // mesurées sur sa propre horloge, très en deçà du délai de perte de lien de 200 ms.
    const Outcome outcome = heartbeatScenario(-50'000);
    t.expect(outcome.controlFrames >= 11'990 && outcome.safetyFrames >= 11'990,
             std::format("environ 12 000 signaux de vie reçus de chaque côté ({} et {})",
                         outcome.controlFrames, outcome.safetyFrames));
    t.expect(outcome.controlMaxGap < 55ms && outcome.safetyMaxGap < 55ms,
             std::format("silence le plus long de {} et {}", outcome.controlMaxGap,
                         outcome.safetyMaxGap));
    // Au plus 10 min à 10 mL/h ; un peu moins, à cause du retard au démarrage.
    t.expect(outcome.deliveredMillilitres > 1.0 && outcome.deliveredMillilitres <= 10.0 / 6.0,
             std::format("le modèle physique avance avec les processeurs ({} mL)",
                         outcome.deliveredMillilitres));
}

void slowClockSeenByTheOtherSide(Expectations& t) {
    // Horloge de sécurité cinq fois trop lente : elle émet toutes les 250 ms réelles. La commande,
    // sur sa propre horloge, mesure un silence supérieur au délai de perte de lien : une horloge
    // fausse d'un côté se voit de l'autre (ADR 0006 § Deux bases de temps distinctes).
    const Outcome outcome = heartbeatScenario(-800'000'000);
    t.expect(outcome.controlMaxGap > 200ms,
             std::format("silence de {} mesuré par la commande", outcome.controlMaxGap));
}

void deterministic(Expectations& t) {
    t.expect(identical(heartbeatScenario(-50'000), heartbeatScenario(-50'000)),
             "deux exécutions identiques donnent le même résultat");
}

} // namespace

int main() {
    return Expectations{}.run([](Expectations& t) {
        quantumStepping(t);
        heartbeatsOverIndependentClocks(t);
        slowClockSeenByTheOtherSide(t);
        deterministic(t);
    });
}
