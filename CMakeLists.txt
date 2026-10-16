# SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
# SPDX-License-Identifier: EUPL-1.2

# 4.0 : plancher de mddlog, bibliothèque de journalisation retenue (ADR 0005).
cmake_minimum_required(VERSION 4.0)

# mddlog utilise « import std », encore expérimental dans CMake : son activation dépend de la
# série de CMake et doit précéder project(), comme la norme et le refus des extensions, qui
# déterminent la compilation du module std. Une série non qualifiée par mddlog est refusée.
if(CMAKE_VERSION VERSION_GREATER_EQUAL 4.4)
    message(FATAL_ERROR "CMake ${CMAKE_VERSION} n'est pas qualifié pour « import std » par mddlog : "
                        "utiliser une version 4.0 à 4.3.")
elseif(CMAKE_VERSION VERSION_GREATER_EQUAL 4.3)
    set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")
else()
    set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")
endif()
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(Clepsydre
    VERSION 0.1.0
    DESCRIPTION "Pompe à perfusion open source et open hardware"
    LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(ClepsydreToolchain)
include(ClepsydreDependencies)

add_subdirectory(src)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

clepsydre_finalize_compile_target()
clepsydre_finalize_coverage()
