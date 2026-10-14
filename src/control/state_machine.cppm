// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <cstdint>

export module clepsydre.control.state_machine;

// Machine à états du logiciel de commande (SW-REQ-001, #24). Conception détaillée, table des
// transitions et justification : docs/design/state-machine.md.
//
// La machine est une fonction pure de l'état courant et d'un événement : elle ne commande rien
// elle-même. Les autres unités (actionneur, alarmes, journal, interface) lisent l'état et le
// verdict de chaque transition. Une demande non admise dans l'état courant est refusée sans
// changer d'état, et le verdict permet de la journaliser ; une incohérence interne (valeur d'état
// ou d'événement invalide) place la pompe en état sûr. Ni allocation dynamique ni exception.

export namespace clepsydre::control {

// États et sous-états. Les valeurs numériques sont stables : elles sont transmises dans le signal
// de vie de la liaison entre les processeurs et journalisées.
enum class State : std::uint8_t {
    Booting = 1,      // démarrage : autotests en cours
    Stopped = 2,      // arrêt : aucune programmation
    Programming = 3,  // programmation en cours de saisie
    Ready = 4,        // prête : programmation confirmée, perfusion non démarrée
    Purging = 5,      // purge (amorçage) avant le premier démarrage
    Infusing = 6,     // perfusion au débit programmé
    Bolus = 7,        // perfusion : bolus en cours
    Kvo = 8,          // perfusion : maintien de veine ouverte après la fin
    Paused = 9,       // pause d'une perfusion au débit programmé
    PausedKvo = 10,   // pause pendant le maintien de veine ouverte
    SafeState = 11,   // état sûr après une condition dangereuse
    Maintenance = 12, // maintenance par un technicien authentifié
    OutOfService = 13 // hors service : refuse toute perfusion jusqu'à la remise en service
};

inline constexpr std::uint8_t stateCount = 13;

// Les dix états de SW-REQ-001, sous-états regroupés : affichage et exigences.
enum class Phase : std::uint8_t {
    Booting,
    Stopped,
    Programming,
    Ready,
    Purging,
    Infusion, // Infusing, Bolus, Kvo
    Pause,    // Paused, PausedKvo
    SafeState,
    Maintenance,
    OutOfService,
};

// Mode de l'actionneur, auquel est attachée la borne de consigne (SW-REQ-006). Il change avec
// l'état, en une seule transition : aucun état intermédiaire ne mêle deux modes.
enum class ActuatorMode : std::uint8_t {
    None, // toute consigne non nulle est hors bornes
    Purge,
    Infusion,
    Bolus,
    Kvo,
};

enum class Event : std::uint8_t {
    SelfTestsPassed = 1,        // autotests du démarrage réussis
    SelfTestsFailed = 2,        // autotests du démarrage en échec (SW-REQ-010)
    ProgrammingStarted = 3,     // le soignant commence ou modifie une programmation
    ProgrammingConfirmed = 4,   // programmation confirmée et acceptée par le canal de sécurité
    ProgrammingCancelled = 5,   // programmation abandonnée
    StartRequested = 6,         // Démarrer : démarrage ou reprise
    PauseRequested = 7,         // Pause
    StopRequested = 8,          // Arrêt
    BolusRequested = 9,         // bolus demandé et confirmé
    BolusEnded = 10,            // volume du bolus délivré
    PurgeRequested = 11,        // purge demandée, ligne non raccordée établie (SYS-REQ-028)
    PurgeEnded = 12,            // fin de la purge : volume atteint ou condition perdue
    VolumeReached = 13,         // volume à perfuser atteint, arrêt prévu par la bibliothèque
    VolumeReachedKvo = 14,      // volume à perfuser atteint, maintien de veine ouverte prévu
    SyringeEmpty = 15,          // seringue vide (SYS-REQ-032)
    DangerDetected = 16,        // condition dangereuse (SYS-REQ-002) : perte de lien, occlusion...
    SafeStateAcknowledged = 17, // état sûr acquitté, condition levée
    MaintenanceEntered = 18,    // technicien authentifié
    MaintenanceExited = 19,
    TakenOutOfService = 20, // mise hors service par un technicien (SYS-REQ-041)
    ReturnedToService = 21, // remise en service par un technicien : nouveaux autotests
    FatalFault = 22,        // défaillance qui exige la mise hors service (SYS-REQ-046, 053)
};

inline constexpr std::uint8_t eventCount = 22;

enum class Verdict : std::uint8_t {
    Accepted,      // transition prévue, éventuellement vers le même état
    Refused,       // demande non admise dans l'état : état inchangé, à journaliser
    InternalFault, // état ou événement invalide : état sûr, ou hors service
};

struct Transition {
    State from;
    Event event;
    State to;
    Verdict verdict;
};

// Transition depuis from sur event. Fonction totale : toute valeur, même invalide, a un résultat.
[[nodiscard]] Transition transition(State from, Event event) noexcept;

[[nodiscard]] Phase phase(State state) noexcept;
[[nodiscard]] ActuatorMode actuatorMode(State state) noexcept;

class StateMachine {
public:
    // Applique event et renvoie la transition, à journaliser par l'appelant.
    Transition apply(Event event) noexcept;

    [[nodiscard]] State state() const noexcept { return state_; }

private:
    State state_ = State::Booting;
};

} // namespace clepsydre::control
