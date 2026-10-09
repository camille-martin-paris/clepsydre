# SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
# SPDX-License-Identifier: EUPL-1.2

add_library(clepsydre_test_support INTERFACE)
target_include_directories(clepsydre_test_support INTERFACE support)

clepsydre_add_test(version_test SOURCES version_test.cpp LIBRARIES clepsydre::core)
