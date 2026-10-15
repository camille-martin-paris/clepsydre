// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

// Détection d'occlusion et décompression (#27) : plages des réglages ; seuil choisi dans ses
// bornes ; détection confirmée par la durée au seuil ; course de recul calculée, bornée et
// tronquée au pas ; contrôle du compteur de pas pendant le recul ; défauts verrouillés. Les
// attendus sont calculés à la main, avec une vis de 1 mm à 3 200 pas par tour, un moteur à
// 2 000 pas/s et une période de 10 ms : un pas fait 312,5 nm, et le recul avance de 20 pas par
// période. Avec 25 µm de recul par newton, une détection à 20 N recule de 500 µm, soit 1 600 pas.

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <expect.hpp>
#include <expected>
#include <format>
#include <optional>
#include <string>

import clepsydre.control.drive;
import clepsydre.control.occlusion;

namespace {

using namespace clepsydre::control;
using namespace std::chrono_literals;
using clepsydre::test::Expectations;

Force mN(std::int32_t millinewtons) {
    return {.millinewtons = millinewtons};
}

bool same(std::optional<Force> force, std::int32_t millinewtons) {
    return force && force->millinewtons == millinewtons;
}

bool admitted(const std::expected<Force, ThresholdRefusal>& threshold, std::int32_t millinewtons) {
    return threshold && threshold->millinewtons == millinewtons;
}

DriveMechanism mechanism() {
    return {.screwLeadNanometres = 1'000'000,
            .stepsPerRevolution = 3'200,
            .maxStepRate = 2'000,
            .takeUpStepRate = 2'000,
            .period = 10ms};
}

// 500 mm² ; rattrapage terminé à 5 N.
SyringeDrive syringe() {
    return {.strokeVolume = 500'000, .takeUpForce = mN(5'000), .takeUpTravel = 100};
}

