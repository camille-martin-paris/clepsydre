// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

// Tests du modèle physique du pousse-seringue (#21). Ils vérifient que le modèle reproduit
// les comportements qu'il prétend reproduire ; ils ne vérifient aucune exigence de la pompe.

#include <chrono>
#include <cmath>
#include <cstdint>
#include <expect.hpp>
#include <format>
#include <numbers>

import clepsydre.sim.syringe_pump;

namespace {

using namespace std::chrono_literals;
using clepsydre::sim::exampleLine;
using clepsydre::sim::exampleMechanism;
using clepsydre::sim::exampleSyringes;
using clepsydre::sim::Line;
using clepsydre::sim::SyringeModel;
using clepsydre::sim::SyringePump;
using clepsydre::test::Expectations;
namespace sim = clepsydre::sim;

const SyringeModel& syringe50 = exampleSyringes[2];

// Vitesse moteur qui délivre le débit flow en régime établi avec la seringue syringe.
sim::StepRate rateFor(sim::VolumeFlow flow, const SyringeModel& syringe) {
    const double radius = syringe.innerDiameter.si() / 2.0;
    const double area = std::numbers::pi * radius * radius;
    const double perStep =
        exampleMechanism.screwLead.si() / static_cast<double>(exampleMechanism.stepsPerRevolution);
    return sim::StepRate::fromSi(flow.si() / area / perStep);
}

void steadyFlow(Expectations& t) {
    SyringePump pump{exampleMechanism, syringe50, exampleLine};
    pump.setStepRate(rateFor(sim::millilitresPerHour(10.0), syringe50));
    pump.run(30min);
    const sim::Volume before = pump.deliveredVolume();
    pump.run(30min);
    const double flow = sim::inMillilitres(pump.deliveredVolume() - before) * 2.0; // mL/h
    t.expect(std::abs(flow - 10.0) < 0.05,
             std::format("débit établi de 10 mL/h (obtenu : {} mL/h)", flow));
    t.expect(sim::inKilopascals(pump.pressure()) > 1.0,
             "la pression dépasse la pression veineuse en perfusion");
}

void startupDelay(Expectations& t) {
    // À 1 mL/h, le jeu de l'écrou puis la compression du joint retardent le premier écoulement.
    SyringePump pump{exampleMechanism, syringe50, exampleLine};
    pump.setStepRate(rateFor(sim::millilitresPerHour(1.0), syringe50));
    const double speed =
        1e-6 / 3600.0 / (std::numbers::pi * std::pow(syringe50.innerDiameter.si() / 2.0, 2.0));
    const auto backlashTime = std::chrono::duration<double>{exampleMechanism.backlash.si() / speed};
    std::chrono::nanoseconds firstFlow{};
    while (pump.elapsed() < 2h && firstFlow == std::chrono::nanoseconds::zero()) {
        pump.run(1s);
        if (sim::inMicrolitres(pump.deliveredVolume()) > 1.0) {
            firstFlow = pump.elapsed();
        }
    }
    t.expect(firstFlow > backlashTime,
             std::format("premier écoulement après le rattrapage du jeu ({} s pour {} s)",
                         std::chrono::duration<double>{firstFlow}.count(), backlashTime.count()));
    t.expect(firstFlow > std::chrono::nanoseconds::zero(), "l'écoulement finit par commencer");
}

void occlusionAndBolus(Expectations& t) {
    SyringePump pump{exampleMechanism, syringe50, exampleLine};
    pump.setStepRate(rateFor(sim::millilitresPerHour(25.0), syringe50));
    pump.run(10min);
    const sim::Force forceBefore = pump.contactForce();
    const sim::Pressure pressureBefore = pump.pressure();
    const sim::Volume deliveredBefore = pump.deliveredVolume();

    Line occluded = exampleLine;
    occluded.resistance = occluded.resistance * 1e6;
    pump.setLine(occluded);
    pump.run(2min);
    const double rise = sim::inNewtons(pump.contactForce() - forceBefore);
    const double leak = sim::inMicrolitres(pump.deliveredVolume() - deliveredBefore);
    t.expect(rise > 5.0, std::format("la force monte pendant l'occlusion (+{} N)", rise));
    t.expect(leak < 5.0, std::format("presque rien ne passe pendant l'occlusion ({} µL)", leak));

    // Moteur arrêté : la vis irréversible ne recule pas sous la charge.
    pump.setStepRate(sim::StepRate{});
    const sim::Length held = pump.pusherPosition();
    pump.run(10s);
    t.expect(pump.pusherPosition() >= held, "le pousseur ne recule pas sous la charge");

    // Levée de l'occlusion sans recul : le volume accumulé part en bolus.
    const sim::Volume atRelease = pump.deliveredVolume();
    pump.setLine(exampleLine);
    pump.run(30s);
    const double bolus = sim::inMicrolitres(pump.deliveredVolume() - atRelease);
    // Borne : volume poussé pendant l'occlusion (2 min à 25 mL/h), plus le volume déjà stocké
    // en perfusion établie, que le corps et le joint restituent en revenant à la pression veineuse.
    const double area = std::numbers::pi * std::pow(syringe50.innerDiameter.si() / 2.0, 2.0);
    const double compliance =
        syringe50.compliance.si() + (area * area / syringe50.stopperStiffness.si());
    const double stored =
        (pressureBefore - exampleLine.patientPressure).si() * compliance * 1e9; // µL
    const double bound = sim::inMicrolitres(sim::millilitres(25.0 / 30.0)) - leak + stored;
    t.expect(bolus > 10.0, std::format("bolus à la levée de l'occlusion ({} µL)", bolus));
    t.expect(bolus <= bound,
             std::format("bolus borné par le volume accumulé ({} µL pour {} µL)", bolus, bound));
}

void retractionLimitsBolus(Expectations& t) {
    // Le recul du pousseur avant la levée réduit le bolus (CTRL-008).
    auto bolusAfter = [](std::chrono::seconds retraction) {
        SyringePump pump{exampleMechanism, syringe50, exampleLine};
        pump.setStepRate(rateFor(sim::millilitresPerHour(25.0), syringe50));
        pump.run(10min);
        Line occluded = exampleLine;
        occluded.resistance = occluded.resistance * 1e6;
        pump.setLine(occluded);
        pump.run(2min);
        pump.setStepRate(-exampleMechanism.maxStepRate);
        pump.run(retraction);
        pump.setStepRate(sim::StepRate{});
        const sim::Volume atRelease = pump.deliveredVolume();
        pump.setLine(exampleLine);
        pump.run(30s);
        return pump.deliveredVolume() - atRelease;
    };
    const double without = sim::inMicrolitres(bolusAfter(0s));
    const double with = sim::inMicrolitres(bolusAfter(2s));
    t.expect(with < without / 2.0,
             std::format("bolus réduit par le recul ({} µL contre {} µL)", with, without));
}

void siphoning(Expectations& t) {
    // Seringue 1 m au-dessus du patient, joint à faible frottement.
    SyringeModel slippery = syringe50;
    slippery.breakawayFriction = sim::newtons(2.0);
    slippery.slidingFriction = sim::newtons(1.5);
    Line raised = exampleLine;
    raised.syringeHeight = sim::millimetres(1000.0);

    // Piston capturé, pousseur immobile : la vis retient le piston.
    SyringePump held{exampleMechanism, slippery, exampleLine};
    held.setLine(raised);
    held.run(10min);
    // Seuls passent le jeu de l'écrou, l'élasticité du joint et la compliance : quelques dizaines
    // de µL.
    t.expect(sim::inMicrolitres(held.deliveredVolume()) < 200.0,
             std::format("piston retenu : pas d'écoulement libre ({} µL)",
                         sim::inMicrolitres(held.deliveredVolume())));

    // Piston libéré : le liquide s'écoule par gravité (RISK-003).
    SyringePump released{exampleMechanism, slippery, exampleLine};
    released.setPlungerCaptured(false);
    released.setStepRate(-exampleMechanism.maxStepRate);
    released.run(2s);
    released.setStepRate(sim::StepRate{});
    released.setLine(raised);
    released.run(10min);
    t.expect(sim::inMillilitres(released.deliveredVolume()) > 1.0,
             std::format("piston libéré : écoulement libre ({} mL)",
                         sim::inMillilitres(released.deliveredVolume())));
}

void emptySyringe(Expectations& t) {
    const SyringeModel& syringe10 = exampleSyringes[0];
    SyringePump pump{exampleMechanism, syringe10, exampleLine};
    pump.setStepRate(rateFor(sim::millilitresPerHour(200.0), syringe10));
    pump.run(4min);
    t.expect(pump.plungerAtEnd(), "le piston atteint la fin de course");
    t.expect(std::abs(sim::inMillilitres(pump.deliveredVolume()) - 10.0) < 0.05,
             std::format("le volume utile est délivré ({} mL)",
                         sim::inMillilitres(pump.deliveredVolume())));
    t.expect(sim::inNewtons(pump.contactForce()) > 50.0, "la force monte en butée");
}

void stepRateBound(Expectations& t) {
    SyringePump pump{exampleMechanism, syringe50, exampleLine};
    pump.setStepRate(exampleMechanism.maxStepRate * 3.0);
    t.expect(pump.stepRate() == exampleMechanism.maxStepRate, "vitesse bornée par le matériel");
    pump.run(1s);
    t.expect(pump.motorSteps() == static_cast<std::int64_t>(exampleMechanism.maxStepRate.si()),
             std::format("pas exécutés en 1 s ({})", pump.motorSteps()));
}

void sensors(Expectations& t) {
    SyringePump pump{exampleMechanism, syringe50, exampleLine};
    t.expect(pump.measuredSyringeDiameter() == syringe50.outerDiameter,
             "diamètre extérieur mesuré");
    pump.setStepRate(rateFor(sim::millilitresPerHour(25.0), syringe50));
    pump.run(5min);
    const double force = pump.measuredForce() / exampleMechanism.forceResolution;
    const double position = pump.measuredPlungerPosition() / exampleMechanism.positionResolution;
    t.expect(std::abs(force - std::round(force)) < 1e-6,
             "force quantifiée à la résolution du capteur");
    t.expect(std::abs(position - std::round(position)) < 1e-6,
             "position quantifiée à la résolution du capteur");
    t.expect(std::abs((pump.measuredForce() - pump.contactForce()).si()) <=
                 exampleMechanism.forceResolution.si(),
             "force mesurée à une résolution près");
}

void plungerSensorInOcclusion(Expectations& t) {
    // En occlusion, le joint se comprime : le pousseur avance, le piston presque pas. La mesure
    // du canal de sécurité doit suivre le piston (ADR 0003, SYS-REQ-001), pas le pousseur.
    SyringePump pump{exampleMechanism, syringe50, exampleLine};
    Line occluded = exampleLine;
    occluded.resistance = occluded.resistance * 1e6;
    pump.setLine(occluded);
    pump.setStepRate(rateFor(sim::millilitresPerHour(25.0), syringe50));
    pump.run(2min);
    const double pusher = sim::inMillimetres(pump.pusherPosition());
    const double plunger = sim::inMillimetres(pump.plungerPosition());
    t.expect(pusher - plunger > 1.0,
             std::format("pousseur et piston divergent ({} mm contre {} mm)", pusher, plunger));
    t.expect(std::abs((pump.measuredPlungerPosition() - pump.plungerPosition()).si()) <=
                 exampleMechanism.positionResolution.si() / 2.0,
             std::format("la mesure suit le piston à la résolution près ({} mm)",
                         sim::inMillimetres(pump.measuredPlungerPosition())));
}

void deterministic(Expectations& t) {
    auto scenario = [] {
        SyringePump pump{exampleMechanism, syringe50, exampleLine};
        pump.setStepRate(rateFor(sim::millilitresPerHour(7.3), syringe50));
        pump.run(3min, 700us);
        return pump.deliveredVolume();
    };
    t.expect(scenario().si() == scenario().si(),
             "deux exécutions identiques donnent le même résultat");
}

} // namespace

int main() {
    return Expectations{}.run([](Expectations& t) {
        steadyFlow(t);
        startupDelay(t);
        occlusionAndBolus(t);
        retractionLimitsBolus(t);
        siphoning(t);
        emptySyringe(t);
        stepRateBound(t);
        sensors(t);
        plungerSensorInOcclusion(t);
        deterministic(t);
    });
}
