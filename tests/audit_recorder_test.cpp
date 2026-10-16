// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

// Enregistreur des événements d'audit (#136) : identifiants du vocabulaire composés exactement
// (flux, corrélation, cible), temps monotone en tête du détail, refus explicites et comptés
// (action, phase, producteur ou cible non admis, détail trop long, anneau plein), jamais de
// troncature.

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expect.hpp>
#include <optional>
#include <span>
#include <string>
#include <string_view>

import clepsydre.audit.recorder;

namespace {

using namespace std::chrono_literals;
using clepsydre::test::Expectations;
using namespace clepsydre::audit;

constexpr Origin commandOrigin{.serial = "SIM0001", .producer = Producer::Command, .boot = 12};

// Valeurs hors des énumérations, pour les refus.
Producer invalidProducer() {
    return static_cast<Producer>(7); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
}

Target invalidTarget() {
    return static_cast<Target>(42); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
}

Action invalidAction() {
    return static_cast<Action>(200); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
}

AuditPhase invalidPhase(std::uint8_t value) {
    return static_cast<AuditPhase>(value); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
}

AlarmCondition invalidCondition() {
    return static_cast<AlarmCondition>(99); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
}

// Faits d'un événement ; les autres champs gardent leur valeur par défaut.
Entry entry(AuditPhase phase, std::string_view actor, Target target, std::string_view correlation,
            const Detail* detail = nullptr) {
    Entry out;
    out.phase = phase;
    out.actor = actor;
    out.target = target;
    out.correlation = correlation;
    out.detail = detail;
    return out;
}

// Dernier événement admis dans l'anneau, sans le libérer.
mddlog::core::AuditEvent last(Recorder& recorder) {
    const auto view = recorder.ring().drain();
    const auto part = view.second().empty() ? view.first() : view.second();
    return part.back();
}

void text(Expectations& t) {
    Text bounded{8};
    bounded.append("abc");
    bounded.append(std::uint64_t{42});
    t.expect(bounded.view() == "abc42" && !bounded.overflowed(), "texte et nombre ajoutés");
    bounded.append("defg");
    t.expect(bounded.overflowed() && bounded.view() == "abc42",
             "dépassement refusé, sans troncature");
    bounded.append("x");
    t.expect(bounded.view() == "abc42", "plus rien après un dépassement");
    Text zero{4};
    zero.append(std::uint64_t{0});
    t.expect(zero.view() == "0", "zéro");
    Text wide{1000};
    wide.append(std::uint64_t{18'446'744'073'709'551'615U});
    t.expect(wide.view() == "18446744073709551615", "plus grand entier de 64 bits");
    Text capped{1000};
    for (int i = 0; i < 20; ++i) {
        capped.append("0123456789");
    }
    t.expect(capped.overflowed() && capped.view().size() == Text::capacity,
             "limite plafonnée à la capacité");
}

void detail(Expectations& t) {
    Detail d;
    d.add("state", std::uint64_t{4}).add("cause", "volume");
    t.expect(d.view() == "state=4 cause=volume" && !d.overflowed(),
             "paires séparées par une espace");
    Detail longer;
    longer.add("k", std::string(200, 'x'));
    t.expect(longer.overflowed(), "détail trop long refusé");
}

void serials(Expectations& t) {
    t.expect(validSerial("SIM-0001az"), "lettres, chiffres et trait d'union");
    t.expect(!validSerial(""), "vide");
    t.expect(!validSerial(std::string(33, 'A')), "plus de 32 caractères");
    t.expect(validSerial(std::string(32, 'A')), "32 caractères");
    t.expect(!validSerial("SIM_0001"), "souligné refusé");
    t.expect(!validSerial("SIM:1"), "deux-points refusés");
    t.expect(!validSerial("é"), "lettre accentuée refusée");
    t.expect(!validSerial("SIM/1") && !validSerial("SIM 1") && !validSerial("a.b"),
             "autres caractères refusés");
    t.expect(!validSerial("[") && !validSerial("{") && !validSerial("@"),
             "caractères voisins des lettres refusés");
}

void identities(Expectations& t) {
    Recorder recorder{commandOrigin};
    t.expect(recorder.valid(), "enregistreur valide");
    t.expect(recorder.stream() == "pump-SIM0001:commande:boot-12", "flux de l'ADR 0006");
    t.expect(recorder.producer() == Producer::Command, "producteur");
    t.expect(recorder.ring().identity() == recorder.stream(), "l'anneau porte le flux");
    const Correlation first = recorder.input();
    const Correlation second = recorder.input();
    t.expect(first.view() == "pump-SIM0001:commande:boot-12:input-1", "première entrée");
    t.expect(second.view() == "pump-SIM0001:commande:boot-12:input-2", "entrées numérotées");
    t.expect(first.local() == "commande:boot-12:input-1", "sans le préfixe de la pompe");
    t.expect(recorder.condition().view() == "pump-SIM0001:commande:boot-12:cond-1",
             "première condition, compteur distinct");
    t.expect(recorder.boot().view() == "pump-SIM0001:commande:boot-12", "le démarrage lui-même");

    const Recorder safetyRecorder{{.serial = "SIM0001", .producer = Producer::Safety, .boot = 9}};
    t.expect(safetyRecorder.stream() == "pump-SIM0001:securite:boot-9", "flux de la sécurité");

    Recorder badSerial{{.serial = "SIM 1", .producer = Producer::Command, .boot = 1}};
    const Recorder badProducer{{.serial = "SIM1", .producer = invalidProducer(), .boot = 1}};
    t.expect(!badSerial.valid() && !badProducer.valid(), "série ou producteur invalides");
    const Result refused =
        badSerial.record(Action::DeviceBoot, entry(AuditPhase::Executed, "commande", Target::Pump,
                                                   "pump-x:commande:boot-1"));
    t.expect(refused.outcome == Outcome::Refused && refused.refusal.has_value() &&
                 refused.refusal->reason == mddlog::core::AuditRefusalReason::InvalidStream,
             "enregistreur invalide : tout est refusé par mddlog");
    t.expect(badSerial.refused() == 1, "refus compté");
}

void recording(Expectations& t) {
    Recorder recorder{commandOrigin};
    const Correlation input = recorder.input();
    Detail keys;
    keys.add("state", std::uint64_t{4});
    Entry start = entry(AuditPhase::Requested, operatorActor, Target::Channel, input.view(), &keys);
    start.monotonic = 91'877ms;
    const Result first = recorder.record(Action::TherapyStart, start);
    t.expect(admitted(first) && first.sequence == 1, "admis, séquence 1");
    const mddlog::core::AuditEvent event = last(recorder);
    t.expect(event.action() == "therapy.start", "action");
    t.expect(event.category() == AuditCategory::Operator, "catégorie de l'action");
    t.expect(event.phase() == AuditPhase::Requested, "phase");
    t.expect(event.actor() == "operateur", "acteur");
    t.expect(event.target() == "pump-SIM0001/channel-1", "cible du canal");
    t.expect(event.correlationId() == input.view(), "corrélation");
    t.expect(event.requirementRef() == "SYS-REQ-044" && event.riskRef() == "RISK-001",
             "références du vocabulaire");
    t.expect(event.detail() == "mono=91877 state=4", "temps monotone en tête");
    t.expect(event.time().availability() == mddlog::core::TimeAvailability::Unavailable,
             "heure indisponible");
    t.expect(!event.sourceSequence().has_value(), "pas de séquence source");

    const Correlation condition = recorder.condition();
    Entry raise = entry(AuditPhase::Executed, "commande", Target::Channel, condition.view());
    raise.monotonic = 5ms;
    raise.clockDoubtful = true;
    const Result doubtful = recorder.record(AlarmCondition::Occlusion, AlarmStep::Raise, raise);
    t.expect(admitted(doubtful) && doubtful.sequence == 2, "alarme admise");
    t.expect(last(recorder).detail() == "mono=5 clock=doubtful", "heure douteuse signalée");
    t.expect(last(recorder).action() == "alarm.occlusion.raise", "action d'alarme");

    const Detail empty;
    const Correlation boot = recorder.boot();
    const Result bare =
        recorder.record(Action::DeviceBoot,
                        entry(AuditPhase::Executed, "commande", Target::Pump, boot.view(), &empty));
    t.expect(admitted(bare) && last(recorder).detail() == "mono=0", "détail vide");
    t.expect(last(recorder).target() == "pump-SIM0001", "cible de la pompe");
    t.expect(recorder.refused() == 0, "aucun refus");
}

void targets(Expectations& t) {
    Recorder recorder{commandOrigin};
    const Correlation boot = recorder.boot();
    const std::array<std::string_view, 10> expected{
        "pump-SIM0001",          "pump-SIM0001/channel-1",
        "pump-SIM0001/library",  "pump-SIM0001/syringe-list",
        "pump-SIM0001/software", "pump-SIM0001/clock",
        "pump-SIM0001/journal",  "pump-SIM0001/link",
        "pump-SIM0001/battery",  "pump-SIM0001/keypad"};
    for (std::size_t index = 0; index < std::size(expected); ++index) {
        const Result result =
            recorder.record(Action::JournalLoss, entry(AuditPhase::Executed, "commande",
                                                       static_cast<Target>(index), boot.view()));
        t.expect(admitted(result) && last(recorder).target() == std::span{expected}[index],
                 "cible composée");
    }
    const Result invalid = recorder.record(
        Action::JournalLoss, entry(AuditPhase::Executed, "commande", invalidTarget(), boot.view()));
    t.expect(invalid.outcome == Outcome::TargetInvalid, "cible hors énumération");
}

void refusals(Expectations& t) {
    Recorder recorder{commandOrigin};
    const Correlation input = recorder.input();
    const Entry requested =
        entry(AuditPhase::Requested, operatorActor, Target::Channel, input.view());
    const Correlation boot = recorder.boot();
    t.expect(recorder.record(invalidAction(), requested).outcome == Outcome::UnknownAction,
             "action hors vocabulaire");
    t.expect(recorder.record(invalidCondition(), AlarmStep::Raise, requested).outcome ==
                 Outcome::UnknownAction,
             "alarme hors vocabulaire");
    for (const std::uint8_t value : {std::uint8_t{4}, std::uint8_t{32}, std::uint8_t{255}}) {
        Entry outside = requested;
        outside.phase = invalidPhase(value);
        t.expect(recorder.record(Action::TherapyProgram, outside).outcome ==
                     Outcome::PhaseNotAdmitted,
                 "phase hors énumération : non admise");
    }
    t.expect(recorder.record(Action::DeviceBoot, requested).outcome == Outcome::PhaseNotAdmitted,
             "démarrage demandé : phase non admise");
    t.expect(recorder.record(Action::JournalWitnessRefusal,
                             entry(AuditPhase::Executed, "commande", Target::Journal, boot.view()))
                     .outcome == Outcome::ProducerNotAdmitted,
             "refus du témoin enregistré par la commande : non admis");

    Detail overflowing;
    overflowing.add("k", std::string(200, 'x'));
    Entry tooLong = requested;
    tooLong.detail = &overflowing;
    t.expect(recorder.record(Action::TherapyStart, tooLong).outcome == Outcome::DetailTooLong,
             "détail débordé refusé");
    Detail full;
    full.add("k", std::string(150, 'x')); // tient seul, pas avec le temps monotone
    Entry withMono = requested;
    withMono.detail = &full;
    withMono.monotonic = 123'456'789ms;
    t.expect(!full.overflowed() &&
                 recorder.record(Action::TherapyStart, withMono).outcome == Outcome::DetailTooLong,
             "détail trop long avec le temps monotone refusé");

    Entry badActor = requested;
    badActor.actor = "opérateur";
    const Result invalid = recorder.record(Action::TherapyStart, badActor);
    t.expect(invalid.outcome == Outcome::Refused && invalid.refusal.has_value() &&
                 invalid.refusal->reason == mddlog::core::AuditRefusalReason::InvalidIdentifier &&
                 invalid.refusal->field == mddlog::core::AuditField::Actor,
             "acteur accentué refusé par mddlog, jamais tronqué");
    t.expect(recorder.refused() == 10, "chaque refus compté");

    // Anneau plein : refus explicite, sans écrasement.
    std::size_t count = 0;
    for (std::size_t i = 0; i < Recorder::ringCapacity + 1; ++i) {
        count += admitted(recorder.record(Action::TherapyStart, requested)) ? 1U : 0U;
    }
    t.expect(count == Recorder::ringCapacity, "64 événements admis");
    const Result full65 = recorder.record(Action::TherapyStart, requested);
    t.expect(full65.outcome == Outcome::Refused && full65.refusal.has_value() &&
                 full65.refusal->reason == mddlog::core::AuditRefusalReason::RingFull,
             "anneau plein : refusé");
    t.expect(recorder.ring().refusalCount() == 2, "refus de l'anneau comptés par mddlog");
    t.expect(recorder.refused() == 12, "et par l'enregistreur");
}

} // namespace

int main() {
    Expectations t;
    return t.run([](Expectations& e) {
        text(e);
        detail(e);
        serials(e);
        identities(e);
        recording(e);
        targets(e);
        refusals(e);
    });
}
