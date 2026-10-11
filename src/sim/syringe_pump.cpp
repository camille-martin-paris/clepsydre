// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>

module clepsydre.sim.syringe_pump;

namespace clepsydre::sim {

namespace {

// Perfusat assimilé à de l'eau.
constexpr double density = 1000.0;  // kg/m³
constexpr double gravity = 9.80665; // m/s²

[[nodiscard]] double quantize(double value, double resolution) noexcept {
    return std::round(value / resolution) * resolution;
}

// Mesure rendue par un capteur, selon son défaut ; stuck est la valeur figée.
template <typename Q>
[[nodiscard]] std::optional<Q> sensed(const SensorFault<Q>& fault, Q measured, Q stuck) noexcept {
    switch (fault.kind) {
    case SensorFaultKind::none:
        return measured;
    case SensorFaultKind::stuck:
        return stuck;
    case SensorFaultKind::offset:
        return measured + fault.offset;
    case SensorFaultKind::lost:
        return std::nullopt;
    }
    return std::nullopt;
}

} // namespace

SyringePump::SyringePump(const Mechanism& mechanism, const SyringeModel& syringe,
                         const Line& line) noexcept
    : mechanism_{mechanism}, syringe_{syringe}, line_{line} {
    // Liquide au repos, en équilibre avec la ligne : aucun écoulement transitoire au départ.
    initialPressure_ = outsidePressure();
}

void SyringePump::setStepRate(StepRate rate) noexcept {
    stepRate_ = std::clamp(rate, -mechanism_.maxStepRate, mechanism_.maxStepRate);
}

void SyringePump::setPlungerCaptured(bool captured) noexcept {
    plungerCaptured_ = captured;
}

void SyringePump::setLine(const Line& line) noexcept {
    line_ = line;
}

void SyringePump::setDriverFault(DriverFault fault, double lostFraction) noexcept {
    driverFault_ = fault;
    lostFraction_ = std::clamp(lostFraction, 0.0, 1.0);
}

void SyringePump::setForceSensorFault(SensorFault<Force> fault) noexcept {
    stuckForce_ = Force::fromSi(quantize(contactForce().si(), mechanism_.forceResolution.si()));
    forceFault_ = fault;
}

void SyringePump::setPositionSensorFault(SensorFault<Length> fault) noexcept {
    stuckPosition_ = Length::fromSi(
        quantize((barrelOffset_ + plungerPosition_).si(), mechanism_.positionResolution.si()));
    positionFault_ = fault;
}

void SyringePump::setDiameterSensorFault(SensorFault<Length> fault) noexcept {
    stuckDiameter_ = syringe_.outerDiameter;
    diameterFault_ = fault;
}

void SyringePump::setSyringePlacement(SyringePlacement placement) noexcept {
    if (placement_ == SyringePlacement::absent && placement != SyringePlacement::absent) {
        // Seringue remise au contact du pousseur, sans compression du joint.
        barrelOffset_ = pusherPosition_ - plungerPosition_;
    }
    placement_ = placement;
}

void SyringePump::injectAir(Volume bubble) {
    bubbles_.push_back({.enteredAt = delivered_, .volume = bubble});
}

void SyringePump::step(std::chrono::nanoseconds dt) noexcept {
    if (dt <= std::chrono::nanoseconds::zero()) {
        return;
    }
    elapsed_ += dt;
    const double dtSeconds = std::chrono::duration<double>{dt}.count();

    // Pas commandés et pas exécutés : les pas entiers comptent, la fraction est reportée au pas
    // suivant. Sans défaut ni coupure, les deux suites sont identiques.
    commandPhase_ += stepRate_.si() * dtSeconds;
    const double commanded = std::trunc(commandPhase_);
    commandedSteps_ += static_cast<std::int64_t>(commanded);
    commandPhase_ -= commanded;

    const double rate = rotorRate();
    motorRunning_ = rate != 0.0;
    rotorPhase_ += rate * dtSeconds;
    const double executed = std::trunc(rotorPhase_);
    rotorSteps_ += static_cast<std::int64_t>(executed);
    rotorPhase_ -= executed;

    movePusher();
    if (placement_ == SyringePlacement::absent) {
        return;
    }
    movePlunger();
    dischargeLine(dtSeconds);
}

void SyringePump::run(std::chrono::nanoseconds duration, std::chrono::nanoseconds dt) noexcept {
    if (dt <= std::chrono::nanoseconds::zero()) {
        return;
    }
    while (duration > std::chrono::nanoseconds::zero()) {
        const auto current = std::min(duration, dt);
        step(current);
        duration -= current;
    }
}

Length SyringePump::nutPosition() const noexcept {
    // L'écrou part du fond du jeu : il doit parcourir le jeu avant de pousser.
    const double perStep =
        mechanism_.screwLead.si() / static_cast<double>(mechanism_.stepsPerRevolution);
    return Length::fromSi(static_cast<double>(rotorSteps_) * perStep) - mechanism_.backlash;
}

std::optional<Force> SyringePump::measuredForce() const noexcept {
    const Force measured =
        Force::fromSi(quantize(contactForce().si(), mechanism_.forceResolution.si()));
    return sensed(forceFault_, measured, stuckForce_);
}

std::optional<Length> SyringePump::measuredPlungerPosition() const noexcept {
    if (placement_ == SyringePlacement::absent) {
        return std::nullopt;
    }
    const Length measured = Length::fromSi(
        quantize((barrelOffset_ + plungerPosition_).si(), mechanism_.positionResolution.si()));
    return sensed(positionFault_, measured, stuckPosition_);
}

std::optional<Length> SyringePump::measuredSyringeDiameter() const noexcept {
    if (placement_ == SyringePlacement::absent) {
        return std::nullopt;
    }
    return sensed(diameterFault_, syringe_.outerDiameter, stuckDiameter_);
}

bool SyringePump::airDetected() const noexcept {
    const Volume detector = line_.airDetectorVolume;
    return std::ranges::any_of(bubbles_, [&](const Bubble& bubble) {
        // La bulle occupe la ligne de front - volume à front, comptés depuis la seringue.
        const Volume front = delivered_ - bubble.enteredAt;
        return front >= detector && front - bubble.volume <= detector;
    });
}

Volume SyringePump::airDelivered() const noexcept {
    Volume total;
    for (const Bubble& bubble : bubbles_) {
        const Volume beyond = delivered_ - bubble.enteredAt - line_.internalVolume;
        total += std::clamp(beyond, Volume{}, bubble.volume);
    }
    return total;
}

Force SyringePump::contactForce() const noexcept {
    if (placement_ == SyringePlacement::absent) {
        return Force{};
    }
    const double compression = (pusherInBarrel() - plungerPosition_).si();
    // Un piston non capturé n'est que poussé ; capturé, il peut aussi être tiré.
    const double effective = plungerCaptured_ ? compression : std::max(compression, 0.0);
    return Force::fromSi(syringe_.stopperStiffness.si() * effective);
}

Pressure SyringePump::pressure() const noexcept {
    return pressureAt(plungerPosition_);
}

Volume SyringePump::remainingVolume() const noexcept {
    return syringe_.usableVolume - Volume::fromSi(area() * plungerPosition_.si());
}

bool SyringePump::plungerAtEnd() const noexcept {
    return plungerPosition_ >= stroke();
}

double SyringePump::rotorRate() const noexcept {
    // La validation matérielle prime : sans elle, même un pilote emballé ne fait pas tourner le
    // moteur (SYS-REQ-045).
    if (!motorEnabled_) {
        return 0.0;
    }
    switch (driverFault_) {
    case DriverFault::none:
        return stepRate_.si();
    case DriverFault::lostSteps:
        return stepRate_.si() * (1.0 - lostFraction_);
    case DriverFault::stall:
        return 0.0;
    case DriverFault::runaway:
        return mechanism_.maxStepRate.si();
    }
    return 0.0;
}

Length SyringePump::pusherInBarrel() const noexcept {
    return pusherPosition_ - barrelOffset_;
}

void SyringePump::movePusher() noexcept {
    // Le pousseur est libre dans le jeu de l'écrou ; la vis irréversible ne recule pas sous charge.
    const Length before = pusherPosition_;
    const Length nut = nutPosition();
    pusherPosition_ = std::clamp(pusherPosition_, nut, nut + mechanism_.backlash);
    switch (placement_) {
    case SyringePlacement::seated: {
        // La charge du piston plaque le pousseur contre le flanc de l'écrou qui la retient.
        const Length plunger = barrelOffset_ + plungerPosition_;
        const Force load = contactForce();
        if (load > Force{}) {
            pusherPosition_ = std::max(nut, plunger);
        } else if (load < Force{}) {
            pusherPosition_ = std::min(nut + mechanism_.backlash, plunger);
        }
        break;
    }
    case SyringePlacement::misplaced:
        // Corps non tenu : la seringue entière suit le pousseur, sans délivrer.
        barrelOffset_ += pusherPosition_ - before;
        break;
    case SyringePlacement::absent:
        break;
    }
}

void SyringePump::movePlunger() noexcept {
    // Piston sans masse : il reste immobile tant que la force nette ne dépasse pas le frottement
    // statique, puis glisse jusqu'à la position où elle égale le frottement dynamique.
    const Force net = netForceOnPlunger(plungerPosition_);
    const Force threshold = sliding_ ? syringe_.slidingFriction : syringe_.breakawayFriction;
    if (std::abs(net.si()) <= threshold.si()) {
        sliding_ = false;
        return;
    }
    const double direction = net > Force{} ? 1.0 : -1.0;
    const double a = area();
    const double c = syringe_.compliance.si();
    const double k = syringe_.stopperStiffness.si();
    const double p0 = initialPressure_.si();
    const double v = delivered_.si();
    const double target = direction * syringe_.slidingFriction.si();

    // Force nette affine en la position x du piston, au contact du pousseur :
    // k (xp - x) - a (p0 + (a x - v) / c) = target.
    const double pusher = pusherInBarrel().si();
    double position = (k * pusher - a * p0 + a * v / c - target) / (k + a * a / c);
    if (!plungerCaptured_ && position > pusher) {
        // Sans contact, seule la pression agit sur le piston : -a (p0 + (a x - v) / c) = target.
        position = (c * (-target / a - p0) + v) / a;
    }
    const Length next = std::clamp(Length::fromSi(position), Length{}, stroke());
    sliding_ = next != plungerPosition_;
    plungerPosition_ = next;
}

void SyringePump::dischargeLine(double dtSeconds) noexcept {
    // Le liquide s'écoule vers le patient à travers la résistance de la ligne ; piston immobile
    // pendant le pas, la décharge de la compliance est exponentielle, donc stable à tout pas.
    const double c = syringe_.compliance.si();
    const double toEquilibrium = (pressure() - outsidePressure()).si() * c;
    const double tau = line_.resistance.si() * c;
    delivered_ += Volume::fromSi(toEquilibrium * -std::expm1(-dtSeconds / tau));
}

Force SyringePump::netForceOnPlunger(Length plunger) const noexcept {
    const double compression = (pusherInBarrel() - plunger).si();
    const double effective = plungerCaptured_ ? compression : std::max(compression, 0.0);
    return Force::fromSi((syringe_.stopperStiffness.si() * effective) -
                         (area() * pressureAt(plunger).si()));
}

Pressure SyringePump::outsidePressure() const noexcept {
    // Pression vue depuis la seringue : celle du patient, diminuée de la colonne de liquide
    // quand la seringue est au-dessus du point d'injection (siphonage).
    return line_.patientPressure - Pressure::fromSi(density * gravity * line_.syringeHeight.si());
}

Pressure SyringePump::pressureAt(Length plunger) const noexcept {
    // Le volume chassé par le piston et non encore sorti est stocké dans la compliance.
    const double stored = (area() * plunger.si()) - delivered_.si();
    return initialPressure_ + Pressure::fromSi(stored / syringe_.compliance.si());
}

double SyringePump::area() const noexcept {
    const double radius = syringe_.innerDiameter.si() / 2.0;
    return std::numbers::pi * radius * radius;
}

Length SyringePump::stroke() const noexcept {
    return Length::fromSi(syringe_.usableVolume.si() / area());
}

} // namespace clepsydre::sim
