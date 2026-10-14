// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>

export module clepsydre.protocol.frame;

import clepsydre.protocol.cobs;

// Trames de la liaison entre le processeur de commande et le processeur de sécurité (ADR 0006,
// SYS-REQ-053). Une trame est formée d'un en-tête, d'une charge utile et d'un CRC-32C, encodée en
// COBS et suivie d'un octet nul. Format, bornes et diagnostics : docs/design/link-protocol.md.
//
// Ce module ne fait ni allocation dynamique ni exception : toutes les tailles sont bornées à la
// compilation. La surveillance du lien (délais, numéros de transport et de démarrage, version)
// relève d'un autre module ; celui-ci ne fait que former et reconnaître des trames.

export namespace clepsydre::protocol {

// Enveloppe commune à toutes les versions du protocole, qui ne changera jamais : COBS et
// délimiteur nul, version au premier octet, CRC-32C sur les quatre derniers, et une trame décodée
// de envelopeMinSize à envelopeMaxSize octets. Un récepteur reconnaît ainsi une trame intègre de
// n'importe quelle version sans en lire la suite.
constexpr std::size_t crcSize = 4;
constexpr std::size_t envelopeMinSize = 1 + crcSize;
constexpr std::size_t envelopeMaxSize = 1024;
// Encodage COBS de la plus longue enveloppe, suivi du délimiteur.
constexpr std::size_t maxEncodedEnvelopeSize = cobsMaxEncodedSize(envelopeMaxSize) + 1;

// Version 1 du protocole, seule version émise.
constexpr std::uint8_t protocolVersion = 1;
constexpr std::size_t headerSize = 12;
// Trame décodée au plus de 254 octets : son encodage COBS tient en un bloc plein et un code.
constexpr std::size_t maxFrameSize = 254;
constexpr std::size_t maxPayloadSize = maxFrameSize - headerSize - crcSize;
// Encodage COBS de la plus longue trame, suivi du délimiteur.
constexpr std::size_t maxEncodedFrameSize = cobsMaxEncodedSize(maxFrameSize) + 1;
static_assert(maxFrameSize <= envelopeMaxSize);

enum class Sender : std::uint8_t {
    Control = 1, // processeur de commande
    Safety = 2,  // processeur de sécurité
};

// Types de messages de l'ADR 0006. Les charges utiles sont définies avec les fonctions qui les
// produisent.
enum class MessageType : std::uint8_t {
    Heartbeat = 1,         // signal de vie, dans les deux sens
    Programming = 2,       // programmation à confirmer, de la commande vers la sécurité
    Echo = 3,              // copie de la programmation, de la sécurité vers la commande
    ModeChangeRequest = 4, // pause, reprise, bolus, arrêt, de la commande vers la sécurité
    SupervisionState = 5,  // état de supervision, de la sécurité vers la commande
    Event = 6,             // événement d'audit, dans les deux sens
    EventAck = 7,          // accusé d'événement, dans les deux sens
    Anchor = 8,            // ancrage du journal, dans les deux sens
    Test = 9,              // tests de coupure et d'alarme au démarrage, dans les deux sens
};

struct Header {
    Sender sender = Sender::Control;
    std::uint32_t bootNumber = 0;      // numéro de démarrage de l'émetteur
    std::uint32_t transportNumber = 0; // augmenté de 1 à chaque trame émise
    MessageType type = MessageType::Heartbeat;
};

class Payload;
struct Frame;
enum class FrameError : std::uint8_t;

// Reconnaît une trame à partir de ses octets décodés (sans COBS ni délimiteur).
[[nodiscard]] std::expected<Frame, FrameError>
parseFrame(std::span<const std::byte> decoded) noexcept;

// Charge utile d'au plus maxPayloadSize octets.
class Payload {
public:
    Payload() = default;

    // std::nullopt si bytes dépasse maxPayloadSize.
    [[nodiscard]] static std::optional<Payload> from(std::span<const std::byte> bytes) noexcept;

    [[nodiscard]] std::span<const std::byte> bytes() const noexcept;

private:
    friend std::expected<Frame, FrameError> parseFrame(std::span<const std::byte> decoded) noexcept;

    // Précondition : bytes.size() <= maxPayloadSize.
    [[nodiscard]] static Payload copyOf(std::span<const std::byte> bytes) noexcept;

    std::array<std::byte, maxPayloadSize> bytes_{};
    std::size_t size_ = 0;
};

struct Frame {
    Header header;
    Payload payload;
};

// Trame encodée, délimiteur compris, prête à émettre.
class EncodedFrame {
public:
    explicit EncodedFrame(const Frame& frame) noexcept;

    [[nodiscard]] std::span<const std::byte> bytes() const noexcept;

private:
    std::array<std::byte, maxEncodedFrameSize> bytes_{};
    std::size_t size_ = 0;
};

// Motif de rejet d'une suite d'octets reçue entre deux délimiteurs.
enum class FrameError : std::uint8_t {
    TooLong,     // plus longue que l'enveloppe, ou trame de version 1 plus longue que maxFrameSize
    InvalidCobs, // pas un encodage COBS valide, ou trame décodée trop longue
    TooShort,    // plus courte que l'enveloppe, ou trame de version 1 sans en-tête complet
    BadCrc,      // CRC faux : trame altérée
    VersionMismatch, // trame d'une autre version du protocole
    LengthMismatch,  // longueur de l'en-tête différente de la charge utile reçue
    UnknownSender,
    UnknownType,
};

// Reconnaît les trames dans le flux d'octets reçus. Les octets reçus avant le premier délimiteur
// forment une trame incomplète, rejetée comme les autres.
class FrameReceiver {
public:
    // Ajoute un octet reçu. Au délimiteur, renvoie la trame reconnue ou le motif de son rejet ;
    // sinon, std::nullopt.
    [[nodiscard]] std::optional<std::expected<Frame, FrameError>> push(std::byte byte) noexcept;

private:
    std::array<std::byte, maxEncodedEnvelopeSize - 1> buffer_{};
    std::size_t size_ = 0;
    bool overflow_ = false;
};

} // namespace clepsydre::protocol
