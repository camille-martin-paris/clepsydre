# SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
# SPDX-License-Identifier: EUPL-1.2

# 4.0 : plancher de mddlog, bibliothèque de journalisation envisagée (ADR #16).
cmake_minimum_required(VERSION 4.0)

project(Clepsydre
    VERSION 0.1.0
    DESCRIPTION "Pompe à perfusion open source et open hardware"
    LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(ClepsydreToolchain)

add_subdirectory(src)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

clepsydre_finalize_compile_target()
