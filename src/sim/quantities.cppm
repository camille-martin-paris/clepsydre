// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <compare>

export module clepsydre.sim.quantities;

// Grandeurs physiques du simulateur, en unités SI et en virgule flottante.
// Le simulateur s'exécute sur l'hôte ; le logiciel embarqué utilisera ses propres types,
// en arithmétique entière ou à virgule fixe (SW-REQ-005).

export namespace clepsydre::sim {

// Valeur en unité SI, distinguée par son type : deux grandeurs de nature différente ne se
// mélangent pas sans conversion explicite. Tag n'est jamais défini.
template <typename Tag>
class Quantity {
public:
    constexpr Quantity() noexcept = default;

    [[nodiscard]] static constexpr Quantity fromSi(double value) noexcept {
        return Quantity{value};
    }

    [[nodiscard]] constexpr double si() const noexcept { return value_; }

    [[nodiscard]] constexpr Quantity operator+(Quantity other) const noexcept {
        return Quantity{value_ + other.value_};
    }

    [[nodiscard]] constexpr Quantity operator-(Quantity other) const noexcept {
        return Quantity{value_ - other.value_};
    }

    [[nodiscard]] constexpr Quantity operator-() const noexcept { return Quantity{-value_}; }

    [[nodiscard]] constexpr Quantity operator*(double factor) const noexcept {
        return Quantity{value_ * factor};
    }

    [[nodiscard]] constexpr Quantity operator/(double divisor) const noexcept {
        return Quantity{value_ / divisor};
    }

    // Rapport sans dimension de deux grandeurs de même nature.
    [[nodiscard]] constexpr double operator/(Quantity other) const noexcept {
        return value_ / other.value_;
    }

    constexpr Quantity& operator+=(Quantity other) noexcept {
        value_ += other.value_;
        return *this;
    }

    constexpr Quantity& operator-=(Quantity other) noexcept {
        value_ -= other.value_;
        return *this;
    }

    constexpr auto operator<=>(const Quantity&) const noexcept = default;

private:
    constexpr explicit Quantity(double value) noexcept : value_{value} {}

    double value_ = 0.0;
};

struct LengthTag;
struct VolumeTag;
struct ForceTag;
struct PressureTag;
struct VolumeFlowTag;
struct StiffnessTag;
struct ComplianceTag;
struct ResistanceTag;
struct StepRateTag;

using Length = Quantity<LengthTag>;         // m
using Volume = Quantity<VolumeTag>;         // m³
using Force = Quantity<ForceTag>;           // N
using Pressure = Quantity<PressureTag>;     // Pa, relative à la pression atmosphérique
using VolumeFlow = Quantity<VolumeFlowTag>; // m³/s
using Stiffness = Quantity<StiffnessTag>;   // N/m
using Compliance = Quantity<ComplianceTag>; // m³/Pa
using Resistance = Quantity<ResistanceTag>; // Pa·s/m³
using StepRate = Quantity<StepRateTag>;     // pas moteur par seconde

[[nodiscard]] constexpr Length millimetres(double value) noexcept {
    return Length::fromSi(value * 1e-3);
}

[[nodiscard]] constexpr Length micrometres(double value) noexcept {
    return Length::fromSi(value * 1e-6);
}

[[nodiscard]] constexpr Volume millilitres(double value) noexcept {
    return Volume::fromSi(value * 1e-6);
}

[[nodiscard]] constexpr Volume microlitres(double value) noexcept {
    return Volume::fromSi(value * 1e-9);
}

[[nodiscard]] constexpr Force newtons(double value) noexcept {
    return Force::fromSi(value);
}

[[nodiscard]] constexpr Pressure kilopascals(double value) noexcept {
    return Pressure::fromSi(value * 1e3);
}

[[nodiscard]] constexpr VolumeFlow millilitresPerHour(double value) noexcept {
    return VolumeFlow::fromSi(value * 1e-6 / 3600.0);
}

[[nodiscard]] constexpr Stiffness newtonsPerMillimetre(double value) noexcept {
    return Stiffness::fromSi(value * 1e3);
}

[[nodiscard]] constexpr Compliance microlitresPerKilopascal(double value) noexcept {
    return Compliance::fromSi(value * 1e-9 / 1e3);
}

[[nodiscard]] constexpr StepRate stepsPerSecond(double value) noexcept {
    return StepRate::fromSi(value);
}

// Résistance hydraulique d'une ligne qui laisse passer le débit flow sous la chute de pression
// drop.
[[nodiscard]] constexpr Resistance hydraulicResistance(Pressure drop, VolumeFlow flow) noexcept {
    return Resistance::fromSi(drop.si() / flow.si());
}

[[nodiscard]] constexpr double inMillimetres(Length value) noexcept {
    return value.si() * 1e3;
}

[[nodiscard]] constexpr double inMillilitres(Volume value) noexcept {
    return value.si() * 1e6;
}

[[nodiscard]] constexpr double inMicrolitres(Volume value) noexcept {
    return value.si() * 1e9;
}

[[nodiscard]] constexpr double inNewtons(Force value) noexcept {
    return value.si();
}

[[nodiscard]] constexpr double inKilopascals(Pressure value) noexcept {
    return value.si() * 1e-3;
}

[[nodiscard]] constexpr double inMillilitresPerHour(VolumeFlow value) noexcept {
    return value.si() * 3600.0 * 1e6;
}

} // namespace clepsydre::sim
