// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <chrono>
#include <cstdint>
#include <istream>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

export module clepsydre.sim.console;

import clepsydre.sim.audit_journal;
import clepsydre.sim.command;
import clepsydre.sim.journal;
import clepsydre.sim.power;
import clepsydre.sim.simulation;
import clepsydre.sim.syringe_pump;

// Démonstration interactive du simulateur (#21) : un interprète de commandes texte qui pilote
// le pousse-seringue et son alimentation, injecte des défauts et affiche, côte à côte, les
// grandeurs réelles et ce que mesurent les capteurs. Les commandes sont décrites par « aide »
// et dans docs/development/simulator.md.
//
// La console tient le rôle du banc et de l'opérateur. Elle reproduit deux câblages matériels :
// la validation du moteur exige l'alimentation, et une perte totale d'alimentation remet la
// commande du moteur à zéro (SYS-REQ-038).
//
// Elle fait aussi tourner une commande simulée (#136) : les commandes du soignant (programmer,
// demarrer, pause, arret, bolus, acquitter) passent par la machine à états et les unités de
// src/control, qui pilotent alors le moteur, et chaque action est enregistrée dans le journal
// d'audit selon le vocabulaire de docs/event-vocabulary.md. Une perte totale d'alimentation
// arrête la commande ; son retour la redémarre avec un nouveau numéro de démarrage, donc un
// nouveau flux.
//
// Le journal de diagnostic (#43, #44), s'il est activé, consigne les commandes de l'opérateur qui
// agissent sur la simulation, les erreurs, les vérifications échouées, et les changements d'état
// observés pendant que le temps avance : alimentation, moteur, détecteur d'air.

export namespace clepsydre::sim {

class Console {
public:
    enum class Outcome : std::uint8_t {
        ok,
        failed, // une vérification a échoué
        error,  // commande invalide
        quit,
    };

    // Le journal de diagnostic est rendu dans journal s'il est donné ; journal doit survivre à
    // la console. Le journal d'audit est audit, ou un journal en mémoire s'il est nul.
    explicit Console(std::ostream& out, std::ostream* journal = nullptr,
                     std::unique_ptr<AuditJournal> audit = nullptr);

    Console(const Console&) = delete;
    Console(Console&&) = delete;
    Console& operator=(const Console&) = delete;
    Console& operator=(Console&&) = delete;
    // Clôt le journal d'audit : derniers événements confirmés, registre clos.
    ~Console();

    // Faux si la commande simulée n'a pas pu démarrer : compteur de démarrage illisible.
    [[nodiscard]] bool ready() const noexcept { return command_ != nullptr; }

    [[nodiscard]] AuditJournal& audit() noexcept { return *audit_; }

    // Exécute une ligne : commande, commentaire (#) ou ligne vide.
    Outcome execute(std::string_view line);

    // Exécute les lignes de in. En mode script, chaque ligne est recopiée et l'exécution
    // s'arrête à la première erreur ou vérification échouée. Renvoie le code de sortie : 0 si
    // tout a réussi, 1 si une vérification a échoué, 2 en cas d'erreur, y compris de lecture.
    int run(std::istream& in, bool interactive);

private:
    Outcome interpret(const std::vector<std::string_view>& w);
    void note(JournalBinding& binding, mddlog::core::LogLevel level, std::string_view message);
    void observe();
    void reset(const SyringeModel& syringe);
    void advance(std::chrono::nanoseconds duration);
    void wire();
    void applyLine();
    void applyStepRate();
    void printStatus();
    void printHelp();
    [[nodiscard]] std::optional<double> measure(std::string_view name, bool& known);
    [[nodiscard]] std::chrono::nanoseconds commandNow() const noexcept;
    void startCommand(ResetCause reset, bool interrupted);
    void controlStep(std::chrono::nanoseconds dt);
    Outcome reply(const Reply& answer);
    Outcome commandAudit(std::string_view argument);
    [[nodiscard]] std::optional<double> measureAudit(std::string_view name, bool& known);

    Outcome commandSyringe(std::string_view argument);
    Outcome commandFriction(std::string_view breakaway, std::string_view sliding);
    Outcome commandDriver(std::string_view mode, std::string_view argument);
    Outcome commandSensor(std::string_view sensor, std::string_view mode,
                          std::string_view argument);
    Outcome commandPlacement(std::string_view argument);
    Outcome commandCheck(std::string_view name, std::string_view operation,
                         std::string_view expected);
    Outcome error(std::string_view message);

    std::ostream* out_;
    // Journal d'audit et commande simulée ; la commande vit moins longtemps que le journal.
    std::unique_ptr<AuditJournal> audit_;
    std::unique_ptr<Command> command_;
    std::chrono::nanoseconds sinceCycle_{0};
    // Temps simulé des réinitialisations précédentes : le temps de la commande ne recule pas.
    std::chrono::nanoseconds earlier_{0};
    bool halted_ = false;      // commande arrêtée : pompe hors tension
    bool interrupted_ = false; // perfusion interrompue par la dernière perte d'alimentation
    // Journal et contextes de chaque composant, fixés à la construction.
    Journal journal_;
    JournalBinding operatorLog_;
    JournalBinding consoleLog_;
    JournalBinding powerLog_;
    JournalBinding motorLog_;
    JournalBinding airLog_;
    SyringeModel syringe_;
    SyringeModel listed_; // modèle choisi dans la liste vérifiée, connu de la commande
    SyringePump pump_;
    PowerSupply power_;
    Simulation world_;
    bool occluded_ = false;
    Length height_;
    bool enabled_ = true;
    // États observés au pas précédent, pour journaliser leurs changements.
    bool wasPowered_ = true;
    bool wasOnBattery_ = false;
    bool wasImminent_ = false;
    bool wasRunning_ = false;
    bool wasAirDetected_ = false;
    double millilitresPerHour_ = 0.0;
    Volume mark_;
};

} // namespace clepsydre::sim
