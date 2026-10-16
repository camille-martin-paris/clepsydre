# SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
# SPDX-License-Identifier: EUPL-1.2

# clepsydre-sim refuse un journal qui désigne le scénario, par le même chemin, un lien symbolique
# ou un lien physique, sans le modifier ; un scénario absent est refusé sans créer le journal.
# Usage : cmake -DSIM=<clepsydre-sim> -DSOURCE=<scénario> -DWORK=<répertoire> -P journal_collision.cmake

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(scenario "${WORK}/scenario.txt")
file(COPY_FILE "${SOURCE}" "${scenario}")
file(READ "${scenario}" expected)
file(CREATE_LINK "scenario.txt" "${WORK}/symbolique.txt" SYMBOLIC)
file(CREATE_LINK "${scenario}" "${WORK}/physique.txt")

foreach(journal IN ITEMS "${scenario}" "${WORK}/symbolique.txt" "${WORK}/physique.txt")
    execute_process(COMMAND "${SIM}" --journal "${journal}" "${scenario}"
        RESULT_VARIABLE code OUTPUT_QUIET ERROR_VARIABLE error)
    file(READ "${scenario}" content)
    if(NOT code EQUAL 2 OR NOT error MATCHES "Le journal désigne le scénario")
        message(FATAL_ERROR "journal ${journal} : code ${code}, attendu 2 ; erreur : ${error}")
    endif()
    if(NOT content STREQUAL expected)
        message(FATAL_ERROR "journal ${journal} : le scénario a été modifié")
    endif()
endforeach()

execute_process(COMMAND "${SIM}" --journal "${WORK}/absent.txt" "${WORK}/absent.txt"
    RESULT_VARIABLE code OUTPUT_QUIET ERROR_QUIET)
if(NOT code EQUAL 2 OR EXISTS "${WORK}/absent.txt")
    message(FATAL_ERROR "scénario absent : code ${code}, attendu 2, et aucun journal créé")
endif()
