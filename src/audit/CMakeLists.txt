# SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
# SPDX-License-Identifier: EUPL-1.2

# Événements d'audit (#41, #136) : vocabulaire fermé et enregistreur, partagés par le logiciel de
# commande et celui de la sécurité. Code embarqué, de classe C ; il n'emploie que le cœur sans
# allocation de mddlog (ADR 0005).
add_library(clepsydre_audit)
add_library(clepsydre::audit ALIAS clepsydre_audit)
target_sources(clepsydre_audit
    PUBLIC FILE_SET CXX_MODULES FILES vocabulary.cppm recorder.cppm
    PRIVATE vocabulary.cpp recorder.cpp)
target_link_libraries(clepsydre_audit PUBLIC mddlog::core PRIVATE clepsydre_warnings)
clepsydre_register_objects(clepsydre_audit)
