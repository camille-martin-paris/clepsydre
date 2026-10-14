// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>

export module clepsydre.protocol.link;

import clepsydre.protocol.frame;

// Surveillance de la liaison entre les processeurs (ADR 0006 § Règles, SYS-REQ-053) : émission
// numérotée et signal de vie périodique d'un côté, détection de la perte de lien de l'autre.
// Conception détaillée : docs/design/link-protocol.md.
//
// Le temps est celui du compteur monotone du processeur (SW-REQ-018), en durée depuis son
// démarrage. Ce module ne fait ni allocation dynamique ni exception.

export namespace clepsydre::protocol {

using MonotonicTime = std::chrono::nanoseconds;

constexpr std::chrono::milliseconds heartbeatPeriod{50};
constexpr std::chrono::milliseconds linkTimeout{200};

// Charge utile du signal de vie : état de l'émetteur, et dernier numéro de transport reçu de
// l'autre processeur, s'il en a reçu un depuis son démarrage.
struct Heartbeat {
    std::uint8_t state = 0; // valeurs définies par la machine à états (#24)
    std::optional<std::uint32_t> lastReceived;
};

constexpr std::size_t heartbeatSize = 6;

[[nodiscard]] Payload encodeHeartbeat(const Heartbeat& heartbeat) noexcept;
// std::nullopt si bytes n'est pas l'encodage d'un signal de vie.
[[nodiscard]] std::optional<Heartbeat> decodeHeartbeat(std::span<const std::byte> bytes) noexcept;

// Émission : chaque trame reçoit le numéro de transport suivant, à partir de 0 au démarrage.
class LinkTransmitter {
public:
    LinkTransmitter(Sender self, std::uint32_t bootNumber) noexcept;

    [[nodiscard]] EncodedFrame encode(MessageType type, const Payload& payload) noexcept;

    // Vrai si aucun signal de vie n'a été émis depuis heartbeatPeriod, ou jamais.
    [[nodiscard]] bool heartbeatDue(MonotonicTime now) const noexcept;
    [[nodiscard]] EncodedFrame heartbeat(MonotonicTime now, const Heartbeat& heartbeat) noexcept;

    [[nodiscard]] std::uint32_t nextTransportNumber() const noexcept { return next_; }

private:
    Sender self_;
    std::uint32_t bootNumber_;
    std::uint32_t next_ = 0;
    std::optional<MonotonicTime> lastHeartbeat_;
};

enum class LinkState : std::uint8_t {
    Waiting,      // aucune trame valide reçue depuis la mise en route
    Up,           // lien établi
    Lost,         // perte de lien ; rétabli par la prochaine trame valide
    OutOfService, // désaccord de version : définitif
};

// Cause de la dernière perte de lien.
enum class LinkFault : std::uint8_t {
    None,
    Timeout,           // aucune trame valide pendant linkTimeout
    Rejected,          // trame rejetée par la couche trame (voir rejection())
    WrongSender,       // trame valide dont l'émetteur n'est pas l'autre processeur
    TransportGap,      // numéro de transport en avance : trame manquante
    TransportRepeated, // même numéro que la trame précédente
    TransportBackward, // numéro en arrière
    BootChanged,       // numéro de démarrage de l'émetteur différent
    VersionMismatch,   // trame d'une autre version du protocole : mise hors service
};

// Réception : surveille les trames de l'autre processeur.
//
// Lien établi, toute anomalie est une perte de lien : délai dépassé, trame rejetée, émetteur
// inattendu, numéro de transport autre que le précédent plus un, numéro de démarrage changé.
// Avant l'établissement et après une perte, les trames rejetées sont ignorées (octets reçus en
// cours de trame à la mise en route, par exemple) et la première trame valide de l'autre
// processeur (ré)établit le lien, avec ses numéros pour référence. Un désaccord de version met
// la liaison hors service, quel que soit l'état, et définitivement.
class LinkMonitor {
public:
    LinkMonitor(Sender peer, MonotonicTime start) noexcept;

    // Traite le résultat de la couche trame reçu à now ; contrôle d'abord le délai.
    void onReceived(MonotonicTime now, const std::expected<Frame, FrameError>& received) noexcept;
    // Contrôle le délai ; à appeler périodiquement, même sans réception.
    void poll(MonotonicTime now) noexcept;

    [[nodiscard]] LinkState state() const noexcept { return state_; }

    [[nodiscard]] LinkFault fault() const noexcept { return fault_; }

    // Motif du rejet, si fault() == LinkFault::Rejected.
    [[nodiscard]] std::optional<FrameError> rejection() const noexcept { return rejection_; }

    // Nombre de pertes de lien depuis la mise en route.
    [[nodiscard]] std::uint32_t losses() const noexcept { return losses_; }

    // Numéro de démarrage de l'autre processeur, depuis le dernier établissement du lien.
    [[nodiscard]] std::optional<std::uint32_t> peerBootNumber() const noexcept { return peerBoot_; }

private:
    void lose(LinkFault fault, std::optional<FrameError> rejection = std::nullopt) noexcept;
    void establish(MonotonicTime now, const Header& header) noexcept;
    [[nodiscard]] LinkFault check(const Header& header) const noexcept;

    Sender peer_;
    LinkState state_ = LinkState::Waiting;
    LinkFault fault_ = LinkFault::None;
    std::optional<FrameError> rejection_;
    std::uint32_t losses_ = 0;
    MonotonicTime lastValid_; // dernière trame valide, ou mise en route
    std::optional<std::uint32_t> peerBoot_;
    std::uint32_t lastTransport_ = 0;
};

} // namespace clepsydre::protocol
