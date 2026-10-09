// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <string_view>

export module clepsydre.version;

export namespace clepsydre {

// Version du logiciel, fixée par project(VERSION) dans CMakeLists.txt.
struct Version {
    int major;
    int minor;
    int patch;
};

[[nodiscard]] Version version() noexcept;

// Forme « majeure.mineure.correctif », par exemple « 0.1.0 ».
[[nodiscard]] std::string_view versionString() noexcept;

} // namespace clepsydre
