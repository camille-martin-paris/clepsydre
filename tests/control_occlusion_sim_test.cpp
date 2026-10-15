// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

// Détection d'occlusion et décompression (#27) en boucle fermée avec l'asservissement du moteur et
// le modèle physique du pousse-seringue (ADR 0002) : délai de détection d'une occlusion aval à 1 et
// 25 mL/h, au seuil par défaut et au seuil le plus élevé (CTRL-007, SYS-REQ-008) ; bolus libéré à
// la levée de l'occlusion, avec et sans recul du pousseur (CTRL-008, SYS-REQ-009) ; aucune fausse
// détection en marche normale de 0,1 à 200 mL/h ; capteur de force perdu ou figé.
//
// Les résultats valent pour le modèle et ses paramètres d'exemple, jamais pour la pompe : les
// délais d'occlusion et le bolus se mesurent sur banc (#74). Les tableaux imprimés sont repris
// dans docs/design/occlusion.md.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <expect.hpp>
#include <format>
#include <numbers>
#include <optional>
#include <print>
#include <string>

import clepsydre.control.drive;
import clepsydre.control.infusion;
import clepsydre.control.occlusion;
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

control::Force newtons(double value) {
    return {.millinewtons = static_cast<std::int32_t>(std::lround(value * 1e3))};
}

control::Rate rate(double millilitresPerHour) {
    return {.microlitresPerHour =
                static_cast<std::uint32_t>(std::lround(millilitresPerHour * 1e3))};
}

// Mécanisme et seringue tirés du modèle comme dans control_drive_sim_test.
control::DriveMechanism driveMechanism() {
    const auto stepRate = static_cast<std::uint32_t>(sim::exampleMechanism.maxStepRate.si());
    return {.screwLeadNanometres =
                static_cast<std::uint32_t>(std::lround(sim::exampleMechanism.screwLead.si() * 1e9)),
            .stepsPerRevolution =
                static_cast<std::uint32_t>(sim::exampleMechanism.stepsPerRevolution),
            .maxStepRate = stepRate,
            .takeUpStepRate = stepRate,
            .period = period};
}

control::SyringeDrive syringeDrive(const sim::SyringeModel& listed) {
    const double force = 0.75 * listed.breakawayFriction.si();
    const double compression = force / listed.stopperStiffness.si();
    const double travel = sim::exampleMechanism.backlash.si() + (1.25 * compression);
    return {.strokeVolume = static_cast<std::uint32_t>(std::lround(area(listed) * 1e9)),
            .takeUpForce = newtons(force),
            .takeUpTravel = static_cast<std::uint32_t>(std::lround(travel * 1e6))};
}

// Course de recul par newton, en m/N : compliance vue du pousseur quand le piston est retenu, celle
// du joint (1/k) et celle du corps et du prolongateur ramenée à la course (C/A²).
double backoffPerNewton(const sim::SyringeModel& listed) {
    const double a = area(listed);
    return (1.0 / listed.stopperStiffness.si()) + (listed.compliance.si() / (a * a));
}

// Réglages que le banc d'occlusion établirait pour le modèle de la liste (#74) : seuils à 1,5, 2 et
// 4 fois le frottement statique du joint, au-dessus de la force de marche au débit maximal ;
// confirmation de 100 ms ; recul par newton égal à la compliance vue du pousseur ; course maximale
// du recul : celle du seuil le plus élevé, avec 25 % de marge.
control::OcclusionSettings occlusionSettings(const sim::SyringeModel& listed) {
    const double breakaway = listed.breakawayFriction.si();
    const double perNewton = backoffPerNewton(listed);
    return {.minThreshold = newtons(1.5 * breakaway),
            .defaultThreshold = newtons(2.0 * breakaway),
            .maxThreshold = newtons(4.0 * breakaway),
            .confirmation = 100ms,
            .backoffPerNewton = static_cast<std::uint32_t>(std::lround(perNewton * 1e9)),
            .maxBackoffTravel =
                static_cast<std::uint32_t>(std::lround(1.25 * 4.0 * breakaway * perNewton * 1e6))};
}

std::optional<control::Force> sensedForce(const sim::SyringePump& pump) {
    const auto force = pump.measuredForce();
    if (!force) {
        return std::nullopt;
    }
    return newtons(force->si());
}

sim::Line occluded() {
    sim::Line line = sim::exampleLine;
    line.resistance = line.resistance * 1e6;
    return line;
}

// Boucle de commande : à chaque période, le surveillant lit le compteur de pas et la force ; tant
// qu'il ne détecte rien, l'entraînement commande le moteur, puis le surveillant le remplace et fait
// reculer le pousseur. Le générateur émet les pas demandés pendant la période suivante.
class Bench {
public:
    Bench(const sim::SyringeModel& syringe, control::Force threshold, bool decompression)
        : pump_{sim::exampleMechanism, syringe, sim::exampleLine},
          drive_{driveMechanism(), syringeDrive(syringe), pump_.motorSteps()},
          monitor_{driveMechanism(), syringeDrive(syringe), occlusionSettings(syringe), threshold},
          decompression_{decompression} {}

