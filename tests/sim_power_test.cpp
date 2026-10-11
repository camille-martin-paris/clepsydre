// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

// Tests de l'alimentation simulée et de ses défauts (#21, ADR 0004) : bascule sur batterie,
// décharge, vieillissement, perte totale et réserve de maintien, effets sur le moteur et les
// processeurs. Ils vérifient le simulateur ; ils ne vérifient aucune exigence de la pompe.

#include <chrono>
#include <cmath>
#include <cstdint>
#include <expect.hpp>
#include <format>
#include <numbers>

import clepsydre.sim.power;
import clepsydre.sim.processor;
import clepsydre.sim.serial_link;
import clepsydre.sim.simulation;
import clepsydre.sim.syringe_pump;

namespace {

using namespace std::chrono_literals;
using clepsydre::sim::examplePower;
using clepsydre::sim::MonotonicClock;
using clepsydre::sim::PowerSupply;
using clepsydre::sim::Processor;
using clepsydre::sim::SerialChannel;
using clepsydre::sim::Simulation;
using clepsydre::sim::TrueTime;
using clepsydre::test::Expectations;
namespace sim = clepsydre::sim;

// Charge utile de la batterie, de la pleine charge à la coupure par le circuit de protection.
double usableMilliampereHours(double capacityFactor) {
    const auto& battery = examplePower.battery;
    const double cutoff =
        (battery.protectionCutoff - battery.empty) / (battery.full - battery.empty);
    return sim::inMilliampereHours(battery.capacity) * capacityFactor * (1.0 - cutoff);
}

// Durée sur batterie, à la consommation moteur en marche, jusqu'à la perte de l'alimentation.
std::chrono::duration<double> autonomy(double capacityFactor) {
    PowerSupply power{examplePower};
    power.setCapacityFactor(capacityFactor);
    power.setMains(false);
    std::chrono::nanoseconds elapsed{};
    while (power.powered() && elapsed < 24h) {
        power.step(100ms, true);
        elapsed += 100ms;
    }
    return elapsed;
}

void mainsAndBattery(Expectations& t) {
    PowerSupply power{examplePower, 0.5};
    power.step(1h, false);
    t.expect(power.powered() && !power.onBattery(), "sur secteur");
    t.expect(std::abs(power.stateOfCharge() - 0.75) < 1e-9,
             std::format("charge de 500 mA pendant 1 h ({})", power.stateOfCharge()));

    // Perte du secteur : bascule sans interruption (SYS-REQ-015).
    power.setMains(false);
    power.step(1ms, true);
    t.expect(power.powered() && power.onBattery() && !power.powerFailImminent(),
             "bascule sur batterie sans interruption");
    t.expect(std::abs(sim::inMilliamperes(power.measuredBatteryCurrent()) - 330.0) < 1e-9,
             "courant mesuré : repos et moteur");
    const sim::Charge before = power.remainingCharge();
    power.step(1h, true);
    t.expect(std::abs(sim::inMilliampereHours(before - power.remainingCharge()) - 330.0) < 1e-6,
             "330 mAh consommés en 1 h");
    const auto voltage = power.measuredBatteryVoltage();
    t.expect(voltage && *voltage < examplePower.battery.full &&
                 *voltage > examplePower.battery.empty,
             "tension mesurée entre vide et plein");
}

void autonomyAndAging(Expectations& t) {
    const double expected = usableMilliampereHours(1.0) / 330.0 * 3600.0;
    const double fresh = autonomy(1.0).count();
    t.expect(std::abs(fresh - expected) < 1.0,
             std::format("autonomie de {} s pour {} s attendues", fresh, expected));
    const double aged = autonomy(0.5).count();
    t.expect(std::abs(aged - (fresh / 2.0)) < 1.0,
             std::format("capacité réduite de moitié : autonomie de {} s", aged));
}

void totalLossAndHoldUp(Expectations& t) {
    PowerSupply power{examplePower};
    power.setBatteryConnected(false);
    t.expect(!power.measuredBatteryVoltage().has_value(), "batterie déconnectée : aucune tension");
    power.setMains(false);
    power.step(1ms, false);
    t.expect(power.powered() && power.powerFailImminent(),
             "perte des deux sources : réserve de maintien et signal de coupure imminente");
    power.step(48ms, false);
    t.expect(power.powered(), "la réserve tient 50 ms");
    power.step(2ms, false);
    t.expect(!power.powered() && !power.powerFailImminent(), "puis la pompe s'éteint");
    power.setMains(true);
    power.step(1ms, false);
    t.expect(power.powered(), "le retour du secteur la rallume");
}

// Logiciel de commande réduit à l'essentiel : au démarrage, il arrête le moteur, sans jamais
// reprendre de lui-même (SYS-REQ-038).
struct ControlStub {
    sim::SyringePump* pump = nullptr;
    int boots = 0;
};

void powerCycleInWorld(Expectations& t) {
    // Pompe, alimentation et deux processeurs dans le même monde simulé. Le câblage reproduit la
    // bascule matérielle : la perte du secteur ne touche ni le moteur ni les processeurs ; la
    // perte totale, après la réserve de maintien, éteint tout. Le moteur n'est validé que si
    // les deux processeurs sont en marche.
    const auto& syringe = sim::exampleSyringes[2];
    sim::SyringePump pump{sim::exampleMechanism, syringe, sim::exampleLine};
    PowerSupply power{examplePower};
    SerialChannel toSafety{115'200, 10us};
    SerialChannel toControl{115'200, 10us};
    ControlStub stub{.pump = &pump};
    Processor control{MonotonicClock{0ns, 0},
                      toSafety,
                      toControl,
                      {.boot =
                           [&stub](Processor&) {
                               ++stub.boots;
                               stub.pump->setStepRate(sim::StepRate{});
                           },
                       .run = {}}};
    Processor safety{MonotonicClock{1s, 0}, toControl, toSafety, {}};

    Simulation world;
    world.add([&](TrueTime, std::chrono::nanoseconds dt) {
        power.step(dt, pump.motorRunning());
        // Validation du pilote : alimentation et autorisation des deux processeurs en marche ;
        // par défaut, non validé (SYS-REQ-045).
        pump.setMotorEnabled(power.powered() && control.state() == Processor::State::running &&
                             safety.state() == Processor::State::running);
        for (Processor* processor : {&control, &safety}) {
            if (power.powered()) {
                processor->powerOn(100ms);
            } else {
                processor->powerOff();
            }
        }
    });
    world.add([&](TrueTime now, std::chrono::nanoseconds) { control.step(now); });
    world.add([&](TrueTime now, std::chrono::nanoseconds) { safety.step(now); });
    world.add([&](TrueTime, std::chrono::nanoseconds dt) { pump.step(dt); });

    world.run(1s);
    const double area = std::numbers::pi * std::pow(syringe.innerDiameter.si() / 2.0, 2.0);
    const double perStep = sim::exampleMechanism.screwLead.si() /
                           static_cast<double>(sim::exampleMechanism.stepsPerRevolution);
    pump.setStepRate(sim::StepRate::fromSi(sim::millilitresPerHour(25.0).si() / area / perStep));
    world.run(1min);

    power.setMains(false);
    const std::int64_t steps = pump.rotorSteps();
    world.run(1min);
    t.expect(control.bootNumber() == 1 && safety.bootNumber() == 1,
             "perte du secteur : aucun processeur réinitialisé");
    t.expect(pump.rotorSteps() > steps, "perte du secteur : le moteur continue");

    power.setBatteryConnected(false);
    world.run(1s);
    t.expect(control.state() == Processor::State::off && safety.state() == Processor::State::off,
             "perte totale : processeurs éteints après la réserve de maintien");
    const std::int64_t stopped = pump.rotorSteps();
    world.run(10s);
    t.expect(pump.rotorSteps() == stopped, "perte totale : moteur arrêté");

    power.setMains(true);
    world.run(1min);
    t.expect(control.bootNumber() == 2 && safety.bootNumber() == 2 && stub.boots == 2,
             "retour du secteur : les deux processeurs redémarrent");
    t.expect(pump.rotorSteps() == stopped, "le logiciel ne reprend pas la perfusion de lui-même");
}

} // namespace

int main() {
    return Expectations{}.run([](Expectations& t) {
        mainsAndBattery(t);
        autonomyAndAging(t);
        totalLossAndHoldUp(t);
        powerCycleInWorld(t);
    });
}
