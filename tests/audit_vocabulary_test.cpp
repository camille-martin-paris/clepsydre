// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

// Vocabulaire fermé des événements d'audit (#42, #136) : chaque action a un nom unique, une
// catégorie, des phases et des producteurs admis ; les noms des alarmes sont composés ; une valeur
// hors des énumérations n'a pas de description. La concordance avec docs/event-vocabulary.md est
// vérifiée par audit_vocabulary_doc_test.

#include <cstddef>
#include <cstdint>
#include <expect.hpp>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

import clepsydre.audit.vocabulary;

namespace {

using clepsydre::test::Expectations;
using namespace clepsydre::audit;

Priority invalidPriority() {
    return static_cast<Priority>(9); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
}

// Description présente, ou une exception que Expectations::run compte comme un échec.
ActionSpec must(const std::optional<ActionSpec>& action) {
    if (!action) {
        throw std::runtime_error{"action sans description"};
    }
    return *action;
}

void names(Expectations& t) {
    std::set<std::string, std::less<>> seen;
    for (std::size_t index = 0; index < vocabularySize; ++index) {
        const std::optional<ActionSpec> action = at(index);
        if (!action) {
            t.expect(false, "toute action du vocabulaire a une description");
            continue;
        }
        t.expect(seen.insert(std::string{action->name}).second, "nom unique");
        t.expect(!action->name.starts_with("mddlog."), "préfixe réservé à mddlog jamais employé");
        t.expect(action->phases.bits != 0, "au moins une phase admise");
        t.expect(action->producers.bits != 0, "au moins un producteur");
        t.expect(find(action->name).value_or(ActionSpec{}).name == action->name,
                 "action retrouvée par son nom");
    }
    t.expect(seen.size() == vocabularySize, "121 actions distinctes");
    t.expect(vocabularySize == 121, "49 actions et 18 conditions d'alarme à 4 étapes");
    t.expect(!at(vocabularySize).has_value(), "au-delà du vocabulaire : rien");
    t.expect(!find("diagnostic libre").has_value(), "texte de diagnostic hors du vocabulaire");
    t.expect(!find("alarm.occlusion").has_value(), "alarme sans étape hors du vocabulaire");
    t.expect(!find("").has_value(), "nom vide hors du vocabulaire");
}

void actions(Expectations& t) {
    for (std::size_t index = 0; index < actionCount; ++index) {
        t.expect(must(spec(static_cast<Action>(index))).name == must(at(index)).name,
                 "spec et at concordent pour les actions");
    }
    const ActionSpec program = must(spec(Action::TherapyProgram));
    t.expect(program.name == "therapy.program", "première action");
    t.expect(program.category == AuditCategory::Operator, "programmation : opérateur");
    t.expect(program.requirement == "SYS-REQ-011" && program.risk == "RISK-007",
             "références de la programmation");
    t.expect(contains(program.phases, AuditPhase::Requested) &&
                 contains(program.phases, AuditPhase::Confirmed) &&
                 contains(program.phases, AuditPhase::Executed) &&
                 contains(program.phases, AuditPhase::Failed),
             "programmation : quatre phases");
    t.expect(contains(program.producers, Producer::Command) &&
                 contains(program.producers, Producer::Safety),
             "programmation : deux producteurs");
    const ActionSpec boot = must(spec(Action::DeviceBoot));
    t.expect(contains(boot.phases, AuditPhase::Executed) &&
                 !contains(boot.phases, AuditPhase::Requested),
             "démarrage : exécuté seulement");
    const ActionSpec cancel = must(spec(Action::TherapyProgramCancel));
    t.expect(cancel.risk.empty(), "référence de risque vide quand le document n'en donne pas");
    const ActionSpec witness = must(spec(Action::JournalWitnessRefusal));
    t.expect(contains(witness.producers, Producer::Safety) &&
                 !contains(witness.producers, Producer::Command),
             "refus du témoin : sécurité seulement");
    t.expect(must(spec(Action::JournalCrossCheck)).name == "journal.cross-check",
             "dernière action");
    t.expect(!spec(static_cast<Action>(actionCount)).has_value(), "action hors énumération");
    t.expect(!spec(static_cast<Action>(255)).has_value(), "action invalide");
}

void alarms(Expectations& t) {
    std::size_t index = actionCount;
    for (std::size_t c = 0; c < alarmConditionCount; ++c) {
        for (std::size_t s = 0; s < alarmStepCount; ++s) {
            const ActionSpec alarm =
                must(spec(static_cast<AlarmCondition>(c), static_cast<AlarmStep>(s)));
            t.expect(alarm.name == must(at(index)).name,
                     "alarmes dans l'ordre condition puis étape");
            t.expect(alarm.category == AuditCategory::RiskControl, "alarme : maîtrise du risque");
            ++index;
        }
    }
    const ActionSpec raise = must(spec(AlarmCondition::Occlusion, AlarmStep::Raise));
    t.expect(raise.name == "alarm.occlusion.raise", "nom composé du déclenchement");
    t.expect(raise.requirement == "SYS-REQ-008" && raise.risk == "RISK-005",
             "références de la condition");
    t.expect(contains(raise.phases, AuditPhase::Executed) &&
                 !contains(raise.phases, AuditPhase::Requested),
             "déclenchement : exécuté seulement");
    const ActionSpec silence = must(spec(AlarmCondition::JournalWitness, AlarmStep::Silence));
    t.expect(silence.name == "alarm.journal-witness.silence", "dernière condition");
    t.expect(contains(silence.phases, AuditPhase::Requested) &&
                 contains(silence.phases, AuditPhase::Failed) &&
                 !contains(silence.phases, AuditPhase::Confirmed),
             "pause du signal : demandée, exécutée, échouée");
    t.expect(must(spec(AlarmCondition::Clock, AlarmStep::Clear)).name == "alarm.clock.clear",
             "fin d'alarme");
    t.expect(must(spec(AlarmCondition::Air, AlarmStep::Acknowledge)).name ==
                 "alarm.air.acknowledge",
             "acquittement d'alarme");
    t.expect(!spec(static_cast<AlarmCondition>(alarmConditionCount), AlarmStep::Raise),
             "condition hors énumération");
    t.expect(!spec(AlarmCondition::Air, static_cast<AlarmStep>(alarmStepCount)),
             "étape hors énumération");
}

void helpers(Expectations& t) {
    t.expect(contains(requested | failed, AuditPhase::Failed), "union de phases");
    t.expect(!contains(PhaseSet{}, AuditPhase::Executed), "ensemble vide");
    t.expect(contains(both, Producer::Safety) && !contains(command, Producer::Safety) &&
                 contains(safety, Producer::Safety),
             "ensembles de producteurs");
    t.expect(producerName(Producer::Command) == "commande", "producteur commande");
    t.expect(producerName(Producer::Safety) == "securite", "producteur sécurité, sans accent");
    t.expect(priorityName(Priority::High) == "high", "priorité haute");
    t.expect(priorityName(Priority::Medium) == "medium", "priorité moyenne");
    t.expect(priorityName(Priority::Low) == "low", "priorité basse");
    t.expect(priorityName(Priority::Info) == "info", "signal d'information");
    t.expect(priorityName(invalidPriority()) == "info", "priorité invalide : information");
    t.expect(operatorActor == "operateur" && technicianActor == "role:technicien", "acteurs");
}

} // namespace

int main() {
    Expectations t;
    return t.run([](Expectations& e) {
        names(e);
        actions(e);
        alarms(e);
        helpers(e);
    });
}
