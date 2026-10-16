// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

// Tests de l'interprète de la démonstration du simulateur (#21) : syntaxe, erreurs, codes de
// sortie et vérifications ; journal de diagnostic minimal (#43). Les scénarios d'examples/sim
// sont exécutés à part, comme tests de fumée.

#include <chrono>
#include <expect.hpp>
#include <ios>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <utility>

import clepsydre.sim.console;
import clepsydre.sim.journal;

namespace {

using clepsydre::sim::Console;
using clepsydre::sim::Journal;
using clepsydre::sim::Severity;
using clepsydre::test::Expectations;

// Exécute un script ; renvoie le code de sortie et laisse la sortie dans output.
int script(const std::string& text, std::string& output) {
    std::ostringstream out;
    Console console{out};
    std::istringstream in{text};
    const int code = console.run(in, false);
    output = out.str();
    return code;
}

void exitCodes(Expectations& t) {
    std::string output;
    t.expect(script("# commentaire\n\ndebit 10\navance 1s\nverifie temps = 1\n", output) == 0,
             "script valide : code 0");
    t.expect(script("debit 10\ninconnue\navance 1s\n", output) == 2, "commande inconnue : code 2");
    t.expect(output.find("Arrêt à la ligne 2.") != std::string::npos, "ligne de l'erreur");
    t.expect(output.find("avance 1s") == std::string::npos, "arrêt à la première erreur");
    t.expect(script("verifie volume > 1\n", output) == 1, "vérification échouée : code 1");
    t.expect(output.find("ÉCHEC : volume vaut 0") != std::string::npos, "échec expliqué");
    t.expect(script("quitter\ninconnue\n", output) == 0, "quitter arrête le script");
}

// Tampon qui fournit text puis échoue, comme un fichier dont la lecture est interrompue.
class FailingBuffer : public std::streambuf {
public:
    explicit FailingBuffer(std::string text) : text_{std::move(text)} {
        setg(text_.data(), text_.data(), std::to_address(text_.end()));
    }

protected:
    int_type underflow() override { throw std::runtime_error{"lecture interrompue"}; }

private:
    std::string text_;
};

void readErrors(Expectations& t) {
    std::ostringstream out;
    {
        Console console{out};
        std::istringstream in{"debit 10\n"};
        in.setstate(std::ios::badbit);
        t.expect(console.run(in, false) == 2, "flux en erreur dès le début : code 2");
        t.expect(out.str().find("Erreur de lecture après la ligne 0.") != std::string::npos,
                 "erreur de lecture signalée");
    }
    out.str("");
    {
        Console console{out};
        FailingBuffer buffer{"debit 10\navance 1s\n"};
        std::istream in{&buffer};
        t.expect(console.run(in, false) == 2, "lecture interrompue après des commandes : code 2");
        t.expect(out.str().find("Erreur de lecture après la ligne 2.") != std::string::npos,
                 "ligne de l'interruption signalée");
    }
}

void syntax(Expectations& t) {
    std::ostringstream out;
    Console console{out};
    t.expect(console.execute("avance 1,5s") == Console::Outcome::ok, "virgule décimale admise");
    t.expect(console.execute("verifie temps = 1.5") == Console::Outcome::ok, "1,5 s écoulées");
    t.expect(console.execute("avance 5") == Console::Outcome::error, "durée sans unité refusée");
    t.expect(console.execute("avance -1s") == Console::Outcome::error, "durée négative refusée");
    t.expect(console.execute("avance 2j") == Console::Outcome::error, "unité inconnue refusée");
    t.expect(console.execute("debit dix") == Console::Outcome::error, "débit non numérique");
    t.expect(console.execute("debit 10 20") == Console::Outcome::error, "argument en trop");
    t.expect(console.execute("seringue 30") == Console::Outcome::error, "seringue inconnue");
    t.expect(console.execute("pilote pertes 2") == Console::Outcome::error, "fraction hors bornes");
    t.expect(console.execute("capteur pression perdu") == Console::Outcome::error,
             "capteur inconnu");
    t.expect(console.execute("verifie debit > 1") == Console::Outcome::error, "grandeur inconnue");
    t.expect(console.execute("verifie volume ~ 1") == Console::Outcome::error, "opérateur inconnu");
    t.expect(console.execute("   # commentaire seul") == Console::Outcome::ok, "commentaire");
}

void resetAndSensors(Expectations& t) {
    std::ostringstream out;
    Console console{out};
    t.expect(console.execute("avance 1min") == Console::Outcome::ok, "le temps avance");
    t.expect(console.execute("seringue 20") == Console::Outcome::ok, "changement de seringue");
    t.expect(console.execute("verifie temps = 0") == Console::Outcome::ok,
             "le changement de seringue réinitialise la simulation");
    t.expect(console.execute("capteur position perdu") == Console::Outcome::ok,
             "capteur de position perdu");
    t.expect(console.execute("verifie piston-mesure >= 0") == Console::Outcome::failed,
             "aucune comparaison ne tient sans mesure");
    t.expect(console.execute("verifie piston-mesure ~ 0") == Console::Outcome::error,
             "opérateur inconnu refusé même sans mesure");
    out.str("");
    t.expect(console.execute("etat") == Console::Outcome::ok, "état affiché");
    t.expect(out.str().find("mesuré — par rapport au bâti") != std::string::npos,
             "mesure absente affichée par un tiret");
    t.expect(console.execute("verifie berceau = oui") == Console::Outcome::ok, "booléen oui");
    t.expect(console.execute("berceau absente") == Console::Outcome::ok, "seringue retirée");
    t.expect(console.execute("verifie berceau = non") == Console::Outcome::ok, "booléen non");
}

void journalFormat(Expectations& t) {
    std::ostringstream out;
    Journal journal{out};
    t.expect(journal.enabled(), "journal écrit dans un flux : activé");
    journal.record(std::chrono::hours{26} + std::chrono::minutes{3} + std::chrono::seconds{4} +
                       std::chrono::microseconds{5'678},
                   Severity::alert, "alimentation", "coupure imminente");
    journal.record(std::chrono::nanoseconds{999'999}, Severity::info, "moteur", "moteur en marche");
    t.expect(out.str() == "26:03:04.005 ALERTE [alimentation] coupure imminente\n"
                          "00:00:00.000 INFO [moteur] moteur en marche\n",
             "heure tronquée à la milliseconde, gravité, composant, message");
    const Journal disabled;
    t.expect(!disabled.enabled(), "journal par défaut : désactivé");
}

// Exécute les lignes sur une console journalisée ; renvoie le journal.
std::string journaled(const std::string& text) {
    std::ostringstream out;
    std::ostringstream journal;
    Console console{out, Journal{journal}};
    std::istringstream in{text};
    static_cast<void>(console.run(in, false));
    return journal.str();
}

void journalEvents(Expectations& t) {
    t.expect(journaled("debit 25\navance 10min\nsecteur non\navance 1min\nbatterie non\n"
                       "avance 20ms\navance 1s\nsecteur oui\navance 1s\n") ==
                 "00:00:00.000 INFO [operateur] debit 25\n"
                 "00:00:00.001 INFO [moteur] moteur en marche\n"
                 "00:10:00.000 INFO [operateur] secteur non\n"
                 "00:10:00.001 INFO [alimentation] bascule sur batterie\n"
                 "00:11:00.000 INFO [operateur] batterie non\n"
                 "00:11:00.001 INFO [alimentation] fin du fonctionnement sur batterie\n"
                 "00:11:00.001 ALERTE [alimentation] coupure imminente\n"
                 "00:11:00.050 ALERTE [alimentation] perte totale de l'alimentation\n"
                 "00:11:00.050 INFO [moteur] moteur arrêté\n"
                 "00:11:01.020 INFO [operateur] secteur oui\n"
                 "00:11:01.021 INFO [alimentation] alimentation rétablie\n",
             "coupure : commandes, bascule, coupure imminente, perte, moteur, retour du secteur");
    t.expect(journaled("debit 10\navance 30min\nair 60\navance 1min\n") ==
                 "00:00:00.000 INFO [operateur] debit 10\n"
                 "00:00:00.001 INFO [moteur] moteur en marche\n"
                 "00:30:00.000 INFO [operateur] air 60\n"
                 "00:30:36.000 ALERTE [air] air détecté\n"
                 "00:30:57.601 INFO [air] plus d'air détecté\n",
             "bulle : apparition et disparition devant le détecteur");
    t.expect(journaled("aide\netat\nverifie liste\navance 1s\nrepere # commentaire\n"
                       "verifie temps = 1\nquitter\n") == "00:00:01.000 INFO [operateur] repere\n",
             "lecture seule et passage du temps non journalisés ; commentaire retiré");
    t.expect(journaled("debit x\n") == "00:00:00.000 INFO [operateur] debit x\n"
                                       "00:00:00.000 ALERTE [console] erreur : débit attendu, "
                                       "en mL/h, positif ou nul\n",
             "commande refusée : consignée, puis l'erreur");
    t.expect(journaled("verifie volume > 5\n") ==
                 "00:00:00.000 ALERTE [console] ÉCHEC : volume vaut 0, attendu > 5\n",
             "vérification échouée : alerte");
    t.expect(journaled("debit 10\navance 1s\nseringue 20\navance 1s\n") ==
                 "00:00:00.000 INFO [operateur] debit 10\n"
                 "00:00:00.001 INFO [moteur] moteur en marche\n"
                 "00:00:01.000 INFO [operateur] seringue 20\n",
             "réinitialisation : moteur arrêté sans transition journalisée, temps remis à zéro");
}

} // namespace

int main() {
    return Expectations{}.run([](Expectations& t) {
        exitCodes(t);
        readErrors(t);
        syntax(t);
        resetAndSensors(t);
        journalFormat(t);
        journalEvents(t);
    });
}
