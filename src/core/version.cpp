// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <string_view>

module clepsydre.version;

#define CLEPSYDRE_STRINGIFY_IMPL(x) #x
#define CLEPSYDRE_STRINGIFY(x) CLEPSYDRE_STRINGIFY_IMPL(x)

namespace clepsydre {

Version version() noexcept {
    return {.major = CLEPSYDRE_VERSION_MAJOR,
            .minor = CLEPSYDRE_VERSION_MINOR,
            .patch = CLEPSYDRE_VERSION_PATCH};
}

std::string_view versionString() noexcept {
    return CLEPSYDRE_STRINGIFY(CLEPSYDRE_VERSION_MAJOR) "." CLEPSYDRE_STRINGIFY(
        CLEPSYDRE_VERSION_MINOR) "." CLEPSYDRE_STRINGIFY(CLEPSYDRE_VERSION_PATCH);
}

} // namespace clepsydre
