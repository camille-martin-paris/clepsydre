// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <span>
#include <vector>

export module clepsydre.sim.serial_link;

export import clepsydre.sim.clock;

// Liaison série asynchrone point à point entre les processeurs (ADR 0006 § Liaison) : chaque
// sens transporte des octets, sans notion de trame. Le format des trames relève du module de
// protocole partagé par les deux logiciels et par le simulateur.

export namespace clepsydre::sim {

// Un sens de la liaison : les octets sont émis l'un après l'autre au débit de la ligne, puis
// reçus après la latence, dans l'ordre d'émission.
class SerialChannel {
public:
    // bitsPerSecond : débit de la ligne, au format 8N1 (10 bits par octet) ; latency : délai
    // ajouté à la réception de chaque octet.
    SerialChannel(std::int64_t bitsPerSecond, std::chrono::nanoseconds latency) noexcept;

    // Confie des octets à l'émetteur au temps simulé now ; ils partent dès que la ligne est libre.
    void send(TrueTime now, std::span<const std::byte> bytes);
    // Octets reçus au plus tard au temps simulé now, retirés de la ligne.
    [[nodiscard]] std::vector<std::byte> receive(TrueTime now);

    // Durée d'émission d'un octet.
    [[nodiscard]] std::chrono::nanoseconds byteTime() const noexcept { return byteTime_; }

    // Octets émis et pas encore reçus.
    [[nodiscard]] std::size_t inFlight() const noexcept { return pending_.size(); }

private:
    struct Arrival {
        TrueTime at;
        std::byte value;
    };

    std::chrono::nanoseconds byteTime_;
    std::chrono::nanoseconds latency_;
    TrueTime lineFree_{};
    std::deque<Arrival> pending_;
};

// Liaison entre le processeur de commande et le processeur de sécurité : deux sens
// indépendants.
struct SerialLink {
    SerialChannel controlToSafety;
    SerialChannel safetyToControl;
};

} // namespace clepsydre::sim
