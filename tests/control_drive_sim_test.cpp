// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

// Asservissement du moteur (#26) en boucle fermée avec le modèle physique du pousse-seringue
// (ADR 0002) : délai au premier écoulement avec et sans rattrapage des jeux (CTRL-027,
// SYS-REQ-048), précision du débit en régime établi pour chaque seringue d'exemple sur la plage de
// 0,1 à 200 mL/h (CTRL-002, SYS-REQ-003), effet d'un écart de diamètre au modèle de la liste, et
// défauts pendant le rattrapage.
//
// Les résultats valent pour le modèle et ses paramètres d'exemple, jamais pour la pompe : la
// précision et le délai au premier écoulement se mesurent sur banc (#73). Le tableau imprimé est
// repris dans docs/design/drive.md.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expect.hpp>
#include <format>
#include <numbers>
#include <optional>
#include <print>
#include <string>
#include <vector>

import clepsydre.control.drive;
import clepsydre.control.infusion;
import clepsydre.sim.syringe_pump;

namespace {

using namespace std::chrono_literals;
using clepsydre::test::Expectations;
namespace control = clepsydre::control;
namespace sim = clepsydre::sim;

constexpr std::chrono::milliseconds period = 10ms;

double seconds(std::chrono::nanoseconds duration) {
    return std::chrono::duration<double>{duration}.count();
}

double area(const sim::SyringeModel& syringe) {
    const double radius = syringe.innerDiameter.si() / 2.0;
    return std::numbers::pi * radius * radius;
}

control::DriveMechanism driveMechanism() {
    const auto rate = static_cast<std::uint32_t>(sim::exampleMechanism.maxStepRate.si());
    return {.screwLeadNanometres =
                static_cast<std::uint32_t>(std::lround(sim::exampleMechanism.screwLead.si() * 1e9)),
            .stepsPerRevolution =
                static_cast<std::uint32_t>(sim::exampleMechanism.stepsPerRevolution),
            .maxStepRate = rate,
            .takeUpStepRate = rate,
            .period = period};
}

// Caractéristiques d'entraînement que le banc mesurerait sur la seringue listed (#73) : section
// nominale ; fin du rattrapage aux trois quarts du frottement statique ; course du jeu et de la
// compression du joint jusqu'à cette force, avec 25 % de marge sur la compression.
control::SyringeDrive syringeDrive(const sim::SyringeModel& listed) {
    const double force = 0.75 * listed.breakawayFriction.si();
    const double compression = force / listed.stopperStiffness.si();
    const double travel = sim::exampleMechanism.backlash.si() + (1.25 * compression);
    // Section en nL/mm : 1 mm² chasse 1 µL, soit 1 000 nL, par millimètre.
    return {.strokeVolume = static_cast<std::uint32_t>(std::lround(area(listed) * 1e9)),
            .takeUpForce = {.millinewtons = static_cast<std::int32_t>(std::lround(force * 1e3))},
            .takeUpTravel = static_cast<std::uint32_t>(std::lround(travel * 1e6))};
}

std::optional<control::Force> sensedForce(const sim::SyringePump& pump) {
    const auto force = pump.measuredForce();
    if (!force) {
        return std::nullopt;
    }
    return control::Force{.millinewtons =
                              static_cast<std::int32_t>(std::lround(force->si() * 1e3))};
}

// Commande et pousse-seringue : à chaque période, l'entraînement lit le compteur de pas et la
// force, puis le générateur émet les pas demandés, régulièrement, pendant la période suivante.
class Bench {
public:
    Bench(const sim::SyringeModel& actual, const sim::SyringeModel& listed,
          std::chrono::nanoseconds simulationStep)
        : pump_{sim::exampleMechanism, actual, sim::exampleLine},
          drive_{driveMechanism(), syringeDrive(listed), pump_.motorSteps()},
          simulationStep_{simulationStep} {}

    void cycle(control::Rate setpoint) {
        const control::DriveCommand command =
            drive_.update(period, setpoint, pump_.motorSteps(), sensedForce(pump_));
        largestCommand_ = std::max(largestCommand_, command.steps);
        pump_.setStepRate(sim::stepsPerSecond(static_cast<double>(command.steps) * 1e3 /
                                              static_cast<double>(period.count())));
        pump_.run(period, simulationStep_);
    }

