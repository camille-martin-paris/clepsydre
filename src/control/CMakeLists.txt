# SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
# SPDX-License-Identifier: EUPL-1.2

# Logiciel de commande (#23) : code embarqué, de classe C.
add_library(clepsydre_control)
add_library(clepsydre::control ALIAS clepsydre_control)
target_sources(clepsydre_control
    PUBLIC FILE_SET CXX_MODULES FILES state_machine.cppm
    PRIVATE state_machine.cpp)
target_link_libraries(clepsydre_control PRIVATE clepsydre_warnings)
clepsydre_register_objects(clepsydre_control)
