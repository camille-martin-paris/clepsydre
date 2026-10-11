// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

export module clepsydre.sim.syringe_pump;

export import clepsydre.sim.quantities;

// Modèle physique du pousse-seringue (ADR 0002), pour développer et éprouver le logiciel
// avant le matériel (#21). Les équations, les paramètres et les limites du modèle sont décrits
// dans docs/development/simulator.md : le modèle ne remplace pas les essais sur banc.

export namespace clepsydre::sim {

// Mécanisme d'entraînement : moteur pas à pas, vis irréversible, écrou et pousseur.
struct Mechanism {
    Length screwLead;                // avance de l'écrou par tour de vis
    std::int32_t stepsPerRevolution; // pas moteur par tour, micro-pas compris
    Length backlash;                 // jeu entre l'écrou et le pousseur
    StepRate
        maxStepRate; // vitesse bornée par le matériel, indépendamment du logiciel (SYS-REQ-043)
    Length positionResolution; // résolution du capteur de position du piston du canal de sécurité
    Force forceResolution;     // résolution du capteur de force du pousseur
};

// Modèle de seringue de la liste des seringues vérifiées (SYS-REQ-047).
struct SyringeModel {
    std::string_view name;
    Length innerDiameter;       // diamètre intérieur du corps : fixe la section
    Length outerDiameter;       // diamètre extérieur, mesuré par la pompe (SYS-REQ-047)
    Volume usableVolume;        // volume délivrable sur la course du piston
    Stiffness stopperStiffness; // raideur du joint du piston, entre le pousseur et le liquide
    Compliance compliance;      // compliance du corps de la seringue et du prolongateur
    Force breakawayFriction;    // frottement statique du joint sur le corps
    Force slidingFriction;      // frottement dynamique, inférieur au frottement statique
};

// Ligne en aval de la seringue : prolongateur, cathéter et patient.
struct Line {
    Resistance resistance;    // résistance hydraulique de la ligne et du cathéter
    Pressure patientPressure; // pression au point d'injection
    Length syringeHeight;     // hauteur de la seringue au-dessus du point d'injection ; négative en
                              // dessous
    Volume internalVolume;    // volume intérieur de la ligne, de la seringue au patient
    Volume
        airDetectorVolume; // volume de ligne entre la seringue et le détecteur d'air (SYS-REQ-007)
};

// Défaillance du pilote moteur, en aval de l'autorisation (SYS-REQ-045).
enum class DriverFault : std::uint8_t {
    none,
    lostSteps, // une fraction des pas commandés n'est pas exécutée
    stall,     // le moteur cale : aucun pas n'est exécuté
    runaway,   // le moteur tourne à la vitesse maximale, quelle que soit la commande
};

// Défaillance d'un capteur.
enum class SensorFaultKind : std::uint8_t {
    none,
    stuck,  // la mesure reste figée à sa valeur au moment du défaut
    offset, // la mesure est décalée de SensorFault::offset
    lost,   // aucune mesure
};

template <typename Q>
struct SensorFault {
    SensorFaultKind kind = SensorFaultKind::none;
    Q offset{};
};

// Position de la seringue dans le berceau (SYS-REQ-006).
enum class SyringePlacement : std::uint8_t {
    seated,    // corps tenu dans le berceau
    misplaced, // corps non tenu : le pousseur entraîne la seringue entière sans délivrer
    absent,    // aucune seringue
};

// Paramètres d'exemple : ordres de grandeur, non mesurés. Les valeurs réelles viendront
// des bancs de précision et d'occlusion (#73, #74).
inline constexpr Mechanism exampleMechanism{
    .screwLead = millimetres(1.0),
    .stepsPerRevolution = 200 * 16,
    .backlash = millimetres(0.05),
    .maxStepRate = stepsPerSecond(2000.0),
    .positionResolution = micrometres(1.0),
    .forceResolution = newtons(0.01),
};

inline constexpr std::array exampleSyringes{
    SyringeModel{.name = "exemple 10 mL",
                 .innerDiameter = millimetres(14.5),
                 .outerDiameter = millimetres(16.5),
                 .usableVolume = millilitres(10.0),
                 .stopperStiffness = newtonsPerMillimetre(15.0),
                 .compliance = microlitresPerKilopascal(0.3),
                 .breakawayFriction = newtons(4.0),
                 .slidingFriction = newtons(3.0)},
    SyringeModel{.name = "exemple 20 mL",
                 .innerDiameter = millimetres(19.1),
                 .outerDiameter = millimetres(21.5),
                 .usableVolume = millilitres(20.0),
                 .stopperStiffness = newtonsPerMillimetre(20.0),
                 .compliance = microlitresPerKilopascal(0.6),
                 .breakawayFriction = newtons(6.0),
                 .slidingFriction = newtons(4.5)},
    SyringeModel{.name = "exemple 50 mL",
                 .innerDiameter = millimetres(26.6),
                 .outerDiameter = millimetres(29.5),
                 .usableVolume = millilitres(50.0),
                 .stopperStiffness = newtonsPerMillimetre(30.0),
                 .compliance = microlitresPerKilopascal(1.2),
                 .breakawayFriction = newtons(10.0),
                 .slidingFriction = newtons(7.0)},
};

// Ligne d'exemple : 5 kPa de chute de pression à 100 mL/h ; pression veineuse de 1 kPa.
inline constexpr Line exampleLine{
    .resistance = hydraulicResistance(kilopascals(5.0), millilitresPerHour(100.0)),
    .patientPressure = kilopascals(1.0),
    .syringeHeight = millimetres(0.0),
    .internalVolume = millilitres(1.0),
    .airDetectorVolume = microlitres(100.0),
};

// Pousse-seringue simulé, à pas de temps fixe et déterministe : deux exécutions avec les
// mêmes commandes donnent les mêmes résultats.
//
// État initial : seringue pleine et en place, piston capturé, pousseur au contact du piston,
// moteur autorisé, aucun défaut, et jeu de l'écrou entièrement ouvert (cas le plus défavorable
// pour le démarrage, CTRL-027).
class SyringePump {
public:
    SyringePump(const Mechanism& mechanism, const SyringeModel& syringe, const Line& line) noexcept;

