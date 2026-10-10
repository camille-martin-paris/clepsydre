// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <chrono>
#include <cstdint>

export module clepsydre.sim.clock;

// Horloges simulées des processeurs (ADR 0006 § Horloge et datation) : chaque processeur lit
// son propre compteur monotone, sur son propre oscillateur, et le processeur de commande lit
// en plus l'heure civile de son horloge temps réel. Aucune base de temps n'est partagée.

export namespace clepsydre::sim {

// Temps du monde simulé, commun à l'ordonnanceur, au modèle physique et à la liaison.
// C'est la référence de la simulation : aucun logiciel simulé n'y a accès.
using TrueTime = std::chrono::nanoseconds;

// Heure civile en UTC, à la résolution de 1 s (SYS-REQ-035).
using CivilTime = std::chrono::sys_seconds;

// Borne de l'écart de fréquence, en parties par milliard : un oscillateur simulé va de presque
// arrêté à presque deux fois trop vite. Elle garantit le calcul de la dérive sans débordement,
// quelle que soit la durée. Les horloges ramènent à cette borne tout écart qui la dépasse.
inline constexpr std::int64_t maxDriftPpb = 999'999'999;

// Écart accumulé sur elapsed par un oscillateur dont la fréquence s'écarte de driftPpb parties
// par milliard, driftPpb étant d'abord ramené dans [-maxDriftPpb, maxDriftPpb] ; calculé en
// entiers, sans débordement pour toute durée représentable.
[[nodiscard]] std::chrono::nanoseconds driftOver(std::chrono::nanoseconds elapsed,
                                                 std::int64_t driftPpb) noexcept;

// Compteur monotone d'un processeur, sur son oscillateur : il sert à tous les délais de sécurité
// (SW-REQ-018). Il n'est jamais réglé.
class MonotonicClock {
public:
    // offset : valeur du compteur au temps simulé nul ; driftPpb : écart de fréquence, ramené
    // dans [-maxDriftPpb, maxDriftPpb], de sorte que le compteur avance toujours ; tick : période
    // du compteur.
    MonotonicClock(std::chrono::nanoseconds offset, std::int64_t driftPpb,
                   std::chrono::nanoseconds tick = std::chrono::microseconds{1}) noexcept;

    // Valeur du compteur au temps simulé now, tronquée à la période du compteur.
    [[nodiscard]] std::chrono::nanoseconds read(TrueTime now) const noexcept;

    [[nodiscard]] std::int64_t driftPpb() const noexcept { return driftPpb_; }

private:
    std::chrono::nanoseconds offset_;
    std::int64_t driftPpb_;
    std::chrono::nanoseconds tick_;
};

// Lecture de l'horloge temps réel : l'heure et l'indicateur d'arrêt de l'oscillateur.
struct CivilReading {
    CivilTime time;
    bool oscillatorStopped = false;
};

// Horloge temps réel du processeur de commande, sauvegardée par sa propre réserve.
class CivilClock {
public:
    // L'horloge indique initial au temps simulé now ; driftPpb est ramené dans
    // [-maxDriftPpb, maxDriftPpb].
    CivilClock(TrueTime now, CivilTime initial, std::int64_t driftPpb) noexcept;

    [[nodiscard]] CivilReading read(TrueTime now) const noexcept;

    // Mise à l'heure par un technicien : l'oscillateur repart et l'indicateur d'arrêt est effacé.
    void set(TrueTime now, CivilTime time) noexcept;
    // Saut de l'heure, que le logiciel doit détecter (ADR 0006 § Heure indisponible ou
    // incohérente).
    void jump(TrueTime now, std::chrono::seconds delta) noexcept;
    // Réserve épuisée : l'heure se fige et l'indicateur d'arrêt se lève.
    void stopOscillator(TrueTime now) noexcept;

private:
    [[nodiscard]] std::chrono::sys_time<std::chrono::nanoseconds>
    valueAt(TrueTime now) const noexcept;

    TrueTime anchor_;
    std::chrono::sys_time<std::chrono::nanoseconds> anchorValue_;
    std::int64_t driftPpb_;
    bool stopped_ = false;
};

} // namespace clepsydre::sim
