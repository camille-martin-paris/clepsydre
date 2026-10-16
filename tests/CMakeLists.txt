# SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
# SPDX-License-Identifier: EUPL-1.2

add_library(clepsydre_test_support INTERFACE)
target_include_directories(clepsydre_test_support INTERFACE support)

clepsydre_add_test(version_test SOURCES version_test.cpp LIBRARIES clepsydre::core)
clepsydre_add_test(control_state_machine_test SOURCES control_state_machine_test.cpp
    LIBRARIES clepsydre::control)
clepsydre_add_test(control_infusion_test SOURCES control_infusion_test.cpp LIBRARIES clepsydre::control)
clepsydre_add_test(control_drive_test SOURCES control_drive_test.cpp LIBRARIES clepsydre::control)
clepsydre_add_test(control_drive_sim_test SOURCES control_drive_sim_test.cpp
    LIBRARIES clepsydre::control clepsydre::sim)
clepsydre_add_test(control_occlusion_test SOURCES control_occlusion_test.cpp
    LIBRARIES clepsydre::control)
clepsydre_add_test(control_occlusion_sim_test SOURCES control_occlusion_sim_test.cpp
    LIBRARIES clepsydre::control clepsydre::sim)
clepsydre_add_test(protocol_crc32_test SOURCES protocol_crc32_test.cpp LIBRARIES clepsydre::protocol)
clepsydre_add_test(protocol_cobs_test SOURCES protocol_cobs_test.cpp LIBRARIES clepsydre::protocol)
clepsydre_add_test(protocol_frame_test SOURCES protocol_frame_test.cpp LIBRARIES clepsydre::protocol)
clepsydre_add_test(protocol_link_test SOURCES protocol_link_test.cpp LIBRARIES clepsydre::protocol)
clepsydre_add_test(protocol_link_sim_test SOURCES protocol_link_sim_test.cpp
    LIBRARIES clepsydre::protocol clepsydre::sim)
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

# Option --journal de clepsydre-sim (#43) : journal écrit dans un fichier, option incomplète
# refusée (code 2), journal qui désigne le scénario refusé sans le modifier.
add_test(NAME sim_cli_journal
    COMMAND clepsydre-sim --journal "${CMAKE_CURRENT_BINARY_DIR}/sim_cli_journal.log"
            "${PROJECT_SOURCE_DIR}/examples/sim/coupure-alimentation.txt")
clepsydre_coverage_test(sim_cli_journal clepsydre-sim)
add_test(NAME sim_cli_journal_missing COMMAND clepsydre-sim --journal)
set_tests_properties(sim_cli_journal_missing PROPERTIES WILL_FAIL TRUE)
clepsydre_coverage_test(sim_cli_journal_missing clepsydre-sim)
add_test(NAME sim_cli_journal_collision
    COMMAND "${CMAKE_COMMAND}" -DSIM=$<TARGET_FILE:clepsydre-sim>
            -DSOURCE=${PROJECT_SOURCE_DIR}/examples/sim/air.txt
            -DWORK=${CMAKE_CURRENT_BINARY_DIR}/sim_cli_journal_collision
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cli/journal_collision.cmake)
clepsydre_coverage_test(sim_cli_journal_collision clepsydre-sim)
