// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <format>
#include <istream>
#include <memory>
#include <numbers>
#include <optional>
#include <ostream>
#include <print>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

module clepsydre.sim.console;

namespace clepsydre::sim {

namespace {

constexpr double occlusionFactor = 1e6; // résistance de la ligne multipliée en occlusion

// Opérateurs de comparaison de « verifie ».
constexpr std::array<std::string_view, 5> operators{"<", "<=", ">", ">=", "="};

// Mots de la ligne, séparés par des espaces ; un # commence un commentaire.
std::vector<std::string_view> words(std::string_view line) {
    line = line.substr(0, line.find('#'));
    std::vector<std::string_view> result;
    std::size_t position = 0;
    while (position < line.size()) {
        const std::size_t start = line.find_first_not_of(" \t\r", position);
        if (start == std::string_view::npos) {
            break;
        }
        const std::size_t end = std::min(line.find_first_of(" \t\r", start), line.size());
        result.push_back(line.substr(start, end - start));
        position = end;
    }
    return result;
}

// Nombre décimal, avec un point ou une virgule.
std::optional<double> number(std::string_view text) {
    std::string copy{text};
    std::ranges::replace(copy, ',', '.');
    double value = 0.0;
    const char* const end = std::to_address(copy.cend());
    const auto [stop, code] = std::from_chars(copy.data(), end, value);
    if (code != std::errc{} || stop != end || !std::isfinite(value)) {
        return std::nullopt;
    }
    return value;
}

// Durée strictement positive : nombre suivi de ms, s, min ou h.
std::optional<std::chrono::nanoseconds> duration(std::string_view text) {
    const std::size_t unitStart = text.find_first_not_of("0123456789.,");
    if (unitStart == 0 || unitStart == std::string_view::npos) {
        return std::nullopt;
    }
    const auto value = number(text.substr(0, unitStart));
    const std::string_view unit = text.substr(unitStart);
    double seconds = 0.0;
    if (!value) {
        return std::nullopt;
    }
    if (unit == "ms") {
        seconds = *value / 1000.0;
    } else if (unit == "s") {
        seconds = *value;
    } else if (unit == "min") {
        seconds = *value * 60.0;
    } else if (unit == "h") {
        seconds = *value * 3600.0;
    } else {
        return std::nullopt;
    }
    if (seconds <= 0.0 || seconds > 1e7) {
        return std::nullopt;
    }
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::duration<double>{seconds});
}

std::optional<bool> yesNo(std::string_view text) {
    if (text == "oui") {
        return true;
    }
    if (text == "non") {
        return false;
    }
    return std::nullopt;
}

std::string_view yesNo(bool value) {
    return value ? "oui" : "non";
}

// Valeur mesurée, ou un tiret si le capteur ne rend rien.
template <typename Q>
std::string shown(const std::optional<Q>& value, double (*convert)(Q), std::string_view unit) {
    if (!value) {
        return "—";
    }
    return std::format("{:.3f} {}", convert(*value), unit);
}

std::string clock(std::chrono::nanoseconds now) {
    const auto hours = std::chrono::duration_cast<std::chrono::hours>(now);
    const auto minutes = std::chrono::duration_cast<std::chrono::minutes>(now - hours);
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now - hours - minutes);
    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - hours - minutes - seconds);
    return std::format("{:02}:{:02}:{:02}.{:03}", hours.count(), minutes.count(), seconds.count(),
                       milliseconds.count());
}

double inMillimetresOf(Length value) {
    return inMillimetres(value);
}

double inNewtonsOf(Force value) {
    return inNewtons(value);
}

double inVoltsOf(Voltage value) {
    return inVolts(value);
}

constexpr std::string_view help = R"(Commandes (une par ligne ; # commence un commentaire) :
  seringue 10|20|50          choisit une seringue d'exemple et réinitialise la simulation
  frottement <N> <N>         frottements statique et dynamique du joint ; réinitialise
  debit <mL/h>               débit commandé
  recul <durée>              recul du pousseur à la vitesse maximale, puis arrêt
  avance <durée>             fait avancer le temps (ms, s, min, h : 500ms, 30s, 5min, 2h)
  etat                       grandeurs réelles et mesures des capteurs
  repere                     repère le volume délivré pour volume-depuis-repere
  verifie <grandeur> <op> <valeur>
                             op : < <= > >= = ; « verifie liste » liste les grandeurs
