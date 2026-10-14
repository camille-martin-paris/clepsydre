// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <chrono>
#include <cstdint>
#include <istream>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

export module clepsydre.sim.console;

import clepsydre.sim.power;
import clepsydre.sim.simulation;
import clepsydre.sim.syringe_pump;

// Démonstration interactive du simulateur (#21) : un interprète de commandes texte qui pilote
// le pousse-seringue et son alimentation, injecte des défauts et affiche, côte à côte, les
// grandeurs réelles et ce que mesurent les capteurs. Les commandes sont décrites par « aide »
// et dans docs/development/simulator.md.
//
// La console tient le rôle de l'opérateur : il n'y a pas encore de logiciel de pompe. Elle
// reproduit seulement deux câblages matériels : la validation du moteur exige l'alimentation,
// et une perte totale d'alimentation remet la commande du moteur à zéro, comme le logiciel de
// commande devra le faire à son redémarrage (SYS-REQ-038).

export namespace clepsydre::sim {

class Console {
public:
    enum class Outcome : std::uint8_t {
        ok,
        failed, // une vérification a échoué
        error,  // commande invalide
        quit,
    };

    explicit Console(std::ostream& out);

    Console(const Console&) = delete;
    Console(Console&&) = delete;
    Console& operator=(const Console&) = delete;
    Console& operator=(Console&&) = delete;
    ~Console() = default;

    // Exécute une ligne : commande, commentaire (#) ou ligne vide.
    Outcome execute(std::string_view line);

    // Exécute les lignes de in. En mode script, chaque ligne est recopiée et l'exécution
    // s'arrête à la première erreur ou vérification échouée. Renvoie le code de sortie : 0 si
    // tout a réussi, 1 si une vérification a échoué, 2 en cas d'erreur, y compris de lecture.
    int run(std::istream& in, bool interactive);

private:
    void reset(const SyringeModel& syringe);
    void advance(std::chrono::nanoseconds duration);
    void wire();
    void applyLine();
    void applyStepRate();
    void printStatus();
    void printHelp();
    [[nodiscard]] std::optional<double> measure(std::string_view name, bool& known) const;

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
    SyringeModel syringe_;
    SyringePump pump_;
    PowerSupply power_;
    Simulation world_;
    bool occluded_ = false;
    Length height_;
    bool enabled_ = true;
    bool wasPowered_ = true;
    double millilitresPerHour_ = 0.0;
    Volume mark_;
};

} // namespace clepsydre::sim
