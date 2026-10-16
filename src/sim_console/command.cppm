// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

export module clepsydre.sim.command;

export import clepsydre.audit.recorder;
export import clepsydre.control.drive;
export import clepsydre.control.infusion;
export import clepsydre.control.occlusion;
export import clepsydre.control.state_machine;
export import clepsydre.sim.syringe_pump;

// Commande simulée (#136) : un assemblage minimal des unités du logiciel de commande
// (src/control) autour du modèle physique, pour que le simulateur produise une séance auditée. Ce
// n'est pas le logiciel de commande (#23) : il n'y a ni canal de sécurité (#29), ni alarmes
// (#30), ni interface (#35), ni bibliothèque de médicaments (#37). Il s'exécute sur l'hôte.
//
// - Le soignant agit par les opérations publiques ; chacune est enregistrée en phase demandée,
//   puis exécutée ou échouée (docs/event-vocabulary.md).
// - La boucle de commande, à chaque période de 10 ms, fait avancer le décompte des volumes,
//   l'entraînement et le surveillant d'occlusion, qui commandent le moteur. Une occlusion, un
//   défaut de l'entraînement ou du surveillant placent la pompe en état sûr ; le recul suit.
// - Sans horloge temps réel simulée, l'heure civile des événements est « indisponible » ; le temps
//   monotone depuis le démarrage est en tête du détail.

export namespace clepsydre::sim {

// Numéro de série de la pompe simulée.
inline constexpr std::string_view simulatedSerial = "SIM0001";

// Cause de la réinitialisation précédente, connue du processeur (device.boot).
enum class ResetCause : std::uint8_t {
    Unknown,
    Power,
};

// Réponse d'une opération du soignant, à afficher.
struct Reply {
    bool accepted = false;
    std::string message;
};

class Command {
public:
    // Période de la boucle de commande.
    static constexpr std::chrono::milliseconds period{10};

    // Démarrage du processeur de commande, numéro boot, à l'instant now du temps simulé : il
    // enregistre device.boot. interrupted signale une perfusion interrompue par la
    // réinitialisation, que l'état enregistré aurait conservée (SYS-REQ-038).
    Command(SyringePump& pump, const SyringeModel& syringe, std::uint64_t boot,
            std::chrono::nanoseconds now, ResetCause reset, bool interrupted);

    Command(const Command&) = delete;
    Command(Command&&) = delete;
    Command& operator=(const Command&) = delete;
    Command& operator=(Command&&) = delete;
    ~Command();

    [[nodiscard]] audit::Recorder& recorder() noexcept { return *recorder_; }

    // Opérations du soignant.
    Reply selectSyringe(const SyringeModel& syringe, std::chrono::nanoseconds now);
    Reply program(control::Rate rate, control::Volume volume, std::chrono::nanoseconds now);
    Reply start(std::chrono::nanoseconds now); // démarrage ou reprise après une pause
    Reply pause(std::chrono::nanoseconds now);
    Reply stop(std::chrono::nanoseconds now);
    Reply bolus(control::Volume volume, control::Rate rate, bool confirmedAboveSoftLimit,
                std::chrono::nanoseconds now);
    Reply acknowledge(std::chrono::nanoseconds now); // acquittement de l'état sûr

    // Coupure imminente signalée par le superviseur de tension (SYS-REQ-050).
    void powerFailing(std::chrono::nanoseconds now);

    // Une période de la boucle de commande, à appeler toutes les 10 ms du temps simulé.
    void cycle(std::chrono::nanoseconds now);

    // La commande pilote le moteur : perfusion en cours ou recul après une occlusion.
    [[nodiscard]] bool drivesMotor() const noexcept {
        return drive_ != nullptr || monitor_ != nullptr;
    }

    [[nodiscard]] control::State state() const noexcept { return machine_.state(); }

    [[nodiscard]] control::Volume infused() const noexcept;

    // Pertes de l'anneau d'audit consignées par journal.loss.
    [[nodiscard]] std::uint64_t reportedLosses() const noexcept { return reportedLosses_; }

private:
    audit::Result emit(audit::Action action, audit::AuditPhase phase, std::string_view actor,
                       audit::Target target, std::string_view correlation,
                       const audit::Detail& detail);
    audit::Result alarm(audit::AlarmCondition condition, audit::AlarmStep step,
                        audit::AuditPhase phase, std::string_view actor,
                        std::string_view correlation, const audit::Detail& detail);
    void reportLosses();
    control::Transition apply(control::Event event, std::string_view correlation);
    Reply refuseState(audit::Action action, std::string_view correlation, std::string_view what);
    void danger(audit::AlarmCondition condition, const audit::Detail& alarmKeys,
                std::optional<audit::Action> fault, const audit::Detail& faultKeys);
    void raise(audit::AlarmCondition condition, const audit::Correlation& correlation,
               const audit::Detail& keys);
    void clearAlarm(std::string_view cause);
    void endBolus(std::string_view cause, std::string_view condition);
    void endInfusion(control::Event event);
    void release();
    void setStepRate(double stepsPerPeriod);

    SyringePump* pump_;
    SyringeModel syringe_;
    std::unique_ptr<audit::Recorder> recorder_;
    std::chrono::nanoseconds bootTime_;
    std::chrono::nanoseconds now_;
    std::uint64_t reportedLosses_ = 0;
    control::StateMachine machine_;
    std::optional<control::Program> program_;
    std::unique_ptr<control::Delivery> delivery_;
    std::unique_ptr<control::Drive> drive_;
    std::unique_ptr<control::OcclusionMonitor> monitor_;
    std::optional<audit::Correlation> bolusInput_; // demande du bolus en cours
    std::optional<audit::Correlation> condition_;  // condition de l'alarme en cours
    std::optional<audit::AlarmCondition> alarm_;   // alarme en cours, close à l'acquittement
    bool occlusionSeen_ = false;
};

} // namespace clepsydre::sim