Défauts et événements :
  occlusion oui|non          occlusion de la ligne en aval
  hauteur <mm>               hauteur de la seringue au-dessus du patient (siphonage)
  piston capture|libere      capture du piston par le pousseur
  berceau en-place|mal-placee|absente
  air <µL>                   bulle d'air à la sortie de la seringue
  pilote normal|calage|emballement|pertes <fraction>
  capteur force|position|diametre normal|fige|perdu|decale <N ou mm>
  validation oui|non         validation matérielle du pilote moteur
  secteur oui|non            présence du secteur
  batterie oui|non           batterie connectée
  capacite <facteur>         capacité réelle de la batterie, de 0 à 1 (vieillissement)
Autres : aide, quitter)";

constexpr std::string_view measures = R"(Grandeurs pour « verifie » :
  temps (s), volume (mL), volume-depuis-repere (mL), air (µL), pression (kPa),
  force (N), force-mesuree (N), piston (mm), piston-mesure (mm), batterie (%),
  tension (V), courant (mA)
  alimente, sur-batterie, coupure-imminente, moteur, air-detecte, berceau,
  piston-capture : oui ou non)";

// Commandes qui n'agissent pas sur la simulation, ou ne font qu'avancer le temps : elles ne sont
// pas journalisées.
constexpr std::array<std::string_view, 5> unjournaled{"aide", "etat", "verifie", "quitter",
                                                      "avance"};

} // namespace

Console::Console(std::ostream& out, Journal journal)
    : out_{&out}, journal_{journal}, syringe_{exampleSyringes[2]},
      pump_{exampleMechanism, exampleSyringes[2], exampleLine}, power_{examplePower} {
    wire();
}

void Console::reset(const SyringeModel& syringe) {
    syringe_ = syringe;
    pump_ = SyringePump{exampleMechanism, syringe_, exampleLine};
    power_ = PowerSupply{examplePower};
    world_ = Simulation{};
    occluded_ = false;
    height_ = Length{};
    enabled_ = true;
    millilitresPerHour_ = 0.0;
    mark_ = Volume{};
    wire();
}

void Console::wire() {
    wasPowered_ = power_.powered();
    wasOnBattery_ = power_.onBattery();
    wasImminent_ = power_.powerFailImminent();
    wasRunning_ = pump_.motorRunning();
    wasAirDetected_ = pump_.airDetected();
    world_.add([this](TrueTime, std::chrono::nanoseconds dt) {
        power_.step(dt, pump_.motorRunning());
        const bool powered = power_.powered();
        if (!powered && wasPowered_) {
            // Perte totale : au redémarrage, le logiciel de commande ne relance pas le moteur.
            millilitresPerHour_ = 0.0;
            applyStepRate();
        }
        pump_.setMotorEnabled(powered && enabled_);
        pump_.step(dt);
        observe();
    });
}

// Journalise les changements d'état depuis le pas précédent.
void Console::observe() {
    const auto changed = [](bool& previous, bool current) {
        const bool result = previous != current;
        previous = current;
        return result;
    };
    if (changed(wasPowered_, power_.powered())) {
        note(wasPowered_ ? Severity::info : Severity::alert, "alimentation",
             wasPowered_ ? "alimentation rétablie" : "perte totale de l'alimentation");
    }
    if (changed(wasOnBattery_, power_.onBattery())) {
        note(Severity::info, "alimentation",
             wasOnBattery_ ? "bascule sur batterie" : "fin du fonctionnement sur batterie");
    }
    if (changed(wasImminent_, power_.powerFailImminent()) && wasImminent_) {
        note(Severity::alert, "alimentation", "coupure imminente");
    }
    if (changed(wasRunning_, pump_.motorRunning())) {
        note(Severity::info, "moteur", wasRunning_ ? "moteur en marche" : "moteur arrêté");
    }
    if (changed(wasAirDetected_, pump_.airDetected())) {
        note(wasAirDetected_ ? Severity::alert : Severity::info, "air",
             wasAirDetected_ ? "air détecté" : "plus d'air détecté");
    }
}

void Console::note(Severity severity, std::string_view component, std::string_view message) {
    journal_.record(world_.now(), severity, component, message);
}

