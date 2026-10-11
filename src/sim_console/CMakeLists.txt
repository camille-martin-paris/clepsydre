# SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
# SPDX-License-Identifier: EUPL-1.2

# Démonstration interactive du simulateur (#21) : s'exécute sur l'hôte, jamais sur la cible.
add_library(clepsydre_sim_console)
add_library(clepsydre::sim_console ALIAS clepsydre_sim_console)
target_sources(clepsydre_sim_console
    PUBLIC FILE_SET CXX_MODULES FILES console.cppm
    PRIVATE console.cpp)
target_link_libraries(clepsydre_sim_console PUBLIC clepsydre::sim PRIVATE clepsydre_warnings)
clepsydre_register_objects(clepsydre_sim_console)

add_executable(clepsydre-sim main.cpp)
target_link_libraries(clepsydre-sim PRIVATE clepsydre::sim_console clepsydre_warnings)
clepsydre_register_objects(clepsydre-sim)
