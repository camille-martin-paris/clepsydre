// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <chrono>
#include <cstdint>
#include <expected>
#include <optional>

export module clepsydre.control.occlusion;

import clepsydre.control.drive;

// Détection d'occlusion aval et décompression du pousse-seringue (#27, ADR 0002) : l'occlusion est
// détectée par la force sur le pousseur, au seuil choisi dans les bornes du modèle de seringue
// (CTRL-007, SYS-REQ-008) ; le pousseur recule ensuite d'une course calculée selon la force à la
// détection, pour limiter le bolus libéré à la levée de l'occlusion (CTRL-008, SYS-REQ-009).
// Conception détaillée : docs/design/occlusion.md.
//
// Arithmétique entière ; ni allocation dynamique ni exception. Une occlusion détectée et un défaut
// sont verrouillés : un nouveau surveillant est construit pour une nouvelle perfusion.

export namespace clepsydre::control {

// Réglages d'occlusion d'un modèle de la liste des seringues vérifiées (SYS-REQ-047), établis sur
// le banc d'occlusion pour chaque modèle (#74).
struct OcclusionSettings {
    // Bornes du seuil réglable, et seuil par défaut, celui auquel SYS-REQ-008 fixe les délais
    // d'alarme. La borne basse dépasse la force de fin du rattrapage et la force de marche au débit
    // maximal ; la borne haute est celle à laquelle SYS-REQ-009 borne le bolus.
    Force minThreshold;
    Force defaultThreshold;
    Force maxThreshold;
    // Durée pendant laquelle la force doit rester au seuil ou au-delà : filtre les pointes isolées
    // du capteur.
    std::chrono::milliseconds confirmation{};
    // Course de recul par newton de force à la détection, en nanomètres : compliance vue du
    // pousseur, celle du joint et du corps de la seringue.
    std::uint32_t backoffPerNewton = 0;
    // Course maximale du recul, en micromètres : elle borne l'aspiration qu'un recul excessif
    // provoquerait sur un piston capturé.
    std::uint32_t maxBackoffTravel = 0;
};

// Motif du refus d'un seuil, affiché au soignant. Les valeurs sont stables : elles seront
// journalisées.
enum class ThresholdRefusal : std::uint8_t {
    InvalidSettings = 1, // réglages du modèle hors des plages admises : tout seuil est refusé
    BelowMinimum = 2,
    AboveMaximum = 3,
};

enum class OcclusionPhase : std::uint8_t {
    Watching = 1,     // surveillance de la force
    Stopping = 2,     // occlusion détectée : une période sans pas, le temps que le moteur s'arrête
    BackingOff = 3,   // recul du pousseur
    Decompressed = 4, // recul achevé : le moteur reste arrêté
    Faulted = 5,      // défaut verrouillé : aucun pas
};

// Cause d'un défaut. Les valeurs sont stables : elles seront journalisées.
enum class OcclusionFault : std::uint8_t {
    InvalidParameters = 1,   // réglages ou seuil hors des plages admises
    InvalidStep = 2,         // intervalle négatif ou supérieur à maxStep
    ForceUnavailable = 3,    // aucune mesure de force pendant la surveillance
    CounterInconsistent = 4, // compteur qui avance, ou recule au-delà des pas demandés
    StepsNotEmitted = 5,     // pas de recul demandés non émis dans la période
    CounterOutOfRange = 6,   // compteur sans place, sous l'origine, pour toute la course de recul
};

// Commande d'une période.
struct OcclusionCommand {
    // Occlusion détectée : l'entraînement ne doit plus avancer, et l'appelant applique
    // DangerDetected à la machine à états. Reste vrai jusqu'à la fin du surveillant.
    bool occlusion = false;
    // Pas de recul que le générateur doit émettre, régulièrement espacés, pendant la période
    // suivante.
    std::uint32_t backoffSteps = 0;
    std::optional<OcclusionFault> fault; // défaut verrouillé : backoffSteps est nul
};

// Plages admises : paramètres d'entraînement valides, seuils ordonnés au-dessus de la force de fin
// du rattrapage, course maximale du recul d'au moins un pas.
[[nodiscard]] bool validOcclusionSettings(const DriveMechanism& mechanism,
                                          const SyringeDrive& syringe,
                                          const OcclusionSettings& settings) noexcept;

// Seuil choisi par le soignant, ou par la configuration : refusé hors des bornes, jamais corrigé.
[[nodiscard]] std::expected<Force, ThresholdRefusal>
selectThreshold(Force requested, const DriveMechanism& mechanism, const SyringeDrive& syringe,
                const OcclusionSettings& settings) noexcept;

class OcclusionMonitor {
public:
    // Intervalle maximal entre deux appels à update(), celui de l'entraînement.
    static constexpr std::chrono::milliseconds maxStep = Drive::maxStep;

    // threshold : seuil retenu par selectThreshold(). Un seuil ou des réglages refusés verrouillent
    // le surveillant en défaut.
    OcclusionMonitor(const DriveMechanism& mechanism, const SyringeDrive& syringe,
                     const OcclusionSettings& settings, Force threshold) noexcept;

    // À appeler à chaque période, avec l'intervalle écoulé depuis l'appel précédent, le compteur de
    // pas émis et la force mesurée. Pendant la surveillance, l'entraînement commande le moteur et
    // contrôle le compteur ; dès la détection, le surveillant le remplace.
    [[nodiscard]] OcclusionCommand update(std::chrono::milliseconds elapsed, std::int64_t stepCount,
                                          std::optional<Force> force) noexcept;

    [[nodiscard]] OcclusionPhase phase() const noexcept { return phase_; }

    [[nodiscard]] std::optional<OcclusionFault> fault() const noexcept { return fault_; }

    [[nodiscard]] Force threshold() const noexcept { return threshold_; }

    // Force mesurée à la confirmation de l'occlusion : celle d'où la course de recul est calculée.
    [[nodiscard]] std::optional<Force> detectionForce() const noexcept { return detection_; }

    // Course de recul calculée, en pas, et pas de recul déjà demandés.
    [[nodiscard]] std::uint32_t backoffTarget() const noexcept { return target_; }

    [[nodiscard]] std::uint32_t backoffRequested() const noexcept { return backedOff_; }

private:
    OcclusionCommand fail(OcclusionFault fault) noexcept;
    OcclusionCommand watch(std::chrono::milliseconds elapsed, std::optional<Force> force) noexcept;
    OcclusionCommand startBackoff(std::int64_t stepCount) noexcept;
    OcclusionCommand backOff() noexcept;

    DriveMechanism mechanism_;
    OcclusionSettings settings_;
    Force threshold_;
    OcclusionPhase phase_ = OcclusionPhase::Watching;
    std::optional<OcclusionFault> fault_;
    std::optional<Force> detection_;
    std::optional<std::chrono::milliseconds> above_; // durée continue au seuil ou au-delà
    std::uint32_t stepsPerPeriod_ = 0;               // recul à la vitesse maximale du moteur
    std::uint32_t target_ = 0;
    std::uint32_t backedOff_ = 0;
    std::int64_t requested_ = 0; // position demandée au générateur : compteur attendu
    std::int64_t counted_ = 0;   // dernier compteur lu
    std::uint32_t lastSteps_ = 0;
};

} // namespace clepsydre::control