void Console::advance(std::chrono::nanoseconds duration) {
    world_.run(duration);
}

void Console::applyLine() {
    Line line = exampleLine;
    line.syringeHeight = height_;
    if (occluded_) {
        line.resistance = line.resistance * occlusionFactor;
    }
    pump_.setLine(line);
}

void Console::applyStepRate() {
    const double radius = syringe_.innerDiameter.si() / 2.0;
    const double area = std::numbers::pi * radius * radius;
    const double perStep =
        exampleMechanism.screwLead.si() / static_cast<double>(exampleMechanism.stepsPerRevolution);
    pump_.setStepRate(
        StepRate::fromSi(millilitresPerHour(millilitresPerHour_).si() / area / perStep));
}

Console::Outcome Console::execute(std::string_view line) {
    const std::vector<std::string_view> w = words(line);
    if (w.empty()) {
        return Outcome::ok;
    }
    // La commande est consignée avant son exécution, qui peut faire avancer le temps (recul) ;
    // un refus suit dans le journal.
    if (journal_.enabled() && !std::ranges::contains(unjournaled, w[0])) {
        std::string text;
        for (const std::string_view word : w) {
            text += text.empty() ? "" : " ";
            text += word;
        }
        note(Severity::info, "operateur", text);
    }
    return interpret(w);
}

Console::Outcome Console::interpret(const std::vector<std::string_view>& w) {
    const std::string_view command = w[0];
    const auto argument = [&w](std::size_t index) {
        return index < w.size() ? w[index] : std::string_view{};
    };
    const auto expectCount = [&w](std::size_t count) { return w.size() == count; };

    if (command == "aide" && expectCount(1)) {
        printHelp();
        return Outcome::ok;
    }
    if (command == "quitter" && expectCount(1)) {
        return Outcome::quit;
    }
    if (command == "etat" && expectCount(1)) {
        printStatus();
        return Outcome::ok;
    }
    if (command == "repere" && expectCount(1)) {
        mark_ = pump_.deliveredVolume();
        return Outcome::ok;
    }
    if (command == "seringue" && expectCount(2)) {
        return commandSyringe(argument(1));
    }
    if (command == "frottement" && expectCount(3)) {
        return commandFriction(argument(1), argument(2));
    }
    if (command == "debit" && expectCount(2)) {
        const auto value = number(argument(1));
        if (!value || *value < 0.0) {
            return error("débit attendu, en mL/h, positif ou nul");
        }
        millilitresPerHour_ = *value;
        applyStepRate();
        return Outcome::ok;
    }
    if ((command == "avance" || command == "recul") && expectCount(2)) {
        const auto value = duration(argument(1));
        if (!value) {
            return error("durée attendue : nombre suivi de ms, s, min ou h");
        }
        if (command == "recul") {
            pump_.setStepRate(-exampleMechanism.maxStepRate);
            advance(*value);
            millilitresPerHour_ = 0.0;
            applyStepRate();
        } else {
            advance(*value);
        }
        return Outcome::ok;
    }
    if (command == "occlusion" && expectCount(2)) {
        const auto value = yesNo(argument(1));
        if (!value) {
            return error("oui ou non attendu");
        }
        occluded_ = *value;
        applyLine();
        return Outcome::ok;
    }
    if (command == "hauteur" && expectCount(2)) {
        const auto value = number(argument(1));
        if (!value) {
            return error("hauteur attendue, en mm");
        }
        height_ = millimetres(*value);
        applyLine();
        return Outcome::ok;
    }
    if (command == "piston" && expectCount(2)) {
        if (argument(1) != "capture" && argument(1) != "libere") {
            return error("capture ou libere attendu");
        }
        pump_.setPlungerCaptured(argument(1) == "capture");
        return Outcome::ok;
    }
    if (command == "berceau" && expectCount(2)) {
        return commandPlacement(argument(1));
    }
    if (command == "air" && expectCount(2)) {
        const auto value = number(argument(1));
        if (!value || *value <= 0.0) {
            return error("volume de la bulle attendu, en µL, positif");
        }
        pump_.injectAir(microlitres(*value));
        return Outcome::ok;
    }
    if (command == "pilote" && (expectCount(2) || expectCount(3))) {
        return commandDriver(argument(1), argument(2));
    }
    if (command == "capteur" && (expectCount(3) || expectCount(4))) {
        return commandSensor(argument(1), argument(2), argument(3));
    }
    if ((command == "validation" || command == "secteur" || command == "batterie") &&
        expectCount(2)) {
        const auto value = yesNo(argument(1));
        if (!value) {
            return error("oui ou non attendu");
        }
        if (command == "validation") {
            enabled_ = *value;
        } else if (command == "secteur") {
            power_.setMains(*value);
        } else {
            power_.setBatteryConnected(*value);
        }
        return Outcome::ok;
    }
    if (command == "capacite" && expectCount(2)) {
        const auto value = number(argument(1));
        if (!value || *value < 0.0 || *value > 1.0) {
            return error("facteur de capacité attendu, entre 0 et 1");
        }
        power_.setCapacityFactor(*value);
        return Outcome::ok;
    }
    if (command == "verifie" && expectCount(2) && argument(1) == "liste") {
        std::println(*out_, "{}", measures);
        return Outcome::ok;
    }
    if (command == "verifie" && expectCount(4)) {
        return commandCheck(argument(1), argument(2), argument(3));
    }
    return error(std::format("commande inconnue ou nombre d'arguments incorrect : « {} » ; "
                             "« aide » liste les commandes",
                             command));
}

