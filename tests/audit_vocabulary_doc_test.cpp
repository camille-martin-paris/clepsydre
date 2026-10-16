// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

// Concordance du vocabulaire du code avec docs/event-vocabulary.md (#136) : mêmes actions, mêmes
// catégories, phases, producteurs et références ; chaque référence existe dans le registre de
// traçabilité. Usage : audit_vocabulary_doc_test [<event-vocabulary.md> <répertoire du registre>] ;
// sans argument, les fichiers de l'arbre des sources (CLEPSYDRE_SOURCE_DIR).

#include <cstddef>
#include <cstdint>
#include <expect.hpp>
#include <filesystem>
#include <format>
#include <fstream>
#include <map>
#include <optional>
#include <print>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

import clepsydre.audit.vocabulary;

namespace {

using clepsydre::test::Expectations;
using namespace clepsydre::audit;

struct Documented {
    AuditCategory category = AuditCategory::Lifecycle;
    std::uint8_t phases = 0;
    std::uint8_t producers = 0;
    std::string requirement;
    std::string risk;
};

std::string_view trim(std::string_view text) {
    const std::size_t start = text.find_first_not_of(' ');
    if (start == std::string_view::npos) {
        return {};
    }
    return text.substr(start, text.find_last_not_of(' ') - start + 1);
}

std::vector<std::string> cells(std::string_view line) {
    std::vector<std::string> out;
    line.remove_prefix(1); // « | » initial
    while (!line.empty()) {
        const std::size_t end = line.find('|');
        out.emplace_back(trim(line.substr(0, end)));
        if (end == std::string_view::npos) {
            break;
        }
        line.remove_prefix(end + 1);
    }
    if (!out.empty() && out.back().empty()) {
        out.pop_back();
    }
    return out;
}

// Texte entre les premiers accents graves.
std::string codeSpan(std::string_view cell) {
    const std::size_t start = cell.find('`');
    const std::size_t end = cell.find('`', start + 1);
    if (start == std::string_view::npos || end == std::string_view::npos) {
        return {};
    }
    return std::string{cell.substr(start + 1, end - start - 1)};
}

std::optional<AuditCategory> category(std::string_view text) {
    if (text.starts_with("opérateur")) {
        return AuditCategory::Operator;
    }
    if (text.starts_with("maîtrise du risque")) {
        return AuditCategory::RiskControl;
    }
    if (text.starts_with("cycle de vie")) {
        return AuditCategory::Lifecycle;
    }
    if (text.starts_with("configuration")) {
        return AuditCategory::Configuration;
    }
    if (text.starts_with("accès")) {
        return AuditCategory::Access;
    }
    return std::nullopt;
}

std::uint8_t phases(std::string_view cell) {
    std::uint8_t bits = 0;
    for (const char c : cell) {
        if (c == 'R') {
            bits |= requested.bits;
        } else if (c == 'C') {
            bits |= confirmed.bits;
        } else if (c == 'E') {
            bits |= executed.bits;
        } else if (c == 'F') {
            bits |= failed.bits;
        }
    }
    return bits;
}

std::uint8_t producers(std::string_view cell) {
    std::uint8_t bits = 0;
    for (const char c : cell) {
        if (c == 'C') {
            bits |= command.bits;
        } else if (c == 'S') {
            bits |= safety.bits;
        }
    }
    return bits;
}

// Actions décrites par les tableaux de la section « Actions » du document.
std::map<std::string, Documented> readDocument(const std::filesystem::path& path, Expectations& t) {
    std::ifstream file{path};
    t.expect(file.good(), std::format("document lisible : {}", path.string()));
    std::vector<std::string> lines;
    for (std::string line; std::getline(file, line);) {
        lines.push_back(line);
    }
    std::map<std::string, Documented> out;
    bool inActions = false;
    std::optional<AuditCategory> sectionCategory;
    std::vector<std::pair<std::string, std::uint8_t>> steps; // suffixe, phases
    std::vector<std::vector<std::string>> conditions;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const std::string_view line = lines[i];
        if (line.starts_with("## ")) {
            inActions = line == "## Actions";
            continue;
        }
        if (!inActions) {
            continue;
        }
        if (line.starts_with("### ")) {
            sectionCategory.reset();
            continue;
        }
        if (line.starts_with("Catégorie : ")) {
            sectionCategory = category(line.substr(std::string_view{"Catégorie : "}.size()));
            continue;
        }
        if (!line.starts_with("|") || i + 1 >= lines.size() || !lines[i + 1].starts_with("| ---")) {
            continue;
        }
        // En-tête d'un tableau, puis ses lignes.
        const std::vector<std::string> header = cells(line);
        std::map<std::string, std::size_t> column;
        for (std::size_t c = 0; c < header.size(); ++c) {
            column[header[c]] = c;
        }
        std::size_t row = i + 2;
        for (; row < lines.size() && lines[row].starts_with("|"); ++row) {
            const std::vector<std::string> values = cells(lines[row]);
            if (header.front() == "Action" && column.contains("Phases")) {
                const std::string& first = values.at(0);
                Documented documented;
                std::optional<AuditCategory> cat = sectionCategory;
                if (const std::size_t open = first.find('('); open != std::string::npos) {
                    cat = category(std::string_view{first}.substr(open + 1));
                }
                t.expect(cat.has_value(), std::format("catégorie de {}", first));
                documented.category = cat.value_or(AuditCategory::Lifecycle);
                documented.phases = phases(values.at(column["Phases"]));
                documented.producers = column.contains("Producteur")
                                           ? producers(values.at(column["Producteur"]))
                                           : command.bits;
                documented.requirement = values.at(column["Exigence"]);
                documented.risk = values.at(column["Risque"]);
                out[codeSpan(first)] = documented;
            } else if (header.front() == "Étape") {
                const std::string name = codeSpan(values.at(column["Action"]));
                steps.emplace_back(name.substr(name.rfind('.') + 1),
                                   phases(values.at(column["Phases"])));
            } else if (header.front() == "`<condition>`") {
                conditions.push_back(values);
            }
        }
        i = row - 1;
    }
    t.expect(steps.size() == alarmStepCount, "quatre étapes d'alarme documentées");
    for (const auto& condition : conditions) {
        for (const auto& [suffix, stepPhases] : steps) {
            out[std::format("alarm.{}.{}", codeSpan(condition.at(0)), suffix)] = {
                .category = AuditCategory::RiskControl,
                .phases = stepPhases,
                .producers = both.bits,
                .requirement = condition.at(2),
                .risk = condition.at(3)};
        }
    }
    return out;
}

