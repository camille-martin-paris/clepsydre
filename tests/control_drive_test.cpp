// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

// Asservissement du moteur (#26) : plages des paramètres ; rattrapage des jeux, sa fin par la
// force et sa course maximale ; conversion exacte de la consigne en pas, sans dérive ; contrôle
// du compteur de pas ; défauts verrouillés. Les attendus sont calculés à la main, avec une
// seringue de 500 mm² et une vis de 1 mm à 3 200 pas par tour : un pas chasse 156,25 nL, et
// 1 mL/h vaut 6 400 pas par heure.

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <expect.hpp>
#include <format>
#include <optional>
#include <string>

import clepsydre.control.drive;
import clepsydre.control.infusion;

namespace {

using namespace clepsydre::control;
using namespace std::chrono_literals;
using clepsydre::test::Expectations;

Rate uLh(std::uint32_t microlitresPerHour) {
    return {.microlitresPerHour = microlitresPerHour};
}

Force mN(std::int32_t millinewtons) {
    return {.millinewtons = millinewtons};
}

// 1 mm par tour, 3 200 pas par tour, 2 000 pas/s ; rattrapage à 2 000 pas/s, période de 10 ms :
// 20 pas par période de rattrapage.
DriveMechanism mechanism() {
    return {.screwLeadNanometres = 1'000'000,
            .stepsPerRevolution = 3'200,
            .maxStepRate = 2'000,
            .takeUpStepRate = 2'000,
            .period = 10ms};
}

// 500 mm² ; rattrapage terminé à 5 N, sur 100 µm au plus, soit 320 pas.
SyringeDrive syringe() {
    return {.strokeVolume = 500'000, .takeUpForce = mN(5'000), .takeUpTravel = 100};
}

// Le constructeur suit validDriveParameters : rattrapage si admis, défaut verrouillé sinon.
bool constructorAgrees(const DriveMechanism& m, const SyringeDrive& s) {
    const Drive drive{m, s, 0};
    if (validDriveParameters(m, s)) {
        return drive.phase() == DrivePhase::TakingUp && !drive.fault();
    }
    return drive.phase() == DrivePhase::Faulted && drive.fault() == DriveFault::InvalidParameters;
}

