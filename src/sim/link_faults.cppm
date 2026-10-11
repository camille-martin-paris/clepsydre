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
    delay,     // la trame part FrameFaultRule::delay plus tard
    reorder,   // la trame part juste après la trame suivante
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
// Composition des défauts d'une trame, dans l'ordre des règles : une perte l'emporte sur tout ;
// altération et troncature modifient le contenu ; un doublon émet le contenu obtenu deux fois.
// Le retard et le réordonnancement fixent ensuite son départ, au plus tôt :
// - retard : à son échéance, l'instant de remise augmenté du retard ;
// - réordonnancement : juste après le départ de la trame suivante, dans l'ordre de remise à
//   l'injecteur, que celle-ci parte aussitôt ou après son propre retard. Une trame perdue ne
//   compte pas : la trame retenue attend la suivante encore. Sans trame suivante, elle reste
//   retenue.
// Une trame touchée par les deux part au plus tôt à son échéance, et après la suivante.
// Les départs se font dans l'ordre de leurs instants ; à instant égal, dans l'ordre de remise.
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
    // Trame remise à l'injecteur et pas encore retirée de la file.
    struct Pending {
        std::vector<std::byte> bytes;
        TrueTime due;           // départ au plus tôt, retard compris
        bool afterNext = false; // réordonnancement : part après la trame suivante
        bool departed = false;
        TrueTime departedAt{};
    };

    void process(TrueTime now, std::vector<std::byte> frame);
    // Fait partir les trames prêtes au temps simulé now.
    void release(TrueTime now);
    // Instant de départ possible de la trame d'indice index, si elle est prête à now.
    [[nodiscard]] std::optional<TrueTime> readyAt(std::size_t index, TrueTime now) const;
    [[nodiscard]] bool hits(const FrameFaultRule& rule, TrueTime now);
    void corrupt(std::vector<std::byte>& frame);

    ByteChannel* line_;
    std::mt19937_64 random_;
    std::vector<FrameFaultRule> rules_;
    std::vector<std::byte> partial_;
    std::deque<Pending> pending_; // dans l'ordre de remise
    std::array<std::size_t, frameFaultCount> injected_{};
};

} // namespace clepsydre::sim