int Console::run(std::istream& in, bool interactive) {
    if (interactive) {
        std::println(*out_, "Simulateur Clepsydre : « aide » liste les commandes. Le simulateur "
                            "ne remplace pas les essais sur banc.");
    }
    std::string line;
    std::size_t lineNumber = 0;
    while (true) {
        if (interactive) {
            std::print(*out_, "> ");
            out_->flush();
        }
        if (!std::getline(in, line)) {
            if (!in.eof()) {
                // Lecture interrompue (flux en erreur) : le scénario n'a pas été lu en entier.
                std::println(*out_, "Erreur de lecture après la ligne {}.", lineNumber);
                return 2;
            }
            break;
        }
        ++lineNumber;
        if (!interactive) {
            std::println(*out_, "{}", line.empty() ? std::string{} : "> " + line);
        }
        const Outcome outcome = execute(line);
        if (outcome == Outcome::quit) {
            break;
        }
        if (!interactive && outcome != Outcome::ok) {
            std::println(*out_, "Arrêt à la ligne {}.", lineNumber);
            return outcome == Outcome::failed ? 1 : 2;
        }
    }
    return 0;
}

Console::Outcome Console::commandSyringe(std::string_view argument) {
    for (const SyringeModel& syringe : exampleSyringes) {
        if (std::format("exemple {} mL", argument) == syringe.name) {
            reset(syringe);
            std::println(*out_, "Seringue « {} » ; simulation réinitialisée.", syringe.name);
            return Outcome::ok;
        }
    }
    return error("seringue 10, 20 ou 50 attendue");
}

Console::Outcome Console::commandFriction(std::string_view breakaway, std::string_view sliding) {
    const auto staticFriction = number(breakaway);
    const auto dynamicFriction = number(sliding);
    if (!staticFriction || !dynamicFriction || *dynamicFriction <= 0.0 ||
        *dynamicFriction > *staticFriction) {
        return error("frottements statique et dynamique attendus, en N, avec 0 < dynamique <= "
                     "statique");
    }
    SyringeModel syringe = syringe_;
    syringe.breakawayFriction = newtons(*staticFriction);
    syringe.slidingFriction = newtons(*dynamicFriction);
    reset(syringe);
    std::println(*out_, "Frottements de {} N et {} N ; simulation réinitialisée.", *staticFriction,
                 *dynamicFriction);
    return Outcome::ok;
}

Console::Outcome Console::commandDriver(std::string_view mode, std::string_view argument) {
    if (mode == "pertes") {
        const auto fraction = number(argument);
        if (!fraction || *fraction < 0.0 || *fraction > 1.0) {
            return error("fraction de pas perdus attendue, entre 0 et 1");
        }
        pump_.setDriverFault(DriverFault::lostSteps, *fraction);
        return Outcome::ok;
    }
    if (!argument.empty()) {
        return error("pilote normal, calage, emballement ou pertes <fraction> attendu");
    }
    if (mode == "normal") {
        pump_.setDriverFault(DriverFault::none);
    } else if (mode == "calage") {
        pump_.setDriverFault(DriverFault::stall);
    } else if (mode == "emballement") {
        pump_.setDriverFault(DriverFault::runaway);
    } else {
        return error("pilote normal, calage, emballement ou pertes <fraction> attendu");
    }
    return Outcome::ok;
}

