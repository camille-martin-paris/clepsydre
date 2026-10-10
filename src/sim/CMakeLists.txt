# SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
# SPDX-License-Identifier: EUPL-1.2

# Simulateur physique de la pompe (#21) : s'exécute sur l'hôte, jamais sur la cible.
add_library(clepsydre_sim)
add_library(clepsydre::sim ALIAS clepsydre_sim)
target_sources(clepsydre_sim
    PUBLIC FILE_SET CXX_MODULES FILES
        quantities.cppm syringe_pump.cppm clock.cppm serial_link.cppm simulation.cppm
    PRIVATE syringe_pump.cpp clock.cpp serial_link.cpp simulation.cpp)
target_link_libraries(clepsydre_sim PRIVATE clepsydre_warnings)
clepsydre_register_objects(clepsydre_sim)