    void cycle(control::Rate setpoint) {
        const std::int64_t steps = pump_.motorSteps();
        const std::optional<control::Force> force = sensedForce(pump_);
        const control::OcclusionCommand occlusion = monitor_.update(period, steps, force);
        double stepRate = 0.0;
        if (occlusion.occlusion || occlusion.fault) {
            stepRate = decompression_ ? -static_cast<double>(occlusion.backoffSteps) : 0.0;
        } else {
            const control::DriveCommand command = drive_.update(period, setpoint, steps, force);
            stepRate = static_cast<double>(command.steps);
        }
        pump_.setStepRate(
            sim::stepsPerSecond(stepRate * 1e3 / static_cast<double>(period.count())));
        pump_.run(period, 1ms);
        lowest_ = std::min(lowest_, pump_.deliveredVolume().si());
    }

    void takeUp() {
        while (drive_.phase() == control::DrivePhase::TakingUp && pump_.elapsed() < 10s) {
            cycle(control::Rate{});
        }
    }

    void run(control::Rate setpoint, std::chrono::nanoseconds duration) {
        const std::chrono::nanoseconds end = pump_.elapsed() + duration;
        while (pump_.elapsed() < end) {
            cycle(setpoint);
        }
    }

    // Jusqu'à la détection, bornée par limit ; rend le délai, ou rien sans détection.
    std::optional<std::chrono::nanoseconds> untilDetected(control::Rate setpoint,
                                                          std::chrono::nanoseconds limit) {
        const std::chrono::nanoseconds start = pump_.elapsed();
        while (!monitor_.detectionForce() && !monitor_.fault() && pump_.elapsed() - start < limit) {
            cycle(setpoint);
        }
        if (!monitor_.detectionForce()) {
            return std::nullopt;
        }
        return pump_.elapsed() - start;
    }

    // Marque le volume de référence de l'aspiration.
    void mark() { lowest_ = pump_.deliveredVolume().si(); }

    sim::SyringePump& pump() { return pump_; }

    [[nodiscard]] const control::Drive& drive() const { return drive_; }

    [[nodiscard]] const control::OcclusionMonitor& monitor() const { return monitor_; }

    // Plus petit volume délivré depuis mark(), en m³ : un recul qui aspire le fait baisser.
    [[nodiscard]] double lowest() const { return lowest_; }

private:
    sim::SyringePump pump_;
    control::Drive drive_;
    control::OcclusionMonitor monitor_;
    bool decompression_;
    double lowest_ = 0.0;
};

// Mise en régime avant l'occlusion : premier écoulement atteint et force établie.
std::chrono::nanoseconds settling(double flow) {
    return flow < 5.0 ? 30min : 5min;
}

struct Trial {
    std::optional<std::chrono::nanoseconds> delay;
    double detectionForce = 0.0; // N
    double backoff = 0.0;        // mm, course de recul demandée
    double aspiration = 0.0;     // µL revenus de la ligne pendant le recul et l'attente
    double bolus = 0.0;          // µL sortis dans la minute qui suit la levée
};

// Essai à la manière de l'IEC 60601-2-24, sans en revendiquer la conformité : occlusion en
// régime établi, délai jusqu'à la détection, puis levée de l'occlusion 1 min après la détection et
// volume sorti dans la minute qui suit.
Trial occlusionTrial(const sim::SyringeModel& syringe, double flow, control::Force threshold,
                     bool decompression) {
    Bench bench{syringe, threshold, decompression};
    bench.takeUp();
    bench.run(rate(flow), settling(flow));
    bench.pump().setLine(occluded());
    Trial trial;
    trial.delay = bench.untilDetected(rate(flow), 2h);
    if (!trial.delay) {
        return trial;
    }
    trial.detectionForce =
        static_cast<double>(
            bench.monitor().detectionForce().value_or(control::Force{}).millinewtons) /
        1e3;
    bench.mark();
    const double atDetection = bench.pump().deliveredVolume().si();
    bench.run(rate(flow), 1min);
    trial.backoff = static_cast<double>(bench.monitor().backoffRequested()) *
                    sim::exampleMechanism.screwLead.si() * 1e3 /
                    static_cast<double>(sim::exampleMechanism.stepsPerRevolution);
    trial.aspiration = (atDetection - bench.lowest()) * 1e9;
    const sim::Volume atRelease = bench.pump().deliveredVolume();
    bench.pump().setLine(sim::exampleLine);
    bench.run(rate(flow), 1min);
    trial.bolus = sim::inMicrolitres(bench.pump().deliveredVolume() - atRelease);
    return trial;
}

std::string minutes(std::optional<std::chrono::nanoseconds> delay) {
    return delay ? std::format("{:.1f} min", seconds(*delay) / 60.0) : std::string{"aucune"};
}

