// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <ostream>
#include <string_view>

export module clepsydre.sim.journal;

export import mddlog.core.governedbinding;

// Journal de diagnostic du simulateur, sur le cœur gouverné de mddlog (#44, ADR 0005) : le chemin
// sans allocation que l'ADR retient pour le diagnostic sur la cible. Il ne s'exécute que sur
// l'hôte.
//
// Chaque composant écrit par une liaison (GovernedBinding) qui porte son contexte, dans un anneau
// borné (RingLog) ; le journal rend ensuite les enregistrements admis en texte, une ligne par
// événement, datée sur le temps simulé : « HH:MM:SS.mmm GRAVITÉ [composant] message ». Un
// anneau plein refuse au lieu d'écraser ; chaque refus est signalé au rendu suivant.

export namespace clepsydre::sim {

// Enregistrements admis entre deux rendus. La console rend le journal après chaque commande et à
// chaque pas de simulation : quelques enregistrements au plus.
inline constexpr std::size_t journalCapacity = 64;

using JournalBinding = mddlog::core::GovernedBinding<journalCapacity>;

// Instant du temps simulé, compté depuis l'origine de l'horloge Unix : mddlog date chaque
// enregistrement par un temps fourni par l'hôte.
[[nodiscard]] mddlog::core::RawTime simulatedTime(std::chrono::nanoseconds now) noexcept;

class Journal {
public:
    // Journal désactivé : les enregistrements sont admis, puis écartés au rendu.
    Journal() noexcept = default;

    // Journal rendu dans out, qui doit lui survivre ; désactivé si out est nul.
    explicit Journal(std::ostream* out) noexcept : out_{out} {}

    Journal(const Journal&) = delete;
    Journal(Journal&&) = delete;
    Journal& operator=(const Journal&) = delete;
    Journal& operator=(Journal&&) = delete;
    ~Journal() = default;

    [[nodiscard]] bool enabled() const noexcept { return out_ != nullptr; }

    // Contexte réutilisable d'un composant, qui écrit dans l'anneau du journal. Refusé, jamais
    // tronqué, si le nom dépasse la capacité de mddlog (32 octets). Le journal doit survivre à
    // la liaison ; toutes les liaisons écrivent depuis le même fil d'exécution.
    [[nodiscard]] std::expected<JournalBinding, mddlog::core::Refusal>
    bind(std::string_view component);

    // Rend les enregistrements admis, dans l'ordre, puis les refus survenus depuis le rendu
    // précédent, datés de now.
    void flush(std::chrono::nanoseconds now);

private:
    mddlog::core::RingLog<journalCapacity> ring_;
    std::ostream* out_ = nullptr;
    std::uint64_t reportedRefusals_ = 0;
};

} // namespace clepsydre::sim
