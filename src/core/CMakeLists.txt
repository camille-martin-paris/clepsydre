# SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
# SPDX-License-Identifier: EUPL-1.2

add_library(clepsydre_core)
add_library(clepsydre::core ALIAS clepsydre_core)
target_sources(clepsydre_core
    PUBLIC FILE_SET CXX_MODULES FILES version.cppm
    PRIVATE version.cpp)
target_compile_definitions(clepsydre_core PRIVATE
    CLEPSYDRE_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    CLEPSYDRE_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    CLEPSYDRE_VERSION_PATCH=${PROJECT_VERSION_PATCH})
target_link_libraries(clepsydre_core PRIVATE clepsydre_warnings)
clepsydre_register_objects(clepsydre_core)