    // Rattrapage jusqu'à la perfusion ou au défaut, borné à 10 s.
    void takeUp() {
        while (drive_.phase() == control::DrivePhase::TakingUp && pump_.elapsed() < 10s) {
            cycle(control::Rate{});
        }
        tracking_ = pump_.elapsed();
    }

    void run(control::Rate setpoint, std::chrono::nanoseconds duration) {
        const std::chrono::nanoseconds end = pump_.elapsed() + duration;
        while (pump_.elapsed() < end) {
            cycle(setpoint);
        }
    }

    sim::SyringePump& pump() { return pump_; }

    [[nodiscard]] const control::Drive& drive() const { return drive_; }

    [[nodiscard]] std::chrono::nanoseconds trackingSince() const { return tracking_; }

    [[nodiscard]] std::uint32_t largestCommand() const { return largestCommand_; }

private:
    sim::SyringePump pump_;
    control::Drive drive_;
    std::chrono::nanoseconds simulationStep_;
    std::chrono::nanoseconds tracking_{};
    std::uint32_t largestCommand_ = 0;
};

control::Rate rate(double millilitresPerHour) {
    return {.microlitresPerHour =
                static_cast<std::uint32_t>(std::lround(millilitresPerHour * 1e3))};
}

// Premier écoulement : plus de 1 µL sorti de la seringue, comme dans sim_syringe_pump_test.
constexpr double firstFlowVolume = 1.0; // µL

// Délai au premier écoulement sans rattrapage : moteur à la vitesse du débit dès le démarrage.
std::chrono::nanoseconds openLoopFirstFlow(const sim::SyringeModel& syringe, double flow) {
    sim::SyringePump pump{sim::exampleMechanism, syringe, sim::exampleLine};
    const double perStep = sim::exampleMechanism.screwLead.si() /
                           static_cast<double>(sim::exampleMechanism.stepsPerRevolution);
    pump.setStepRate(
        sim::StepRate::fromSi(sim::millilitresPerHour(flow).si() / area(syringe) / perStep));
    while (pump.elapsed() < 2h && sim::inMicrolitres(pump.deliveredVolume()) <= firstFlowVolume) {
        pump.run(1s);
    }
    return pump.elapsed();
}

void firstFlow(Expectations& t) {
    std::println("Délai au premier écoulement à 1 mL/h (modèle, paramètres d'exemple)");
    std::println(
        "| Seringue | Sans rattrapage | Rattrapage | Avec rattrapage | Volume du rattrapage |");
    for (const sim::SyringeModel& syringe : sim::exampleSyringes) {
        const std::chrono::nanoseconds openLoop = openLoopFirstFlow(syringe, 1.0);

        Bench bench{syringe, syringe, 1ms};
        bench.takeUp();
        const double takeUpVolume = sim::inMicrolitres(bench.pump().deliveredVolume());
        const bool tracking = bench.drive().phase() == control::DrivePhase::Tracking;
        while (bench.pump().elapsed() < 2h &&
               sim::inMicrolitres(bench.pump().deliveredVolume()) <= firstFlowVolume) {
            bench.run(rate(1.0), 1s);
        }
        const std::chrono::nanoseconds withTakeUp = bench.pump().elapsed();
        std::println("| {} | {:.0f} s | {:.2f} s | {:.0f} s | {:.4f} µL |", syringe.name,
                     seconds(openLoop), seconds(bench.trackingSince()), seconds(withTakeUp),
                     takeUpVolume);

        t.expect(tracking && seconds(bench.trackingSince()) < 1.0,
                 std::format("{} : rattrapage terminé en moins de 1 s", syringe.name));
        t.expect(takeUpVolume < 0.01, std::format("{} : le rattrapage ne délivre rien ({} µL)",
                                                  syringe.name, takeUpVolume));
        t.expect(withTakeUp < 5min,
                 std::format("{} : premier écoulement en moins de 5 min avec rattrapage ({} s)",
                             syringe.name, seconds(withTakeUp)));
        t.expect(withTakeUp < openLoop,
                 std::format("{} : le rattrapage raccourcit le délai", syringe.name));
        t.expect(bench.largestCommand() <= 20,
                 std::format("{} : au plus 20 pas par période, rattrapage compris", syringe.name));
    }
    t.expect(openLoopFirstFlow(sim::exampleSyringes[2], 1.0) > 5min,
             "50 mL sans rattrapage : plus de 5 min, le rattrapage est nécessaire");
}

// Extrêmes de l'erreur du débit moyen sur des fenêtres glissantes d'une durée donnée, comme les
// courbes en trompette de l'IEC 60601-2-24 : relevés toutes les 30 s.
struct Trumpet {
    std::optional<double> minimum; // aucune valeur si la fenêtre de mesure est plus courte
    std::optional<double> maximum;
};

struct Accuracy {
    double error = 0.0;  // relative, sur toute la fenêtre de mesure
    double window = 0.0; // h
    Trumpet twoMinutes;
    Trumpet thirtyOneMinutes;
};

constexpr std::chrono::seconds sampling = 30s;

std::chrono::nanoseconds timeFor(double millilitres, double millilitresPerHour) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::duration<double>{millilitres / millilitresPerHour * 3600.0});
}

