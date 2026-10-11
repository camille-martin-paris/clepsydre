// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <random>
#include <span>
#include <vector>

export module clepsydre.sim.link_faults;

export import clepsydre.sim.serial_link;

// Injection de défauts sur la liaison entre les processeurs (ADR 0006 § Règles) : pertes,
// doublons, altérations, troncatures, retards et réordonnancements de trames.

export namespace clepsydre::sim {

// Défauts injectables sur une trame.
enum class FrameFault : std::uint8_t {
    drop,      // la trame est perdue
    duplicate, // la trame est émise deux fois
    corrupt,   // un bit d'un octet de la trame est inversé
    truncate,  // la seconde moitié de la trame est perdue, délimiteur conservé
    delay,     // la trame est retenue pendant FrameFaultRule::delay
    reorder,   // la trame est retenue et part juste après la suivante
};

inline constexpr std::size_t frameFaultCount = 6;

// Règle d'injection : un défaut, sa fenêtre d'activité [from, until) en temps simulé et sa
// probabilité par trame.
struct FrameFaultRule {
    FrameFault fault = FrameFault::drop;
    TrueTime from{};
    TrueTime until = TrueTime::max();
    std::uint32_t perMillion = 1'000'000; // probabilité par trame, en millionièmes
    std::chrono::nanoseconds delay{};     // durée de rétention, pour FrameFault::delay
};

// Injecteur de défauts placé devant un canal d'octets. Il découpe le flux en trames selon le
// délimiteur de l'ADR 0006 (encodage COBS : un octet nul termine chaque trame), sans en lire
// le contenu. Une trame n'est confiée au canal que lorsque son délimiteur l'est.
//
// Les tirages viennent d'un générateur à graine fixe dont la suite est définie par la norme :
// deux exécutions avec la même graine injectent les mêmes défauts.
class FaultyChannel final : public ByteChannel {
public:
    static constexpr std::byte delimiter{0};

    FaultyChannel(ByteChannel& line, std::uint64_t seed);

    // Les règles s'appliquent dans l'ordre d'ajout ; plusieurs défauts peuvent toucher une trame.
    void add(const FrameFaultRule& rule);

    void send(TrueTime now, std::span<const std::byte> bytes) override;
    [[nodiscard]] std::vector<std::byte> receive(TrueTime now) override;

    // Nombre de trames touchées par chaque défaut.
    [[nodiscard]] std::size_t injected(FrameFault fault) const noexcept;

private:
    struct Delayed {
        TrueTime due;
        std::vector<std::byte> frame;
    };

    void process(TrueTime now, std::vector<std::byte> frame);
    void release(TrueTime now);
    [[nodiscard]] bool hits(const FrameFaultRule& rule, TrueTime now);
    void corrupt(std::vector<std::byte>& frame);

    ByteChannel* line_;
    std::mt19937_64 random_;
    std::vector<FrameFaultRule> rules_;
    std::vector<std::byte> partial_;
    std::deque<Delayed> delayed_;
    std::optional<std::vector<std::byte>> held_;
    std::array<std::size_t, frameFaultCount> injected_{};
};

} // namespace clepsydre::sim
