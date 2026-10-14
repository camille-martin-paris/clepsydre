# SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
# SPDX-License-Identifier: EUPL-1.2

# Protocole de la liaison entre les processeurs (ADR 0006, #125) : partagé par le logiciel de
# commande, le logiciel de sécurité et le simulateur. Code embarqué, de classe C.
add_library(clepsydre_protocol)
add_library(clepsydre::protocol ALIAS clepsydre_protocol)
target_sources(clepsydre_protocol
    PUBLIC FILE_SET CXX_MODULES FILES crc32.cppm cobs.cppm frame.cppm link.cppm
    PRIVATE crc32.cpp cobs.cpp frame.cpp link.cpp)
target_link_libraries(clepsydre_protocol PRIVATE clepsydre_warnings)
clepsydre_register_objects(clepsydre_protocol)
