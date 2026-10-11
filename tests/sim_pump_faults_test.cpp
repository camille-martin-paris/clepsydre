// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

// Tests des défauts physiques et matériels du pousse-seringue (#21) : validation et défauts du
// pilote, défauts des capteurs, seringue mal placée ou absente, air dans la ligne. Ils vérifient
// que chaque défaut est injecté comme annoncé ; ils ne vérifient aucune exigence de la pompe.

#include <chrono>
#include <cmath>
#include <cstdint>
#include <expect.hpp>
#include <format>
#include <numbers>
#include <optional>
#include <stdexcept>

import clepsydre.sim.syringe_pump;

namespace {

using namespace std::chrono_literals;
using clepsydre::sim::DriverFault;
using clepsydre::sim::exampleLine;
using clepsydre::sim::exampleMechanism;
using clepsydre::sim::exampleSyringes;
using clepsydre::sim::Line;
using clepsydre::sim::SensorFaultKind;
using clepsydre::sim::SyringeModel;
using clepsydre::sim::SyringePlacement;
using clepsydre::sim::SyringePump;
using clepsydre::test::Expectations;
namespace sim = clepsydre::sim;

const SyringeModel& syringe50 = exampleSyringes[2];

double area(const SyringeModel& syringe) {
    return std::numbers::pi * std::pow(syringe.innerDiameter.si() / 2.0, 2.0);
}

double stepLength() {
    return exampleMechanism.screwLead.si() /
           static_cast<double>(exampleMechanism.stepsPerRevolution);
}

sim::StepRate rateFor(double millilitresPerHour) {
    return sim::StepRate::fromSi(sim::millilitresPerHour(millilitresPerHour).si() /
                                 area(syringe50) / stepLength());
}

// Pompe en perfusion établie à 10 mL/h.
SyringePump steadyPump() {
    SyringePump pump{exampleMechanism, syringe50, exampleLine};
    pump.setStepRate(rateFor(10.0));
    pump.run(30min);
    return pump;
}

template <typename Q>
Q reading(const std::optional<Q>& measured) {
    if (!measured) {
        throw std::runtime_error{"capteur sans mesure"};
    }
    return *measured;
}

// Volume délivré pendant duration, en mL.
double deliveredDuring(SyringePump& pump, std::chrono::nanoseconds duration) {
    const sim::Volume before = pump.deliveredVolume();
    pump.run(duration);
    return sim::inMillilitres(pump.deliveredVolume() - before);
}

void motorEnable(Expectations& t) {
    SyringePump pump = steadyPump();
    pump.setMotorEnabled(false);
    const std::int64_t commanded = pump.motorSteps();
    const std::int64_t executed = pump.rotorSteps();
    pump.run(1min);
    t.expect(pump.motorSteps() > commanded, "les pas commandés continuent d'être comptés");
    t.expect(pump.rotorSteps() == executed && !pump.motorRunning(),
             "sans validation, le moteur ne tourne pas");
    pump.setMotorEnabled(true);
    pump.run(1min);
    t.expect(pump.rotorSteps() > executed, "validé, le moteur repart");
}

void runaway(Expectations& t) {
    // Pilote emballé : le moteur tourne à la vitesse maximale, sans que les pas commandés le
    // montrent ; le capteur de position du piston le voit.
    SyringePump pump = steadyPump();
    const std::int64_t commandedBefore = pump.motorSteps();
    const sim::Length positionBefore = reading(pump.measuredPlungerPosition());
    pump.setDriverFault(DriverFault::runaway);
    const double delivered = deliveredDuring(pump, 10s);
    const double commanded =
        static_cast<double>(pump.motorSteps() - commandedBefore) * stepLength() * area(syringe50);
    const double measured =
        (reading(pump.measuredPlungerPosition()) - positionBefore).si() * area(syringe50);
    t.expect(delivered > 20.0 * sim::inMillilitres(sim::Volume::fromSi(commanded)),
             std::format("emballement : {} mL délivrés pour {} mL commandés", delivered,
                         sim::inMillilitres(sim::Volume::fromSi(commanded))));
    t.expect(std::abs(sim::inMillilitres(sim::Volume::fromSi(measured)) - delivered) < 0.1,
             "la position du piston suit le volume réellement poussé");

    // La validation matérielle coupe le moteur, même emballé (SYS-REQ-045).
    pump.setMotorEnabled(false);
    const std::int64_t executed = pump.rotorSteps();
    pump.run(10s);
    t.expect(pump.rotorSteps() == executed, "la coupure de la validation arrête l'emballement");
}

void lostStepsAndStall(Expectations& t) {
    SyringePump pump = steadyPump();
    pump.setDriverFault(DriverFault::lostSteps, 0.5);
    pump.run(30min);
    const std::int64_t commanded = pump.motorSteps();
    const std::int64_t executed = pump.rotorSteps();
    const double half = deliveredDuring(pump, 30min) * 2.0;
    const double ratio = static_cast<double>(pump.rotorSteps() - executed) /
                         static_cast<double>(pump.motorSteps() - commanded);
    t.expect(std::abs(ratio - 0.5) < 0.01, std::format("un pas sur deux perdu ({})", ratio));
    t.expect(std::abs(half - 10.0) > 4.0 && std::abs(half - 5.0) < 0.5,
             std::format("débit réduit de moitié ({} mL/h)", half));

    pump.setDriverFault(DriverFault::stall);
    const sim::Length position = reading(pump.measuredPlungerPosition());
    pump.run(10min);
    t.expect(!pump.motorRunning(), "moteur calé");
    // Seule la détente du joint avance encore le piston, de quelques micromètres, contre 3 mm
    // attendus en 10 min à 10 mL/h.
    const double moved =
        sim::inMillimetres(reading(pump.measuredPlungerPosition()) - position) * 1000.0;
    t.expect(moved < 50.0, std::format("le piston ne bouge presque plus ({} µm) : actionneur "
                                       "inactif (SYS-REQ-004)",
                                       moved));
}

void sensorFaults(Expectations& t) {
    SyringePump pump = steadyPump();
    // Capteur de force figé, puis occlusion : la force réelle monte, la mesure non.
    pump.setForceSensorFault({.kind = SensorFaultKind::stuck});
    const sim::Force stuck = reading(pump.measuredForce());
    Line occluded = exampleLine;
    occluded.resistance = occluded.resistance * 1e6;
    pump.setLine(occluded);
    pump.run(2min);
    t.expect(reading(pump.measuredForce()) == stuck, "force figée");
    t.expect(pump.contactForce() - stuck > sim::newtons(5.0), "la force réelle monte");

    pump.setForceSensorFault({.kind = SensorFaultKind::offset, .offset = sim::newtons(-3.0)});
    t.expect(std::abs((reading(pump.measuredForce()) - pump.contactForce()).si() + 3.0) < 0.02,
             "force décalée de -3 N");
    pump.setForceSensorFault({.kind = SensorFaultKind::lost});
    t.expect(!pump.measuredForce().has_value(), "capteur de force perdu");

    pump.setPositionSensorFault({.kind = SensorFaultKind::lost});
    t.expect(!pump.measuredPlungerPosition().has_value(), "capteur de position perdu");
    pump.setPositionSensorFault({.kind = SensorFaultKind::offset, .offset = sim::millimetres(1.0)});
    t.expect(std::abs((reading(pump.measuredPlungerPosition()) - pump.plungerPosition()).si() -
                      sim::millimetres(1.0).si()) < sim::micrometres(1.0).si(),
             "position décalée de 1 mm");

    // Diamètre lu comme celui d'une autre seringue : mauvaise identification (SYS-REQ-047).
    const sim::Length other = exampleSyringes[1].outerDiameter;
    pump.setDiameterSensorFault(
        {.kind = SensorFaultKind::offset, .offset = other - syringe50.outerDiameter});
    t.expect(std::abs((reading(pump.measuredSyringeDiameter()) - other).si()) < 1e-12,
             "diamètre lu d'une autre seringue");
}

void misplacedSyringe(Expectations& t) {
    // Corps non tenu : le pousseur entraîne la seringue entière. Le capteur de position voit le
    // piston avancer, mais rien n'est délivré ; seul le détecteur de berceau le révèle.
    SyringePump pump = steadyPump();
    pump.setSyringePlacement(SyringePlacement::misplaced);
    pump.setStepRate(rateFor(100.0));
    pump.run(10s); // la ligne se vide de ce qu'elle avait accumulé
    const sim::Length position = reading(pump.measuredPlungerPosition());
    const double delivered = deliveredDuring(pump, 5min);
    const double moved = sim::inMillilitres(sim::Volume::fromSi(
        (reading(pump.measuredPlungerPosition()) - position).si() * area(syringe50)));
    t.expect(!pump.syringeSeated(), "détecteur de berceau : seringue mal placée");
    t.expect(moved > 7.0, std::format("le piston mesuré avance de {} mL", moved));
    t.expect(delivered < 0.05, std::format("rien n'est délivré ({} mL)", delivered));

    pump.setSyringePlacement(SyringePlacement::seated);
    t.expect(deliveredDuring(pump, 5min) > 5.0, "remise en place : la délivrance reprend");
}

void absentSyringe(Expectations& t) {
    SyringePump pump = steadyPump();
    pump.setSyringePlacement(SyringePlacement::absent);
    t.expect(!pump.measuredSyringeDiameter() && !pump.measuredPlungerPosition() &&
                 !pump.syringeSeated() && !pump.plungerCaptured(),
             "sans seringue : ni diamètre, ni position, ni capture");
    t.expect(pump.contactForce() == sim::Force{}, "aucune force");
    t.expect(deliveredDuring(pump, 1min) == 0.0, "rien n'est délivré");

    // Seringue remise : au contact du pousseur, sans à-coup de force.
    pump.setSyringePlacement(SyringePlacement::seated);
    t.expect(pump.contactForce() == sim::Force{}, "remise au contact, sans compression");
    t.expect(deliveredDuring(pump, 30min) > 2.0, "la délivrance reprend");
}

void airInLine(Expectations& t) {
    // Bulle de 60 µL à 10 mL/h : elle passe le détecteur, à 100 µL de la seringue, puis atteint
    // le patient, à 1 mL.
    SyringePump pump = steadyPump();
    pump.injectAir(sim::microlitres(60.0));
    const sim::Volume start = pump.deliveredVolume();
    bool seenBeforeDetector = false;
    bool detected = false;
    bool deliveredEarly = false;
    while (pump.deliveredVolume() - start < sim::microlitres(1'200.0)) {
        pump.run(1s);
        const double outflow = sim::inMicrolitres(pump.deliveredVolume() - start);
        seenBeforeDetector = seenBeforeDetector || (outflow < 99.0 && pump.airDetected());
        detected = detected || pump.airDetected();
        deliveredEarly = deliveredEarly || (outflow < 999.0 && pump.airDelivered() > sim::Volume{});
    }
    t.expect(!seenBeforeDetector, "la bulle n'est pas vue avant le détecteur");
    t.expect(detected, "la bulle passe le détecteur");
    t.expect(!pump.airDetected(), "puis le dépasse");
    t.expect(!deliveredEarly, "pas d'air au patient avant le volume de la ligne");
    t.expect(std::abs(sim::inMicrolitres(pump.airDelivered()) - 60.0) < 1e-6,
             std::format("toute la bulle atteint le patient ({} µL)",
                         sim::inMicrolitres(pump.airDelivered())));
}

} // namespace

int main() {
    return Expectations{}.run([](Expectations& t) {
        motorEnable(t);
        runaway(t);
        lostStepsAndStall(t);
        sensorFaults(t);
        misplacedSyringe(t);
        absentSyringe(t);
        airInLine(t);
    });
}