void detectionAndBolus(Expectations& t) {
    std::println("Occlusion aval : délai de détection et bolus à la levée (modèle, paramètres "
                 "d'exemple)");
    std::println("| Seringue | Débit (mL/h) | Seuil (N) | Délai | Force à la détection (N) | "
                 "Recul (mm) | Aspiration (µL) | Bolus avec recul (µL) | Bolus sans recul (µL) |");
    for (const sim::SyringeModel& syringe : sim::exampleSyringes) {
        const control::OcclusionSettings settings = occlusionSettings(syringe);
        for (const double flow : {1.0, 25.0}) {
            for (const control::Force threshold :
                 {settings.defaultThreshold, settings.maxThreshold}) {
                const Trial with = occlusionTrial(syringe, flow, threshold, true);
                const Trial without = occlusionTrial(syringe, flow, threshold, false);
                const double newtonsThreshold = static_cast<double>(threshold.millinewtons) / 1e3;
                std::println(
                    "| {} | {} | {:.0f} | {} | {:.2f} | {:.3f} | {:.1f} | {:.0f} | {:.0f} |",
                    syringe.name, flow, newtonsThreshold, minutes(with.delay), with.detectionForce,
                    with.backoff, with.aspiration, with.bolus, without.bolus);
                const std::string what = std::format("{} à {} mL/h, seuil de {:.0f} N",
                                                     syringe.name, flow, newtonsThreshold);
                if (!t.expect(with.delay.has_value(), what + " : occlusion détectée")) {
                    continue;
                }
                if (threshold.millinewtons == settings.defaultThreshold.millinewtons) {
                    // SYS-REQ-008 : 30 min au plus à 1 mL/h, 2 min au plus à 25 mL/h.
                    const std::chrono::nanoseconds limit = flow < 5.0 ? 30min : 2min;
                    t.expect(with.delay.value_or(limit + 1s) <= limit,
                             std::format("{} : détection en {}", what, minutes(with.delay)));
                } else {
                    // SYS-REQ-009 : bolus sous 0,2 mL au seuil le plus élevé.
                    t.expect(with.bolus < 200.0,
                             std::format("{} : bolus de {:.0f} µL, sous 0,2 mL", what, with.bolus));
                }
                t.expect(with.bolus < without.bolus,
                         std::format("{} : le recul réduit le bolus ({:.0f} µL contre {:.0f} µL)",
                                     what, with.bolus, without.bolus));
                t.expect(
                    with.aspiration < 10.0,
                    std::format("{} : le recul n'aspire pas ({:.1f} µL)", what, with.aspiration));
            }
        }
    }
}

// Marche normale : aucune détection de 0,1 à 200 mL/h au seuil le plus bas, pendant 10 min, au plus
// le temps de délivrer la moitié du volume utile : en fin de course, la force monte et la détection
// est due.
void noFalseAlarm(Expectations& t) {
    for (const sim::SyringeModel& syringe : sim::exampleSyringes) {
        for (const double flow : {0.1, 1.0, 25.0, 100.0, 200.0}) {
            Bench bench{syringe, occlusionSettings(syringe).minThreshold, true};
            bench.takeUp();
            const double usable = sim::inMillilitres(syringe.usableVolume);
            bench.run(rate(flow),
                      std::min<std::chrono::nanoseconds>(
                          10min, std::chrono::duration_cast<std::chrono::nanoseconds>(
                                     std::chrono::duration<double>{usable / 2.0 / flow * 3600.0})));
            t.expect(bench.monitor().phase() == control::OcclusionPhase::Watching &&
                         !bench.drive().fault(),
                     std::format("{} à {} mL/h : aucune fausse détection au seuil le plus bas",
                                 syringe.name, flow));
        }
    }
}

// Capteur de force perdu : défaut immédiat, moteur arrêté. Capteur figé : l'occlusion n'est pas
// vue par la force, ce qui relève de la plausibilité du capteur et de la voie de secours du canal
// de sécurité (ADR 0003).
void forceSensorFaults(Expectations& t) {
    const sim::SyringeModel& syringe = sim::exampleSyringes[2];
    const control::Force threshold = occlusionSettings(syringe).defaultThreshold;
    {
        Bench bench{syringe, threshold, true};
        bench.takeUp();
        bench.run(rate(25.0), 1min);
        bench.pump().setForceSensorFault({.kind = sim::SensorFaultKind::lost, .offset = {}});
        bench.run(rate(25.0), 1s);
        t.expect(bench.monitor().fault() == control::OcclusionFault::ForceUnavailable &&
                     !bench.pump().motorRunning(),
                 "capteur de force perdu en perfusion : défaut, moteur arrêté");
    }
    {
        Bench bench{syringe, threshold, true};
        bench.takeUp();
        bench.run(rate(25.0), 5min);
        bench.pump().setForceSensorFault({.kind = sim::SensorFaultKind::stuck, .offset = {}});
        bench.pump().setLine(occluded());
        const auto delay = bench.untilDetected(rate(25.0), 10min);
        t.expect(!delay && sim::inNewtons(bench.pump().contactForce()) >
                               2.0 * threshold.millinewtons / 1e3,
                 "capteur de force figé : l'occlusion échappe à la force, qui dépasse pourtant "
                 "le seuil");
    }
}

} // namespace

int main() {
    return Expectations{}.run([](Expectations& t) {
        detectionAndBolus(t);
        noFalseAlarm(t);
        forceSensorFaults(t);
    });
}
