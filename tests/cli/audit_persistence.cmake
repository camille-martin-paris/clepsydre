# SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
# SPDX-License-Identifier: EUPL-1.2

# Option --audit de clepsydre-sim (#136) : le journal d'audit persiste d'une exécution à l'autre,
# chaque exécution ayant son numéro de démarrage, donc son flux ; un compteur de démarrage
# corrompu empêche le démarrage (SW-REQ-020) ; un répertoire qui désigne le scénario est refusé
# sans le modifier ; une option incomplète est refusée.
# Usage : cmake -DSIM=<clepsydre-sim> -DSOURCE=<scénario> -DWORK=<répertoire> -P audit_persistence.cmake

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(audit "${WORK}/audit")

foreach(run IN ITEMS 1 2)
    execute_process(COMMAND "${SIM}" --audit "${audit}" "${SOURCE}"
        RESULT_VARIABLE code OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT code EQUAL 0)
        message(FATAL_ERROR "exécution ${run} : code ${code}, attendu 0 ; ${error}\n${output}")
    endif()
    if(NOT output MATCHES "Flux courant pump-SIM0001:commande:boot-${run} ")
        message(FATAL_ERROR "exécution ${run} : flux du démarrage ${run} attendu\n${output}")
    endif()
endforeach()
file(READ "${audit}/boot" boot)
if(NOT boot STREQUAL "2\n")
    message(FATAL_ERROR "compteur de démarrage : « ${boot} », attendu 2")
endif()
# Les deux flux, et la clôture du premier, sont relus à la seconde exécution.
if(NOT output MATCHES "Flux pump-SIM0001:commande:boot-1 : internally consistent"
   OR NOT output MATCHES "pump-SIM0001:commande:boot-1 : closed at 18")
    message(FATAL_ERROR "flux du premier démarrage non relu, ou non clos\n${output}")
endif()

file(WRITE "${audit}/boot" "illisible\n")
execute_process(COMMAND "${SIM}" --audit "${audit}" "${SOURCE}"
    RESULT_VARIABLE code OUTPUT_VARIABLE output ERROR_QUIET)
if(NOT code EQUAL 2 OR NOT output MATCHES "compteur de démarrage du journal d'audit illisible")
    message(FATAL_ERROR "compteur corrompu : code ${code}, attendu 2\n${output}")
endif()

set(scenario "${WORK}/scenario.txt")
file(COPY_FILE "${SOURCE}" "${scenario}")
file(READ "${scenario}" expected)
execute_process(COMMAND "${SIM}" --audit "${scenario}" "${scenario}"
    RESULT_VARIABLE code OUTPUT_QUIET ERROR_VARIABLE error)
file(READ "${scenario}" content)
if(NOT code EQUAL 2 OR NOT error MATCHES "Le journal d'audit désigne un autre fichier"
   OR NOT content STREQUAL expected)
    message(FATAL_ERROR "répertoire qui désigne le scénario : code ${code}, attendu 2 ; ${error}")
endif()

execute_process(COMMAND "${SIM}" --audit RESULT_VARIABLE code OUTPUT_QUIET ERROR_QUIET)
if(NOT code EQUAL 2)
    message(FATAL_ERROR "option --audit incomplète : code ${code}, attendu 2")
endif()
