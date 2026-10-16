// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

module clepsydre.audit.vocabulary;

namespace clepsydre::audit {

namespace {

using enum AuditCategory;

constexpr PhaseSet rcef = requested | confirmed | executed | failed;
constexpr PhaseSet ref = requested | executed | failed;
constexpr PhaseSet re = requested | executed;
constexpr PhaseSet ef = executed | failed;

constexpr ActionSpec act(std::string_view name, AuditCategory category, PhaseSet phases,
                         ProducerSet producers, std::string_view requirement,
                         std::string_view risk) {
    return {.name = name,
            .category = category,
            .phases = phases,
            .producers = producers,
            .requirement = requirement,
            .risk = risk};
}

// Dans l'ordre de l'énumération Action.
constexpr std::array<ActionSpec, actionCount> actions{
    // Thérapie
    act("therapy.program", Operator, rcef, both, "SYS-REQ-011", "RISK-007"),
    act("therapy.program-cancel", Operator, ref, command, "SW-REQ-001", ""),
    act("therapy.start", Operator, rcef, both, "SYS-REQ-044", "RISK-001"),
    act("therapy.pause", Operator, ref, both, "SYS-REQ-044", "RISK-015"),
    act("therapy.resume", Operator, rcef, both, "SYS-REQ-044", "RISK-015"),
    act("therapy.stop", Operator, ref, both, "SYS-REQ-044", "RISK-015"),
    act("therapy.bolus", Operator, rcef, both, "SYS-REQ-014", "RISK-009"),
    act("therapy.bolus-end", Operator, executed, command, "SYS-REQ-014", "RISK-009"),
    act("therapy.purge", Operator, ref, command, "SYS-REQ-028", "RISK-009"),
    act("therapy.purge-end", Operator, executed, command, "SYS-REQ-028", ""),
    act("therapy.end", Lifecycle, executed, command, "SYS-REQ-032", "RISK-002"),
    act("therapy.recovery", Operator, rcef, both, "SYS-REQ-038", "RISK-010"),
    act("syringe.select", Operator, ref, command, "SYS-REQ-047", "RISK-001"),
    // Sûreté de fonctionnement
    act("safety.safe-state", RiskControl, executed, both, "SYS-REQ-002", "RISK-001"),
    act("safety.safe-state-acknowledge", RiskControl, ref, command, "SW-REQ-001", "RISK-013"),
    act("safety.out-of-service", RiskControl, executed, both, "SW-REQ-001", "RISK-012"),
    act("fault.drive", RiskControl, executed, command, "SYS-REQ-002", "RISK-012"),
    act("fault.occlusion-monitor", RiskControl, executed, command, "SYS-REQ-002", "RISK-005"),
    act("fault.state-machine", RiskControl, executed, command, "SW-REQ-001", "RISK-012"),
    act("fault.self-test", RiskControl, executed, both, "SW-REQ-011", "RISK-012"),
    act("fault.channel-mismatch", RiskControl, executed, both, "SW-REQ-013", "RISK-001"),
    act("link.loss", RiskControl, executed, both, "SYS-REQ-053", "RISK-012"),
    // Cycle de vie
    act("device.boot", Lifecycle, executed, both, "SW-REQ-020", "RISK-011"),
    act("device.self-test", Lifecycle, ef, both, "SW-REQ-010", "RISK-012"),
    act("device.prototype-warning", Operator, executed, command, "SYS-REQ-027", "RISK-021"),
    act("device.shutdown", Lifecycle, re, command, "SYS-REQ-016", "RISK-010"),
    act("device.power-fail", Lifecycle, executed, both, "SYS-REQ-050", "RISK-011"),
    act("power.source", Lifecycle, executed, command, "SYS-REQ-015", "RISK-010"),
    act("power.battery-capacity", Lifecycle, executed, command, "SYS-REQ-049", "RISK-010"),
    act("clock.jump", Lifecycle, executed, command, "SYS-REQ-054", "RISK-011"),
    act("clock.unavailable", Lifecycle, executed, command, "SYS-REQ-054", "RISK-011"),
    act("maintenance.enter", Lifecycle, ref, command, "SYS-REQ-023", "RISK-015"),
    act("maintenance.exit", Lifecycle, re, command, "SYS-REQ-023", ""),
    act("maintenance.functional-check", Lifecycle, ref, command, "SYS-REQ-041", "RISK-012"),
    // Configuration
    act("config.library", Configuration, ref, command, "SW-REQ-014", "RISK-019"),
    act("config.syringe-list", Configuration, ref, command, "SYS-REQ-047", "RISK-001"),
    act("config.software", Configuration, ref, command, "SW-REQ-015", "RISK-020"),
    act("config.clock", Configuration, ref, command, "SYS-REQ-035", "RISK-011"),
    act("config.setting", Configuration, ref, command, "SYS-REQ-023", "RISK-015"),
    act("config.out-of-service", Configuration, ref, command, "SYS-REQ-041", "RISK-012"),
    act("config.return-to-service", Configuration, ref, command, "SYS-REQ-041", "RISK-012"),
    // Accès
    act("access.login", Access, ref, command, "SW-REQ-016", "RISK-015"),
    act("access.logout", Access, executed, command, "SW-REQ-016", "RISK-015"),
    act("access.keypad-lock", Access, ref, command, "SYS-REQ-023", "RISK-015"),
    act("access.keypad-unlock", Access, ref, command, "SYS-REQ-023", "RISK-015"),
    // Journal
    act("journal.loss", RiskControl, executed, both, "SW-REQ-008", "RISK-011"),
    act("journal.stream-gap", RiskControl, executed, both, "SW-REQ-021", "RISK-011"),
    act("journal.witness-refusal", RiskControl, executed, safety, "SYS-REQ-052", "RISK-011"),
    act("journal.cross-check", RiskControl, ef, both, "SYS-REQ-052", "RISK-011"),
};

struct Condition {
    std::string_view name;
    std::string_view requirement;
    std::string_view risk;
};

// Dans l'ordre de l'énumération AlarmCondition.
constexpr std::array<Condition, alarmConditionCount> conditions{{
    {.name = "occlusion", .requirement = "SYS-REQ-008", .risk = "RISK-005"},
    {.name = "air", .requirement = "SYS-REQ-007", .risk = "RISK-004"},
    {.name = "syringe", .requirement = "SYS-REQ-006", .risk = "RISK-003"},
    {.name = "syringe-empty", .requirement = "SYS-REQ-032", .risk = "RISK-002"},
    {.name = "actuator-idle", .requirement = "SYS-REQ-004", .risk = "RISK-002"},
    {.name = "supervision", .requirement = "SYS-REQ-001", .risk = "RISK-001"},
    {.name = "technical", .requirement = "SYS-REQ-017", .risk = "RISK-012"},
    {.name = "link-loss", .requirement = "SYS-REQ-053", .risk = "RISK-012"},
    {.name = "reservation", .requirement = "SYS-REQ-051", .risk = "RISK-001"},
    {.name = "battery-depleted", .requirement = "SYS-REQ-016", .risk = "RISK-010"},
    {.name = "reset-recovery", .requirement = "SYS-REQ-038", .risk = "RISK-010"},
    {.name = "infusion-end", .requirement = "SYS-REQ-032", .risk = "RISK-002"},
    {.name = "battery-low", .requirement = "SYS-REQ-016", .risk = "RISK-010"},
    {.name = "idle-reminder", .requirement = "SYS-REQ-042", .risk = "RISK-002"},
    {.name = "infusion-end-prealarm", .requirement = "SYS-REQ-032", .risk = "RISK-002"},
    {.name = "syringe-near-empty", .requirement = "SYS-REQ-032", .risk = "RISK-002"},
    {.name = "clock", .requirement = "SYS-REQ-054", .risk = "RISK-011"},
    {.name = "journal-witness", .requirement = "SYS-REQ-052", .risk = "RISK-011"},
}};

struct Step {
    std::string_view suffix;
    PhaseSet phases;
};

// Dans l'ordre de l'énumération AlarmStep.
constexpr std::array<Step, alarmStepCount> steps{{
    {.suffix = "raise", .phases = executed},
    {.suffix = "silence", .phases = ref},
    {.suffix = "acknowledge", .phases = ref},
    {.suffix = "clear", .phases = executed},
}};

// Nom composé « alarm.<condition>.<étape> », dans une mémoire statique.
struct Name {
    std::array<char, mddlog::core::auditActionCapacity> text{};
    std::size_t size = 0;
};

consteval std::array<Name, alarmConditionCount * alarmStepCount> composeAlarmNames() {
    std::array<Name, alarmConditionCount * alarmStepCount> names{};
    for (std::size_t c = 0; c < alarmConditionCount; ++c) {
        for (std::size_t s = 0; s < alarmStepCount; ++s) {
            Name& name = names.at((c * alarmStepCount) + s);
            for (const std::string_view part : {std::string_view{"alarm."}, conditions.at(c).name,
                                                std::string_view{"."}, steps.at(s).suffix}) {
                for (const char letter : part) {
                    name.text.at(name.size) = letter;
                    ++name.size;
                }
            }
        }
    }
    return names;
}

constexpr std::array<Name, alarmConditionCount * alarmStepCount> alarmNames = composeAlarmNames();

ActionSpec alarmSpec(std::size_t condition, std::size_t step) noexcept {
    const Name& name = std::span{alarmNames}[(condition * alarmStepCount) + step];
    const Condition& c = std::span{conditions}[condition];
    return act(std::string_view{name.text.data(), name.size}, RiskControl,
               std::span{steps}[step].phases, both, c.requirement, c.risk);
}

} // namespace

std::optional<ActionSpec> spec(Action action) noexcept {
    const auto index = static_cast<std::size_t>(action);
    if (index >= actionCount) {
        return std::nullopt;
    }
    return std::span{actions}[index];
}

std::optional<ActionSpec> spec(AlarmCondition condition, AlarmStep step) noexcept {
    const auto c = static_cast<std::size_t>(condition);
    const auto s = static_cast<std::size_t>(step);
    if (c >= alarmConditionCount || s >= alarmStepCount) {
        return std::nullopt;
    }
    return alarmSpec(c, s);
}

std::optional<ActionSpec> at(std::size_t index) noexcept {
    if (index < actionCount) {
        return std::span{actions}[index];
    }
    if (index >= vocabularySize) {
        return std::nullopt;
    }
    const std::size_t alarm = index - actionCount;
    return alarmSpec(alarm / alarmStepCount, alarm % alarmStepCount);
}

std::optional<ActionSpec> find(std::string_view name) noexcept {
    for (const ActionSpec& candidate : actions) {
        if (candidate.name == name) {
            return candidate;
        }
    }
    for (std::size_t alarm = 0; alarm < alarmNames.size(); ++alarm) {
        const Name& candidate = std::span{alarmNames}[alarm];
        if (std::string_view{candidate.text.data(), candidate.size} == name) {
            return alarmSpec(alarm / alarmStepCount, alarm % alarmStepCount);
        }
    }
    return std::nullopt;
}

} // namespace clepsydre::audit
