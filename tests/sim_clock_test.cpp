// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

// Tests des horloges simulées (#21, ADR 0006). Ils vérifient le modèle des horloges ; ils ne
// vérifient aucune exigence de la pompe.

#include <chrono>
#include <expect.hpp>
#include <format>

import clepsydre.sim.clock;

namespace {

using namespace std::chrono_literals;
using clepsydre::sim::CivilClock;
using clepsydre::sim::CivilTime;
using clepsydre::sim::driftOver;
using clepsydre::sim::MonotonicClock;
using clepsydre::test::Expectations;

constexpr std::chrono::days month{30};

void drift(Expectations& t) {
    // 23 ppm pendant 30 jours : la dérive tolérée par SYS-REQ-035, à peu près 1 min par mois.
    t.expect(driftOver(month, 23'000) == 59'616ms, "23 ppm sur 30 jours donnent 59,616 s");
    t.expect(driftOver(1500ms, 2'000) == 3us, "secondes entières et reste comptés");
    t.expect(driftOver(1s, -100'000) == -100us, "dérive négative");
    // Dix ans à 1000 ppm : pas de débordement des entiers de 64 bits.
    constexpr std::chrono::seconds decade{315'360'000};
    t.expect(driftOver(decade, 1'000'000) == std::chrono::seconds{315'360},
             "pas de débordement sur dix ans");
}

void driftBounds(Expectations& t) {
    // Cas limite de la relecture : 999 ms à 10^10 ppb faisaient déborder le produit
    // intermédiaire. L'écart est ramené à la borne, et le calcul reste exact.
    using clepsydre::sim::maxDriftPpb;
    t.expect(driftOver(999ms, 10'000'000'000) == driftOver(999ms, maxDriftPpb),
             "écart au-delà de la borne ramené à la borne");
    t.expect(driftOver(999ms, maxDriftPpb) == 999ms - 1ns, "999 ms à la borne : exact");
    t.expect(driftOver(999ms, -10'000'000'000) == -(999ms - 1ns), "borne négative");
    // Durée maximale et écart maximal, dans les deux signes : aucun débordement.
    constexpr auto longest = std::chrono::nanoseconds::max();
    t.expect(driftOver(longest, maxDriftPpb) < longest && driftOver(longest, maxDriftPpb) > 0ns,
             "durée maximale, écart maximal");
    t.expect(driftOver(-longest, -maxDriftPpb) < longest, "durée et écart négatifs maximaux");
    const MonotonicClock fast{0ns, 10'000'000'000};
    t.expect(fast.driftPpb() == maxDriftPpb, "le compteur ramène son écart à la borne");
    t.expect(fast.read(1s) == 2s - 1us, "presque deux fois trop vite, tronqué à la période");
}

void monotonic(Expectations& t) {
    const MonotonicClock clock{5s, 50'000};
    t.expect(clock.read(0ns) == 5s, "décalage au temps simulé nul");
    t.expect(clock.read(10s) == 15s + 500us, "50 ppm d'avance sur 10 s");
    t.expect(clock.read(1500ns) == 5s + 1us, "valeur tronquée à la période du compteur");

    const MonotonicClock slow{0ns, -100'000};
    bool increasing = true;
    auto previous = slow.read(0ns);
    for (auto now = 1ms; now <= 2s; now += 1ms) {
        const auto value = slow.read(now);
        increasing = increasing && value > previous;
        previous = value;
    }
    t.expect(increasing, "un compteur en retard reste strictement croissant");
    t.expect(slow.read(1s) == 999'900us, "100 ppm de retard sur 1 s");
}

void independentClocks(Expectations& t) {
    // Deux processeurs, deux oscillateurs : leurs compteurs divergent sans jamais se recaler.
    const MonotonicClock control{0ns, 50'000};
    const MonotonicClock safety{2s, -50'000};
    const auto gap = [&](std::chrono::nanoseconds now) {
        return control.read(now) - safety.read(now);
    };
    t.expect(gap(0ns) == -2s, "décalages initiaux distincts");
    t.expect(gap(10min) == -2s + 60ms,
             std::format("100 ppm d'écart relatif sur 10 min donnent 60 ms ({})",
                         std::chrono::duration<double>{gap(10min)}));
}

void civilClock(Expectations& t) {
    const CivilTime start = std::chrono::sys_days{std::chrono::year{2026} / 10 / 10} + 12h;
    CivilClock clock{0ns, start, 23'000};
    t.expect(clock.read(500ms).time == start, "heure à la seconde, tronquée");
    t.expect(!clock.read(0ns).oscillatorStopped, "oscillateur en marche au départ");
    t.expect(clock.read(month).time == start + month + 59s, "dérive de 59,616 s en 30 jours");

    // Saut : l'heure change d'un coup, sans que l'oscillateur s'arrête.
    const auto beforeJump = clock.read(1h).time;
    clock.jump(1h, 3600s);
    t.expect(clock.read(1h).time == beforeJump + 3600s, "saut d'une heure");
    t.expect(!clock.read(1h).oscillatorStopped, "un saut ne lève pas l'indicateur d'arrêt");

    // Réserve épuisée : l'heure se fige et l'indicateur se lève.
    clock.stopOscillator(2h);
    const auto frozen = clock.read(2h).time;
    t.expect(clock.read(5h).time == frozen, "heure figée après l'arrêt de l'oscillateur");
    t.expect(clock.read(5h).oscillatorStopped, "indicateur d'arrêt levé");

    // Mise à l'heure par un technicien : l'oscillateur repart et l'indicateur est effacé.
    const CivilTime technician = start + 6h;
    clock.set(6h, technician);
    t.expect(clock.read(6h + 10s).time == technician + 10s, "l'heure repart de la mise à l'heure");
    t.expect(!clock.read(6h).oscillatorStopped, "indicateur effacé par la mise à l'heure");
}

} // namespace

int main() {
    return Expectations{}.run([](Expectations& t) {
        drift(t);
        driftBounds(t);
        monotonic(t);
        independentClocks(t);
        civilClock(t);
    });
}