    // Commandes.
    void setStepRate(StepRate rate) noexcept; // signée ; bornée par Mechanism::maxStepRate

    // Validation matérielle du pilote : sans elle, le moteur ne tourne pas (SYS-REQ-045).
    void setMotorEnabled(bool enabled) noexcept { motorEnabled_ = enabled; }

    void setPlungerCaptured(bool captured) noexcept;
    void setLine(const Line& line) noexcept;

    // Défauts.
    // lostFraction : part des pas perdus, entre 0 et 1, pour DriverFault::lostSteps.
    void setDriverFault(DriverFault fault, double lostFraction = 0.0) noexcept;
    void setForceSensorFault(SensorFault<Force> fault) noexcept;
    void setPositionSensorFault(SensorFault<Length> fault) noexcept;
    void setDiameterSensorFault(SensorFault<Length> fault) noexcept;
    // Remettre une seringue absente la place au contact du pousseur, sans compression.
    void setSyringePlacement(SyringePlacement placement) noexcept;
    // Bulle d'air entrant dans la ligne à la sortie de la seringue.
    void injectAir(Volume bubble);

    // Avance la simulation de dt ; un pas de 1 ms au plus est recommandé.
    void step(std::chrono::nanoseconds dt) noexcept;
    // Avance de duration par pas de dt au plus.
    void run(std::chrono::nanoseconds duration,
             std::chrono::nanoseconds dt = std::chrono::milliseconds{1}) noexcept;

    // Mesures accessibles au logiciel, quantifiées à la résolution des capteurs ; aucune valeur
    // si le capteur est perdu.
    // Pas commandés, comptés par le processeur de commande ; le moteur peut en exécuter d'autres.
    [[nodiscard]] std::int64_t motorSteps() const noexcept { return commandedSteps_; }