// Identifiants `id = "…"` d'un fichier du registre.
std::set<std::string, std::less<>> registryIds(const std::filesystem::path& path, Expectations& t) {
    std::ifstream file{path};
    t.expect(file.good(), std::format("registre lisible : {}", path.string()));
    std::set<std::string, std::less<>> ids;
    constexpr std::string_view key = "id = \"";
    for (std::string line; std::getline(file, line);) {
        if (line.starts_with(key)) {
            ids.insert(line.substr(key.size(), line.find('"', key.size()) - key.size()));
        }
    }
    return ids;
}

void compare(const std::filesystem::path& document, const std::filesystem::path& registry,
             Expectations& t) {
    const std::map<std::string, Documented> documented = readDocument(document, t);
    const auto requirements = registryIds(registry / "requirements.toml", t);
    const auto risks = registryIds(registry / "risks.toml", t);
    t.expect(documented.size() == vocabularySize,
             std::format("le document décrit {} actions, le code {}", documented.size(),
                         vocabularySize));
    for (std::size_t index = 0; index < vocabularySize; ++index) {
        const std::optional<ActionSpec> candidate = at(index);
        if (!candidate) {
            t.expect(false, "toute action du vocabulaire a une description");
            continue;
        }
        const ActionSpec& action = *candidate;
        const auto found = documented.find(std::string{action.name});
        if (!t.expect(found != documented.end(),
                      std::format("{} figure dans le document", action.name))) {
            continue;
        }
        const Documented& d = found->second;
        t.expect(d.category == action.category, std::format("catégorie de {}", action.name));
        t.expect(d.phases == action.phases.bits, std::format("phases de {}", action.name));
        t.expect(d.producers == action.producers.bits,
                 std::format("producteurs de {}", action.name));
        t.expect(d.requirement == action.requirement, std::format("exigence de {}", action.name));
        t.expect(d.risk == action.risk, std::format("risque de {}", action.name));
        t.expect(action.requirement.empty() || requirements.contains(action.requirement),
                 std::format("{} : {} existe dans le registre", action.name, action.requirement));
        t.expect(action.risk.empty() || risks.contains(action.risk),
                 std::format("{} : {} existe dans le registre", action.name, action.risk));
    }
}

} // namespace

int main(int argc, char** argv) {
    Expectations t;
    return t.run([argc, argv](Expectations& e) {
        const std::span arguments(argv, static_cast<std::size_t>(argc));
        const std::filesystem::path source{CLEPSYDRE_SOURCE_DIR};
        std::filesystem::path document = source / "docs" / "event-vocabulary.md";
        std::filesystem::path registry = source / "software_development_file" / "registry";
        if (arguments.size() == 3) {
            document = arguments[1];
            registry = arguments[2];
        }
        e.expect(arguments.size() == 1 || arguments.size() == 3,
                 "usage : audit_vocabulary_doc_test [<event-vocabulary.md> <registre>]");
        compare(document, registry, e);
    });
}
