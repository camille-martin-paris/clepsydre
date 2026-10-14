// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

// Machine à états du logiciel de commande (#24, SW-REQ-001) : chacun des 13 x 22 couples
// (état, événement) est comparé à la table attendue, écrite ici indépendamment de celle du code
// d'après docs/design/state-machine.md ; valeurs invalides ; regroupement en phases, mode de
// l'actionneur ; parcours complets.

#include <array>
#include <cstddef>
#include <cstdint>
#include <expect.hpp>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>

import clepsydre.control.state_machine;

namespace {

using namespace clepsydre::control;
using clepsydre::test::Expectations;

// Table attendue : (état, événement, état suivant) pour chaque transition propre à un état.
struct Expected {
    State from;
    Event event;
    State to;
};

constexpr Expected row(State from, Event event, State to) {
    return {.from = from, .event = event, .to = to};
}

constexpr std::array expected{
    row(State::Booting, Event::SelfTestsPassed, State::Stopped),
    row(State::Booting, Event::SelfTestsFailed, State::OutOfService),
    row(State::Stopped, Event::ProgrammingStarted, State::Programming),
    row(State::Stopped, Event::MaintenanceEntered, State::Maintenance),
    row(State::Programming, Event::ProgrammingConfirmed, State::Ready),
    row(State::Programming, Event::ProgrammingCancelled, State::Stopped),
    row(State::Programming, Event::StopRequested, State::Stopped),
    row(State::Ready, Event::StartRequested, State::Infusing),
    row(State::Ready, Event::PurgeRequested, State::Purging),
    row(State::Ready, Event::ProgrammingStarted, State::Programming),
    row(State::Ready, Event::ProgrammingCancelled, State::Stopped),
    row(State::Ready, Event::StopRequested, State::Stopped),
    row(State::Purging, Event::PurgeEnded, State::Ready),
    row(State::Purging, Event::StopRequested, State::Ready),
    row(State::Infusing, Event::PauseRequested, State::Paused),
    row(State::Infusing, Event::StopRequested, State::Stopped),
    row(State::Infusing, Event::BolusRequested, State::Bolus),
    row(State::Infusing, Event::VolumeReached, State::Stopped),
    row(State::Infusing, Event::VolumeReachedKvo, State::Kvo),
    row(State::Infusing, Event::SyringeEmpty, State::Stopped),
    row(State::Bolus, Event::BolusEnded, State::Infusing),
    row(State::Bolus, Event::PauseRequested, State::Paused),
    row(State::Bolus, Event::StopRequested, State::Stopped),
    row(State::Bolus, Event::VolumeReached, State::Stopped),
    row(State::Bolus, Event::VolumeReachedKvo, State::Kvo),
    row(State::Bolus, Event::SyringeEmpty, State::Stopped),
    row(State::Kvo, Event::PauseRequested, State::PausedKvo),
    row(State::Kvo, Event::StopRequested, State::Stopped),
    row(State::Kvo, Event::SyringeEmpty, State::Stopped),
    row(State::Paused, Event::StartRequested, State::Infusing),
    row(State::Paused, Event::StopRequested, State::Stopped),
    row(State::PausedKvo, Event::StartRequested, State::Kvo),
    row(State::PausedKvo, Event::StopRequested, State::Stopped),
    row(State::SafeState, Event::SafeStateAcknowledged, State::Stopped),
    row(State::Maintenance, Event::MaintenanceExited, State::Stopped),
    row(State::Maintenance, Event::TakenOutOfService, State::OutOfService),
    row(State::OutOfService, Event::ReturnedToService, State::Booting),
};

// Les tests convertissent volontairement des valeurs hors des énumérations, pour vérifier le
// traitement des incohérences internes.
State stateAt(std::uint8_t value) {
    return static_cast<State>(value); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
}

Event eventAt(std::uint8_t value) {
    return static_cast<Event>(value); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
}

bool untested(State state) {
    return state == State::Booting || state == State::OutOfService;
}

Transition result(State from, Event event, State to, Verdict verdict) {
    return {.from = from, .event = event, .to = to, .verdict = verdict};
}

// Résultat attendu pour (from, event), d'après la table et les règles globales.
Transition expectedTransition(State from, Event event) {
    if (event == Event::FatalFault) {
        return result(from, event, State::OutOfService, Verdict::Accepted);
    }
    if (event == Event::DangerDetected) {
        return result(from, event, untested(from) ? State::OutOfService : State::SafeState,
                      Verdict::Accepted);
    }
    for (const Expected& candidate : expected) {
        if (candidate.from == from && candidate.event == event) {
            return result(from, event, candidate.to, Verdict::Accepted);
        }
    }
    return result(from, event, from, Verdict::Refused);
}

std::string describe(State from, Event event) {
    return std::format("état {}, événement {}", static_cast<int>(from), static_cast<int>(event));
}

void exhaustive(Expectations& t) {
    std::size_t accepted = 0;
    std::size_t refused = 0;
    for (std::uint8_t s = 1; s <= stateCount; ++s) {
        for (std::uint8_t e = 1; e <= eventCount; ++e) {
            const Transition want = expectedTransition(stateAt(s), eventAt(e));
            const Transition got = transition(stateAt(s), eventAt(e));
            t.expect(got.from == want.from && got.event == want.event && got.to == want.to &&
                         got.verdict == want.verdict,
                     describe(stateAt(s), eventAt(e)));
            accepted += got.verdict == Verdict::Accepted ? 1U : 0U;
            refused += got.verdict == Verdict::Refused ? 1U : 0U;
        }
    }
    // 37 transitions propres, plus DangerDetected et FatalFault dans les 13 états.
    constexpr std::size_t states = stateCount;
    constexpr std::size_t events = eventCount;
    t.expect(accepted == expected.size() + (2 * states), "nombre de transitions admises");
    t.expect(accepted + refused == states * events,
             "aucune incohérence interne sur des valeurs valides");
}

void invalidValues(Expectations& t) {
    // Remarque de relecture de #128 : un état invalide ne dit pas si les autotests ont réussi. Il
    // mène hors service quel que soit l'événement, FatalFault compris, jamais à l'état sûr dont
    // l'acquittement ramènerait à l'arrêt sans autotests.
    for (const std::uint8_t value :
         {std::uint8_t{0}, std::uint8_t{stateCount + 1}, std::uint8_t{255}}) {
        for (std::uint8_t e = 0; e <= eventCount + 1; ++e) {
            const Transition got = transition(stateAt(value), eventAt(e));
            t.expect(got.verdict == Verdict::InternalFault && got.to == State::OutOfService,
                     std::format("état invalide {}, événement {} : hors service", value, e));
        }
    }
    for (const std::uint8_t value :
         {std::uint8_t{0}, std::uint8_t{eventCount + 1}, std::uint8_t{255}}) {
        for (std::uint8_t s = 1; s <= stateCount; ++s) {
            const Transition got = transition(stateAt(s), eventAt(value));
            const State want = untested(stateAt(s)) ? State::OutOfService : State::SafeState;
            t.expect(got.verdict == Verdict::InternalFault && got.to == want,
                     std::format("événement invalide {} depuis l'état {}", value, s));
        }
    }
}

// Récupération après un état invalide : seule la remise en service, puis des autotests réussis,
// ramènent à l'arrêt.
void recoveryFromInvalidState(Expectations& t) {
    State state = transition(stateAt(0), Event::SafeStateAcknowledged).to;
    t.expect(state == State::OutOfService, "état invalide : hors service");
    for (const Event event : {Event::SafeStateAcknowledged, Event::ProgrammingStarted,
                              Event::StartRequested, Event::SelfTestsPassed}) {
        const Transition got = transition(state, event);
        t.expect(got.verdict == Verdict::Refused && got.to == State::OutOfService,
                 std::format("hors service : événement {} refusé", static_cast<int>(event)));
    }
    state = transition(state, Event::ReturnedToService).to;
    t.expect(state == State::Booting, "remise en service : démarrage");
    t.expect(transition(state, Event::StartRequested).verdict == Verdict::Refused &&
                 transition(state, Event::ProgrammingStarted).verdict == Verdict::Refused,
             "aucune programmation ni démarrage avant les autotests");
    t.expect(transition(state, Event::SelfTestsPassed).to == State::Stopped,
             "autotests réussis : arrêt");
}

void phasesAndModes(Expectations& t) {
    const std::array<std::pair<State, Phase>, stateCount> phases{{
        {State::Booting, Phase::Booting},
        {State::Stopped, Phase::Stopped},
        {State::Programming, Phase::Programming},
        {State::Ready, Phase::Ready},
        {State::Purging, Phase::Purging},
        {State::Infusing, Phase::Infusion},
        {State::Bolus, Phase::Infusion},
        {State::Kvo, Phase::Infusion},
        {State::Paused, Phase::Pause},
        {State::PausedKvo, Phase::Pause},
        {State::SafeState, Phase::SafeState},
        {State::Maintenance, Phase::Maintenance},
        {State::OutOfService, Phase::OutOfService},
    }};
    for (const auto& [state, want] : phases) {
        t.expect(phase(state) == want, std::format("phase de l'état {}", static_cast<int>(state)));
    }
    t.expect(phase(stateAt(0)) == Phase::OutOfService && phase(stateAt(200)) == Phase::OutOfService,
             "état invalide affiché comme hors service");

    for (std::uint8_t s = 1; s <= stateCount; ++s) {
        const State state = stateAt(s);
        ActuatorMode want = ActuatorMode::None;
        if (state == State::Purging) {
            want = ActuatorMode::Purge;
        } else if (state == State::Infusing) {
            want = ActuatorMode::Infusion;
        } else if (state == State::Bolus) {
            want = ActuatorMode::Bolus;
        } else if (state == State::Kvo) {
            want = ActuatorMode::Kvo;
        }
        t.expect(actuatorMode(state) == want, std::format("mode de l'actionneur, état {}", s));
    }
    t.expect(actuatorMode(stateAt(0)) == ActuatorMode::None, "état invalide : aucune consigne");
}

void stableValues(Expectations& t) {
    // Valeurs transmises et journalisées : leur changement est une modification du protocole.
    t.expect(static_cast<int>(State::Booting) == 1 && static_cast<int>(State::Infusing) == 6 &&
                 static_cast<int>(State::SafeState) == 11 &&
                 static_cast<int>(State::OutOfService) == 13,
             "valeurs des états");
    t.expect(static_cast<int>(Event::SelfTestsPassed) == 1 &&
                 static_cast<int>(Event::DangerDetected) == 16 &&
                 static_cast<int>(Event::FatalFault) == 22,
             "valeurs des événements");
}

// Applique une suite d'événements ; renvoie le premier verdict non accepté, s'il y en a un.
std::optional<Transition> run(StateMachine& machine, const std::vector<Event>& events) {
    for (const Event event : events) {
        const Transition result = machine.apply(event);
        if (result.verdict != Verdict::Accepted) {
            return result;
        }
    }
    return std::nullopt;
}

// Remarque de relecture de #128 : le volume à perfuser peut être atteint pendant un bolus, selon
// les deux politiques de la bibliothèque, et en même temps que la fin du bolus, dans les deux
// ordres.
void endOfVolumeDuringBolus(Expectations& t) {
    const std::vector<Event> toBolus{Event::SelfTestsPassed, Event::ProgrammingStarted,
                                     Event::ProgrammingConfirmed, Event::StartRequested,
                                     Event::BolusRequested};
    for (const auto& [end, after] : {std::pair{Event::VolumeReached, State::Stopped},
                                     std::pair{Event::VolumeReachedKvo, State::Kvo}}) {
        const std::string policy = end == Event::VolumeReached ? "arrêt" : "maintien de veine";
        {
            StateMachine machine;
            static_cast<void>(run(machine, toBolus));
            const Transition reached = machine.apply(end);
            t.expect(reached.verdict == Verdict::Accepted && machine.state() == after,
                     "volume atteint pendant le bolus : " + policy);
            const Transition bolusEnd = machine.apply(Event::BolusEnded);
            t.expect(bolusEnd.verdict == Verdict::Refused && machine.state() == after,
                     "fin de bolus simultanée, reçue après : refusée, " + policy + " maintenu");
            t.expect(actuatorMode(machine.state()) != ActuatorMode::Bolus,
                     "plus de consigne de bolus : " + policy);
        }
        {
            StateMachine machine;
            static_cast<void>(run(machine, toBolus));
            const auto refusal = run(machine, {Event::BolusEnded, end});
            t.expect(!refusal && machine.state() == after,
                     "fin de bolus puis volume atteint : " + policy);
        }
    }
}

void journeys(Expectations& t) {
    {
        StateMachine machine;
        t.expect(machine.state() == State::Booting, "démarrage à la mise sous tension");
        const auto refusal =
            run(machine, {Event::SelfTestsPassed, Event::ProgrammingStarted,
                          Event::ProgrammingConfirmed, Event::PurgeRequested, Event::PurgeEnded,
                          Event::StartRequested, Event::BolusRequested, Event::BolusEnded,
                          Event::PauseRequested, Event::StartRequested, Event::VolumeReachedKvo,
                          Event::PauseRequested, Event::StartRequested, Event::StopRequested});
        t.expect(!refusal && machine.state() == State::Stopped,
                 "parcours : purge, perfusion, bolus, pause, maintien de veine, arrêt");
    }
    {
        StateMachine machine;
        static_cast<void>(run(machine, {Event::SelfTestsPassed, Event::ProgrammingStarted,
                                        Event::ProgrammingConfirmed, Event::StartRequested}));
        const Transition purgeAfterStart = machine.apply(Event::PurgeRequested);
        t.expect(purgeAfterStart.verdict == Verdict::Refused && machine.state() == State::Infusing,
                 "purge refusée une fois la perfusion démarrée (SYS-REQ-014)");
        machine.apply(Event::PauseRequested);
        t.expect(machine.apply(Event::PurgeRequested).verdict == Verdict::Refused,
                 "purge refusée en pause");
        t.expect(machine.apply(Event::ProgrammingStarted).verdict == Verdict::Refused,
                 "nouvelle programmation refusée pendant la perfusion (SYS-REQ-044)");
        const Transition danger = machine.apply(Event::DangerDetected);
        t.expect(danger.verdict == Verdict::Accepted && machine.state() == State::SafeState,
                 "condition dangereuse en pause : état sûr");
        t.expect(machine.apply(Event::StartRequested).verdict == Verdict::Refused,
                 "pas de reprise depuis l'état sûr");
        machine.apply(Event::SafeStateAcknowledged);
        t.expect(machine.state() == State::Stopped, "acquittement : arrêt, programmation perdue");
    }
    {
        StateMachine machine;
        const auto refusal = run(machine, {Event::SelfTestsPassed, Event::MaintenanceEntered,
                                           Event::TakenOutOfService, Event::ReturnedToService,
                                           Event::SelfTestsFailed});
        t.expect(!refusal && machine.state() == State::OutOfService,
                 "mise hors service, remise en service, autotests en échec");
        t.expect(machine.apply(Event::ProgrammingStarted).verdict == Verdict::Refused,
                 "hors service : toute perfusion refusée (SYS-REQ-041)");
        t.expect(machine.apply(Event::DangerDetected).to == State::OutOfService,
                 "hors service : une condition dangereuse ne mène pas à l'état sûr");
    }
}

} // namespace

int main() {
    return Expectations{}.run([](Expectations& t) {
        exhaustive(t);
        invalidValues(t);
        recoveryFromInvalidState(t);
        phasesAndModes(t);
        stableValues(t);
        endOfVolumeDuringBolus(t);
        journeys(t);
    });
}
