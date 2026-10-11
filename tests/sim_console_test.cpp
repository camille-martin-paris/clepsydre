// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

// Tests de l'interprète de la démonstration du simulateur (#21) : syntaxe, erreurs, codes de
// sortie et vérifications. Les scénarios d'examples/sim sont exécutés à part, comme tests de
// fumée.

#include <expect.hpp>
#include <sstream>
#include <string>

import clepsydre.sim.console;

namespace {

using clepsydre::sim::Console;
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
    out.str("");
    t.expect(console.execute("etat") == Console::Outcome::ok, "état affiché");
    t.expect(out.str().find("mesuré — par rapport au bâti") != std::string::npos,
             "mesure absente affichée par un tiret");
    t.expect(console.execute("verifie berceau = oui") == Console::Outcome::ok, "booléen oui");
    t.expect(console.execute("berceau absente") == Console::Outcome::ok, "seringue retirée");
    t.expect(console.execute("verifie berceau = non") == Console::Outcome::ok, "booléen non");
}

} // namespace

int main() {
    return Expectations{}.run([](Expectations& t) {
        exitCodes(t);
        syntax(t);
        resetAndSensors(t);
    });
}
