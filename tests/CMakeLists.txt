# SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
# SPDX-License-Identifier: EUPL-1.2

add_library(clepsydre_test_support INTERFACE)
target_include_directories(clepsydre_test_support INTERFACE support)

clepsydre_add_test(version_test SOURCES version_test.cpp LIBRARIES clepsydre::core)
clepsydre_add_test(sim_syringe_pump_test SOURCES sim_syringe_pump_test.cpp LIBRARIES clepsydre::sim)
clepsydre_add_test(sim_clock_test SOURCES sim_clock_test.cpp LIBRARIES clepsydre::sim)
clepsydre_add_test(sim_serial_link_test SOURCES sim_serial_link_test.cpp LIBRARIES clepsydre::sim)
clepsydre_add_test(sim_simulation_test SOURCES sim_simulation_test.cpp LIBRARIES clepsydre::sim)
