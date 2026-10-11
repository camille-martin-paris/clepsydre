// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <chrono>
#include <optional>

export module clepsydre.sim.power;

export import clepsydre.sim.quantities;

// Alimentation simulée (ADR 0004) : bloc secteur externe, batterie et bascule matérielle entre
// les deux, superviseur de tension et réserve de maintien.

export namespace clepsydre::sim {

// Batterie : tension à vide affine en fonction de l'état de charge, sans résistance interne.
struct Battery {
    Charge capacity;          // capacité nominale
    Voltage full;             // tension à pleine charge
    Voltage empty;            // tension à charge nulle
    Voltage protectionCutoff; // tension sous laquelle le circuit de protection déconnecte
    Current chargeCurrent;    // courant de charge sur secteur
};

struct PowerParameters {
    Battery battery;
    Current idleCurrent;               // consommation de la pompe, moteur à l'arrêt
    Current motorCurrent;              // consommation ajoutée par le moteur en rotation
    std::chrono::nanoseconds holdUp{}; // réserve de maintien après la perte des deux sources
};

// Paramètres d'exemple : ordres de grandeur, non mesurés (ADR 0004, #63).
inline constexpr PowerParameters examplePower{
    .battery = {.capacity = milliampereHours(2000.0),
                .full = volts(13.4),
                .empty = volts(10.0),
                .protectionCutoff = volts(10.4),
                .chargeCurrent = milliamperes(500.0)},
    .idleCurrent = milliamperes(80.0),
    .motorCurrent = milliamperes(250.0),
    .holdUp = std::chrono::milliseconds{50},
};

class PowerSupply {
public:
    explicit PowerSupply(const PowerParameters& parameters, double stateOfCharge = 1.0) noexcept;

    // Avance de dt ; motorRunning ajoute la consommation du moteur.
    void step(std::chrono::nanoseconds dt, bool motorRunning) noexcept;

    // Défauts et événements.
    void setMains(bool present) noexcept { mains_ = present; }

    void setBatteryConnected(bool connected) noexcept { batteryConnected_ = connected; }

    // Vieillissement : la capacité réelle devient factor fois la capacité nominale ; la charge
    // restante est ramenée sous la nouvelle capacité.
    void setCapacityFactor(double factor) noexcept;

    // Capteurs.
    [[nodiscard]] bool mainsPresent() const noexcept { return mains_; }

    // Tension de la batterie ; aucune valeur si elle est déconnectée.
    [[nodiscard]] std::optional<Voltage> measuredBatteryVoltage() const noexcept;

    // Courant moyen de la batterie sur le dernier pas, pour le comptage coulométrique
    // (SYS-REQ-049) : positif en décharge, négatif en charge, nul batterie pleine.
    [[nodiscard]] Current measuredBatteryCurrent() const noexcept { return batteryCurrent_; }

    // Superviseur de tension : les deux sources sont perdues, la réserve de maintien alimente
    // encore la pompe (SYS-REQ-050).
    [[nodiscard]] bool powerFailImminent() const noexcept;

    // Grandeurs réelles.
    // La pompe est alimentée : par une source, ou par la réserve de maintien.
    [[nodiscard]] bool powered() const noexcept;
    [[nodiscard]] bool onBattery() const noexcept;

    [[nodiscard]] double stateOfCharge() const noexcept {
        return capacity_ > Charge{} ? charge_ / capacity_ : 0.0;
    }

    [[nodiscard]] Charge remainingCharge() const noexcept { return charge_; }

private:
    [[nodiscard]] Voltage batteryVoltage() const noexcept;
    // Charge sous laquelle le circuit de protection déconnecte la batterie.
    [[nodiscard]] Charge cutoffCharge() const noexcept;
    [[nodiscard]] bool batteryAvailable() const noexcept;
    [[nodiscard]] bool sourceAvailable() const noexcept;

    PowerParameters parameters_;
    Charge capacity_;
    Charge charge_;
    bool mains_ = true;
    bool batteryConnected_ = true;
    Current batteryCurrent_;
    std::chrono::nanoseconds elapsed_{};
    std::optional<std::chrono::nanoseconds> lostAt_;
};

} // namespace clepsydre::sim