// Entraînement sorti du rattrapage au compteur 0, sans aucun pas.
Drive trackingDrive(Expectations& t) {
    Drive drive{mechanism(), syringe(), 0};
    const DriveCommand command = drive.update(10ms, uLh(0), 0, mN(5'000));
    t.expect(command.steps == 0 && !command.fault && drive.phase() == DrivePhase::Tracking,
             "force atteinte au premier relevé : perfusion");
    return drive;
}

void parameters(Expectations& t) {
    t.expect(validDriveParameters(mechanism(), syringe()) &&
                 constructorAgrees(mechanism(), syringe()),
             "paramètres de référence admis");

    const auto refused = [&](DriveMechanism m, SyringeDrive s, const std::string& what) {
        t.expect(!validDriveParameters(m, s) && constructorAgrees(m, s), what + " : refusé");
    };
    const auto admitted = [&](DriveMechanism m, SyringeDrive s, const std::string& what) {
        t.expect(validDriveParameters(m, s) && constructorAgrees(m, s), what + " : admis");
    };

    DriveMechanism m = mechanism();
    m.screwLeadNanometres = 0;
    refused(m, syringe(), "avance nulle");
    m.screwLeadNanometres = 10'000'001;
    refused(m, syringe(), "avance de plus de 10 mm");
    m.screwLeadNanometres = 10'000'000;
    admitted(m, syringe(), "avance de 10 mm");

    m = mechanism();
    m.stepsPerRevolution = 0;
    refused(m, syringe(), "aucun pas par tour");
    m.stepsPerRevolution = 65'537;
    refused(m, syringe(), "plus de 65 536 pas par tour");
    m.stepsPerRevolution = 65'536;
    m.maxStepRate = 100'000;
    admitted(m, syringe(), "65 536 pas par tour");

    // 200 mL/h avec la seringue de référence : 1 280 000 pas/h, soit 355,6 pas/s.
    m = mechanism();
    m.maxStepRate = 355;
    m.takeUpStepRate = 355;
    refused(m, syringe(), "200 mL/h au-delà de la vitesse maximale du moteur");
    m.maxStepRate = 356;
    m.takeUpStepRate = 356;
    admitted(m, syringe(), "200 mL/h sous la vitesse maximale du moteur");
    m = mechanism();
    m.maxStepRate = 0;
    refused(m, syringe(), "vitesse maximale nulle");
    m.maxStepRate = 100'001;
    refused(m, syringe(), "vitesse maximale au-delà de 100 000 pas/s");
    m.maxStepRate = 100'000;
    admitted(m, syringe(), "vitesse maximale de 100 000 pas/s");

    m = mechanism();
    m.takeUpStepRate = 0;
    refused(m, syringe(), "rattrapage immobile");
    m.takeUpStepRate = 2'001;
    refused(m, syringe(), "rattrapage plus rapide que le moteur");
    m.takeUpStepRate = 99;
    refused(m, syringe(), "moins d'un pas de rattrapage par période");
    m.takeUpStepRate = 100;
    admitted(m, syringe(), "un pas de rattrapage par période");

    m = mechanism();
    m.period = 0ms;
    refused(m, syringe(), "période nulle");
    m.period = 101ms;
    refused(m, syringe(), "période au-delà de maxStep");
    m.period = 100ms;
    admitted(m, syringe(), "période de 100 ms");
    m.period = 1ms;
    admitted(m, syringe(), "période de 1 ms, 2 pas de rattrapage");

    SyringeDrive s = syringe();
    s.strokeVolume = 0;
    refused(mechanism(), s, "section nulle");
    s.strokeVolume = 1'000'001;
    refused(mechanism(), s, "section de plus de 1 000 mm²");
    s.strokeVolume = 1'000'000;
    admitted(mechanism(), s, "section de 1 000 mm²");
    // Petite section : 200 mL/h exigent 2 000 pas/s quand la section vaut 88,89 mm².
    s.strokeVolume = 88'888;
    refused(mechanism(), s, "section trop petite pour 200 mL/h");
    s.strokeVolume = 88'889;
    admitted(mechanism(), s, "section juste assez grande pour 200 mL/h");

    s = syringe();
    s.takeUpForce = mN(0);
    refused(mechanism(), s, "force de fin de rattrapage nulle");
    s.takeUpForce = mN(-1);
    refused(mechanism(), s, "force de fin de rattrapage négative");
    s.takeUpForce = mN(1);
    admitted(mechanism(), s, "force de fin de rattrapage de 1 mN");

    s = syringe();
    s.takeUpTravel = 0;
    refused(mechanism(), s, "course de rattrapage nulle");
    s.takeUpTravel = 10'001;
    refused(mechanism(), s, "course de rattrapage de plus de 10 mm");
    s.takeUpTravel = 10'000;
    admitted(mechanism(), s, "course de rattrapage de 10 mm");
    // Course plus courte qu'un pas : 1 µm avec une vis de 10 mm à 1 000 pas par tour.
    m = mechanism();
    m.screwLeadNanometres = 10'000'000;
    m.stepsPerRevolution = 1'000;
    m.maxStepRate = 100'000;
    s = syringe();
    s.takeUpTravel = 1;
    refused(m, s, "course de rattrapage inférieure à un pas");
    s.takeUpTravel = 10;
    admitted(m, s, "course de rattrapage d'un pas");

    // Paramètres refusés : aucun pas, défaut verrouillé.
    m = mechanism();
    m.period = 0ms;
    Drive drive{m, syringe(), 42};
    const DriveCommand command = drive.update(10ms, uLh(1'000), 42, mN(0));
    t.expect(command.steps == 0 && command.fault == DriveFault::InvalidParameters &&
                 drive.phase() == DrivePhase::Faulted,
             "entraînement mal paramétré : aucun pas");
}

void takeUp(Expectations& t) {
    {
        // Rattrapage : 20 pas par période jusqu'à la force, la consigne est ignorée.
        Drive drive{mechanism(), syringe(), 1'000};
        std::int64_t count = 1'000;
        for (int period = 0; period < 5; ++period) {
            const DriveCommand command = drive.update(10ms, uLh(200'000), count, mN(4'999));
            t.expect(command.steps == 20 && !command.fault,
                     std::format("période {} de rattrapage : 20 pas", period));
            count += command.steps;
        }
        const DriveCommand contact = drive.update(10ms, uLh(200'000), count, mN(5'000));
        t.expect(contact.steps == 0 && !contact.fault && drive.phase() == DrivePhase::Tracking &&
                     drive.trackedSteps() == 0,
                 "force atteinte : fin du rattrapage, aucun pas de perfusion");
        const DriveCommand first = drive.update(10ms, uLh(1'000), count, mN(0));
        t.expect(!first.fault && drive.phase() == DrivePhase::Tracking,
                 "en perfusion, la force n'est plus lue");
    }
    {
        // Course maximale : 320 pas, puis défaut si la force n'est toujours pas atteinte.
        Drive drive{mechanism(), syringe(), 0};
        std::int64_t count = 0;
        std::uint32_t total = 0;
        DriveCommand command;
        for (int period = 0; period < 16; ++period) {
            command = drive.update(10ms, uLh(0), count, mN(0));
            count += command.steps;
            total += command.steps;
        }
        t.expect(total == 320 && !command.fault, "320 pas de rattrapage, toute la course");
        command = drive.update(10ms, uLh(0), count, mN(4'999));
        t.expect(command.steps == 0 && command.fault == DriveFault::TakeUpTravelExceeded &&
                     drive.phase() == DrivePhase::Faulted,
                 "course maximale sans la force : défaut");
    }
    {
        // Course non multiple de la période : dernière commande tronquée (105 µm, 336 pas).
        SyringeDrive s = syringe();
        s.takeUpTravel = 105;
        Drive drive{mechanism(), s, 0};
        std::int64_t count = 0;
        DriveCommand command;
        for (int period = 0; period < 17; ++period) {
            command = drive.update(10ms, uLh(0), count, mN(0));
            count += command.steps;
        }
        t.expect(command.steps == 16 && count == 336, "dernière commande tronquée à la course");
        command = drive.update(10ms, uLh(0), count, mN(5'000));
        t.expect(!command.fault && drive.phase() == DrivePhase::Tracking,
                 "force atteinte au bout exact de la course : perfusion");
    }
    {
        Drive drive{mechanism(), syringe(), 0};
        const DriveCommand command = drive.update(10ms, uLh(0), 0, std::nullopt);
        t.expect(command.steps == 0 && command.fault == DriveFault::ForceUnavailable,
                 "aucune mesure de force pendant le rattrapage : défaut");
    }
}

void tracking(Expectations& t) {
    {
        // 1 mL/h pendant une heure, par périodes de 10 ms : exactement 6 400 pas.
        Drive drive = trackingDrive(t);
        std::int64_t count = 0;
        std::uint32_t largest = 0;
        bool faulted = false;
        for (int period = 0; period < 360'000; ++period) {
            const DriveCommand command = drive.update(10ms, uLh(1'000), count, std::nullopt);
            faulted = faulted || command.fault.has_value();
            largest = std::max(largest, command.steps);
            count += command.steps;
        }
        t.expect(!faulted && count == 6'400 && drive.trackedSteps() == 6'400,
                 std::format("1 mL/h pendant 1 h : 6 400 pas (obtenu : {})", count));
        t.expect(largest == 1, "au plus un pas par période à 1 mL/h");
    }
    {
        // Intervalles irréguliers et consignes changeantes : la somme des pas est la partie
        // entière de l'intégrale, 6 400 pas par mL.
        Drive drive = trackingDrive(t);
        std::int64_t count = 0;
        std::uint64_t integral = 0; // µL/h × ms
        std::uint32_t seed = 12'345;
        bool exact = true;
        for (int period = 0; period < 200'000; ++period) {
            seed = seed * 1'103'515'245U + 12'345U;
            const auto elapsed = std::chrono::milliseconds{seed % 101};
            const Rate rate = uLh((seed >> 8) % 200'001);
            const DriveCommand command = drive.update(elapsed, rate, count, std::nullopt);
            count += command.steps;
            integral += std::uint64_t{rate.microlitresPerHour} *
                        static_cast<std::uint64_t>(elapsed.count());
            // 1 µL/h × ms = 1 / 3 600 nL ; un pas chasse 156,25 nL : pas = intégrale / 562 500.
            exact =
                exact && !command.fault && static_cast<std::uint64_t>(count) == integral / 562'500;
        }
        t.expect(exact, std::format("200 000 périodes irrégulières : {} pas, sans dérive", count));
    }
    {
        // 200 mL/h, débit maximal de la pompe : 1 280 000 pas/h, soit 3 ou 4 pas par 10 ms.
        Drive drive = trackingDrive(t);
        std::int64_t count = 0;
        for (int period = 0; period < 360'000; ++period) {
            count += drive.update(10ms, pumpMaxRate, count, std::nullopt).steps;
        }
        t.expect(count == 1'280'000, "200 mL/h pendant 1 h : 1 280 000 pas");
        const DriveCommand above = drive.update(10ms, uLh(200'001), count, std::nullopt);
        t.expect(above.steps == 0 && above.fault == DriveFault::SetpointAboveMaximum,
                 "consigne au-delà de 200 mL/h : défaut");
    }
    {
        Drive drive = trackingDrive(t);
        t.expect(drive.update(0ms, uLh(1'000), 0, std::nullopt).steps == 0 &&
                     drive.update(Drive::maxStep, uLh(1'000), 0, std::nullopt).fault ==
                         std::nullopt,
                 "intervalles nul et maximal admis");
        const DriveCommand command =
            drive.update(Drive::maxStep + 1ms, uLh(1'000), 0, std::nullopt);
        t.expect(command.steps == 0 && command.fault == DriveFault::InvalidStep,
                 "intervalle trop long : défaut");
    }
    {
        Drive drive = trackingDrive(t);
        const DriveCommand command = drive.update(-1ms, uLh(1'000), 0, std::nullopt);
        t.expect(command.fault == DriveFault::InvalidStep, "intervalle négatif : défaut");
    }
}

void counter(Expectations& t) {
    {
        // Le générateur peut retarder d'un pas, et ne pas avoir fini la dernière commande.
        Drive drive{mechanism(), syringe(), 0};
        const DriveCommand first = drive.update(10ms, uLh(0), 0, mN(0));
        const DriveCommand late = drive.update(10ms, uLh(0), 0, mN(0));
        t.expect(first.steps == 20 && late.steps == 20 && !late.fault,
                 "dernière commande pas encore émise : admis");
        const DriveCommand behind = drive.update(10ms, uLh(0), 19, mN(0));
        t.expect(!behind.fault, "un pas de retard sur les commandes antérieures : admis");
        const DriveCommand lost = drive.update(10ms, uLh(0), 38, mN(0));
        t.expect(lost.fault == DriveFault::StepsNotEmitted && lost.steps == 0,
                 "deux pas de retard sur les commandes antérieures : défaut");
    }
    {
        Drive drive{mechanism(), syringe(), 100};
        const DriveCommand command = drive.update(10ms, uLh(0), 99, mN(0));
        t.expect(command.fault == DriveFault::CounterInconsistent, "compteur qui recule : défaut");
    }
    {
        Drive drive{mechanism(), syringe(), 100};
        const DriveCommand first = drive.update(10ms, uLh(0), 100, mN(0));
        const DriveCommand command = drive.update(10ms, uLh(0), 100 + first.steps + 1, mN(0));
        t.expect(command.fault == DriveFault::CounterInconsistent,
                 "compteur au-delà des pas demandés : défaut");
    }
}

void latched(Expectations& t) {
    Drive drive{mechanism(), syringe(), 0};
    static_cast<void>(drive.update(10ms, uLh(0), 0, std::nullopt));
    bool still = true;
    for (int period = 0; period < 10; ++period) {
        const DriveCommand command = drive.update(10ms, uLh(1'000), 0, mN(5'000));
        still = still && command.steps == 0 && command.fault == DriveFault::ForceUnavailable;
    }
    t.expect(still && drive.phase() == DrivePhase::Faulted &&
                 drive.fault() == DriveFault::ForceUnavailable,
             "défaut verrouillé : aucun pas, même cause, malgré la force retrouvée");
}

void stableValues(Expectations& t) {
    t.expect(static_cast<int>(DrivePhase::TakingUp) == 1 &&
                 static_cast<int>(DrivePhase::Tracking) == 2 &&
                 static_cast<int>(DrivePhase::Faulted) == 3,
             "valeurs des phases");
    t.expect(static_cast<int>(DriveFault::InvalidParameters) == 1 &&
                 static_cast<int>(DriveFault::TakeUpTravelExceeded) == 5 &&
                 static_cast<int>(DriveFault::StepsNotEmitted) == 7,
             "valeurs des défauts");
}

} // namespace

int main() {
    return Expectations{}.run([](Expectations& t) {
        parameters(t);
        takeUp(t);
        tracking(t);
        counter(t);
        latched(t);
        stableValues(t);
    });
}
