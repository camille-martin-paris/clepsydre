// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

// Journal d'audit persistant du simulateur (#136) : les événements d'un enregistreur sont remis
// au sink persistant de mddlog, confirmés durablement, relus et vérifiés par le lecteur de mddlog ;
// chaque démarrage a son flux et son registre ; une altération du support est détectée ; un
// compteur de démarrage corrompu empêche le démarrage (SW-REQ-020). Une séance de la console
// (programmation, perfusion, bolus, occlusion, état sûr, acquittement) produit un journal qui
// suit le vocabulaire de docs/event-vocabulary.md.

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expect.hpp>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <iterator>
#include <memory>
#include <optional>
#include <print>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

import clepsydre.sim.audit_journal;
import clepsydre.sim.command;
import clepsydre.sim.console;

namespace {

using namespace std::chrono_literals;
using clepsydre::test::Expectations;
using namespace clepsydre;
namespace fs = std::filesystem;

// Répertoire de travail neuf, propre à ce test.
fs::path workspace(std::string_view name) {
    const fs::path path = fs::temp_directory_path() /
                          std::format("clepsydre-{}-{}", name,
                                      std::chrono::steady_clock::now().time_since_epoch().count());
    fs::remove_all(path);
    return path;
}

audit::Entry executed(std::string_view correlation, const audit::Detail* detail = nullptr) {
    audit::Entry entry;
    entry.phase = audit::AuditPhase::Executed;
    entry.actor = "commande";
    entry.target = audit::Target::Pump;
    entry.correlation = correlation;
    entry.detail = detail;
    return entry;
}

// Démarre un producteur sur journal et enregistre count événements device.boot puis journal.loss.
std::unique_ptr<audit::Recorder> produce(sim::AuditJournal& journal, std::uint64_t boot,
                                         std::size_t count, Expectations& t) {
    auto recorder = std::make_unique<audit::Recorder>(audit::Origin{
        .serial = sim::simulatedSerial, .producer = audit::Producer::Command, .boot = boot});
    t.expect(journal.attach(recorder->ring()), "anneau confié au journal");
    const audit::Correlation start = recorder->boot();
    audit::Detail detail;
    detail.add("boot", boot);
    t.expect(audit::admitted(
                 recorder->record(audit::Action::DeviceBoot, executed(start.view(), &detail))),
             "démarrage admis");
    for (std::size_t i = 1; i < count; ++i) {
        audit::Detail loss;
        loss.add("count", i).add("cause", "ring-full");
        t.expect(audit::admitted(
                     recorder->record(audit::Action::JournalLoss, executed(start.view(), &loss))),
                 "événement admis");
    }
    journal.drain();
    return recorder;
}

void memory(Expectations& t) {
    std::string error;
    auto journal = sim::AuditJournal::open(std::nullopt, sim::simulatedSerial, error);
    t.expect(journal != nullptr && !journal->directory().has_value(), "journal en mémoire");
    audit::Recorder::Ring orphan{"pump-SIM0001:commande:boot-9"};
    t.expect(!journal->attach(orphan), "aucun anneau confié avant le démarrage");
    t.expect(journal->start() == 1, "premier démarrage");
    const auto recorder = produce(*journal, 1, 3, t);
    const sim::AuditStatus status = journal->status();
    t.expect(status.handedOff == 3 && status.durable == 3 && status.pending == 0 &&
                 status.losses == 0 && status.storageFailures == 0,
             "trois événements remis et confirmés durablement");
    const auto events = journal->events();
    t.expect(events.size() == 3, "trois événements relus, registre omis");
    t.expect(events.front().stream == "pump-SIM0001:commande:boot-1" &&
                 events.front().sequence == 1 && events.front().action == "device.boot" &&
                 events.front().inVocabulary,
             "premier événement relu");
    t.expect(events.back().detail == "mono=0 count=2 cause=ring-full", "détail relu à l'identique");
    t.expect(events.back().requirement == "SW-REQ-008" && events.back().risk == "RISK-011",
             "références relues");
    t.expect(sim::coherent(journal->verify()), "flux et registre cohérents, non ancrés");

    std::string invalid;
    t.expect(sim::AuditJournal::open(std::nullopt, "SIM 1", invalid) == nullptr && !invalid.empty(),
             "numéro de série invalide refusé");
}

void files(Expectations& t) {
    const fs::path directory = workspace("audit");
    std::string error;
    {
        auto journal = sim::AuditJournal::open(directory, sim::simulatedSerial, error);
        t.expect(journal != nullptr && fs::is_directory(directory), "répertoire créé");
        t.expect(journal->start() == 1, "démarrage 1");
        const auto first = produce(*journal, 1, 4, t);
        // Coupure : un nouveau démarrage sans clôture du précédent.
        t.expect(journal->start() == 2, "démarrage 2 après une coupure");
        const auto second = produce(*journal, 2, 2, t);
        journal->close();
    }
    {
        auto journal = sim::AuditJournal::open(directory, sim::simulatedSerial, error);
        t.expect(journal->start() == 3, "le compteur persiste d'une exécution à l'autre");
        const auto third = produce(*journal, 3, 1, t);
        const auto events = journal->events();
        t.expect(events.size() == 7, "4 + 2 + 1 événements relus des trois démarrages");
        const mddlog::adapter::LogReport report = journal->verify();
        t.expect(sim::coherent(report), "trois flux et trois registres cohérents");
        t.expect(report.streams.size() == 6, "un flux et un registre par démarrage");
        t.expect(report.find("pump-SIM0001:commande:boot-1") != nullptr &&
                     report.find("pump-SIM0001:commande:boot-1")->report.lastPresent == 4,
                 "flux du premier démarrage complet");
        journal->close();
    }

    // Altération d'un octet d'un enregistrement sur le support : détectée par le lecteur.
    std::vector<fs::path> segments;
    for (const auto& entry : fs::directory_iterator{directory}) {
        if (entry.path().filename().string().starts_with("segment-")) {
            segments.push_back(entry.path());
        }
    }
    // Le segment du premier flux qui porte « count=2 » : un seul, le démarrage 2 s'arrêtant à 1.
    std::string bytes;
    std::size_t at = std::string::npos;
    fs::path modified;
    for (const fs::path& segment : segments) {
        std::ifstream in{segment, std::ios::binary};
        bytes.assign(std::istreambuf_iterator<char>{in}, {});
        at = bytes.find("count=2");
        if (at != std::string::npos) {
            modified = segment;
            break;
        }
    }
    t.expect(at != std::string::npos, "détail présent en clair dans un segment");
    if (at != std::string::npos) {
        bytes[at + 6] = '9';
        std::ofstream out{modified, std::ios::binary | std::ios::trunc};
        out << bytes;
    }
    {
        auto journal = sim::AuditJournal::open(directory, sim::simulatedSerial, error);
        const mddlog::adapter::LogReport report = journal->verify();
        const auto* altered = report.find("pump-SIM0001:commande:boot-1");
        // L'enregistrement modifié ne passe plus le contrôle de sa trame : le flux paraît
        // s'arrêter avant lui. Sans ancrage, seul le registre du démarrage suivant, qui cite la
        // position 4 du flux, le révèle.
        t.expect(altered != nullptr && altered->report.lastPresent < 4,
                 "enregistrement modifié : le flux ne le contient plus");
        t.expect(std::ranges::any_of(
                     report.notes,
                     [](const mddlog::adapter::BoundaryNote& note) {
                         return note.kind ==
                                    mddlog::adapter::BoundaryKind::RecoveredNotReproduced &&
                                note.position == 4;
                     }),
                 "la citation du registre suivant n'est plus reproduite");
        t.expect(!sim::coherent(report), "le journal n'est plus déclaré cohérent");
    }

    // Compteur de démarrage corrompu : aucun démarrage (SW-REQ-020).
    std::ofstream{directory / "boot", std::ios::trunc} << "trois\n";
    {
        auto journal = sim::AuditJournal::open(directory, sim::simulatedSerial, error);
        t.expect(!journal->start().has_value(), "compteur corrompu : démarrage refusé");
    }
    std::ofstream{directory / "boot", std::ios::trunc}.close();
    {
        auto journal = sim::AuditJournal::open(directory, sim::simulatedSerial, error);
        t.expect(!journal->start().has_value(), "compteur vide : démarrage refusé");
    }
    fs::remove_all(directory);

    const fs::path file = workspace("audit-fichier");
    std::ofstream{file}.close();
    t.expect(sim::AuditJournal::open(file, sim::simulatedSerial, error) == nullptr &&
                 error.contains("inutilisable"),
             "un fichier n'est pas un répertoire de journal");
    fs::remove(file);
}

// Actions et phases des événements d'un flux, dans l'ordre.
std::vector<std::string> trace(const std::vector<sim::StoredEvent>& events) {
    std::vector<std::string> out;
    for (const sim::StoredEvent& event : events) {
        constexpr std::string_view phases = "RCEF";
        out.push_back(
            std::format("{} {}", event.action, phases[static_cast<std::size_t>(event.phase)]));
    }
    return out;
}

void session(Expectations& t) {
    std::ostringstream out;
    sim::Console console{out};
    t.expect(console.ready(), "commande simulée démarrée");
    const std::vector<std::string_view> script{
        "demarrer",          // refusé : rien n'est programmé
        "programmer 300 20", // refusé : au-delà du débit de la pompe
        "programmer 25 20",  // accepté
        "demarrer",          // perfusion
        "avance 1min",
        "bolus 3 50",  // au-delà de la limite souple, sans confirmation : refusé
        "bolus 1 50",  // accepté
        "avance 2min", // bolus délivré
        "debit 10",    // refusé par la console : la commande pilote le moteur
        "occlusion oui",
        "avance 3min", // occlusion détectée, état sûr, recul
        "acquitter",         "occlusion non",
    };
    for (const std::string_view line : script) {
        static_cast<void>(console.execute(line));
    }
    t.expect(out.str().contains("Refusé : Démarrage refusé(e) dans l'état « arrêt »"),
             "démarrage refusé à l'arrêt");
    t.expect(out.str().contains("Erreur : la commande simulée pilote le moteur"),
             "débit direct refusé pendant la perfusion");
    const std::vector<sim::StoredEvent> events = console.audit().events();
    const std::vector<std::string> expected{
        "device.boot E",
        "therapy.start R",
        "therapy.start F",
        "therapy.program R",
        "therapy.program F",
        "therapy.program R",
        "therapy.program E",
        "therapy.start R",
        "therapy.start E",
        "therapy.bolus R",
        "therapy.bolus F",
        "therapy.bolus R",
        "therapy.bolus E",
        "therapy.bolus-end E",
        "alarm.occlusion.raise E",
        "safety.safe-state E",
        "safety.safe-state-acknowledge R",
        "safety.safe-state-acknowledge E",
        "alarm.occlusion.clear E",
    };
    const std::vector<std::string> actual = trace(events);
    t.expect(actual == expected, "séance : actions et phases dans l'ordre");
    if (actual != expected) {
        for (const std::string& line : actual) {
            std::println(stderr, "  {}", line);
        }
    }
    t.expect(std::ranges::all_of(events, [](const auto& e) { return e.inVocabulary; }),
             "toutes les actions sont au vocabulaire");
    t.expect(
        std::ranges::all_of(events, [](const auto& e) { return e.detail.starts_with("mono="); }),
        "temps monotone en tête de chaque détail");
    const auto find = [&events](std::string_view action, char phase) {
        return std::ranges::find_if(events, [&](const sim::StoredEvent& e) {
            return e.action == action && "RCEF"[static_cast<std::size_t>(e.phase)] == phase;
        });
    };
    t.expect(find("therapy.start", 'F')->detail == "mono=0 refus=state state=2",
             "refus d'état : motif et état");
    t.expect(find("therapy.program", 'F')->detail == "mono=0 refus=validation motif=9",
             "refus de validation : motif RateAbovePumpMaximum");
    t.expect(find("therapy.bolus", 'F')->detail == "mono=60000 refus=timeout",
             "bolus au-delà de la limite souple sans confirmation");
    const auto raise = find("alarm.occlusion.raise", 'E');
    const auto safe = find("safety.safe-state", 'E');
    t.expect(raise->correlation == safe->correlation && raise->correlation.ends_with(":cond-1"),
             "alarme et état sûr corrélés à la condition");
    t.expect(safe->detail.ends_with("state=11 cause=alarm.occlusion"), "cause de l'état sûr");
    const auto acknowledge = find("safety.safe-state-acknowledge", 'E');
    t.expect(acknowledge->correlation.ends_with(":input-7") &&
                 acknowledge->detail.contains("cond=commande:boot-1:cond-1"),
             "acquittement : sa propre entrée, et la condition citée");
    const auto bolus = find("therapy.bolus", 'E');
    const auto bolusEnd = find("therapy.bolus-end", 'E');
    t.expect(bolus->correlation.ends_with(":input-6") &&
                 bolusEnd->correlation == bolus->correlation &&
                 bolusEnd->detail.ends_with("delivered_ul=1000 cause=volume"),
             "fin du bolus corrélée à sa demande, volume délivré");
    t.expect(console.audit().status().durable == events.size(), "tout est confirmé durablement");
    t.expect(sim::coherent(console.audit().verify()), "journal cohérent pour le lecteur de mddlog");
}

// Pause pendant le rattrapage des jeux : le moteur s'arrête, puis la reprise repart.
void pauseDuringTakeUp(Expectations& t) {
    std::ostringstream out;
    sim::Console console{out};
    for (const std::string_view line : {"programmer 25 20", "demarrer", "avance 20ms", "pause"}) {
        static_cast<void>(console.execute(line));
    }
    t.expect(console.execute("avance 100ms") == sim::Console::Outcome::ok, "temps avancé");
    t.expect(console.execute("verifie etat-commande = 9") == sim::Console::Outcome::ok,
             "pause acceptée");
    t.expect(console.execute("verifie moteur = non") == sim::Console::Outcome::ok,
             "moteur arrêté pendant la pause, rattrapage compris");
    t.expect(console.execute("repere") == sim::Console::Outcome::ok, "repère");
    static_cast<void>(console.execute("avance 1min"));
    t.expect(console.execute("verifie volume-depuis-repere = 0") == sim::Console::Outcome::ok,
             "rien ne bouge pendant une minute de pause");
    for (const std::string_view line : {"demarrer", "avance 2min"}) {
        static_cast<void>(console.execute(line));
    }
    t.expect(console.execute("verifie etat-commande = 6") == sim::Console::Outcome::ok &&
                 console.execute("verifie volume-perfuse > 0.5") == sim::Console::Outcome::ok,
             "reprise : rattrapage achevé, perfusion");
}

// Compteur de démarrage corrompu entre une coupure et le retour de l'alimentation : la commande
// ne redémarre pas, et le journal ne lit plus l'anneau de la commande arrêtée.
void corruptCounterOnRestart(Expectations& t) {
    const fs::path directory = workspace("audit-redemarrage");
    std::string error;
    {
        std::ostringstream out;
        sim::Console console{out, nullptr,
                             sim::AuditJournal::open(directory, sim::simulatedSerial, error)};
        t.expect(console.ready(), "premier démarrage");
        for (const std::string_view line : {"programmer 25 20", "demarrer", "avance 1min",
                                            "secteur non", "batterie non", "avance 1s"}) {
            static_cast<void>(console.execute(line));
        }
        std::ofstream{directory / "boot", std::ios::trunc} << "illisible\n";
        for (const std::string_view line : {"batterie oui", "avance 1s", "audit"}) {
            static_cast<void>(console.execute(line));
        }
        t.expect(!console.ready(), "compteur corrompu : commande hors service");
        t.expect(out.str().contains("compteur de démarrage du journal d'audit illisible"),
                 "échec du redémarrage signalé");
        t.expect(console.execute("demarrer") == sim::Console::Outcome::error,
                 "plus aucune commande du soignant");
        t.expect(sim::coherent(console.audit().verify()), "le premier flux reste cohérent");
    }
    fs::remove_all(directory);
}

} // namespace

int main() {
    Expectations t;
    return t.run([](Expectations& e) {
        memory(e);
        files(e);
        session(e);
        pauseDuringTakeUp(e);
        corruptCounterOnRestart(e);
    });
}
