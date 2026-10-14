// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

// Modes de perfusion et bolus borné (#25) : cohérence de la bibliothèque et de la seringue ;
// validation de la programmation et du bolus, chaque motif de refus et chaque limite exacte
// (SW-REQ-002) ; volume sur durée et arrondi signalé ; borne de consigne du mode actif
// (SW-REQ-006) ; décompte des volumes sans dérive (SW-REQ-005) ; parcours avec la machine à états.

#include <chrono>
#include <cstdint>
#include <expect.hpp>
#include <expected>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>

import clepsydre.control.infusion;
import clepsydre.control.state_machine;

namespace {

using namespace clepsydre::control;
using namespace std::chrono_literals;
using clepsydre::test::Expectations;

Volume uL(std::uint32_t microlitres) {
    return {.microlitres = microlitres};
}

Rate uLh(std::uint32_t microlitresPerHour) {
    return {.microlitresPerHour = microlitresPerHour};
}

// Seringue de 50 mL, 150 mL/h au plus.
Syringe syringe50() {
    return {.usableVolume = uL(50'000), .maxRate = uLh(150'000)};
}

// Seringue de 10 mL, 30 mL/h au plus : plus contraignante que les limites du médicament.
Syringe syringe10() {
    return {.usableVolume = uL(10'000), .maxRate = uLh(30'000)};
}

// Médicament de référence : 0,5 à 50 mL/h (souple 1 à 20), bolus de 5 mL au plus (souple 2 mL)
// à 100 mL/h au plus, maintien de veine ouverte à 3 mL/h.
DrugLimits drug() {
    return {.hardMinRate = uLh(500),
            .softMinRate = uLh(1'000),
            .softMaxRate = uLh(20'000),
            .hardMaxRate = uLh(50'000),
            .softMaxBolusVolume = uL(2'000),
            .hardMaxBolusVolume = uL(5'000),
            .hardMaxBolusRate = uLh(100'000),
            .endOfInfusion = EndOfInfusion::Kvo,
            .kvoRate = uLh(3'000)};
}

DrugLimits stopAtEnd() {
    DrugLimits limits = drug();
    limits.endOfInfusion = EndOfInfusion::Stop;
    limits.kvoRate = uLh(0);
    return limits;
}

ProgramRequest continuous(std::uint32_t rate, std::uint32_t volume) {
    return {.mode = Mode::ContinuousRate,
            .rate = uLh(rate),
            .volume = uL(volume),
            .duration = std::chrono::minutes{0}};
}

ProgramRequest overDuration(std::uint32_t volume, std::chrono::minutes duration) {
    // Le débit saisi est ignoré en volume sur durée : une valeur quelconque le vérifie.
    return {.mode = Mode::VolumeOverDuration,
            .rate = uLh(12'345),
            .volume = uL(volume),
            .duration = duration};
}

// Les tests convertissent volontairement des valeurs hors des énumérations.
Mode modeAt(std::uint8_t value) {
    return static_cast<Mode>(value); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
}

EndOfInfusion endAt(std::uint8_t value) {
    return static_cast<EndOfInfusion>( // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
        value);
}

ActuatorMode actuatorAt(std::uint8_t value) {
    return static_cast<ActuatorMode>( // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
        value);
}

template <typename T>
bool refused(const std::expected<T, Refusal>& result, Refusal reason) {
    return !result.has_value() && result.error() == reason;
}

// Programmation acceptée ; échec du test et programmation vide sinon.
Program accepted(Expectations& t, const std::expected<Program, Refusal>& result,
                 const std::string& what) {
    t.expect(result.has_value(), what + " : acceptée");
    return result.value_or(Program{});
}

void limitsConsistency(Expectations& t) {
    t.expect(validLimits(drug(), syringe50()), "bibliothèque de référence cohérente");
    t.expect(validLimits(stopAtEnd(), syringe50()), "arrêt en fin, sans débit de maintien");

    struct Case {
        std::string what;
        DrugLimits limits;
        Syringe syringe;
    };

    std::vector<Case> invalid;
    const auto mutate = [&](const std::string& what, auto change) {
        DrugLimits limits = drug();
        change(limits);
        invalid.push_back({.what = what, .limits = limits, .syringe = syringe50()});
    };
    mutate("limite dure basse nulle", [](DrugLimits& l) { l.hardMinRate = uLh(0); });
    mutate("limite dure basse hors résolution", [](DrugLimits& l) { l.hardMinRate = uLh(550); });
    mutate("limite dure haute au-delà de 200 mL/h",
           [](DrugLimits& l) { l.hardMaxRate = uLh(200'100); });
    mutate("limite souple basse hors résolution",
           [](DrugLimits& l) { l.softMinRate = uLh(1'050); });
    mutate("limite souple haute hors résolution",
           [](DrugLimits& l) { l.softMaxRate = uLh(20'050); });
    mutate("limite souple basse sous la limite dure",
           [](DrugLimits& l) { l.softMinRate = uLh(400); });
    mutate("limites souples inversées", [](DrugLimits& l) { l.softMinRate = uLh(30'000); });
    mutate("limite souple haute au-delà de la limite dure",
           [](DrugLimits& l) { l.softMaxRate = uLh(60'000); });
    mutate("fin de perfusion invalide", [](DrugLimits& l) { l.endOfInfusion = endAt(0); });
    mutate("maintien de veine nul", [](DrugLimits& l) { l.kvoRate = uLh(0); });
    mutate("maintien de veine au-delà de 5 mL/h", [](DrugLimits& l) { l.kvoRate = uLh(5'100); });
    mutate("volume de bolus maximal hors résolution",
           [](DrugLimits& l) { l.hardMaxBolusVolume = uL(5'050); });
    mutate("limite souple de bolus hors résolution",
           [](DrugLimits& l) { l.softMaxBolusVolume = uL(2'050); });
    mutate("limite souple de bolus nulle", [](DrugLimits& l) { l.softMaxBolusVolume = uL(0); });
    mutate("limite souple de bolus au-delà de la limite dure",
           [](DrugLimits& l) { l.softMaxBolusVolume = uL(6'000); });
    mutate("débit de bolus maximal nul", [](DrugLimits& l) { l.hardMaxBolusRate = uLh(0); });
    invalid.push_back({.what = "seringue sans volume utile",
                       .limits = drug(),
                       .syringe = {.usableVolume = uL(0), .maxRate = uLh(150'000)}});
    invalid.push_back({.what = "seringue sans débit",
                       .limits = drug(),
                       .syringe = {.usableVolume = uL(50'000), .maxRate = uLh(0)}});
    for (const Case& c : invalid) {
        t.expect(!validLimits(c.limits, c.syringe), "incohérent : " + c.what);
        t.expect(refused(validateProgram(continuous(10'000, 20'000), c.limits, c.syringe),
                         Refusal::InvalidLimits),
                 "programmation refusée : " + c.what);
    }

    DrugLimits noBolus = drug();
    noBolus.hardMaxBolusVolume = uL(0);
    noBolus.softMaxBolusVolume = uL(0);
    noBolus.hardMaxBolusRate = uLh(0);
    t.expect(validLimits(noBolus, syringe50()),
             "bolus interdit : autres limites de bolus ignorées");

    DrugLimits exact = drug();
    exact.softMinRate = exact.hardMinRate;
    exact.softMaxRate = exact.hardMaxRate;
    exact.softMaxBolusVolume = exact.hardMaxBolusVolume;
    exact.kvoRate = maxKvoRate;
    t.expect(validLimits(exact, syringe50()), "limites souples confondues avec les dures, 5 mL/h");
}

void continuousRate(Expectations& t) {
    const Program p = accepted(t, validateProgram(continuous(10'000, 20'000), drug(), syringe50()),
                               "10 mL/h, 20 mL");
    t.expect(p.mode == Mode::ContinuousRate && p.rate == uLh(10'000) && p.volume == uL(20'000),
             "débit et volume conservés tels quels");
    t.expect(p.duration == 2h && !p.rateRoundedDown && p.rateSoftLimit == SoftLimit::Within,
             "durée de 2 h, sans arrondi, dans les limites souples");
    t.expect(p.endOfInfusion == EndOfInfusion::Kvo && p.kvoRate == uLh(3'000),
             "maintien de veine ouverte de la bibliothèque");

    const Program slow =
        accepted(t, validateProgram(continuous(700, 100), drug(), syringe50()), "0,7 mL/h");
    t.expect(slow.duration == 515s, "durée arrondie par excès : 0,1 mL à 0,7 mL/h, 515 s");
    t.expect(slow.rateSoftLimit == SoftLimit::BelowMinimum,
             "sous la limite souple : confirmation exigée");
    t.expect(slow.kvoRate == uLh(700), "maintien de veine plafonné par le débit (SYS-REQ-032)");

    const Program fast =
        accepted(t, validateProgram(continuous(30'000, 1'000), drug(), syringe50()), "30 mL/h");
    t.expect(fast.rateSoftLimit == SoftLimit::AboveMaximum,
             "au-delà de la limite souple : confirmation exigée");

    const Program stop =
        accepted(t, validateProgram(continuous(10'000, 1'000), stopAtEnd(), syringe50()), "arrêt");
    t.expect(stop.endOfInfusion == EndOfInfusion::Stop && stop.kvoRate == uLh(0),
             "arrêt en fin : pas de maintien de veine");

    // Limites exactes : acceptées.
    for (const auto& [rate, volume] : {std::pair{500U, 100U}, std::pair{50'000U, 50'000U}}) {
        static_cast<void>(accepted(t,
                                   validateProgram(continuous(rate, volume), drug(), syringe50()),
                                   std::format("{} µL/h, {} µL : limites exactes", rate, volume)));
    }

    struct Case {
        std::string what;
        ProgramRequest request;
        Syringe syringe;
        Refusal reason;
    };

    const std::vector<Case> cases{
        {.what = "volume hors résolution",
         .request = continuous(10'000, 20'050),
         .syringe = syringe50(),
         .reason = Refusal::VolumeResolution},
        {.what = "volume nul",
         .request = continuous(10'000, 0),
         .syringe = syringe50(),
         .reason = Refusal::VolumeBelowMinimum},
        {.what = "volume au-delà de la seringue",
         .request = continuous(10'000, 50'100),
         .syringe = syringe50(),
         .reason = Refusal::VolumeAboveSyringe},
        {.what = "débit hors résolution",
         .request = continuous(10'050, 20'000),
         .syringe = syringe50(),
         .reason = Refusal::RateResolution},
        {.what = "débit nul",
         .request = continuous(0, 20'000),
         .syringe = syringe50(),
         .reason = Refusal::RateBelowPumpMinimum},
        {.what = "débit au-delà de 200 mL/h",
         .request = continuous(200'100, 20'000),
         .syringe = syringe50(),
         .reason = Refusal::RateAbovePumpMaximum},
        {.what = "débit au-delà de la seringue",
         .request = continuous(30'100, 5'000),
         .syringe = syringe10(),
         .reason = Refusal::RateAboveSyringeMaximum},
        {.what = "débit sous la limite dure",
         .request = continuous(400, 20'000),
         .syringe = syringe50(),
         .reason = Refusal::RateBelowHardLimit},
        {.what = "débit au-delà de la limite dure",
         .request = continuous(50'100, 20'000),
         .syringe = syringe50(),
         .reason = Refusal::RateAboveHardLimit},
    };
    for (const Case& c : cases) {
        t.expect(refused(validateProgram(c.request, drug(), c.syringe), c.reason),
                 "refus explicite : " + c.what);
    }

    for (const std::uint8_t value : {std::uint8_t{0}, std::uint8_t{3}, std::uint8_t{255}}) {
        ProgramRequest request = continuous(10'000, 20'000);
        request.mode = modeAt(value);
        t.expect(refused(validateProgram(request, drug(), syringe50()), Refusal::InvalidMode),
                 std::format("mode invalide {} refusé", value));
    }
}

void volumeOverDuration(Expectations& t) {
    const Program exact = accepted(
        t, validateProgram(overDuration(20'000, 120min), drug(), syringe50()), "20 mL en 2 h");
    t.expect(exact.mode == Mode::VolumeOverDuration && exact.rate == uLh(10'000) &&
                 !exact.rateRoundedDown && exact.duration == 2h,
             "débit exact de 10 mL/h, sans arrondi");

    const Program rounded = accepted(
        t, validateProgram(overDuration(10'000, 180min), drug(), syringe50()), "10 mL en 3 h");
    t.expect(rounded.rate == uLh(3'300) && rounded.rateRoundedDown,
             "3,33 mL/h arrondi par défaut à 3,3 mL/h, arrondi signalé");
    t.expect(rounded.duration == 10'910s, "durée au débit retenu : 3 h 1 min 50 s");
    t.expect(rounded.kvoRate == uLh(3'000), "maintien de veine sous le débit calculé");

    struct Case {
        std::string what;
        ProgramRequest request;
        Syringe syringe;
        Refusal reason;
    };

    const std::vector<Case> cases{
        {.what = "volume hors résolution",
         .request = overDuration(20'050, 120min),
         .syringe = syringe50(),
         .reason = Refusal::VolumeResolution},
        {.what = "durée nulle",
         .request = overDuration(20'000, 0min),
         .syringe = syringe50(),
         .reason = Refusal::DurationMissing},
        {.what = "durée négative",
         .request = overDuration(20'000, -60min),
         .syringe = syringe50(),
         .reason = Refusal::DurationMissing},
        {.what = "débit calculé au-delà de 200 mL/h",
         .request = overDuration(50'000, 1min),
         .syringe = syringe50(),
         .reason = Refusal::RateAbovePumpMaximum},
        {.what = "débit calculé arrondi à zéro",
         .request = overDuration(100, 6'000min),
         .syringe = syringe50(),
         .reason = Refusal::RateBelowPumpMinimum},
        {.what = "débit calculé au-delà de la seringue",
         .request = overDuration(10'000, 15min),
         .syringe = syringe10(),
         .reason = Refusal::RateAboveSyringeMaximum},
        {.what = "débit calculé sous la limite dure",
         .request = overDuration(1'000, 300min),
         .syringe = syringe50(),
         .reason = Refusal::RateBelowHardLimit},
        {.what = "débit calculé au-delà de la limite dure",
         .request = overDuration(30'000, 30min),
         .syringe = syringe50(),
         .reason = Refusal::RateAboveHardLimit},
    };
    for (const Case& c : cases) {
        t.expect(refused(validateProgram(c.request, drug(), c.syringe), c.reason),
                 "volume sur durée, refus explicite : " + c.what);
    }
}

void bolus(Expectations& t) {
    const Program program = accepted(
        t, validateProgram(continuous(10'000, 20'000), drug(), syringe50()), "10 mL/h, 20 mL");
    const auto request = [](std::uint32_t volume, std::uint32_t rate) {
        return BolusRequest{.volume = uL(volume), .rate = uLh(rate)};
    };

    const auto ok = validateBolus(request(1'000, 60'000), program, drug(), syringe50());
    t.expect(ok.has_value() && ok->volume == uL(1'000) && ok->rate == uLh(60'000) &&
                 !ok->volumeAboveSoftLimit,
             "bolus de 1 mL à 60 mL/h accepté");
    const auto atSoft = validateBolus(request(2'000, 100'000), program, drug(), syringe50());
    t.expect(atSoft.has_value() && !atSoft->volumeAboveSoftLimit,
             "bolus à la limite souple et au débit maximal");
    const auto aboveSoft = validateBolus(request(5'000, 60'000), program, drug(), syringe50());
    t.expect(aboveSoft.has_value() && aboveSoft->volumeAboveSoftLimit,
             "bolus à la limite dure : au-delà de la limite souple, confirmation exigée");

    const Program timed = accepted(
        t, validateProgram(overDuration(20'000, 120min), drug(), syringe50()), "20 mL en 2 h");
    DrugLimits noBolus = drug();
    noBolus.hardMaxBolusVolume = uL(0);
    DrugLimits largeBolus = drug();
    largeBolus.hardMaxBolusVolume = uL(20'000);
    largeBolus.softMaxBolusVolume = uL(20'000);
    DrugLimits incoherent = drug();
    incoherent.kvoRate = uLh(9'000);

    struct Case {
        std::string what;
        BolusRequest request;
        Program program;
        DrugLimits limits;
        Syringe syringe;
        Refusal reason;
    };

    const std::vector<Case> cases{
        {.what = "bibliothèque incohérente",
         .request = request(1'000, 60'000),
         .program = program,
         .limits = incoherent,
         .syringe = syringe50(),
         .reason = Refusal::InvalidLimits},
        {.what = "volume sur durée (SYS-REQ-029)",
         .request = request(1'000, 60'000),
         .program = timed,
         .limits = drug(),
         .syringe = syringe50(),
         .reason = Refusal::BolusNotAllowed},
        {.what = "bolus interdit pour le médicament",
         .request = request(1'000, 60'000),
         .program = program,
         .limits = noBolus,
         .syringe = syringe50(),
         .reason = Refusal::BolusNotAllowed},
        {.what = "volume hors résolution",
         .request = request(1'050, 60'000),
         .program = program,
         .limits = drug(),
         .syringe = syringe50(),
         .reason = Refusal::VolumeResolution},
        {.what = "volume nul",
         .request = request(0, 60'000),
         .program = program,
         .limits = drug(),
         .syringe = syringe50(),
         .reason = Refusal::VolumeBelowMinimum},
        {.what = "volume au-delà de la seringue",
         .request = request(15'000, 20'000),
         .program = program,
         .limits = largeBolus,
         .syringe = syringe10(),
         .reason = Refusal::VolumeAboveSyringe},
        {.what = "volume au-delà de la limite dure",
         .request = request(5'100, 60'000),
         .program = program,
         .limits = drug(),
         .syringe = syringe50(),
         .reason = Refusal::BolusVolumeAboveHardLimit},
        {.what = "débit hors résolution",
         .request = request(1'000, 60'050),
         .program = program,
         .limits = drug(),
         .syringe = syringe50(),
         .reason = Refusal::RateResolution},
        {.what = "débit nul",
         .request = request(1'000, 0),
         .program = program,
         .limits = drug(),
         .syringe = syringe50(),
         .reason = Refusal::RateBelowPumpMinimum},
        {.what = "débit au-delà de 200 mL/h",
         .request = request(1'000, 250'000),
         .program = program,
         .limits = drug(),
         .syringe = syringe50(),
         .reason = Refusal::RateAbovePumpMaximum},
        {.what = "débit au-delà de la seringue",
         .request = request(1'000, 40'000),
         .program = program,
         .limits = drug(),
         .syringe = syringe10(),
         .reason = Refusal::RateAboveSyringeMaximum},
        {.what = "débit au-delà de la limite dure",
         .request = request(1'000, 120'000),
         .program = program,
         .limits = drug(),
         .syringe = syringe50(),
         .reason = Refusal::BolusRateAboveHardLimit},
    };
    for (const Case& c : cases) {
        t.expect(refused(validateBolus(c.request, c.program, c.limits, c.syringe), c.reason),
                 "bolus refusé : " + c.what);
    }
}

void bounds(Expectations& t) {
    const Program program = accepted(
        t, validateProgram(continuous(10'000, 20'000), drug(), syringe50()), "10 mL/h, 20 mL");
    t.expect(setpointBound(ActuatorMode::Infusion, program, drug()) == uLh(10'000),
             "perfusion : débit programmé");
    t.expect(setpointBound(ActuatorMode::Bolus, program, drug()) == uLh(100'000),
             "bolus : débit maximal du médicament");
    t.expect(setpointBound(ActuatorMode::Kvo, program, drug()) == uLh(3'000),
             "maintien de veine ouverte : son débit");
    for (const ActuatorMode mode : {ActuatorMode::None, ActuatorMode::Purge, actuatorAt(200)}) {
        t.expect(
            setpointBound(mode, program, drug()) == uLh(0),
            std::format("mode {} : toute consigne non nulle hors bornes", static_cast<int>(mode)));
    }
    // Remarque de relecture de #129 : chaque borne est plafonnée par une limite indépendante de la
    // consigne.
    Program corrupted = program;
    corrupted.rate = uLh(300'000);
    corrupted.kvoRate = uLh(9'000);
    t.expect(setpointBound(ActuatorMode::Infusion, corrupted, drug()) == uLh(50'000),
             "perfusion : borne plafonnée par la limite dure du médicament");
    t.expect(setpointBound(ActuatorMode::Kvo, corrupted, drug()) == uLh(3'000),
             "maintien de veine : borne plafonnée par la bibliothèque, pas par sa consigne");
    DrugLimits incoherent = drug();
    incoherent.hardMaxRate = uLh(300'000);
    incoherent.kvoRate = uLh(9'000);
    t.expect(setpointBound(ActuatorMode::Infusion, corrupted, incoherent) == pumpMaxRate,
             "borne plafonnée par le débit maximal de la pompe");
    t.expect(setpointBound(ActuatorMode::Kvo, corrupted, incoherent) == maxKvoRate,
             "maintien de veine : borne plafonnée à 5 mL/h");
    Program slow = program;
    slow.rate = uLh(700);
    t.expect(setpointBound(ActuatorMode::Kvo, slow, drug()) == uLh(700),
             "maintien de veine : borne plafonnée par le débit programmé");
    t.expect(setpointBound(ActuatorMode::Kvo, program, stopAtEnd()) == uLh(0),
             "arrêt en fin de perfusion : aucune consigne de maintien de veine");
}

// Programmation construite sans validation, pour les tests du décompte.
Program program(std::uint32_t rate, std::uint32_t volume, EndOfInfusion end) {
    return {.mode = Mode::ContinuousRate,
            .rate = uLh(rate),
            .volume = uL(volume),
            .duration = std::chrono::seconds{0},
            .rateRoundedDown = false,
            .rateSoftLimit = SoftLimit::Within,
            .endOfInfusion = end,
            .kvoRate = end == EndOfInfusion::Kvo ? uLh(3'000) : uLh(0)};
}

Bolus bolusOf(std::uint32_t volume, std::uint32_t rate) {
    return {.volume = uL(volume), .rate = uLh(rate), .volumeAboveSoftLimit = false};
}

void delivery(Expectations& t) {
    {
        // 1 mL à 10 mL/h : 360 s exactement.
        Delivery d{program(10'000, 1'000, EndOfInfusion::Kvo), drug()};
        t.expect(d.setpoint(ActuatorMode::Infusion) == uLh(10'000), "consigne de perfusion");
        std::optional<Event> event;
        for (int i = 0; i < 359 && !event; ++i) {
            event = d.advance(ActuatorMode::Infusion, 1s);
        }
        t.expect(!event && d.infused() == uL(997) && d.remaining() == uL(3),
                 "359 s : 997,2 µL arrondis par défaut, 3 µL restants");
        t.expect(d.advance(ActuatorMode::Infusion, 1s) == Event::VolumeReachedKvo &&
                     d.infused() == uL(1'000) && d.remaining() == uL(0),
                 "360 s : volume atteint, maintien de veine ouverte prévu");
        t.expect(d.setpoint(ActuatorMode::Infusion) == uLh(0), "volume atteint : consigne nulle");
        t.expect(d.advance(ActuatorMode::Infusion, 60s) == Event::VolumeReachedKvo &&
                     d.infused() == uL(1'000),
                 "aucun volume au-delà du volume à perfuser");
        t.expect(!d.advance(ActuatorMode::Kvo, 60s) && d.kvoDelivered() == uL(50) &&
                     d.infused() == uL(1'000),
                 "maintien de veine : 50 µL en 1 min à 3 mL/h, décomptés à part");
    }
    {
        Delivery d{program(10'000, 100, EndOfInfusion::Stop), drug()};
        t.expect(d.advance(ActuatorMode::Infusion, 60s) == Event::VolumeReached &&
                     d.infused() == uL(100),
                 "pas de 167 µL tronqué au volume restant de 100 µL, arrêt prévu");
    }
    {
        // SW-REQ-005 : des milliers de pas irréguliers donnent l'intégrale exacte.
        Delivery d{program(700, 50'000, EndOfInfusion::Stop), drug()};
        std::uint64_t elapsed = 0;
        for (int i = 0; i < 200'000; ++i) {
            const auto step = std::chrono::milliseconds{7 + (i % 13)};
            static_cast<void>(d.advance(ActuatorMode::Infusion, step));
            elapsed += static_cast<std::uint64_t>(step.count());
        }
        const auto exact = static_cast<std::uint32_t>(700 * elapsed / 3'600'000);
        t.expect(d.infused() == uL(exact),
                 std::format("sans dérive : {} µL après {} ms à 0,7 mL/h", exact, elapsed));
    }
    {
        Delivery d{program(10'000, 20'000, EndOfInfusion::Kvo), drug()};
        t.expect(d.setpoint(ActuatorMode::Bolus) == uLh(0), "pas de consigne de bolus sans bolus");
        t.expect(d.advance(ActuatorMode::Bolus, 1s) == Event::DangerDetected,
                 "mode bolus sans bolus validé : état sûr");
        d.startBolus(bolusOf(1'000, 60'000));
        t.expect(d.setpoint(ActuatorMode::Bolus) == uLh(60'000), "consigne de bolus");
        t.expect(!d.advance(ActuatorMode::Bolus, 59s), "bolus en cours");
        t.expect(d.advance(ActuatorMode::Bolus, 1s) == Event::BolusEnded &&
                     d.bolusDelivered() == uL(1'000) && d.infused() == uL(1'000),
                 "1 mL à 60 mL/h : fin du bolus en 60 s, compté dans le volume perfusé");
        t.expect(d.setpoint(ActuatorMode::Bolus) == uLh(0), "bolus délivré : consigne nulle");
        t.expect(d.advance(ActuatorMode::Bolus, 1s) == Event::BolusEnded &&
                     d.infused() == uL(1'000),
                 "aucun volume au-delà du bolus");
        t.expect(!d.advance(ActuatorMode::Infusion, 0s), "retour au débit programmé");
        t.expect(d.advance(ActuatorMode::Bolus, 1s) == Event::DangerDetected,
                 "bolus terminé non repris");
    }
    {
        Delivery d{program(10'000, 20'000, EndOfInfusion::Kvo), drug()};
        d.startBolus(bolusOf(1'000, 60'000));
        static_cast<void>(d.advance(ActuatorMode::Bolus, 30s));
        t.expect(!d.advance(ActuatorMode::None, 60s) && d.infused() == uL(500),
                 "pause pendant le bolus : rien n'est délivré");
        t.expect(d.advance(ActuatorMode::Bolus, 1s) == Event::DangerDetected,
                 "bolus interrompu par une pause non repris");
        d.startBolus(bolusOf(1'000, 60'000));
        t.expect(d.bolusDelivered() == uL(0), "nouveau bolus : décompte remis à zéro");
        static_cast<void>(d.advance(ActuatorMode::Bolus, 1s));
        t.expect(!d.advance(ActuatorMode::Kvo, 1s) &&
                     d.advance(ActuatorMode::Bolus, 1s) == Event::DangerDetected,
                 "bolus abandonné au maintien de veine ouverte");
        d.startBolus(bolusOf(1'000, 60'000));
        t.expect(!d.advance(ActuatorMode::Purge, 1s) &&
                     d.advance(ActuatorMode::Bolus, 1s) == Event::DangerDetected,
                 "purge : rien n'est délivré ni décompté, bolus abandonné");
    }
    {
        // Volume à perfuser atteint pendant le bolus, avant ou en même temps que sa fin.
        Delivery before{program(10'000, 500, EndOfInfusion::Stop), drug()};
        before.startBolus(bolusOf(1'000, 60'000));
        t.expect(before.advance(ActuatorMode::Bolus, 60s) == Event::VolumeReached &&
                     before.bolusDelivered() == uL(500) && before.infused() == uL(500),
                 "volume atteint avant la fin du bolus : bolus tronqué");
        t.expect(before.setpoint(ActuatorMode::Bolus) == uLh(0),
                 "volume atteint : plus de consigne de bolus");
        Delivery together{program(10'000, 1'000, EndOfInfusion::Kvo), drug()};
        together.startBolus(bolusOf(1'000, 60'000));
        t.expect(together.advance(ActuatorMode::Bolus, 60s) == Event::VolumeReachedKvo,
                 "fins simultanées : la fin du volume l'emporte");
    }
    {
        Delivery d{program(10'000, 20'000, EndOfInfusion::Kvo), drug()};
        t.expect(!d.advance(ActuatorMode::Infusion, 0ms) &&
                     !d.advance(ActuatorMode::Infusion, Delivery::maxStep),
                 "intervalles nul et maximal admis");
        const Volume before = d.infused();
        t.expect(d.advance(ActuatorMode::Infusion, -1ms) == Event::DangerDetected &&
                     d.advance(ActuatorMode::Infusion, Delivery::maxStep + 1ms) ==
                         Event::DangerDetected &&
                     d.infused() == before,
                 "intervalle négatif ou trop long : état sûr, rien n'est délivré");
        t.expect(d.setpoint(ActuatorMode::Kvo) == uLh(3'000) &&
                     d.setpoint(ActuatorMode::None) == uLh(0),
                 "consignes du maintien de veine et hors perfusion");
        t.expect(d.advance(actuatorAt(200), 1s) == Event::DangerDetected &&
                     d.setpoint(actuatorAt(200)) == uLh(0),
                 "mode invalide : état sûr, consigne nulle");
    }
    {
        // SW-REQ-006 : une consigne hors bornes n'est jamais appliquée.
        Delivery corrupted{program(300'000, 20'000, EndOfInfusion::Kvo), drug()};
        t.expect(corrupted.advance(ActuatorMode::Infusion, 1s) == Event::DangerDetected &&
                     corrupted.infused() == uL(0),
                 "débit au-delà de 200 mL/h : état sûr");
        Delivery d{program(10'000, 20'000, EndOfInfusion::Kvo), drug()};
        d.startBolus(bolusOf(1'000, 150'000));
        t.expect(d.advance(ActuatorMode::Bolus, 1s) == Event::DangerDetected &&
                     d.infused() == uL(0),
                 "bolus non validé au-delà du débit du médicament : état sûr");
        Delivery aboveHard{program(60'000, 20'000, EndOfInfusion::Kvo), drug()};
        t.expect(aboveHard.advance(ActuatorMode::Infusion, 1s) == Event::DangerDetected &&
                     aboveHard.infused() == uL(0),
                 "débit altéré au-delà de la limite dure : état sûr");
    }
    {
        // Remarque de relecture de #129 : 0,1 mL à 1 mL/h, maintien de veine retenu à 1 mL/h puis
        // altéré à 6 mL/h avant la construction du décompte.
        Program p = accepted(t, validateProgram(continuous(1'000, 100), drug(), syringe50()),
                             "0,1 mL à 1 mL/h");
        t.expect(p.kvoRate == uLh(1'000), "maintien de veine retenu : 1 mL/h");
        p.kvoRate = uLh(6'000);
        Delivery d{p, drug()};
        std::optional<Event> event;
        for (int i = 0; i < 6; ++i) {
            event = d.advance(ActuatorMode::Infusion, 60s);
        }
        t.expect(event == Event::VolumeReachedKvo, "volume atteint en 6 min");
        t.expect(d.advance(ActuatorMode::Kvo, 60s) == Event::DangerDetected &&
                     d.kvoDelivered() == uL(0),
                 "maintien de veine altéré au-delà du débit programmé : état sûr, rien décompté");

        const auto kvoAfterEnd = [&](Program altered, const DrugLimits& limits,
                                     const std::string& what) {
            Delivery delivery{altered, limits};
            t.expect(delivery.advance(ActuatorMode::Kvo, 60s) == Event::DangerDetected &&
                         delivery.kvoDelivered() == uL(0),
                     what);
        };
        DrugLimits incoherent = drug();
        incoherent.kvoRate = uLh(9'000);
        Program aboveFive = program(10'000, 20'000, EndOfInfusion::Kvo);
        aboveFive.kvoRate = uLh(6'000);
        kvoAfterEnd(aboveFive, incoherent, "maintien de veine altéré au-delà de 5 mL/h : état sûr");
        Program aboveLibrary = program(10'000, 20'000, EndOfInfusion::Kvo);
        aboveLibrary.kvoRate = uLh(4'000);
        kvoAfterEnd(aboveLibrary, drug(),
                    "maintien de veine altéré au-delà de la bibliothèque : état sûr");
        const Program afterStop = program(10'000, 20'000, EndOfInfusion::Kvo);
        kvoAfterEnd(afterStop, stopAtEnd(),
                    "maintien de veine alors que la bibliothèque prévoit l'arrêt : état sûr");
    }
}

// Parcours complet : la machine donne le mode, le décompte lui renvoie les fins.
void withStateMachine(Expectations& t) {
    const Program p = accepted(t, validateProgram(continuous(10'000, 2'000), drug(), syringe50()),
                               "10 mL/h, 2 mL");
    Delivery d{p, drug()};
    StateMachine machine;
    for (const Event event : {Event::SelfTestsPassed, Event::ProgrammingStarted,
                              Event::ProgrammingConfirmed, Event::StartRequested}) {
        static_cast<void>(machine.apply(event));
    }
    const auto run = [&](std::chrono::seconds duration) {
        for (auto elapsed = 0s; elapsed < duration; elapsed += 1s) {
            if (const auto event = d.advance(actuatorMode(machine.state()), 1s)) {
                t.expect(machine.apply(*event).verdict == Verdict::Accepted,
                         std::format("événement {} accepté", static_cast<int>(*event)));
            }
        }
    };
    run(360s);
    t.expect(machine.state() == State::Infusing && d.infused() == uL(1'000), "1 mL en 6 min");
    const auto b = validateBolus({.volume = uL(500), .rate = uLh(60'000)}, p, drug(), syringe50());
    t.expect(b.has_value() && machine.apply(Event::BolusRequested).verdict == Verdict::Accepted,
             "bolus de 0,5 mL accepté");
    d.startBolus(b.value_or(Bolus{}));
    run(30s);
    t.expect(machine.state() == State::Infusing && d.bolusDelivered() == uL(500) &&
                 d.infused() == uL(1'500),
             "bolus délivré en 30 s, retour au débit programmé");
    run(180s);
    t.expect(machine.state() == State::Kvo && d.infused() == uL(2'000),
             "volume atteint : maintien de veine ouverte");
    run(60s);
    t.expect(machine.state() == State::Kvo && d.kvoDelivered() == uL(50) &&
                 d.infused() == uL(2'000),
             "maintien de veine à 3 mL/h");
}

void stableValues(Expectations& t) {
    t.expect(static_cast<int>(Refusal::InvalidLimits) == 1 &&
                 static_cast<int>(Refusal::DurationMissing) == 6 &&
                 static_cast<int>(Refusal::BolusRateAboveHardLimit) == 15,
             "valeurs des motifs de refus");
    t.expect(static_cast<int>(Mode::ContinuousRate) == 1 &&
                 static_cast<int>(Mode::VolumeOverDuration) == 2 &&
                 static_cast<int>(EndOfInfusion::Stop) == 1 &&
                 static_cast<int>(EndOfInfusion::Kvo) == 2,
             "valeurs des modes et des fins de perfusion");
}

} // namespace

int main() {
    return Expectations{}.run([](Expectations& t) {
        limitsConsistency(t);
        continuousRate(t);
        volumeOverDuration(t);
        bolus(t);
        bounds(t);
        delivery(t);
        withStateMachine(t);
        stableValues(t);
    });
}
