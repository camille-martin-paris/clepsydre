// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

// Contre-épreuve temporaire : module compilé, lié par aucun test.
export module clepsydre.core.untested;

export int untested(int n) {
    if (n > 0) {
        return 1;
    }
    return 0;
}
