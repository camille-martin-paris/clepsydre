// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <chrono>
#include <cstdint>
#include <optional>

export module clepsydre.control.drive;

import clepsydre.control.infusion;

// Asservissement du moteur du pousse-seringue (#26, ADR 0002) : rattrapage des jeux avant la
// perfusion (CTRL-027, SYS-REQ-048), puis commande en position du moteur pas à pas, qui fait suivre
// au pousseur l'intégrale exacte de la consigne de débit selon la section du modèle de seringue
// (CTRL-002). Conception détaillée : docs/design/drive.md.
//
// Arithmétique entière, sans dérive ; ni allocation dynamique ni exception. Un défaut est
// verrouillé : le pousseur s'arrête et le défaut reste signalé jusqu'à la fin de l'entraînement.

export namespace clepsydre::control {

// Force mesurée sur le pousseur, en millinewtons.
struct Force {
    std::int32_t millinewtons = 0;
};

// Mécanisme d'entraînement (#52) : valeurs de conception, communes à toutes les seringues.
struct DriveMechanism {
    std::uint32_t screwLeadNanometres = 0; // avance de l'écrou par tour de vis
    std::uint32_t stepsPerRevolution = 0;  // pas moteur par tour, micro-pas compris
    // Vitesse maximale du moteur, en pas par seconde : celle que le matériel borne
    // indépendamment du logiciel (SYS-REQ-043). Le débit maximal de la pompe doit tenir dessous.
    std::uint32_t maxStepRate = 0;
    std::uint32_t takeUpStepRate = 0;   // vitesse du rattrapage des jeux, en pas par seconde
    std::chrono::milliseconds period{}; // période de la boucle de commande
};

// Caractéristiques d'entraînement d'un modèle de la liste des seringues vérifiées (SYS-REQ-047),
// mesurées sur banc pour chaque modèle (#73).
struct SyringeDrive {
    // Volume chassé par millimètre de course du piston, en nanolitres : la section intérieure.
    std::uint32_t strokeVolume = 0;
    // Force qui termine le rattrapage : jeu fermé et joint précontraint, mais sous le frottement
    // statique du joint, pour que le rattrapage ne délivre rien.
    Force takeUpForce;
    // Course maximale du rattrapage, en micromètres : jeu, compression du joint jusqu'à
    // takeUpForce, et marge. Elle borne le volume qu'un capteur de force défaillant laisserait
    // pousser.
    std::uint32_t takeUpTravel = 0;
};

enum class DrivePhase : std::uint8_t {
    TakingUp = 1, // rattrapage des jeux : avance jusqu'à la force de fin de rattrapage
    Tracking = 2, // perfusion : le pousseur suit l'intégrale de la consigne
    Faulted = 3,  // défaut verrouillé : aucun pas
};

// Cause d'un défaut. Les valeurs sont stables : elles seront journalisées.
enum class DriveFault : std::uint8_t {
    InvalidParameters = 1,    // mécanisme ou seringue hors des plages admises
    InvalidStep = 2,          // intervalle négatif ou supérieur à maxStep
    SetpointAboveMaximum = 3, // consigne au-delà du débit maximal de la pompe
    ForceUnavailable = 4,     // aucune mesure de force pendant le rattrapage
    TakeUpTravelExceeded = 5, // course maximale du rattrapage atteinte sans la force attendue
    CounterInconsistent = 6,  // compteur de pas qui recule ou dépasse les pas demandés
    StepsNotEmitted = 7,      // pas demandés non émis dans la période
};

// Commande d'une période : nombre de pas que le générateur doit émettre, régulièrement espacés,
// pendant la période suivante. Avant, jamais en arrière : le recul relève de la décompression
// (#27).
struct DriveCommand {
    std::uint32_t steps = 0;
    std::optional<DriveFault> fault; // défaut verrouillé : steps est nul
};

// Plages admises : elles garantissent les calculs en entiers de 64 bits sans débordement, et que
// le débit maximal de la pompe tient sous la vitesse maximale du moteur.
[[nodiscard]] bool validDriveParameters(const DriveMechanism& mechanism,
                                        const SyringeDrive& syringe) noexcept;

class Drive {
public:
    // Intervalle maximal entre deux appels à update() ; au-delà, la boucle de commande a manqué
    // ses échéances.
    static constexpr std::chrono::milliseconds maxStep{100};

    // stepCount : compteur des pas émis par le générateur, lu au démarrage. L'entraînement
    // commence par le rattrapage des jeux.
    Drive(const DriveMechanism& mechanism, const SyringeDrive& syringe,
          std::int64_t stepCount) noexcept;

    // À appeler à chaque période, avec l'intervalle écoulé depuis l'appel précédent, la consigne
    // du mode actif déjà vérifiée contre sa borne (Delivery), le compteur de pas émis et la force
    // mesurée. Pendant le rattrapage, la consigne est ignorée : la perfusion, et son décompte,
    // commencent quand phase() passe à Tracking.
    [[nodiscard]] DriveCommand update(std::chrono::milliseconds elapsed, Rate setpoint,
                                      std::int64_t stepCount, std::optional<Force> force) noexcept;

    [[nodiscard]] DrivePhase phase() const noexcept { return phase_; }

    [[nodiscard]] std::optional<DriveFault> fault() const noexcept { return fault_; }

    // Pas demandés depuis la fin du rattrapage : la position commandée du pousseur.
    [[nodiscard]] std::uint64_t trackedSteps() const noexcept { return tracked_; }

private:
    DriveCommand fail(DriveFault fault) noexcept;
    DriveCommand takeUp(std::optional<Force> force) noexcept;
    DriveCommand track(std::chrono::milliseconds elapsed, Rate setpoint) noexcept;

    DriveMechanism mechanism_;
    SyringeDrive syringe_;
    DrivePhase phase_ = DrivePhase::TakingUp;
    std::optional<DriveFault> fault_;
    std::uint64_t stepDivisor_ = 0;   // pas = débit × ms × stepNumerator_ / stepDivisor_
    std::uint64_t stepNumerator_ = 0; // facteur commun au débit et à l'intervalle
    std::uint64_t remainder_ = 0;     // reste de la division, reporté au cycle suivant
    std::int64_t maxTakeUpSteps_ = 0;
    std::uint32_t takeUpStepsPerPeriod_ = 0;
    std::int64_t start_ = 0;     // compteur au début du rattrapage
    std::int64_t requested_ = 0; // position demandée au générateur : compteur attendu
    std::int64_t counted_ = 0;   // dernier compteur lu
    std::uint32_t lastSteps_ = 0;
    std::uint64_t tracked_ = 0;
};

} // namespace clepsydre::control
