// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

#include <expect.hpp>
#include <format>
#include <string>

import clepsydre.version;

int main() {
    return clepsydre::test::Expectations{}.run([](clepsydre::test::Expectations& t) {
        const auto v = clepsydre::version();
        t.expect(v.major >= 0 && v.minor >= 0 && v.patch >= 0, "composantes de version positives");
        t.expect(clepsydre::versionString() == std::format("{}.{}.{}", v.major, v.minor, v.patch),
                 "chaîne de version cohérente avec ses composantes");
    });
}