// Seuils de 10, 20 et 40 N ; confirmation de 100 ms ; 25 µm de recul par newton, au plus 1,25 mm.
OcclusionSettings settings() {
    return {.minThreshold = mN(10'000),
            .defaultThreshold = mN(20'000),
            .maxThreshold = mN(40'000),
            .confirmation = 100ms,
            .backoffPerNewton = 25'000,
            .maxBackoffTravel = 1'250};
}

// Le constructeur suit selectThreshold : surveillance si admis, défaut verrouillé sinon.
bool constructorAgrees(const DriveMechanism& m, const SyringeDrive& s, const OcclusionSettings& o,
                       Force threshold) {
    const OcclusionMonitor monitor{m, s, o, threshold};
    if (selectThreshold(threshold, m, s, o)) {
        return monitor.phase() == OcclusionPhase::Watching && !monitor.fault();
    }
    return monitor.phase() == OcclusionPhase::Faulted &&
           monitor.fault() == OcclusionFault::InvalidParameters;
}

void parameters(Expectations& t) {
    t.expect(validOcclusionSettings(mechanism(), syringe(), settings()),
             "réglages de référence admis");

    const auto check = [&](bool admitted, const DriveMechanism& m, const SyringeDrive& s,
                           const OcclusionSettings& o, const std::string& what) {
        t.expect(validOcclusionSettings(m, s, o) == admitted &&
                     constructorAgrees(m, s, o, o.defaultThreshold),
                 what + (admitted ? " : admis" : " : refusé"));
    };
    const auto withSettings = [&](bool admitted, auto&& change, const std::string& what) {
        OcclusionSettings o = settings();
        change(o);
        check(admitted, mechanism(), syringe(), o, what);
    };

    SyringeDrive invalidDrive = syringe();
    invalidDrive.strokeVolume = 0;
    check(false, mechanism(), invalidDrive, settings(), "paramètres d'entraînement refusés");

    withSettings(
        false, [](auto& o) { o.minThreshold = mN(5'000); },
        "borne basse égale à la force de fin du rattrapage");
    withSettings(
        true, [](auto& o) { o.minThreshold = mN(5'001); },
        "borne basse juste au-dessus de la force de fin du rattrapage");
    withSettings(
        false, [](auto& o) { o.defaultThreshold = mN(9'999); },
        "seuil par défaut sous la borne basse");
    withSettings(
        false, [](auto& o) { o.defaultThreshold = mN(40'001); },
        "seuil par défaut au-dessus de la borne haute");
    withSettings(
        true,
        [](auto& o) {
            o.defaultThreshold = mN(10'000);
            o.maxThreshold = mN(10'000);
        },
        "bornes et seuil par défaut confondus");
    withSettings(
        false, [](auto& o) { o.maxThreshold = mN(200'001); }, "borne haute au-delà de 200 N");
    withSettings(true, [](auto& o) { o.maxThreshold = mN(200'000); }, "borne haute de 200 N");
    withSettings(false, [](auto& o) { o.confirmation = -1ms; }, "confirmation négative");
    withSettings(true, [](auto& o) { o.confirmation = 0ms; }, "confirmation nulle");
    withSettings(true, [](auto& o) { o.confirmation = 1'000ms; }, "confirmation de 1 s");
    withSettings(false, [](auto& o) { o.confirmation = 1'001ms; }, "confirmation au-delà de 1 s");
    withSettings(false, [](auto& o) { o.backoffPerNewton = 0; }, "recul nul par newton");
    withSettings(true, [](auto& o) { o.backoffPerNewton = 1; }, "recul de 1 nm par newton");
    withSettings(true, [](auto& o) { o.backoffPerNewton = 1'000'000; }, "recul de 1 mm par newton");
    withSettings(
        false, [](auto& o) { o.backoffPerNewton = 1'000'001; }, "recul au-delà de 1 mm par newton");
    withSettings(false, [](auto& o) { o.maxBackoffTravel = 0; }, "course maximale nulle");
    withSettings(true, [](auto& o) { o.maxBackoffTravel = 10'000; }, "course maximale de 10 mm");
    withSettings(
        false, [](auto& o) { o.maxBackoffTravel = 10'001; }, "course maximale au-delà de 10 mm");

    // Vis de 10 mm à un pas par tour : la course maximale doit faire au moins un pas de 10 mm.
    const DriveMechanism coarse{.screwLeadNanometres = 10'000'000,
                                .stepsPerRevolution = 1,
                                .maxStepRate = 100,
                                .takeUpStepRate = 100,
                                .period = 10ms};
    SyringeDrive longTakeUp = syringe();
    longTakeUp.takeUpTravel = 10'000;
    OcclusionSettings o = settings();
    o.maxBackoffTravel = 9'999;
    check(false, coarse, longTakeUp, o, "course maximale sous un pas");
    o.maxBackoffTravel = 10'000;
    check(true, coarse, longTakeUp, o, "course maximale d'un pas");
}

void thresholds(Expectations& t) {
    const auto select = [](std::int32_t millinewtons) {
        return selectThreshold(mN(millinewtons), mechanism(), syringe(), settings());
    };
    t.expect(admitted(select(10'000), 10'000), "borne basse admise");
    t.expect(admitted(select(40'000), 40'000), "borne haute admise");
    t.expect(admitted(select(25'000), 25'000), "seuil intermédiaire admis tel quel");
    t.expect(select(9'999) == std::unexpected{ThresholdRefusal::BelowMinimum},
             "sous la borne basse : refusé, jamais corrigé");
    t.expect(select(40'001) == std::unexpected{ThresholdRefusal::AboveMaximum},
             "au-dessus de la borne haute : refusé, jamais corrigé");
    OcclusionSettings invalid = settings();
    invalid.confirmation = -1ms;
    t.expect(selectThreshold(mN(20'000), mechanism(), syringe(), invalid) ==
                 std::unexpected{ThresholdRefusal::InvalidSettings},
             "réglages refusés : tout seuil est refusé");

    for (const std::int32_t threshold : {9'999, 10'000, 40'000, 40'001}) {
        t.expect(constructorAgrees(mechanism(), syringe(), settings(), mN(threshold)),
                 std::format("seuil de {} mN : le constructeur suit selectThreshold", threshold));
    }
    OcclusionMonitor refused{mechanism(), syringe(), settings(), mN(40'001)};
    const OcclusionCommand command = refused.update(10ms, 0, mN(50'000));
    t.expect(!command.occlusion && command.backoffSteps == 0 &&
                 command.fault == OcclusionFault::InvalidParameters,
             "seuil refusé : défaut verrouillé, aucune détection, aucun pas");
}

// Surveillant au seuil par défaut, qui confirme l'occlusion à la force donnée au compteur
// stepCount. Rend le nombre de relevés au seuil avant la détection.
int detect(OcclusionMonitor& monitor, Force force, std::int64_t stepCount) {
    for (int samples = 1; samples <= 1'000; ++samples) {
        if (monitor.update(10ms, stepCount, force).occlusion) {
            return samples;
        }
    }
    return 0;
}

void detection(Expectations& t) {
    {
        OcclusionMonitor monitor{mechanism(), syringe(), settings(), mN(20'000)};
        bool quiet = true;
        for (int i = 0; i < 1'000; ++i) {
            const OcclusionCommand command = monitor.update(10ms, 0, mN(19'999));
            quiet = quiet && !command.occlusion && command.backoffSteps == 0 && !command.fault;
        }
        t.expect(quiet && monitor.phase() == OcclusionPhase::Watching,
                 "force juste sous le seuil pendant 10 s : aucune détection");
    }
    {
        // Le premier relevé au seuil ouvre la confirmation : 100 ms plus tard, au onzième relevé.
        OcclusionMonitor monitor{mechanism(), syringe(), settings(), mN(20'000)};
        t.expect(detect(monitor, mN(20'000), 0) == 11,
                 "force au seuil : détection au onzième relevé, 100 ms après le premier");
        t.expect(monitor.phase() == OcclusionPhase::Stopping &&
                     same(monitor.detectionForce(), 20'000),
                 "détection : arrêt, force retenue");
    }
    {
        // Une pointe interrompue repart de zéro.
        OcclusionMonitor monitor{mechanism(), syringe(), settings(), mN(20'000)};
        for (int i = 0; i < 10; ++i) {
            static_cast<void>(monitor.update(10ms, 0, mN(30'000)));
        }
        const OcclusionCommand dip = monitor.update(10ms, 0, mN(19'999));
        t.expect(!dip.occlusion, "dix relevés au seuil, puis un sous le seuil : aucune détection");
        t.expect(detect(monitor, mN(30'000), 0) == 11,
                 "après un relevé sous le seuil, la confirmation repart de zéro");
    }
    {
        // Intervalles irréguliers : la durée au seuil est celle mesurée.
        OcclusionMonitor monitor{mechanism(), syringe(), settings(), mN(20'000)};
        static_cast<void>(monitor.update(10ms, 0, mN(20'000)));
        static_cast<void>(monitor.update(99ms, 0, mN(20'000)));
        t.expect(monitor.phase() == OcclusionPhase::Watching, "99 ms au seuil : pas encore");
        t.expect(monitor.update(1ms, 0, mN(20'000)).occlusion, "100 ms au seuil : détection");
    }
    {
        OcclusionSettings immediate = settings();
        immediate.confirmation = 0ms;
        OcclusionMonitor monitor{mechanism(), syringe(), immediate, mN(20'000)};
        t.expect(detect(monitor, mN(20'000), 0) == 1,
                 "confirmation nulle : détection au premier relevé");
    }
    {
        OcclusionMonitor monitor{mechanism(), syringe(), settings(), mN(40'000)};
        t.expect(monitor.threshold().millinewtons == 40'000, "seuil retenu : 40 N");
        t.expect(detect(monitor, mN(39'999), 0) == 0 && detect(monitor, mN(40'000), 0) == 11,
                 "seuil réglé à la borne haute : détection à 40 N, pas en dessous");
    }
}

// Recul complet : le générateur émet chaque commande pendant la période suivante. Rend les pas
// demandés, ou -1 sur un défaut.
std::int64_t backOff(OcclusionMonitor& monitor, std::int64_t& stepCount, std::uint32_t& largest,
                     std::optional<Force> force = mN(0)) {
    std::int64_t total = 0;
    for (int i = 0; i < 1'000 && monitor.phase() != OcclusionPhase::Decompressed; ++i) {
        const OcclusionCommand command = monitor.update(10ms, stepCount, force);
        if (command.fault || !command.occlusion) {
            return -1;
        }
        total += command.backoffSteps;
        largest = std::max(largest, command.backoffSteps);
        stepCount -= command.backoffSteps;
    }
    return total;
}

void backoff(Expectations& t) {
    {
        OcclusionMonitor monitor{mechanism(), syringe(), settings(), mN(20'000)};
        t.expect(detect(monitor, mN(20'000), 1'000) == 11, "détection à 20 N");
        // L'entraînement émet encore un pas de sa dernière commande : il sert d'origine.
        std::int64_t stepCount = 1'001;
        std::uint32_t largest = 0;
        const std::int64_t total = backOff(monitor, stepCount, largest);
        t.expect(total == 1'600 && monitor.backoffTarget() == 1'600 &&
                     monitor.backoffRequested() == 1'600,
                 std::format("20 N × 25 µm/N = 500 µm : 1 600 pas de recul ({})", total));
        t.expect(largest == 20, "au plus 20 pas par période : la vitesse maximale du moteur");
        t.expect(stepCount == -599, "le compteur recule de l'origine à l'origine moins 1 600");
        bool still = true;
        for (int i = 0; i < 100; ++i) {
            const OcclusionCommand command = monitor.update(10ms, stepCount, std::nullopt);
            still = still && command.occlusion && command.backoffSteps == 0 && !command.fault;
        }
        t.expect(still && monitor.phase() == OcclusionPhase::Decompressed,
                 "recul achevé : moteur arrêté, occlusion toujours signalée, force non requise");
    }
    {
        // 20,1 N : 502,5 µm, soit 1 608 pas ; la dernière commande est de 8 pas.
        OcclusionMonitor monitor{mechanism(), syringe(), settings(), mN(20'000)};
        static_cast<void>(detect(monitor, mN(20'100), 0));
        std::int64_t stepCount = 0;
        std::uint32_t largest = 0;
        t.expect(backOff(monitor, stepCount, largest) == 1'608,
                 "20,1 N : 1 608 pas, dernière commande tronquée");
    }
    {
        // 20 N avec 25 001 nm/N : 500 020 nm, soit 1 600,064 pas, tronqués à 1 600.
        OcclusionSettings o = settings();
        o.backoffPerNewton = 25'001;
        OcclusionMonitor monitor{mechanism(), syringe(), o, mN(20'000)};
        static_cast<void>(detect(monitor, mN(20'000), 0));
        std::int64_t stepCount = 0;
        std::uint32_t largest = 0;
        t.expect(backOff(monitor, stepCount, largest) == 1'600,
                 "course tronquée au pas : jamais plus que la course calculée");
    }
    {
        // 60 N : 1,5 mm calculés, bornés à 1,25 mm, soit 4 000 pas.
        OcclusionMonitor monitor{mechanism(), syringe(), settings(), mN(40'000)};
        static_cast<void>(detect(monitor, mN(60'000), 0));
        std::int64_t stepCount = 0;
        std::uint32_t largest = 0;
        t.expect(backOff(monitor, stepCount, largest) == 4'000,
                 "force au-delà de la course maximale : recul borné à 1,25 mm");
    }
    {
        // Recul de moins d'un pas : 10 N × 1 nm/N = 10 nm, aucun pas.
        OcclusionSettings o = settings();
        o.backoffPerNewton = 1;
        OcclusionMonitor monitor{mechanism(), syringe(), o, mN(10'000)};
        static_cast<void>(detect(monitor, mN(10'000), 0));
        const OcclusionCommand command = monitor.update(10ms, 0, mN(10'000));
        t.expect(command.occlusion && command.backoffSteps == 0 &&
                     monitor.phase() == OcclusionPhase::Decompressed,
                 "course de moins d'un pas : recul achevé sans pas");
    }
}

// Surveillant en recul : détection à 20 N au compteur 0, première commande de 20 pas demandée.
OcclusionMonitor backingOff(Expectations& t) {
    OcclusionMonitor monitor{mechanism(), syringe(), settings(), mN(20'000)};
    static_cast<void>(detect(monitor, mN(20'000), 0));
    const OcclusionCommand first = monitor.update(10ms, 0, mN(20'000));
    t.expect(first.backoffSteps == 20 && monitor.phase() == OcclusionPhase::BackingOff,
             "première commande de recul : 20 pas");
    return monitor;
}

void counter(Expectations& t) {
    {
        // Commandes de 20 pas : la dernière peut n'être pas encore émise, et la précédente l'être
        // à un pas près.
        OcclusionMonitor monitor = backingOff(t);
        t.expect(monitor.update(10ms, 0, mN(0)).backoffSteps == 20,
                 "dernière commande non encore émise : admis");
        t.expect(monitor.update(10ms, -19, mN(0)).backoffSteps == 20,
                 "un pas de la commande précédente non encore émis : admis");
    }
    {
        OcclusionMonitor monitor = backingOff(t);
        static_cast<void>(monitor.update(10ms, 0, mN(0)));
        t.expect(monitor.update(10ms, -18, mN(0)).fault == OcclusionFault::StepsNotEmitted,
                 "deux pas non émis : défaut");
    }
    {
        OcclusionMonitor monitor = backingOff(t);
        t.expect(monitor.update(10ms, 1, mN(0)).fault == OcclusionFault::CounterInconsistent,
                 "compteur qui avance pendant le recul : défaut");
    }
    {
        OcclusionMonitor monitor = backingOff(t);
        t.expect(monitor.update(10ms, -21, mN(0)).fault == OcclusionFault::CounterInconsistent,
                 "compteur qui recule au-delà des pas demandés : défaut");
    }
    {
        // Générateur en retard d'une commande jusqu'à la dernière : le surveillant attend une
        // période, puis exige l'émission.
        const auto lateGenerator = [&](bool emitsLast) {
            OcclusionMonitor monitor{mechanism(), syringe(), settings(), mN(20'000)};
            static_cast<void>(detect(monitor, mN(20'000), 0));
            std::int64_t emitted = 0;
            std::uint32_t previous = 0;
            for (int i = 0; i < 200; ++i) {
                const OcclusionCommand command = monitor.update(10ms, emitted, mN(0));
                if (command.fault) {
                    return std::optional<OcclusionFault>{command.fault};
                }
                if (command.backoffSteps == 0) {
                    break; // toute la course est demandée : attente de la dernière commande
                }
                emitted -= previous; // la commande précédente est émise maintenant
                previous = command.backoffSteps;
            }
            const bool waited = monitor.phase() == OcclusionPhase::BackingOff &&
                                monitor.backoffRequested() == 1'600;
            const OcclusionCommand last =
                monitor.update(10ms, emitsLast ? emitted - previous : emitted, mN(0));
            if (!waited) {
                return std::optional<OcclusionFault>{OcclusionFault::InvalidParameters};
            }
            return last.fault;
        };
        t.expect(!lateGenerator(true), "dernière commande émise en retard : admis");
        t.expect(lateGenerator(false) == OcclusionFault::StepsNotEmitted,
                 "dernière commande jamais émise : défaut");
    }
    {
        OcclusionMonitor monitor{mechanism(), syringe(), settings(), mN(20'000)};
        static_cast<void>(detect(monitor, mN(20'000), 0));
        std::int64_t stepCount = 0;
        std::uint32_t largest = 0;
        static_cast<void>(backOff(monitor, stepCount, largest));
        t.expect(monitor.update(10ms, stepCount + 1, mN(0)).fault ==
                     OcclusionFault::CounterInconsistent,
                 "recul achevé : un pas vers l'avant est un défaut");
    }
}

void faults(Expectations& t) {
    {
        OcclusionMonitor monitor{mechanism(), syringe(), settings(), mN(20'000)};
        const OcclusionCommand lost = monitor.update(10ms, 0, std::nullopt);
        const OcclusionCommand later = monitor.update(10ms, 0, mN(50'000));
        t.expect(lost.fault == OcclusionFault::ForceUnavailable && !lost.occlusion &&
                     later.fault == OcclusionFault::ForceUnavailable && !later.occlusion &&
                     later.backoffSteps == 0 && monitor.phase() == OcclusionPhase::Faulted,
                 "force absente pendant la surveillance : défaut verrouillé, même si elle revient");
    }
    for (const auto elapsed : {0ms, 100ms}) {
        OcclusionMonitor monitor{mechanism(), syringe(), settings(), mN(20'000)};
        t.expect(!monitor.update(elapsed, 0, mN(0)).fault,
                 std::format("intervalle de {} admis", elapsed));
    }
    for (const auto elapsed : {-1ms, 101ms}) {
        OcclusionMonitor monitor{mechanism(), syringe(), settings(), mN(20'000)};
        t.expect(monitor.update(elapsed, 0, mN(0)).fault == OcclusionFault::InvalidStep,
                 std::format("intervalle de {} refusé", elapsed));
    }
    {
        OcclusionMonitor monitor = backingOff(t);
        const OcclusionCommand command = monitor.update(101ms, -20, mN(0));
        t.expect(command.fault == OcclusionFault::InvalidStep && command.occlusion &&
                     command.backoffSteps == 0,
                 "défaut pendant le recul : occlusion toujours signalée, recul interrompu");
        t.expect(monitor.update(10ms, -20, mN(0)).fault == OcclusionFault::InvalidStep,
                 "défaut verrouillé");
    }
}

} // namespace

int main() {
    return Expectations{}.run([](Expectations& t) {
        parameters(t);
        thresholds(t);
        detection(t);
        backoff(t);
        counter(t);
        faults(t);
    });
}
