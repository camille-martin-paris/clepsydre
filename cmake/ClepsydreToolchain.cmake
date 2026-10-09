# SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
# SPDX-License-Identifier: EUPL-1.2

# Langage, compilateurs pris en charge, avertissements et outils de qualité.
# Les versions minimales sont justifiées dans docs/development/build.md.

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_GENERATOR MATCHES "Ninja")
    message(FATAL_ERROR "Les modules C++ demandent le générateur Ninja (actuel : ${CMAKE_GENERATOR}).")
endif()

# Planchers alignés sur mddlog, bibliothèque de journalisation envisagée (ADR #16) :
# un projet qui l'intègre ne peut pas admettre des compilateurs qu'elle refuse.
# - GCC 16.1 : GCC 15 ne sait pas relire le module std de libstdc++ à travers un second niveau
#   de BMI ; GCC 16.2 a corrompu les BMI de mddlog (« failed to read compiled module cluster »).
#   GCC 14 provoque en outre une erreur interne sur nos modules avec -fsanitize=address,undefined.
# - Clang 20 : clang-tidy 18 ne sait pas charger les modules du projet.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 16.1)
        message(FATAL_ERROR "GCC 16.1 ou plus récent est requis (trouvé : ${CMAKE_CXX_COMPILER_VERSION}).")
    elseif(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 16.2 AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 16.3)
        message(FATAL_ERROR "GCC 16.2 n'est pas admis : il corrompt les BMI des modules "
                            "(constaté sur mddlog). Utiliser GCC 16.1 (trouvé : ${CMAKE_CXX_COMPILER_VERSION}).")
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 20)
        message(FATAL_ERROR "Clang 20 ou plus récent est requis (trouvé : ${CMAKE_CXX_COMPILER_VERSION}).")
    endif()
else()
    message(FATAL_ERROR "Compilateur non pris en charge : ${CMAKE_CXX_COMPILER_ID}. "
                        "Compilateurs pris en charge : GCC, Clang.")
endif()

option(CLEPSYDRE_WARNINGS_AS_ERRORS "Traiter les avertissements du compilateur comme des erreurs" ON)
set(CMAKE_COMPILE_WARNING_AS_ERROR ${CLEPSYDRE_WARNINGS_AS_ERRORS})

add_library(clepsydre_warnings INTERFACE)
target_compile_options(clepsydre_warnings INTERFACE
    -Wall -Wextra -Wpedantic
    -Wconversion -Wsign-conversion -Wdouble-promotion
    -Wshadow -Wold-style-cast -Wcast-align
    -Wnon-virtual-dtor -Woverloaded-virtual
    -Wnull-dereference -Wformat=2 -Wimplicit-fallthrough
    $<$<CXX_COMPILER_ID:GNU>:-Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wuseless-cast>)

option(CLEPSYDRE_CLANG_TIDY "Exécuter clang-tidy pendant la compilation" OFF)
if(CLEPSYDRE_CLANG_TIDY)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR "CLEPSYDRE_CLANG_TIDY demande Clang : clang-tidy doit lire les modules "
                            "compilés par le même compilateur.")
    endif()
    string(REGEX MATCH "^[0-9]+" _clepsydre_clang_major "${CMAKE_CXX_COMPILER_VERSION}")
    find_program(CLEPSYDRE_CLANG_TIDY_EXE
        NAMES clang-tidy-${_clepsydre_clang_major} clang-tidy REQUIRED)
    set(CMAKE_CXX_CLANG_TIDY "${CLEPSYDRE_CLANG_TIDY_EXE}" "--warnings-as-errors=*")
endif()

# Cible « clepsydre_compile » : compile toutes les unités sans édition de liens,
# pour que la CI publie séparément compilation et édition de liens.
add_custom_target(clepsydre_compile)
define_property(GLOBAL PROPERTY CLEPSYDRE_OBJECT_TARGETS
    BRIEF_DOCS "Cibles objets construites par clepsydre_compile")

function(clepsydre_register_objects target)
    set_property(GLOBAL APPEND PROPERTY CLEPSYDRE_OBJECT_TARGETS ${target})
endfunction()

function(clepsydre_finalize_compile_target)
    get_property(targets GLOBAL PROPERTY CLEPSYDRE_OBJECT_TARGETS)
    if(targets)
        add_dependencies(clepsydre_compile ${targets})
    endif()
endfunction()

# clepsydre_add_test(<nom> SOURCES <fichiers> [LIBRARIES <cibles>])
# Compile le test en bibliothèque objet, l'édite en exécutable et l'enregistre dans CTest.
function(clepsydre_add_test name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "SOURCES;LIBRARIES")
    add_library(${name}_objects OBJECT ${arg_SOURCES})
    target_link_libraries(${name}_objects PRIVATE clepsydre_warnings clepsydre_test_support ${arg_LIBRARIES})
    clepsydre_register_objects(${name}_objects)
    add_executable(${name})
    target_link_libraries(${name} PRIVATE ${name}_objects ${arg_LIBRARIES})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

# Cibles « clepsydre_format » (reformate) et « clepsydre_format_check » (vérifie sans modifier).
find_program(CLEPSYDRE_CLANG_FORMAT_EXE NAMES clang-format)
if(CLEPSYDRE_CLANG_FORMAT_EXE)
    file(GLOB_RECURSE _clepsydre_format_sources CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/src/*.cpp" "${PROJECT_SOURCE_DIR}/src/*.cppm"
        "${PROJECT_SOURCE_DIR}/src/*.hpp" "${PROJECT_SOURCE_DIR}/tests/*.cpp"
        "${PROJECT_SOURCE_DIR}/tests/*.hpp")
    add_custom_target(clepsydre_format
        COMMAND "${CLEPSYDRE_CLANG_FORMAT_EXE}" -i ${_clepsydre_format_sources}
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" VERBATIM)
    add_custom_target(clepsydre_format_check
        COMMAND "${CLEPSYDRE_CLANG_FORMAT_EXE}" --dry-run --Werror ${_clepsydre_format_sources}
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" VERBATIM)
endif()
