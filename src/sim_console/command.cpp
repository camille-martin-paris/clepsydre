// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <format>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>

module clepsydre.sim.command;

import clepsydre.version;

namespace clepsydre::sim {

namespace {

using audit::AlarmCondition;
using audit::AlarmStep;
using audit::AuditPhase;
using audit::Detail;
using audit::Target;
using Action = audit::Action;

// Médicament d'exemple : limites en débit, sans valeur clinique (#37).
constexpr std::string_view drugId = "exemple";
constexpr control::DrugLimits drugLimits{
    .hardMinRate = {.microlitresPerHour = 100},
    .softMinRate = {.microlitresPerHour = 500},
    .softMaxRate = {.microlitresPerHour = 50'000},
    .hardMaxRate = {.microlitresPerHour = 100'000},
    .softMaxBolusVolume = {.microlitres = 2'000},
    .hardMaxBolusVolume = {.microlitres = 5'000},
    .hardMaxBolusRate = {.microlitresPerHour = 100'000},
    .endOfInfusion = control::EndOfInfusion::Stop,
    .kvoRate = {},
};

std::uint64_t value(control::State state) {
    return static_cast<std::uint64_t>(state);
}

std::string_view stateName(control::State state) {
    switch (state) {
    case control::State::Booting:
        return "démarrage";
    case control::State::Stopped:
        return "arrêt";
    case control::State::Programming:
        return "programmation";
    case control::State::Ready:
        return "prête";
    case control::State::Purging:
        return "purge";
    case control::State::Infusing:
        return "perfusion";
    case control::State::Bolus:
        return "bolus";
    case control::State::Kvo:
        return "maintien de veine ouverte";
    case control::State::Paused:
    case control::State::PausedKvo:
        return "pause";
    case control::State::SafeState:
        return "état sûr";
    case control::State::Maintenance:
        return "maintenance";
    case control::State::OutOfService:
        break;
    }
    return "hors service";
}

double area(const SyringeModel& syringe) {
    const double radius = syringe.innerDiameter.si() / 2.0;
    return std::numbers::pi * radius * radius;
}

// Grandeurs de la commande tirées du modèle, comme dans tests/control_occlusion_sim_test.cpp :
// ce que les bancs établiraient pour le modèle de la liste vérifiée (#73, #74).
control::DriveMechanism driveMechanism() {
    const auto stepRate = static_cast<std::uint32_t>(exampleMechanism.maxStepRate.si());
    return {.screwLeadNanometres =
                static_cast<std::uint32_t>(std::lround(exampleMechanism.screwLead.si() * 1e9)),
            .stepsPerRevolution = static_cast<std::uint32_t>(exampleMechanism.stepsPerRevolution),
            .maxStepRate = stepRate,
            .takeUpStepRate = stepRate,
            .period = Command::period};
}

control::Force newtons(double force) {
    return {.millinewtons = static_cast<std::int32_t>(std::lround(force * 1e3))};
}

control::SyringeDrive syringeDrive(const SyringeModel& syringe) {
    const double force = 0.75 * syringe.breakawayFriction.si();
    const double compression = force / syringe.stopperStiffness.si();
    const double travel = exampleMechanism.backlash.si() + (1.25 * compression);
    return {.strokeVolume = static_cast<std::uint32_t>(std::lround(area(syringe) * 1e9)),
            .takeUpForce = newtons(force),
            .takeUpTravel = static_cast<std::uint32_t>(std::lround(travel * 1e6))};
}

control::OcclusionSettings occlusionSettings(const SyringeModel& syringe) {
    const double breakaway = syringe.breakawayFriction.si();
    const double a = area(syringe);
    const double perNewton =
        (1.0 / syringe.stopperStiffness.si()) + (syringe.compliance.si() / (a * a));
    return {.minThreshold = newtons(1.5 * breakaway),
            .defaultThreshold = newtons(2.0 * breakaway),
            .maxThreshold = newtons(4.0 * breakaway),
            .confirmation = std::chrono::milliseconds{100},
            .backoffPerNewton = static_cast<std::uint32_t>(std::lround(perNewton * 1e9)),
            .maxBackoffTravel =
                static_cast<std::uint32_t>(std::lround(1.25 * 4.0 * breakaway * perNewton * 1e6))};
}

// Volume utile et débit maximal du modèle : vitesse maximale du moteur, plafonnée à 200 mL/h.
control::Syringe controlSyringe(const SyringeModel& syringe) {
    const double perStep =
        exampleMechanism.screwLead.si() / static_cast<double>(exampleMechanism.stepsPerRevolution);
    const double maxFlow = area(syringe) * perStep * exampleMechanism.maxStepRate.si(); // m³/s
    const auto maxRate = static_cast<std::uint32_t>(std::min(
        static_cast<double>(control::pumpMaxRate.microlitresPerHour), maxFlow * 3600.0 * 1e9));
    return {.usableVolume = {.microlitres = static_cast<std::uint32_t>(
                                 std::lround(syringe.usableVolume.si() * 1e9))},
            .maxRate = {.microlitresPerHour = maxRate}};
}

// Identifiant du modèle dans la liste vérifiée : `exemple-<volume>ml`.
std::string syringeId(const SyringeModel& syringe) {
    return std::format("exemple-{}ml", std::lround(syringe.usableVolume.si() * 1e6));
}

std::optional<control::Force> sensedForce(const SyringePump& pump) {
    const auto force = pump.measuredForce();
    if (!force) {
        return std::nullopt;
    }
    return newtons(force->si());
}

std::string_view softName(control::SoftLimit limit) {
    return limit == control::SoftLimit::AboveMaximum ? "above" : "below";
}

std::string millilitres(control::Volume volume) {
    return std::format("{:.1f} mL", volume.microlitres / 1000.0);
}

// Nom d'une action du vocabulaire ; vide pour une valeur hors des énumérations.
std::string_view nameOf(const std::optional<audit::ActionSpec>& action) {
    return action ? action->name : std::string_view{};
}

// `alarm.<condition>` : l'alarme qui a provoqué l'état sûr, sans l'étape.
std::string_view alarmName(AlarmCondition condition) {
    const std::string_view raise = nameOf(audit::spec(condition, AlarmStep::Raise));
    return raise.substr(0, raise.rfind('.'));
}

} // namespace

Command::Command(SyringePump& pump, const SyringeModel& syringe, std::uint64_t boot,
                 std::chrono::nanoseconds now, ResetCause reset, bool interrupted)
    : pump_{&pump}, syringe_{syringe},
      recorder_{std::make_unique<audit::Recorder>(audit::Origin{
          .serial = simulatedSerial, .producer = audit::Producer::Command, .boot = boot})},
      bootTime_{now}, now_{now} {
    const audit::Correlation start = recorder_->boot();
    Detail detail;
    detail.add("boot", boot)
        .add("software", std::format("v{}", versionString()))
        .add("reset", reset == ResetCause::Power ? "power" : "unknown");
    emit(Action::DeviceBoot, AuditPhase::Executed, audit::producerName(recorder_->producer()),
         Target::Pump, start.view(), detail);
    // Le simulateur n'exécute pas d'autotests : aucun device.self-test n'est enregistré.
    static_cast<void>(machine_.apply(control::Event::SelfTestsPassed));
    if (interrupted) {
        // Reprise après réinitialisation pendant une perfusion (SYS-REQ-038) : alarme haute, sans
        // reprise automatique.
        const audit::Correlation condition = recorder_->condition();
        Detail keys;
        keys.add("priority", audit::priorityName(audit::Priority::High));
        raise(AlarmCondition::ResetRecovery, condition, keys);
    }
}

Command::~Command() = default;

audit::Result Command::emit(Action action, AuditPhase phase, std::string_view actor, Target target,
                            std::string_view correlation, const Detail& detail) {
    reportLosses();
    return recorder_->record(
        action,
        {.phase = phase,
         .actor = actor,
         .target = target,
         .correlation = correlation,
         .time = mddlog::core::RawTime::unavailable(),
         .monotonic = std::chrono::duration_cast<std::chrono::milliseconds>(now_ - bootTime_),
         .clockDoubtful = false,
         .detail = &detail});
}

audit::Result Command::alarm(AlarmCondition condition, AlarmStep step, AuditPhase phase,
                             std::string_view actor, std::string_view correlation,
                             const Detail& detail) {
    reportLosses();
    const Target target =
        condition == AlarmCondition::ResetRecovery ? Target::Pump : Target::Channel;
    return recorder_->record(
        condition, step,
        {.phase = phase,
         .actor = actor,
         .target = target,
         .correlation = correlation,
         .time = mddlog::core::RawTime::unavailable(),
         .monotonic = std::chrono::duration_cast<std::chrono::milliseconds>(now_ - bootTime_),
         .clockDoubtful = false,
         .detail = &detail});
}

// Les événements refusés par l'anneau plein sont consignés dès que l'anneau les admet de nouveau
// (journal.loss, SW-REQ-008) : la perte n'est jamais silencieuse.
void Command::reportLosses() {
    const std::uint64_t refused = recorder_->ring().refusalCount();
    if (refused <= reportedLosses_) {
        return;
    }
    Detail detail;
    detail.add("count", refused - reportedLosses_).add("cause", "ring-full");
    const audit::Correlation start = recorder_->boot();
    const audit::Result result = recorder_->record(
        Action::JournalLoss,
        {.phase = AuditPhase::Executed,
         .actor = audit::producerName(recorder_->producer()),
         .target = Target::Journal,
         .correlation = start.view(),
         .time = mddlog::core::RawTime::unavailable(),
         .monotonic = std::chrono::duration_cast<std::chrono::milliseconds>(now_ - bootTime_),
         .clockDoubtful = false,
         .detail = &detail});
    if (admitted(result)) {
        reportedLosses_ = refused;
    }
}

control::Transition Command::apply(control::Event event, std::string_view correlation) {
    const control::Transition transition = machine_.apply(event);
    if (transition.verdict == control::Verdict::InternalFault) {
        Detail detail;
        detail.add("state", value(transition.from)).add("event", static_cast<std::uint64_t>(event));
        emit(Action::FaultStateMachine, AuditPhase::Executed, "commande", Target::Pump, correlation,
             detail);
    }
    return transition;
}

Reply Command::refuseState(Action action, std::string_view correlation, std::string_view what) {
    Detail detail;
    detail.add("refus", "state").add("state", value(machine_.state()));
    emit(action, AuditPhase::Failed, audit::operatorActor, Target::Channel, correlation, detail);
    return {.accepted = false,
            .message =
                std::format("{} refusé(e) dans l'état « {} »", what, stateName(machine_.state()))};
}

Reply Command::selectSyringe(const SyringeModel& syringe, std::chrono::nanoseconds now) {
    now_ = now;
    const audit::Correlation input = recorder_->input();
    const std::string id = syringeId(syringe);
    Detail request;
    request.add("syringe", id)
        .add("diameter_um",
             static_cast<std::uint64_t>(std::lround(syringe.outerDiameter.si() * 1e6)));
    emit(Action::SyringeSelect, AuditPhase::Requested, audit::operatorActor, Target::Channel,
         input.view(), request);
    if (machine_.state() != control::State::Stopped) {
        return refuseState(Action::SyringeSelect, input.view(), "Changement de seringue");
    }
    syringe_ = syringe;
    emit(Action::SyringeSelect, AuditPhase::Executed, audit::operatorActor, Target::Channel,
         input.view(), Detail{});
    return {.accepted = true, .message = std::format("seringue {} choisie", id)};
}

Reply Command::program(control::Rate rate, control::Volume volume, std::chrono::nanoseconds now) {
    now_ = now;
    const audit::Correlation input = recorder_->input();
    Detail request;
    request.add("mode", static_cast<std::uint64_t>(control::Mode::ContinuousRate))
        .add("rate_ulh", rate.microlitresPerHour)
        .add("volume_ul", volume.microlitres)
        .add("drug", drugId)
        .add("syringe", syringeId(syringe_));
    emit(Action::TherapyProgram, AuditPhase::Requested, audit::operatorActor, Target::Channel,
         input.view(), request);
    if (apply(control::Event::ProgrammingStarted, input.view()).verdict !=
        control::Verdict::Accepted) {
        return refuseState(Action::TherapyProgram, input.view(), "Programmation");
    }
    const auto accepted = control::validateProgram(
        {.mode = control::Mode::ContinuousRate, .rate = rate, .volume = volume, .duration = {}},
        drugLimits, controlSyringe(syringe_));
    if (!accepted) {
        static_cast<void>(apply(control::Event::ProgrammingCancelled, input.view()));
        program_.reset();
        Detail refusal;
        refusal.add("refus", "validation")
            .add("motif", static_cast<std::uint64_t>(accepted.error()));
        emit(Action::TherapyProgram, AuditPhase::Failed, audit::operatorActor, Target::Channel,
             input.view(), refusal);
        return {.accepted = false,
                .message = std::format("Programmation refusée, motif {}",
                                       static_cast<int>(accepted.error()))};
    }
    const control::Program& program = *accepted;
    program_ = program;
    static_cast<void>(apply(control::Event::ProgrammingConfirmed, input.view()));
    Detail done;
    done.add("state", value(machine_.state()))
        .add("duration_s", static_cast<std::uint64_t>(program.duration.count()));
    if (program.rateSoftLimit != control::SoftLimit::Within) {
        done.add("soft", softName(program.rateSoftLimit));
    }
    emit(Action::TherapyProgram, AuditPhase::Executed, audit::operatorActor, Target::Channel,
         input.view(), done);
    return {.accepted = true,
            .message = std::format("programmation : {:.1f} mL/h, {} en {} s",
                                   rate.microlitresPerHour / 1000.0, millilitres(volume),
                                   program.duration.count())};
}

Reply Command::start(std::chrono::nanoseconds now) {
    now_ = now;
    const audit::Correlation input = recorder_->input();
    const control::State from = machine_.state();
    const bool resume = from == control::State::Paused || from == control::State::PausedKvo;
    const Action action = resume ? Action::TherapyResume : Action::TherapyStart;
    Detail request;
    request.add("state", value(from));
    emit(action, AuditPhase::Requested, audit::operatorActor, Target::Channel, input.view(),
         request);
    if (apply(control::Event::StartRequested, input.view()).verdict != control::Verdict::Accepted) {
        return refuseState(action, input.view(), resume ? "Reprise" : "Démarrage");
    }
    if (!resume) {
        const control::DriveMechanism mechanism = driveMechanism();
        const control::SyringeDrive drive = syringeDrive(syringe_);
        const control::OcclusionSettings settings = occlusionSettings(syringe_);
        delivery_ = std::make_unique<control::Delivery>(*program_, drugLimits);
        drive_ = std::make_unique<control::Drive>(mechanism, drive, pump_->motorSteps());
        monitor_ = std::make_unique<control::OcclusionMonitor>(mechanism, drive, settings,
                                                               settings.defaultThreshold);
        occlusionSeen_ = false;
    }
    Detail done;
    done.add("state", value(machine_.state()));
    emit(action, AuditPhase::Executed, audit::operatorActor, Target::Channel, input.view(), done);
    return {.accepted = true, .message = resume ? "perfusion reprise" : "perfusion démarrée"};
}

Reply Command::pause(std::chrono::nanoseconds now) {
    now_ = now;
    const audit::Correlation input = recorder_->input();
    const bool bolus = machine_.state() == control::State::Bolus;
    Detail request;
    request.add("state", value(machine_.state()));
    emit(Action::TherapyPause, AuditPhase::Requested, audit::operatorActor, Target::Channel,
         input.view(), request);
    if (apply(control::Event::PauseRequested, input.view()).verdict != control::Verdict::Accepted) {
        return refuseState(Action::TherapyPause, input.view(), "Pause");
    }
    if (bolus) {
        endBolus("pause", {});
    }
    Detail done;
    done.add("state", value(machine_.state()));
    emit(Action::TherapyPause, AuditPhase::Executed, audit::operatorActor, Target::Channel,
         input.view(), done);
    return {.accepted = true, .message = "perfusion en pause"};
}

Reply Command::stop(std::chrono::nanoseconds now) {
    now_ = now;
    const audit::Correlation input = recorder_->input();
    const bool bolus = machine_.state() == control::State::Bolus;
    Detail request;
    request.add("state", value(machine_.state()));
    emit(Action::TherapyStop, AuditPhase::Requested, audit::operatorActor, Target::Channel,
         input.view(), request);
    if (apply(control::Event::StopRequested, input.view()).verdict != control::Verdict::Accepted) {
        return refuseState(Action::TherapyStop, input.view(), "Arrêt");
    }
    if (bolus) {
        endBolus("stop", {});
    }
    const control::Volume delivered = infused();
    release();
    Detail done;
    done.add("state", value(machine_.state())).add("delivered_ul", delivered.microlitres);
    emit(Action::TherapyStop, AuditPhase::Executed, audit::operatorActor, Target::Channel,
         input.view(), done);
    return {.accepted = true,
            .message = std::format("perfusion arrêtée, {} perfusés", millilitres(delivered))};
}

Reply Command::bolus(control::Volume volume, control::Rate rate, bool confirmedAboveSoftLimit,
                     std::chrono::nanoseconds now) {
    now_ = now;
    const audit::Correlation input = recorder_->input();
    Detail request;
    request.add("volume_ul", volume.microlitres).add("rate_ulh", rate.microlitresPerHour);
    emit(Action::TherapyBolus, AuditPhase::Requested, audit::operatorActor, Target::Channel,
         input.view(), request);
    std::optional<control::Bolus> accepted;
    if (program_) {
        const auto validated = control::validateBolus({.volume = volume, .rate = rate}, *program_,
                                                      drugLimits, controlSyringe(syringe_));
        if (!validated) {
            Detail refusal;
            refusal.add("refus", "validation")
                .add("motif", static_cast<std::uint64_t>(validated.error()));
            emit(Action::TherapyBolus, AuditPhase::Failed, audit::operatorActor, Target::Channel,
                 input.view(), refusal);
            return {.accepted = false,
                    .message =
                        std::format("Bolus refusé, motif {}", static_cast<int>(validated.error()))};
        }
        accepted = *validated;
        // Au-delà de la limite souple, une confirmation spécifique est exigée (SYS-REQ-012).
        if (accepted->volumeAboveSoftLimit) {
            if (!confirmedAboveSoftLimit) {
                Detail refusal;
                refusal.add("refus", "timeout");
                emit(Action::TherapyBolus, AuditPhase::Failed, audit::operatorActor,
                     Target::Channel, input.view(), refusal);
                return {.accepted = false,
                        .message = "Bolus au-delà de la limite souple : confirmation exigée "
                                   "(« bolus <mL> <mL/h> confirme »)"};
            }
            Detail confirmation;
            confirmation.add("soft", "above");
            emit(Action::TherapyBolus, AuditPhase::Confirmed, audit::operatorActor, Target::Channel,
                 input.view(), confirmation);
        }
    }
    if (apply(control::Event::BolusRequested, input.view()).verdict != control::Verdict::Accepted ||
        !accepted || !delivery_) {
        return refuseState(Action::TherapyBolus, input.view(), "Bolus");
    }
    delivery_->startBolus(*accepted);
    bolusInput_ = input;
    Detail done;
    done.add("state", value(machine_.state()));
    emit(Action::TherapyBolus, AuditPhase::Executed, audit::operatorActor, Target::Channel,
         input.view(), done);
    return {.accepted = true, .message = std::format("bolus de {} en cours", millilitres(volume))};
}

Reply Command::acknowledge(std::chrono::nanoseconds now) {
    now_ = now;
    const audit::Correlation input = recorder_->input();
    const bool safeState = machine_.state() == control::State::SafeState;
    if (!safeState && alarm_ && condition_) {
        // Acquittement d'une alarme qui n'a pas mené à l'état sûr : fin de perfusion, reprise
        // après réinitialisation.
        Detail cited;
        cited.add("cond", condition_->local());
        alarm(*alarm_, AlarmStep::Acknowledge, AuditPhase::Requested, audit::operatorActor,
              input.view(), cited);
        alarm(*alarm_, AlarmStep::Acknowledge, AuditPhase::Executed, audit::operatorActor,
              input.view(), cited);
        clearAlarm("resolved");
        return {.accepted = true, .message = "alarme acquittée"};
    }
    Detail request;
    if (condition_) {
        request.add("cond", condition_->local());
    }
    request.add("state", value(machine_.state()));
    emit(Action::SafetySafeStateAcknowledge, AuditPhase::Requested, audit::operatorActor,
         Target::Channel, input.view(), request);
    if (apply(control::Event::SafeStateAcknowledged, input.view()).verdict !=
        control::Verdict::Accepted) {
        return refuseState(Action::SafetySafeStateAcknowledge, input.view(), "Acquittement");
    }
    release();
    Detail done;
    if (condition_) {
        done.add("cond", condition_->local());
    }
    done.add("state", value(machine_.state()));
    emit(Action::SafetySafeStateAcknowledge, AuditPhase::Executed, audit::operatorActor,
         Target::Channel, input.view(), done);
    clearAlarm("resolved");
    return {.accepted = true, .message = "état sûr acquitté : programmation perdue"};
}

void Command::powerFailing(std::chrono::nanoseconds now) {
    now_ = now;
    const audit::Correlation start = recorder_->boot();
    emit(Action::DevicePowerFail, AuditPhase::Executed, "commande", Target::Pump, start.view(),
         Detail{});
}

void Command::raise(AlarmCondition condition, const audit::Correlation& correlation,
                    const Detail& keys) {
    // Une alarme encore en cours est remplacée : sa fin est consignée.
    clearAlarm("superseded");
    alarm(condition, AlarmStep::Raise, AuditPhase::Executed, "commande", correlation.view(), keys);
    condition_ = correlation;
    alarm_ = condition;
}

void Command::clearAlarm(std::string_view cause) {
    if (!alarm_ || !condition_) {
        return;
    }
    Detail detail;
    detail.add("cause", cause);
    alarm(*alarm_, AlarmStep::Clear, AuditPhase::Executed, "commande", condition_->view(), detail);
    alarm_.reset();
    condition_.reset();
}

void Command::danger(AlarmCondition condition, const Detail& alarmKeys, std::optional<Action> fault,
                     const Detail& faultKeys) {
    const audit::Correlation correlation = recorder_->condition();
    if (fault) {
        emit(*fault, AuditPhase::Executed, "commande", Target::Pump, correlation.view(), faultKeys);
    }
    if (machine_.state() == control::State::Bolus) {
        endBolus("safety", correlation.local());
    }
    raise(condition, correlation, alarmKeys);
    static_cast<void>(apply(control::Event::DangerDetected, correlation.view()));
    Detail detail;
    // La cause est l'action qui a provoqué l'état sûr : le défaut s'il y en a un, sinon l'alarme.
    const std::string_view cause = fault ? nameOf(audit::spec(*fault)) : alarmName(condition);
    detail.add("state", value(machine_.state())).add("cause", cause);
    emit(Action::SafetySafeState, AuditPhase::Executed, "commande", Target::Channel,
         correlation.view(), detail);
}

void Command::endBolus(std::string_view cause, std::string_view condition) {
    if (!bolusInput_) {
        return;
    }
    Detail detail;
    detail.add("delivered_ul", delivery_ ? delivery_->bolusDelivered().microlitres : 0U)
        .add("cause", cause);
    if (!condition.empty()) {
        detail.add("cond", condition);
    }
    emit(Action::TherapyBolusEnd, AuditPhase::Executed, "commande", Target::Channel,
         bolusInput_->view(), detail);
    bolusInput_.reset();
}

void Command::endInfusion(control::Event event) {
    const bool bolus = machine_.state() == control::State::Bolus;
    const audit::Correlation correlation = recorder_->condition();
    static_cast<void>(apply(event, correlation.view()));
    if (bolus) {
        endBolus("end", {});
    }
    Detail detail;
    detail.add("cause", "volume")
        .add("next", event == control::Event::VolumeReachedKvo ? "kvo" : "stop")
        .add("delivered_ul", infused().microlitres);
    emit(Action::TherapyEnd, AuditPhase::Executed, "commande", Target::Channel, correlation.view(),
         detail);
    Detail keys;
    keys.add("priority", audit::priorityName(audit::Priority::Medium));
    raise(AlarmCondition::InfusionEnd, correlation, keys);
    if (event == control::Event::VolumeReached) {
        release();
    }
}

void Command::release() {
    if (machine_.state() == control::State::Stopped) {
        program_.reset();
    }
    delivery_.reset();
    drive_.reset();
    monitor_.reset();
    setStepRate(0.0);
}

void Command::setStepRate(double stepsPerPeriod) {
    pump_->setStepRate(stepsPerSecond(stepsPerPeriod * 1e3 / static_cast<double>(period.count())));
}

control::Volume Command::infused() const noexcept {
    return delivery_ ? delivery_->infused() : control::Volume{};
}

void Command::cycle(std::chrono::nanoseconds now) {
    now_ = now;
    if (!drivesMotor()) {
        return;
    }
    const std::int64_t steps = pump_->motorSteps();
    const std::optional<control::Force> force = sensedForce(*pump_);
    if (monitor_) {
        const control::OcclusionCommand occlusion = monitor_->update(period, steps, force);
        if (occlusion.fault) {
            Detail keys;
            keys.add("priority", audit::priorityName(audit::Priority::High))
                .add("unit", "occlusion-monitor")
                .add("cause", static_cast<std::uint64_t>(*occlusion.fault));
            Detail cause;
            cause.add("cause", static_cast<std::uint64_t>(*occlusion.fault));
            danger(AlarmCondition::Technical, keys, Action::FaultOcclusionMonitor, cause);
            drive_.reset();
            monitor_.reset();
            setStepRate(0.0);
            return;
        }
        if (occlusion.occlusion) {
            if (!occlusionSeen_) {
                occlusionSeen_ = true;
                drive_.reset();
                Detail keys;
                keys.add("priority", audit::priorityName(audit::Priority::High));
                if (const auto detected = monitor_->detectionForce()) {
                    keys.add("force_mn",
                             static_cast<std::uint64_t>(std::max(0, detected->millinewtons)));
                }
                danger(AlarmCondition::Occlusion, keys, std::nullopt, Detail{});
            }
            // Recul du pousseur d'une course calculée (CTRL-008), hors consigne de débit.
            setStepRate(-static_cast<double>(occlusion.backoffSteps));
            if (monitor_->phase() == control::OcclusionPhase::Decompressed) {
                monitor_.reset();
            }
            return;
        }
    }
    if (!drive_) {
        setStepRate(0.0);
        return;
    }
    const control::ActuatorMode mode = control::actuatorMode(machine_.state());
    if (mode == control::ActuatorMode::None) {
        // Pause : aucun pas, ni de rattrapage, que l'entraînement poursuivrait malgré une consigne
        // nulle, ni de retard dû. L'entraînement est suspendu tel quel : la commande précédente est
        // émise, le compteur ne bouge plus, et la reprise repart de cet état.
        setStepRate(0.0);
        return;
    }
    const control::Rate setpoint = delivery_ ? delivery_->setpoint(mode) : control::Rate{};
    const control::DriveCommand command = drive_->update(period, setpoint, steps, force);
    if (command.fault) {
        Detail keys;
        keys.add("priority", audit::priorityName(audit::Priority::High))
            .add("unit", "drive")
            .add("cause", static_cast<std::uint64_t>(*command.fault));
        Detail cause;
        cause.add("cause", static_cast<std::uint64_t>(*command.fault));
        danger(AlarmCondition::Technical, keys, Action::FaultDrive, cause);
        drive_.reset();
        monitor_.reset();
        setStepRate(0.0);
        return;
    }
    setStepRate(static_cast<double>(command.steps));
    // Le décompte commence avec la poursuite, une fois les jeux rattrapés (#131).
    if (drive_->phase() != control::DrivePhase::Tracking || !delivery_) {
        return;
    }
    const std::optional<control::Event> event = delivery_->advance(mode, period);
    if (!event) {
        return;
    }
    switch (*event) {
    case control::Event::VolumeReached:
    case control::Event::VolumeReachedKvo:
        endInfusion(*event);
        break;
    case control::Event::BolusEnded:
        static_cast<void>(
            apply(control::Event::BolusEnded, bolusInput_ ? bolusInput_->view() : ""));
        endBolus("volume", {});
        break;
    default: {
        Detail keys;
        keys.add("priority", audit::priorityName(audit::Priority::High)).add("unit", "delivery");
        danger(AlarmCondition::Technical, keys, std::nullopt, Detail{});
        drive_.reset();
        monitor_.reset();
        setStepRate(0.0);
        break;
    }
    }
}

} // namespace clepsydre::sim