Console::Outcome Console::commandSensor(std::string_view sensor, std::string_view mode,
                                        std::string_view argument) {
    SensorFaultKind kind = SensorFaultKind::none;
    double offset = 0.0;
    if (mode == "decale") {
        const auto value = number(argument);
        if (!value) {
            return error("décalage attendu, en N pour la force, en mm sinon");
        }
        kind = SensorFaultKind::offset;
        offset = *value;
    } else if (!argument.empty() || (mode != "normal" && mode != "fige" && mode != "perdu")) {
        return error("capteur <nom> normal, fige, perdu ou decale <valeur> attendu");
    } else if (mode == "fige") {
        kind = SensorFaultKind::stuck;
    } else if (mode == "perdu") {
        kind = SensorFaultKind::lost;
    }
    if (sensor == "force") {
        pump_.setForceSensorFault({.kind = kind, .offset = newtons(offset)});
    } else if (sensor == "position") {
        pump_.setPositionSensorFault({.kind = kind, .offset = millimetres(offset)});
    } else if (sensor == "diametre") {
        pump_.setDiameterSensorFault({.kind = kind, .offset = millimetres(offset)});
    } else {
        return error("capteur force, position ou diametre attendu");
    }
    return Outcome::ok;
}

Console::Outcome Console::commandPlacement(std::string_view argument) {
    if (argument == "en-place") {
        pump_.setSyringePlacement(SyringePlacement::seated);
    } else if (argument == "mal-placee") {
        pump_.setSyringePlacement(SyringePlacement::misplaced);
    } else if (argument == "absente") {
        pump_.setSyringePlacement(SyringePlacement::absent);
    } else {
        return error("berceau en-place, mal-placee ou absente attendu");
    }
    return Outcome::ok;
}

std::optional<double> Console::measure(std::string_view name, bool& known) const {
    known = true;
    const auto flag = [](bool value) { return value ? 1.0 : 0.0; };
    if (name == "temps") {
        return std::chrono::duration<double>{world_.now()}.count();
    }
    if (name == "volume") {
        return inMillilitres(pump_.deliveredVolume());
    }
    if (name == "volume-depuis-repere") {
        return inMillilitres(pump_.deliveredVolume() - mark_);
    }
    if (name == "air") {
        return inMicrolitres(pump_.airDelivered());
    }
    if (name == "pression") {
        return inKilopascals(pump_.pressure());
    }
    if (name == "force") {
        return inNewtons(pump_.contactForce());
    }
    if (name == "force-mesuree") {
        const auto value = pump_.measuredForce();
        return value ? std::optional{inNewtons(*value)} : std::nullopt;
    }
    if (name == "piston") {
        return inMillimetres(pump_.plungerPosition());
    }
    if (name == "piston-mesure") {
        const auto value = pump_.measuredPlungerPosition();
        return value ? std::optional{inMillimetres(*value)} : std::nullopt;
    }
    if (name == "batterie") {
        return power_.stateOfCharge() * 100.0;
    }
    if (name == "tension") {
        const auto value = power_.measuredBatteryVoltage();
        return value ? std::optional{inVolts(*value)} : std::nullopt;
    }
    if (name == "courant") {
        return inMilliamperes(power_.measuredBatteryCurrent());
    }
    if (name == "alimente") {
        return flag(power_.powered());
    }
    if (name == "sur-batterie") {
        return flag(power_.onBattery());
    }
    if (name == "coupure-imminente") {
        return flag(power_.powerFailImminent());
    }
    if (name == "moteur") {
        return flag(pump_.motorRunning());
    }
    if (name == "air-detecte") {
        return flag(pump_.airDetected());
    }
    if (name == "berceau") {
        return flag(pump_.syringeSeated());
    }
    if (name == "piston-capture") {
        return flag(pump_.plungerCaptured());
    }
    known = false;
    return std::nullopt;
}

