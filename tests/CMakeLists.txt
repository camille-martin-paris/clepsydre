# SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
# SPDX-License-Identifier: EUPL-1.2

add_library(clepsydre_test_support INTERFACE)
target_include_directories(clepsydre_test_support INTERFACE support)

clepsydre_add_test(version_test SOURCES version_test.cpp LIBRARIES clepsydre::core)
clepsydre_add_test(protocol_crc32_test SOURCES protocol_crc32_test.cpp LIBRARIES clepsydre::protocol)
clepsydre_add_test(protocol_cobs_test SOURCES protocol_cobs_test.cpp LIBRARIES clepsydre::protocol)
clepsydre_add_test(protocol_frame_test SOURCES protocol_frame_test.cpp LIBRARIES clepsydre::protocol)
clepsydre_add_test(sim_syringe_pump_test SOURCES sim_syringe_pump_test.cpp LIBRARIES clepsydre::sim)
clepsydre_add_test(sim_clock_test SOURCES sim_clock_test.cpp LIBRARIES clepsydre::sim)
clepsydre_add_test(sim_serial_link_test SOURCES sim_serial_link_test.cpp LIBRARIES clepsydre::sim)
clepsydre_add_test(sim_simulation_test SOURCES sim_simulation_test.cpp LIBRARIES clepsydre::sim)
clepsydre_add_test(sim_link_faults_test SOURCES sim_link_faults_test.cpp LIBRARIES clepsydre::sim)
clepsydre_add_test(sim_processor_test SOURCES sim_processor_test.cpp LIBRARIES clepsydre::sim)
clepsydre_add_test(sim_pump_faults_test SOURCES sim_pump_faults_test.cpp LIBRARIES clepsydre::sim)
clepsydre_add_test(sim_power_test SOURCES sim_power_test.cpp LIBRARIES clepsydre::sim)
clepsydre_add_test(sim_console_test SOURCES sim_console_test.cpp LIBRARIES clepsydre::sim_console)

# Scénarios de démonstration du simulateur, exécutés comme tests de fumée : chacun porte ses
# propres vérifications.
file(GLOB _clepsydre_sim_scenarios CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/examples/sim/*.txt")
foreach(_scenario IN LISTS _clepsydre_sim_scenarios)
    get_filename_component(_name "${_scenario}" NAME_WE)
    add_test(NAME sim_scenario_${_name} COMMAND clepsydre-sim "${_scenario}")
    clepsydre_coverage_test(sim_scenario_${_name} clepsydre-sim)
endforeach()
