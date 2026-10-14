// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <algorithm>
#include <array>
#include <cstdint>

module clepsydre.control.state_machine;

namespace clepsydre::control {

namespace {

struct Rule {
    State from;
    Event event;
    State to;
};

consteval Rule rule(State from, Event event, State to) noexcept {
    return {.from = from, .event = event, .to = to};
}

// Transitions propres à un état (docs/design/state-machine.md, « Table des transitions »). Les
// événements DangerDetected et FatalFault, admis dans tous les états, sont traités à part.
constexpr std::array rules{
    rule(State::Booting, Event::SelfTestsPassed, State::Stopped),
    rule(State::Booting, Event::SelfTestsFailed, State::OutOfService),

    rule(State::Stopped, Event::ProgrammingStarted, State::Programming),
    rule(State::Stopped, Event::MaintenanceEntered, State::Maintenance),

    rule(State::Programming, Event::ProgrammingConfirmed, State::Ready),
    rule(State::Programming, Event::ProgrammingCancelled, State::Stopped),
    rule(State::Programming, Event::StopRequested, State::Stopped),

    rule(State::Ready, Event::StartRequested, State::Infusing),
    rule(State::Ready, Event::PurgeRequested, State::Purging),
    rule(State::Ready, Event::ProgrammingStarted, State::Programming),
    rule(State::Ready, Event::ProgrammingCancelled, State::Stopped),
    rule(State::Ready, Event::StopRequested, State::Stopped),

    rule(State::Purging, Event::PurgeEnded, State::Ready),
    rule(State::Purging, Event::StopRequested, State::Ready),

    rule(State::Infusing, Event::PauseRequested, State::Paused),
    rule(State::Infusing, Event::StopRequested, State::Stopped),
    rule(State::Infusing, Event::BolusRequested, State::Bolus),
    rule(State::Infusing, Event::VolumeReached, State::Stopped),
    rule(State::Infusing, Event::VolumeReachedKvo, State::Kvo),
    rule(State::Infusing, Event::SyringeEmpty, State::Stopped),

    rule(State::Bolus, Event::BolusEnded, State::Infusing),
    rule(State::Bolus, Event::PauseRequested, State::Paused),
    rule(State::Bolus, Event::StopRequested, State::Stopped),
    rule(State::Bolus, Event::SyringeEmpty, State::Stopped),

    rule(State::Kvo, Event::PauseRequested, State::PausedKvo),
    rule(State::Kvo, Event::StopRequested, State::Stopped),
    rule(State::Kvo, Event::SyringeEmpty, State::Stopped),

    rule(State::Paused, Event::StartRequested, State::Infusing),
    rule(State::Paused, Event::StopRequested, State::Stopped),

    rule(State::PausedKvo, Event::StartRequested, State::Kvo),
    rule(State::PausedKvo, Event::StopRequested, State::Stopped),

    rule(State::SafeState, Event::SafeStateAcknowledged, State::Stopped),

    rule(State::Maintenance, Event::MaintenanceExited, State::Stopped),
    rule(State::Maintenance, Event::TakenOutOfService, State::OutOfService),

    rule(State::OutOfService, Event::ReturnedToService, State::Booting),
};

[[nodiscard]] bool valid(State state) noexcept {
    const auto value = static_cast<std::uint8_t>(state);
    return value >= 1 && value <= stateCount;
}

[[nodiscard]] bool valid(Event event) noexcept {
    const auto value = static_cast<std::uint8_t>(event);
    return value >= 1 && value <= eventCount;
}

// Démarrage et hors service : la pompe n'a pas passé, ou a échoué, ses autotests. Une défaillance
// y mène hors service, jamais à l'état sûr, dont l'acquittement ramènerait à l'arrêt sans
// autotests.
[[nodiscard]] bool untested(State state) noexcept {
    return state == State::Booting || state == State::OutOfService;
}

} // namespace

Transition transition(State from, Event event) noexcept {
    if (!valid(from) || !valid(event)) {
        const State to = untested(from) ? State::OutOfService : State::SafeState;
        return {.from = from, .event = event, .to = to, .verdict = Verdict::InternalFault};
    }
    if (event == Event::FatalFault) {
        return {
            .from = from, .event = event, .to = State::OutOfService, .verdict = Verdict::Accepted};
    }
    if (event == Event::DangerDetected) {
        const State to = untested(from) ? State::OutOfService : State::SafeState;
        return {.from = from, .event = event, .to = to, .verdict = Verdict::Accepted};
    }
    const auto* const rule = std::ranges::find_if(rules, [&](const Rule& candidate) {
        return candidate.from == from && candidate.event == event;
    });
    if (rule == rules.end()) {
        return {.from = from, .event = event, .to = from, .verdict = Verdict::Refused};
    }
    return {.from = from, .event = event, .to = rule->to, .verdict = Verdict::Accepted};
}

Phase phase(State state) noexcept {
    switch (state) {
    case State::Booting:
        return Phase::Booting;
    case State::Stopped:
        return Phase::Stopped;
    case State::Programming:
        return Phase::Programming;
    case State::Ready:
        return Phase::Ready;
    case State::Purging:
        return Phase::Purging;
    case State::Infusing:
    case State::Bolus:
    case State::Kvo:
        return Phase::Infusion;
    case State::Paused:
    case State::PausedKvo:
        return Phase::Pause;
    case State::Maintenance:
        return Phase::Maintenance;
    case State::OutOfService:
        return Phase::OutOfService;
    case State::SafeState:
    default: // valeur invalide : la machine la traite comme l'état sûr
        return Phase::SafeState;
    }
}

ActuatorMode actuatorMode(State state) noexcept {
    switch (state) {
    case State::Purging:
        return ActuatorMode::Purge;
    case State::Infusing:
        return ActuatorMode::Infusion;
    case State::Bolus:
        return ActuatorMode::Bolus;
    case State::Kvo:
        return ActuatorMode::Kvo;
    default: // tous les autres états, et toute valeur invalide : aucune consigne admise
        return ActuatorMode::None;
    }
}

Transition StateMachine::apply(Event event) noexcept {
    const Transition result = transition(state_, event);
    state_ = result.to;
    return result;
}

} // namespace clepsydre::control
