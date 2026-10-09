// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

// Assertions minimales pour les premiers tests, sans dépendance externe.
// Le choix d'un cadriciel de test sera consigné avec l'épinglage des dépendances (#22).

#ifndef CLEPSYDRE_TESTS_SUPPORT_EXPECT_HPP
#define CLEPSYDRE_TESTS_SUPPORT_EXPECT_HPP

#include <cstdio>
#include <exception>
#include <print>
#include <source_location>
#include <string_view>

namespace clepsydre::test {

class Expectations {
public:
    // Enregistre un échec si la condition est fausse ; renvoie la condition.
    bool expect(bool condition, std::string_view description,
                std::source_location where = std::source_location::current()) {
        ++checks_;
        if (!condition) {
            ++failures_;
            std::println(stderr, "{}:{}: échec : {}", where.file_name(), where.line(), description);
        }
        return condition;
    }

    // Exécute un corps de test ; une exception non rattrapée compte comme un échec.
    template <typename Body>
    int run(Body body) noexcept {
        try {
            body(*this);
            return exitCode();
        } catch (const std::exception& error) {
            report("exception non rattrapée : ", error.what());
        } catch (...) {
            report("exception non rattrapée de type inconnu", "");
        }
        return 1;
    }

    // Code de sortie pour CTest : 0 si toutes les vérifications ont réussi.
    [[nodiscard]] int exitCode() const {
        std::println(stderr, "{} vérification(s), {} échec(s)", checks_, failures_);
        return (failures_ == 0 && checks_ > 0) ? 0 : 1;
    }

private:
    // Sortie de dernier recours, sans formatage susceptible de lever une exception.
    static void report(const char* message, const char* detail) noexcept {
        static_cast<void>(std::fputs(message, stderr));
        static_cast<void>(std::fputs(detail, stderr));
        static_cast<void>(std::fputs("\n", stderr));
    }

    int checks_ = 0;
    int failures_ = 0;
};

} // namespace clepsydre::test

#endif // CLEPSYDRE_TESTS_SUPPORT_EXPECT_HPP
