# SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
# SPDX-License-Identifier: EUPL-1.2

# Dépendances tierces, épinglées par empreinte (docs/development/build.md, « Dépendances
# épinglées »). Chacune est un SOUP inscrit au registre (software_development_file/soup.md).

include(FetchContent)

# mddlog 0.3.0, SOUP-001 (ADR 0005) : commit 073761b7a6d5ed29ed87bc37c85967db72386d3c, étiquette
# v0.3.0. L'archive de GitHub est vérifiée par son SHA-256 : aucun client git n'est requis.
FetchContent_Declare(mddlog
    URL https://github.com/ambroise-leclerc/mddlog/archive/073761b7a6d5ed29ed87bc37c85967db72386d3c.tar.gz
    URL_HASH SHA256=da610a164936f4d31ddee5e7b34372e97f5d9dfd978e014d823970d2ef562ad9
    EXCLUDE_FROM_ALL
    SYSTEM)

block()
    set(MDDLOG_BUILD_TESTS OFF)
    set(MDDLOG_BUILD_EXAMPLES OFF)
    # mddlog n'est pas analysé par les outils de qualité du projet : il a les siens (SOUP).
    set(CMAKE_CXX_CLANG_TIDY "")
    FetchContent_MakeAvailable(mddlog)
endblock()

# Adaptateurs de mddlog (mddlog::mddlog), requis par la persistance de l'audit du simulateur (#136).
# Avec Clang et libstdc++, SinkRegistry emploie atomic_load_explicit sur un shared_ptr, que
# libstdc++ déprécie (mddlog#150) : cet avertissement seul n'est pas une erreur pour cette cible.
# Il reste affiché, et tous les autres restent des erreurs.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(mddlog PRIVATE -Wno-error=deprecated-declarations)
endif()
