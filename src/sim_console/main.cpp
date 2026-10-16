// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

// clepsydre-sim : démonstration interactive du simulateur (#21).
//   clepsydre-sim             commandes tapées au clavier
//   clepsydre-sim <scénario>  commandes lues dans un fichier ; code de sortie 0 si toutes les
//                             vérifications réussissent, 1 si l'une échoue, 2 en cas d'erreur
//   --journal <fichier>       placé en tête : écrit le journal de diagnostic dans le fichier

#include <cstdio>
#include <exception>
#include <fstream>
#include <iostream>
#include <print>
#include <span>
#include <string_view>

import clepsydre.sim.console;
import clepsydre.sim.journal;

int main(int argc, char** argv) {
    try {
        std::span arguments(argv, static_cast<std::size_t>(argc));
        arguments = arguments.subspan(1);
        std::ofstream journalFile;
        clepsydre::sim::Journal journal;
        if (!arguments.empty() && std::string_view{arguments[0]} == "--journal") {
            if (arguments.size() < 2) {
                std::println(stderr, "Usage : clepsydre-sim [--journal <fichier>] [scénario]");
                return 2;
            }
            journalFile.open(arguments[1]);
            if (!journalFile) {
                std::println(stderr, "Journal impossible à ouvrir : {}", arguments[1]);
                return 2;
            }
            journal = clepsydre::sim::Journal{journalFile};
            arguments = arguments.subspan(2);
        }
        if (arguments.size() > 1) {
            std::println(stderr, "Usage : clepsydre-sim [--journal <fichier>] [scénario]");
            return 2;
        }
        clepsydre::sim::Console console{std::cout, journal};
        if (arguments.size() == 1) {
            std::ifstream scenario{arguments[0]};
            if (!scenario) {
                std::println(stderr, "Scénario illisible : {}", arguments[0]);
                return 2;
            }
            return console.run(scenario, false);
        }
        return console.run(std::cin, true);
    } catch (const std::exception& error) {
        static_cast<void>(std::fputs(error.what(), stderr));
        static_cast<void>(std::fputs("\n", stderr));
    } catch (...) {
        static_cast<void>(std::fputs("exception inconnue\n", stderr));
    }
    return 2;
}