Trumpet trumpet(const std::vector<double>& volumes, std::chrono::minutes duration, double flow) {
    const auto span = static_cast<std::size_t>(duration / sampling);
    Trumpet result;
    for (std::size_t first = 0; first + span < volumes.size(); ++first) {
        const double hours = std::chrono::duration<double>{duration}.count() / 3600.0;
        const double error = ((volumes[first + span] - volumes[first]) / hours - flow) / flow;
        result.minimum = std::min(result.minimum.value_or(error), error);
        result.maximum = std::max(result.maximum.value_or(error), error);
    }
    return result;
}

// Erreur du débit en régime établi. Mise en régime : la première heure, au plus le temps de
// délivrer le quart du volume utile ; fenêtre de mesure : une heure, au moins le temps de délivrer
// 1 mL, au plus celui de délivrer la moitié du volume utile.
Accuracy accuracy(const sim::SyringeModel& actual, const sim::SyringeModel& listed, double flow) {
    Bench bench{actual, listed, period};
    bench.takeUp();
    const double usable = sim::inMillilitres(actual.usableVolume);
    const std::chrono::nanoseconds settle =
        std::min<std::chrono::nanoseconds>(1h, timeFor(usable / 4.0, flow));
    const std::chrono::nanoseconds window = std::min(
        std::max<std::chrono::nanoseconds>(1h, timeFor(1.0, flow)), timeFor(usable / 2.0, flow));
    bench.run(rate(flow), settle);
    const std::chrono::nanoseconds start = bench.pump().elapsed();
    std::vector<double> volumes{sim::inMillilitres(bench.pump().deliveredVolume())};
    while (bench.pump().elapsed() - start < window) {
        bench.run(rate(flow), sampling);
        volumes.push_back(sim::inMillilitres(bench.pump().deliveredVolume()));
    }
    const double hours = seconds(bench.pump().elapsed() - start) / 3600.0;
    return {.error = ((volumes.back() - volumes.front()) / hours - flow) / flow,
            .window = hours,
            .twoMinutes = trumpet(volumes, 2min, flow),
            .thirtyOneMinutes = trumpet(volumes, 31min, flow)};
}

std::string percent(std::optional<double> value) {
    return value ? std::format("{:+.1f} %", *value * 100.0) : std::string{"—"};
}

void steadyStateAccuracy(Expectations& t) {
    constexpr std::array flows{0.1, 1.0, 5.0, 25.0, 100.0, 200.0};
    std::println("\nErreur du débit en régime établi (modèle, paramètres d'exemple)");
    std::println("| Seringue | Débit (mL/h) | Fenêtre (h) | Erreur moyenne | 2 min, min. et max. "
                 "| 31 min, min. et max. |");
    for (const sim::SyringeModel& syringe : sim::exampleSyringes) {
        for (const double flow : flows) {
            const Accuracy result = accuracy(syringe, syringe, flow);
            std::println("| {} | {} | {:.3f} | {:+.3f} % | {} ; {} | {} ; {} |", syringe.name, flow,
                         result.window, result.error * 100.0, percent(result.twoMinutes.minimum),
                         percent(result.twoMinutes.maximum),
                         percent(result.thirtyOneMinutes.minimum),
                         percent(result.thirtyOneMinutes.maximum));
            t.expect(std::abs(result.error) < 0.01,
                     std::format("{} à {} mL/h : erreur moyenne de {:+.3f} %, sous 1 %",
                                 syringe.name, flow, result.error * 100.0));
        }
    }
}

