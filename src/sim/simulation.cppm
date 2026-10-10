// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <chrono>
#include <functional>
#include <vector>

export module clepsydre.sim.simulation;

export import clepsydre.sim.clock;

// Ordonnanceur du monde simulé : modèle physique, processeurs et liaison avancent sur le même
// temps simulé, par pas fixes.

export namespace clepsydre::sim {

// À chaque pas, le temps simulé avance d'un quantum, puis chaque participant est appelé dans
// l'ordre d'inscription pour avancer jusqu'au nouveau temps. L'ordre fixe rend la simulation
// déterministe : les mêmes participants donnent les mêmes résultats.
class Simulation {
public:
    // now : nouveau temps simulé ; dt : durée écoulée depuis l'appel précédent.
    using Participant = std::function<void(TrueTime now, std::chrono::nanoseconds dt)>;

    // Un quantum de 1 ms au plus est recommandé pour le modèle physique ; un processeur ne
    // réagit qu'aux frontières des quanta.
    explicit Simulation(std::chrono::nanoseconds quantum = std::chrono::milliseconds{1}) noexcept;

    void add(Participant participant);
    // Avance de duration par pas d'un quantum au plus.
    void run(std::chrono::nanoseconds duration);

    [[nodiscard]] TrueTime now() const noexcept { return now_; }

    [[nodiscard]] std::chrono::nanoseconds quantum() const noexcept { return quantum_; }

private:
    std::chrono::nanoseconds quantum_;
    TrueTime now_{};
    std::vector<Participant> participants_;
};

} // namespace clepsydre::sim
