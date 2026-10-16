// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

// clepsydre-sim : démonstration interactive du simulateur (#21).
//   clepsydre-sim             commandes tapées au clavier
//   clepsydre-sim <scénario>  commandes lues dans un fichier ; code de sortie 0 si toutes les
//                             vérifications réussissent, 1 si l'une échoue, 2 en cas d'erreur
//   --journal <fichier>       placé en tête : écrit le journal de diagnostic dans le fichier
//   --audit <répertoire>      placé en tête : journal d'audit persistant dans le répertoire,
//                             créé au besoin et repris d'une exécution à l'autre ; en mémoire
//                             sans cette option

#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

import clepsydre.sim.audit_journal;
import clepsydre.sim.command;
import clepsydre.sim.console;

namespace {

constexpr std::string_view usage =
    "Usage : clepsydre-sim [--journal <fichier>] [--audit <répertoire>] [scénario]";

} // namespace

int main(int argc, char** argv) {
    try {
        std::span arguments(argv, static_cast<std::size_t>(argc));
        arguments = arguments.subspan(1);
        const char* journalPath = nullptr;
        std::optional<std::filesystem::path> auditDirectory;
        while (!arguments.empty() && std::string_view{arguments[0]}.starts_with("--")) {
            const std::string_view option{arguments[0]};
            if (arguments.size() < 2 || (option != "--journal" && option != "--audit")) {
                std::println(stderr, "{}", usage);
                return 2;
            }
            if (option == "--journal") {
                journalPath = arguments[1];
            } else {
                auditDirectory = arguments[1];
            }
            arguments = arguments.subspan(2);
        }
        if (arguments.size() > 1) {
            std::println(stderr, "{}", usage);
            return 2;
        }
        // Le scénario est ouvert avant le journal, dont l'ouverture tronque le fichier : un
        // journal qui désigne le scénario, par le même chemin ou par un lien, le détruirait.
        std::ifstream scenario;
        if (arguments.size() == 1) {
            scenario.open(arguments[0]);
            if (!scenario) {
                std::println(stderr, "Scénario illisible : {}", arguments[0]);
                return 2;
            }
        }
        std::ofstream journalFile;
        if (journalPath != nullptr) {
            std::error_code ignored;
            if (scenario.is_open() &&
                std::filesystem::equivalent(journalPath, arguments[0], ignored)) {
                std::println(stderr, "Le journal désigne le scénario : {}", journalPath);
                return 2;
            }
            journalFile.open(journalPath);
            if (!journalFile) {
                std::println(stderr, "Journal impossible à ouvrir : {}", journalPath);
                return 2;
            }
        }
        // Journal d'audit : un répertoire qui désigne le scénario ou le journal de diagnostic est
        // refusé, avant toute écriture.
        if (auditDirectory) {
            std::error_code ignored;
            const bool collides =
                (scenario.is_open() &&
                 std::filesystem::equivalent(*auditDirectory, arguments[0], ignored)) ||
                (journalPath != nullptr &&
                 std::filesystem::equivalent(*auditDirectory, journalPath, ignored));
            if (collides) {
                std::println(stderr, "Le journal d'audit désigne un autre fichier : {}",
                             auditDirectory->string());
                return 2;
            }
        }
        std::string error;
        auto audit = clepsydre::sim::AuditJournal::open(auditDirectory,
                                                        clepsydre::sim::simulatedSerial, error);
        if (!audit) {
            std::println(stderr, "{}", error);
            return 2;
        }
        clepsydre::sim::Console console{std::cout, journalFile.is_open() ? &journalFile : nullptr,
                                        std::move(audit)};
        if (!console.ready()) {
            return 2;
        }
        if (scenario.is_open()) {
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
