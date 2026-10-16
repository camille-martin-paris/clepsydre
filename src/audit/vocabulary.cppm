// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

export module clepsydre.audit.vocabulary;

export import mddlog.core.auditevent;

// Vocabulaire fermé des événements d'audit (#42, #136) : actions, catégories, phases admises,
// producteurs et références, tels que les fixe docs/event-vocabulary.md. Le code de commande et
// celui de la sécurité n'enregistrent que ces actions ; tests/audit_vocabulary_doc_test.cpp
// compare ces tables au document et au registre.
//
// Ni allocation dynamique ni exception : les noms des actions d'alarme sont composés à la
// compilation.

export namespace clepsydre::audit {

using mddlog::core::AuditCategory;
using mddlog::core::AuditPhase;

// Ensemble de phases admises, un bit par valeur de AuditPhase.
struct PhaseSet {
    std::uint8_t bits = 0;
};

[[nodiscard]] constexpr bool contains(PhaseSet set, AuditPhase phase) noexcept {
    return ((static_cast<unsigned>(set.bits) >> static_cast<unsigned>(phase)) & 1U) != 0;
}

[[nodiscard]] constexpr PhaseSet operator|(PhaseSet a, PhaseSet b) noexcept {
    return {.bits = static_cast<std::uint8_t>(a.bits | b.bits)};
}

inline constexpr PhaseSet requested{.bits = 1U << static_cast<unsigned>(AuditPhase::Requested)};
inline constexpr PhaseSet confirmed{.bits = 1U << static_cast<unsigned>(AuditPhase::Confirmed)};
inline constexpr PhaseSet executed{.bits = 1U << static_cast<unsigned>(AuditPhase::Executed)};
inline constexpr PhaseSet failed{.bits = 1U << static_cast<unsigned>(AuditPhase::Failed)};

// Processeur qui enregistre l'événement dans son propre flux (ADR 0005, ADR 0006).
enum class Producer : std::uint8_t {
    Command = 1, // processeur de commande
    Safety = 2,  // processeur de sécurité
};

struct ProducerSet {
    std::uint8_t bits = 0;
};

[[nodiscard]] constexpr bool contains(ProducerSet set, Producer producer) noexcept {
    return (set.bits & static_cast<std::uint8_t>(producer)) != 0;
}

inline constexpr ProducerSet command{.bits = static_cast<std::uint8_t>(Producer::Command)};
inline constexpr ProducerSet both{
    .bits = static_cast<std::uint8_t>(static_cast<unsigned>(Producer::Command) |
                                      static_cast<unsigned>(Producer::Safety))};
inline constexpr ProducerSet safety{.bits = static_cast<std::uint8_t>(Producer::Safety)};

// Nom du producteur dans les flux et les corrélations : sans accent (règle de mddlog).
[[nodiscard]] constexpr std::string_view producerName(Producer producer) noexcept {
    return producer == Producer::Safety ? "securite" : "commande";
}

// Acteurs (docs/event-vocabulary.md, « Acteurs »).
inline constexpr std::string_view operatorActor = "operateur";
inline constexpr std::string_view technicianActor = "role:technicien";

// Une action du vocabulaire. Les références sont vides quand le document n'en donne pas.
struct ActionSpec {
    std::string_view name;
    AuditCategory category = AuditCategory::Lifecycle;
    PhaseSet phases;
    ProducerSet producers;
    std::string_view requirement;
    std::string_view risk;
};

// Actions hors alarmes, dans l'ordre du document. Les valeurs ne sont pas enregistrées : seul le
// nom l'est.
enum class Action : std::uint8_t {
    TherapyProgram,
    TherapyProgramCancel,
    TherapyStart,
    TherapyPause,
    TherapyResume,
    TherapyStop,
    TherapyBolus,
    TherapyBolusEnd,
    TherapyPurge,
    TherapyPurgeEnd,
    TherapyEnd,
    TherapyRecovery,
    SyringeSelect,
    SafetySafeState,
    SafetySafeStateAcknowledge,
    SafetyOutOfService,
    FaultDrive,
    FaultOcclusionMonitor,
    FaultStateMachine,
    FaultSelfTest,
    FaultChannelMismatch,
    LinkLoss,
    DeviceBoot,
    DeviceSelfTest,
    DevicePrototypeWarning,
    DeviceShutdown,
    DevicePowerFail,
    PowerSource,
    PowerBatteryCapacity,
    ClockJump,
    ClockUnavailable,
    MaintenanceEnter,
    MaintenanceExit,
    MaintenanceFunctionalCheck,
    ConfigLibrary,
    ConfigSyringeList,
    ConfigSoftware,
    ConfigClock,
    ConfigSetting,
    ConfigOutOfService,
    ConfigReturnToService,
    AccessLogin,
    AccessLogout,
    AccessKeypadLock,
    AccessKeypadUnlock,
    JournalLoss,
    JournalStreamGap,
    JournalWitnessRefusal,
    JournalCrossCheck,
};

inline constexpr std::size_t actionCount = 49;

// Conditions d'alarme, dans l'ordre du catalogue de la spécification.
enum class AlarmCondition : std::uint8_t {
    Occlusion,
    Air,
    Syringe,
    SyringeEmpty,
    ActuatorIdle,
    Supervision,
    Technical,
    LinkLoss,
    Reservation,
    BatteryDepleted,
    ResetRecovery,
    InfusionEnd,
    BatteryLow,
    IdleReminder,
    InfusionEndPrealarm,
    SyringeNearEmpty,
    Clock,
    JournalWitness,
};

inline constexpr std::size_t alarmConditionCount = 18;

// Étapes d'une alarme : chacune est une action `alarm.<condition>.<étape>`.
enum class AlarmStep : std::uint8_t {
    Raise,
    Silence,
    Acknowledge,
    Clear,
};

inline constexpr std::size_t alarmStepCount = 4;

// Priorité d'une alarme (IEC 60601-1-8), enregistrée dans le détail de son déclenchement.
enum class Priority : std::uint8_t {
    High,
    Medium,
    Low,
    Info,
};

[[nodiscard]] constexpr std::string_view priorityName(Priority priority) noexcept {
    switch (priority) {
    case Priority::High:
        return "high";
    case Priority::Medium:
        return "medium";
    case Priority::Low:
        return "low";
    case Priority::Info:
        break;
    }
    return "info";
}

// Description d'une action, ou rien pour une valeur hors de l'énumération.
[[nodiscard]] std::optional<ActionSpec> spec(Action action) noexcept;
[[nodiscard]] std::optional<ActionSpec> spec(AlarmCondition condition, AlarmStep step) noexcept;

// Action du vocabulaire portant ce nom, alarmes comprises : un événement relu dont l'action n'y
// figure pas n'appartient pas au journal d'audit.
[[nodiscard]] std::optional<ActionSpec> find(std::string_view name) noexcept;

// Nombre total d'actions du vocabulaire, et l'action de rang index (0 à total - 1), alarmes après
// les autres : pour parcourir le vocabulaire entier.
inline constexpr std::size_t vocabularySize = actionCount + (alarmConditionCount * alarmStepCount);
[[nodiscard]] std::optional<ActionSpec> at(std::size_t index) noexcept;

} // namespace clepsydre::audit
