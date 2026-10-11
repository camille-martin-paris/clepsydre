// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

// clepsydre-sim : démonstration interactive du simulateur (#21).
//   clepsydre-sim             commandes tapées au clavier
//   clepsydre-sim <scénario>  commandes lues dans un fichier ; code de sortie 0 si toutes les
//                             vérifications réussissent, 1 si l'une échoue, 2 en cas d'erreur

#include <cstdio>
#include <exception>
#include <fstream>
#include <iostream>
#include <print>
#include <span>

import clepsydre.sim.console;

int main(int argc, char** argv) {
    try {
        const std::span arguments(argv, static_cast<std::size_t>(argc));
        if (arguments.size() > 2) {
            std::println(stderr, "Usage : clepsydre-sim [scénario]");
            return 2;
        }
        clepsydre::sim::Console console{std::cout};
        if (arguments.size() == 2) {
            std::ifstream scenario{arguments[1]};
            if (!scenario) {
                std::println(stderr, "Scénario illisible : {}", arguments[1]);
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
