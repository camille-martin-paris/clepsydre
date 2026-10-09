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

set(_clepsydre_minimum_gcc 14)
set(_clepsydre_minimum_clang 18)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS _clepsydre_minimum_gcc)
        message(FATAL_ERROR "GCC ${_clepsydre_minimum_gcc} ou plus récent est requis "
                            "(trouvé : ${CMAKE_CXX_COMPILER_VERSION}).")
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS _clepsydre_minimum_clang)
        message(FATAL_ERROR "Clang ${_clepsydre_minimum_clang} ou plus récent est requis "
                            "(trouvé : ${CMAKE_CXX_COMPILER_VERSION}).")
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