    [[nodiscard]] std::optional<Force> measuredForce() const noexcept;
    // Position du piston par rapport au bâti, et non du pousseur : le joint les sépare (ADR 0003,
    // SYS-REQ-001). Elle suit aussi une seringue mal tenue que le pousseur entraîne.
    [[nodiscard]] std::optional<Length> measuredPlungerPosition() const noexcept;
    // Diamètre extérieur ; aucune valeur sans seringue.
    [[nodiscard]] std::optional<Length> measuredSyringeDiameter() const noexcept;

    [[nodiscard]] bool plungerCaptured() const noexcept {
        return plungerCaptured_ && placement_ != SyringePlacement::absent;
    }

    [[nodiscard]] bool syringeSeated() const noexcept {
        return placement_ == SyringePlacement::seated;
    }

    // Une bulle se trouve devant le détecteur d'air.
    [[nodiscard]] bool airDetected() const noexcept;

    // Grandeurs réelles du modèle, pour les vérifications ; le logiciel n'y a pas accès.
    [[nodiscard]] std::chrono::nanoseconds elapsed() const noexcept { return elapsed_; }

    [[nodiscard]] StepRate stepRate() const noexcept { return stepRate_; }

    // Pas réellement exécutés par le moteur.
    [[nodiscard]] std::int64_t rotorSteps() const noexcept { return rotorSteps_; }

    // Le moteur a tourné pendant le dernier pas.
    [[nodiscard]] bool motorRunning() const noexcept { return motorRunning_; }

    [[nodiscard]] Length nutPosition() const noexcept;

    [[nodiscard]] Length pusherPosition() const noexcept { return pusherPosition_; }

    [[nodiscard]] Length plungerPosition() const noexcept { return plungerPosition_; }

    [[nodiscard]] Force contactForce() const noexcept;
    [[nodiscard]] Pressure pressure() const noexcept;

    // Volume sorti de la seringue vers la ligne.
    [[nodiscard]] Volume deliveredVolume() const noexcept { return delivered_; }

    // Air arrivé au patient.
    [[nodiscard]] Volume airDelivered() const noexcept;

    [[nodiscard]] Volume remainingVolume() const noexcept;
    [[nodiscard]] bool plungerAtEnd() const noexcept;

private:
    struct Bubble {
        Volume enteredAt; // volume sorti de la seringue à l'entrée de la bulle dans la ligne
        Volume volume;
    };

    [[nodiscard]] double rotorRate() const noexcept;
    [[nodiscard]] Length pusherInBarrel() const noexcept;
    void movePusher() noexcept;
    void movePlunger() noexcept;
    void dischargeLine(double dtSeconds) noexcept;
    [[nodiscard]] Force netForceOnPlunger(Length plunger) const noexcept;
    [[nodiscard]] Pressure pressureAt(Length plunger) const noexcept;
    [[nodiscard]] Pressure outsidePressure() const noexcept;
    [[nodiscard]] double area() const noexcept;
    [[nodiscard]] Length stroke() const noexcept;

    Mechanism mechanism_;
    SyringeModel syringe_;
    Line line_;

    StepRate stepRate_;
    double commandPhase_ = 0.0;
    double rotorPhase_ = 0.0;
    std::int64_t commandedSteps_ = 0;
    std::int64_t rotorSteps_ = 0;
    bool motorEnabled_ = true;
    bool motorRunning_ = false;
    DriverFault driverFault_ = DriverFault::none;
    double lostFraction_ = 0.0;
    SensorFault<Force> forceFault_;
    SensorFault<Length> positionFault_;
    SensorFault<Length> diameterFault_;
    Force stuckForce_;
    Length stuckPosition_;
    Length stuckDiameter_;
    SyringePlacement placement_ = SyringePlacement::seated;
    std::vector<Bubble> bubbles_;
    Length pusherPosition_;  // par rapport au bâti
    Length barrelOffset_;    // déplacement du corps de la seringue par rapport au bâti
    Length plungerPosition_; // déplacement depuis la seringue pleine, par rapport au corps
    Volume delivered_;       // volume sorti de la seringue vers le patient
    Pressure initialPressure_;
    bool plungerCaptured_ = true;
    bool sliding_ = false;
    std::chrono::nanoseconds elapsed_{};
};

} // namespace clepsydre::sim