Console::Outcome Console::commandCheck(std::string_view name, std::string_view operation,
                                       std::string_view expected) {
    bool known = false;
    const std::optional<double> value = measure(name, known);
    if (!known) {
        return error(std::format("grandeur inconnue : « {} » ; « verifie liste » les liste", name));
    }
    // L'opérateur est validé avant la mesure : le diagnostic ne dépend pas de l'état du capteur.
    if (!std::ranges::contains(operators, operation)) {
        return error("opérateur <, <=, >, >= ou = attendu");
    }
    std::optional<double> target = number(expected);
    if (const auto answer = yesNo(expected)) {
        target = *answer ? 1.0 : 0.0;
    }
    if (!target) {
        return error("valeur attendue : nombre, oui ou non");
    }
    bool holds = false;
    if (!value) {
        holds = false; // capteur sans mesure : aucune comparaison ne tient
    } else if (operation == "<") {
        holds = *value < *target;
    } else if (operation == "<=") {
        holds = *value <= *target;
    } else if (operation == ">") {
        holds = *value > *target;
    } else if (operation == ">=") {
        holds = *value >= *target;
    } else { // =
        holds = std::abs(*value - *target) <= 1e-9 * std::max(1.0, std::abs(*target));
    }
    const std::string shownValue = value ? std::format("{:.6g}", *value) : "aucune mesure";
    if (!holds) {
        const std::string message =
            std::format("ÉCHEC : {} vaut {}, attendu {} {}", name, shownValue, operation, expected);
        std::println(*out_, "{}", message);
        note(Severity::alert, "console", message);
        return Outcome::failed;
    }
    std::println(*out_, "ok : {} vaut {}", name, shownValue);
    return Outcome::ok;
}

void Console::printStatus() {
    std::println(*out_, "Temps {}   seringue « {} »   débit commandé {} mL/h", clock(world_.now()),
                 syringe_.name, millilitresPerHour_);
    std::println(*out_,
                 "  Délivré       {:.4f} mL sortis de la seringue ; air au patient {:.1f} µL",
                 inMillilitres(pump_.deliveredVolume()), inMicrolitres(pump_.airDelivered()));
    std::println(*out_, "  Pression      {:.2f} kPa", inKilopascals(pump_.pressure()));
    std::println(*out_, "  Force         réelle {:.3f} N ; mesurée {}",
                 inNewtons(pump_.contactForce()), shown(pump_.measuredForce(), &inNewtonsOf, "N"));
    std::println(*out_,
                 "  Piston        réel {:.3f} mm dans le corps ; mesuré {} par rapport au bâti",
                 inMillimetres(pump_.plungerPosition()),
                 shown(pump_.measuredPlungerPosition(), &inMillimetresOf, "mm"));
    std::println(*out_, "  Seringue      diamètre mesuré {} ; berceau {} ; piston capturé {}",
                 shown(pump_.measuredSyringeDiameter(), &inMillimetresOf, "mm"),
                 pump_.syringeSeated() ? "en place" : "non", yesNo(pump_.plungerCaptured()));
    std::println(*out_, "  Moteur        pas commandés {} ; exécutés {} ; en marche {}",
                 pump_.motorSteps(), pump_.rotorSteps(), yesNo(pump_.motorRunning()));
    std::println(*out_, "  Air           détecté {}", yesNo(pump_.airDetected()));
    std::println(
        *out_, "  Alimentation  alimentée {} ; secteur {} ; sur batterie {} ; coupure imminente {}",
        yesNo(power_.powered()), yesNo(power_.mainsPresent()), yesNo(power_.onBattery()),
        yesNo(power_.powerFailImminent()));
    std::println(*out_, "  Batterie      {:.1f} % ; tension {} ; courant {:.1f} mA",
                 power_.stateOfCharge() * 100.0,
                 shown(power_.measuredBatteryVoltage(), &inVoltsOf, "V"),
                 inMilliamperes(power_.measuredBatteryCurrent()));
}

void Console::printHelp() {
    std::println(*out_, "{}", help);
}

Console::Outcome Console::error(std::string_view message) {
    std::println(*out_, "Erreur : {}", message);
    note(Severity::alert, "console", std::format("erreur : {}", message));
    return Outcome::error;
}

} // namespace clepsydre::sim