// Le débit délivré suit la section réelle : une seringue dont le diamètre s'écarte de 1 % du
// modèle de la liste donne une erreur d'environ 2 %, que l'asservissement ne voit pas. D'où
// l'identification de la seringue (SYS-REQ-047) et la caractérisation de chaque modèle (#73).
void diameterMismatch(Expectations& t) {
    const sim::SyringeModel& listed = sim::exampleSyringes[2];
    std::println("\nÉcart de diamètre intérieur au modèle de la liste, 50 mL à 5 mL/h");
    std::println("| Écart de diamètre | Erreur attendue (section) | Erreur obtenue |");
    for (const double deviation : {-0.01, 0.01}) {
        sim::SyringeModel actual = listed;
        actual.innerDiameter = listed.innerDiameter * (1.0 + deviation);
        const double expected = (area(actual) / area(listed)) - 1.0;
        const Accuracy result = accuracy(actual, listed, 5.0);
        std::println("| {:+.0f} % | {:+.2f} % | {:+.2f} % |", deviation * 100.0, expected * 100.0,
                     result.error * 100.0);
        t.expect(std::abs(result.error - expected) < 0.005,
                 std::format("diamètre {:+.0f} % : erreur de {:+.2f} %, attendue {:+.2f} %",
                             deviation * 100.0, result.error * 100.0, expected * 100.0));
    }
}

// Défaut pendant le rattrapage : l'entraînement s'arrête sur un défaut verrouillé, sans avoir
// délivré plus que la course maximale ne le permet.
void takeUpFaults(Expectations& t) {
    const sim::SyringeModel& syringe = sim::exampleSyringes[2];
    const control::SyringeDrive parameters = syringeDrive(syringe);
    const double travelVolume = parameters.takeUpTravel * 1e-6 * area(syringe) * 1e9; // µL

    const auto faulted = [&](const std::string& what, control::DriveFault expected, auto&& inject) {
        Bench bench{syringe, syringe, 1ms};
        inject(bench.pump());
        bench.takeUp();
        bench.run(rate(1.0), 1s);
        const double delivered = sim::inMicrolitres(bench.pump().deliveredVolume());
        t.expect(bench.drive().fault() == expected,
                 std::format("{} : défaut {}", what, static_cast<int>(expected)));
        t.expect(!bench.pump().motorRunning(), std::format("{} : moteur arrêté", what));
        t.expect(delivered < travelVolume,
                 std::format("{} : {} µL délivrés, moins que la course ({} µL)", what, delivered,
                             travelVolume));
        return delivered;
    };

    faulted("capteur de force perdu", control::DriveFault::ForceUnavailable,
            [](sim::SyringePump& pump) {
                pump.setForceSensorFault({.kind = sim::SensorFaultKind::lost, .offset = {}});
            });
    const double stuck =
        faulted("capteur de force figé à zéro", control::DriveFault::TakeUpTravelExceeded,
                [](sim::SyringePump& pump) {
                    pump.setForceSensorFault({.kind = sim::SensorFaultKind::stuck, .offset = {}});
                });
    t.expect(stuck < 0.01, std::format("force figée : la course maximale reste sous le "
                                       "frottement statique ({} µL)",
                                       stuck));
    faulted(
        "seringue mal placée", control::DriveFault::TakeUpTravelExceeded,
        [](sim::SyringePump& pump) { pump.setSyringePlacement(sim::SyringePlacement::misplaced); });
    faulted(
        "seringue absente", control::DriveFault::TakeUpTravelExceeded,
        [](sim::SyringePump& pump) { pump.setSyringePlacement(sim::SyringePlacement::absent); });
    faulted("moteur calé", control::DriveFault::TakeUpTravelExceeded,
            [](sim::SyringePump& pump) { pump.setDriverFault(sim::DriverFault::stall); });
}

} // namespace

int main() {
    return Expectations{}.run([](Expectations& t) {
        firstFlow(t);
        steadyStateAccuracy(t);
        diameterMismatch(t);
        takeUpFaults(t);
    });
}
