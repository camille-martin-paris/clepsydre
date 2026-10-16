// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <chrono>
#include <cstdint>
#include <ostream>
#include <string_view>

export module clepsydre.sim.journal;

// Journal de diagnostic minimal du simulateur (#43), écrit sans bibliothèque de journalisation :
// une ligne de texte par événement, datée sur le temps simulé. Il sert de point de comparaison
// pour l'intégration de la bibliothèque retenue (#44, ADR 0005). Il ne s'exécute que sur l'hôte.
//
// Format d'une ligne : « HH:MM:SS.mmm GRAVITÉ [composant] message ».

export namespace clepsydre::sim {

enum class Severity : std::uint8_t {
    info,
    alert, // défaut, refus ou vérification échouée
};

class Journal {
public:
    // Journal désactivé : les événements sont ignorés.
    Journal() = default;

    // Journal écrit dans out, qui doit lui survivre.
    explicit Journal(std::ostream& out) noexcept : out_{&out} {}

    [[nodiscard]] bool enabled() const noexcept { return out_ != nullptr; }

    void record(std::chrono::nanoseconds now, Severity severity, std::string_view component,
                std::string_view message);

private:
    std::ostream* out_ = nullptr;
};

} // namespace clepsydre::sim
