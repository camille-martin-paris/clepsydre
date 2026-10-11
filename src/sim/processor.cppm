// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

export module clepsydre.sim.processor;

export import clepsydre.sim.clock;
export import clepsydre.sim.serial_link;

// Processeur simulé (ADR 0003, ADR 0006) : il exécute un logiciel sur son propre compteur
// monotone et communique par la liaison. Il peut être bloqué, réinitialisé, et son oscillateur
// peut s'arrêter.

export namespace clepsydre::sim {

class Processor {
public:
    // Logiciel exécuté : boot au démarrage, run à chaque pas de l'ordonnanceur. Il ne voit que
    // ce que le processeur lui expose, jamais le temps simulé.
    struct Firmware {
        std::function<void(Processor&)> boot;
        std::function<void(Processor&)> run;
    };

    enum class State : std::uint8_t {
        running,   // le logiciel s'exécute
        hung,      // le logiciel ne s'exécute plus, jusqu'à une réinitialisation
        resetting, // réinitialisation en cours, puis démarrage
        off,       // hors tension
    };

    Processor(const MonotonicClock& clock, ByteChannel& tx, ByteChannel& rx, Firmware firmware);

    // Appelé par l'ordonnanceur à chaque pas.
    void step(TrueTime now);

    // Vu par le logiciel.
    // Compteur monotone depuis le démarrage : il repart de zéro à chaque démarrage.
    [[nodiscard]] std::chrono::nanoseconds monotonicNow() const noexcept;

    // Numéro de démarrage, persistant, augmenté de 1 à chaque démarrage (ADR 0006 § 3).
    [[nodiscard]] std::uint32_t bootNumber() const noexcept { return bootNumber_; }

    void send(std::span<const std::byte> bytes);
    [[nodiscard]] std::vector<std::byte> receive();

    // Défauts, effectifs au temps du dernier pas.
    // Blocage : le logiciel cesse de s'exécuter ; le compteur continue d'avancer.
    void hang() noexcept;
    // Réinitialisation : le processeur redémarre après downtime ; les octets reçus pendant ce
    // temps sont perdus.
    void reset(std::chrono::nanoseconds downtime) noexcept;
    // Arrêt de l'oscillateur : le compteur monotone se fige, y compris après un redémarrage.
    void freezeClock() noexcept;
    // Mise hors tension, puis remise sous tension : le processeur démarre après bootTime.
    void powerOff() noexcept;
    void powerOn(std::chrono::nanoseconds bootTime = std::chrono::nanoseconds::zero()) noexcept;

    [[nodiscard]] State state() const noexcept { return state_; }

private:
    void boot();

    MonotonicClock clock_;
    ByteChannel* tx_;
    ByteChannel* rx_;
    Firmware firmware_;
    State state_ = State::resetting;
    TrueTime now_{};
    TrueTime bootAt_{};
    TrueTime resumeAt_{};
    std::optional<TrueTime> frozenAt_;
    std::uint32_t bootNumber_ = 0;
};

} // namespace clepsydre::sim
