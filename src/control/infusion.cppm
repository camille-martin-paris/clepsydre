// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <chrono>
#include <compare>
#include <cstdint>
#include <expected>
#include <optional>

export module clepsydre.control.infusion;

import clepsydre.control.state_machine;

// Modes de perfusion et bolus borné (#25) : validation de la programmation et du bolus
// (SW-REQ-002), borne de consigne du mode actif (SW-REQ-006), décompte des volumes en arithmétique
// entière (SW-REQ-005). Conception détaillée : docs/design/infusion-modes.md.
//
// Une valeur hors limites est refusée avec son motif, jamais corrigée. Seul le débit calculé en
// volume sur durée est arrondi à la résolution, par défaut, et l'arrondi est signalé. Ni
// allocation dynamique ni exception.

export namespace clepsydre::control {

// Volume en microlitres : la résolution programmable, 0,1 mL, en est un multiple.
struct Volume {
    std::uint32_t microlitres = 0;
};

// Débit en microlitres par heure : la résolution programmable, 0,1 mL/h, en est un multiple.
struct Rate {
    std::uint32_t microlitresPerHour = 0;
};

[[nodiscard]] constexpr bool operator==(Volume a, Volume b) noexcept {
    return a.microlitres == b.microlitres;
}

[[nodiscard]] constexpr std::strong_ordering operator<=>(Volume a, Volume b) noexcept {
    return a.microlitres <=> b.microlitres;
}

[[nodiscard]] constexpr bool operator==(Rate a, Rate b) noexcept {
    return a.microlitresPerHour == b.microlitresPerHour;
}

[[nodiscard]] constexpr std::strong_ordering operator<=>(Rate a, Rate b) noexcept {
    return a.microlitresPerHour <=> b.microlitresPerHour;
}

inline constexpr Volume volumeResolution{.microlitres = 100};     // SYS-REQ-031
inline constexpr Rate rateResolution{.microlitresPerHour = 100};  // SYS-REQ-030
inline constexpr Rate pumpMinRate{.microlitresPerHour = 100};     // 0,1 mL/h
inline constexpr Rate pumpMaxRate{.microlitresPerHour = 200'000}; // 200 mL/h
inline constexpr Rate maxKvoRate{.microlitresPerHour = 5'000};    // 5 mL/h (SYS-REQ-032)

enum class Mode : std::uint8_t {
    ContinuousRate = 1,     // débit programmé, jusqu'au volume à perfuser
    VolumeOverDuration = 2, // volume sur une durée : le débit en est calculé
};

// Fin de perfusion prévue par la bibliothèque (SYS-REQ-032).
enum class EndOfInfusion : std::uint8_t {
    Stop = 1,
    Kvo = 2, // maintien de veine ouverte
};

// Position d'une valeur acceptée par rapport aux limites souples (SYS-REQ-012) : au-delà, une
// confirmation spécifique est exigée (#36, #37).
enum class SoftLimit : std::uint8_t {
    Within,
    BelowMinimum,
    AboveMaximum,
};

// Motif d'un refus, affiché au soignant. Les valeurs sont stables : elles seront journalisées.
enum class Refusal : std::uint8_t {
    InvalidLimits = 1, // bibliothèque ou seringue incohérente : tout est refusé
    InvalidMode = 2,
    VolumeResolution = 3,         // volume non multiple de 0,1 mL
    VolumeBelowMinimum = 4,       // moins de 0,1 mL
    VolumeAboveSyringe = 5,       // plus que le volume utile de la seringue
    DurationMissing = 6,          // durée nulle ou négative
    RateResolution = 7,           // débit non multiple de 0,1 mL/h
    RateBelowPumpMinimum = 8,     // moins de 0,1 mL/h
    RateAbovePumpMaximum = 9,     // plus de 200 mL/h
    RateAboveSyringeMaximum = 10, // plus que le débit maximal de la seringue
    RateBelowHardLimit = 11,      // sous la limite dure du médicament
    RateAboveHardLimit = 12,      // au-dessus de la limite dure du médicament
    BolusNotAllowed = 13,         // volume sur durée, ou bolus interdit pour le médicament
    BolusVolumeAboveHardLimit = 14,
    BolusRateAboveHardLimit = 15,
};

// Modèle de seringue choisi dans la liste des seringues vérifiées (SYS-REQ-047).
struct Syringe {
    Volume usableVolume;
    Rate maxRate;
};

// Limites du médicament, exprimées en débit. La conversion des limites de dose de la bibliothèque
// en débit, selon la concentration, relève de #37.
struct DrugLimits {
    Rate hardMinRate;
    Rate softMinRate;
    Rate softMaxRate;
    Rate hardMaxRate;
    Volume softMaxBolusVolume;
    Volume hardMaxBolusVolume; // nul : bolus interdit pour ce médicament
    Rate hardMaxBolusRate;
    EndOfInfusion endOfInfusion = EndOfInfusion::Stop;
    Rate kvoRate; // utilisé seulement si endOfInfusion vaut Kvo
};

// Programmation saisie. Le débit n'est lu qu'en débit continu, la durée qu'en volume sur durée ;
// le volume à perfuser est exigé dans les deux modes.
struct ProgramRequest {
    Mode mode = Mode::ContinuousRate;
    Rate rate;
    Volume volume;
    std::chrono::minutes duration{0};
};

// Programmation acceptée, à afficher au récapitulatif (SYS-REQ-011).
struct Program {
    Mode mode = Mode::ContinuousRate;
    Rate rate;                        // programmé, ou calculé et arrondi par défaut
    Volume volume;                    // volume à perfuser
    std::chrono::seconds duration{0}; // durée de la perfusion au débit retenu, arrondie par excès
    bool rateRoundedDown = false;     // volume sur durée : le débit calculé a été arrondi
    SoftLimit rateSoftLimit = SoftLimit::Within;
    EndOfInfusion endOfInfusion = EndOfInfusion::Stop;
    Rate kvoRate; // celui de la bibliothèque plafonné par le débit, nul si arrêt en fin
};

struct BolusRequest {
    Volume volume;
    Rate rate;
};

struct Bolus {
    Volume volume;
    Rate rate;
    bool volumeAboveSoftLimit = false;
};

// Cohérence de la bibliothèque et de la seringue : plages et résolutions, limites dures englobant
// les limites souples, maintien de veine ouverte d'au plus 5 mL/h.
[[nodiscard]] bool validLimits(const DrugLimits& limits, const Syringe& syringe) noexcept;

// SW-REQ-002 : vérifie chaque valeur, refuse la première hors limites avec son motif.
[[nodiscard]] std::expected<Program, Refusal> validateProgram(const ProgramRequest& request,
                                                              const DrugLimits& limits,
                                                              const Syringe& syringe) noexcept;

// SYS-REQ-014, SYS-REQ-029 : bolus borné par les limites du médicament, en débit continu
// seulement.
[[nodiscard]] std::expected<Bolus, Refusal> validateBolus(const BolusRequest& request,
                                                          const Program& program,
                                                          const DrugLimits& limits,
                                                          const Syringe& syringe) noexcept;

// SW-REQ-006 : borne de consigne du mode actif, plafonnée par le débit maximal de la pompe. Nulle
// hors des modes de perfusion ; la borne de la purge viendra avec son unité.
[[nodiscard]] Rate setpointBound(ActuatorMode mode, const Program& program,
                                 const DrugLimits& limits) noexcept;

// Décompte des volumes d'une perfusion acceptée. Le volume est l'intégrale exacte de la consigne
// sur le temps, en microlitres-millisecondes par heure : aucune erreur d'arrondi ne se cumule
// (SW-REQ-005). Les volumes lus sont arrondis par défaut au microlitre.
class Delivery {
public:
    // Intervalle maximal entre deux appels à advance() ; au-delà, la boucle de commande a manqué
    // ses échéances.
    static constexpr std::chrono::milliseconds maxStep{60'000};

    Delivery(const Program& program, const DrugLimits& limits) noexcept;

    // À appeler quand la machine accepte BolusRequested, avec le bolus validé.
    void startBolus(const Bolus& bolus) noexcept;

    // Consigne du mode actif : nulle une fois le volume visé atteint.
    [[nodiscard]] Rate setpoint(ActuatorMode mode) const noexcept;

    // Délivre la consigne de mode pendant elapsed, sans jamais dépasser le volume à perfuser ni
    // celui du bolus. Renvoie l'événement à appliquer à la machine : fin de volume (prioritaire),
    // fin du bolus, ou DangerDetected sur une consigne hors bornes, un bolus inconnu, un mode ou un
    // intervalle invalide.
    [[nodiscard]] std::optional<Event> advance(ActuatorMode mode,
                                               std::chrono::milliseconds elapsed) noexcept;

    [[nodiscard]] Volume infused() const noexcept;   // bolus compris, maintien de veine exclu
    [[nodiscard]] Volume remaining() const noexcept; // volume à perfuser moins infused()
    [[nodiscard]] Volume bolusDelivered() const noexcept;
    [[nodiscard]] Volume kvoDelivered() const noexcept;

private:
    Program program_;
    DrugLimits limits_;
    std::optional<Bolus> bolus_;
    std::uint64_t infused_ = 0; // en µL·ms/h, comme les deux suivants
    std::uint64_t bolusDelivered_ = 0;
    std::uint64_t kvoDelivered_ = 0;
};

} // namespace clepsydre::control
