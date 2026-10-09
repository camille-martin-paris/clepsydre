# Compiler et tester Clepsydre

Ce document décrit la chaîne de build du logiciel : langage, compilateurs pris en charge, commandes et outils de qualité. Issue : [#19](https://github.com/camille-martin-paris/clepsydre/issues/19).

## Prérequis

| Outil | Version minimale | Motif |
| --- | --- | --- |
| CMake | 4.0 | Plancher de [mddlog](https://github.com/ambroise-leclerc/mddlog), bibliothèque de journalisation envisagée ([#16](https://github.com/camille-martin-paris/clepsydre/issues/16)) |
| Ninja | 1.11 | Seul générateur CMake qui gère les dépendances entre modules C++ |
| GCC | 16.1 ; **16.2 refusé** | Plancher de mddlog : GCC 15 ne relit pas le module `std` de libstdc++ à travers un second niveau de BMI ; GCC 16.2 a corrompu les BMI de mddlog. GCC 14 provoque en outre une erreur interne sur nos modules avec `-fsanitize=address,undefined` (CI du 2026-10-09). |
| Clang | 20 | Plancher de mddlog ; clang-tidy 18 ne charge pas les modules du projet (« module not found », CI du 2026-10-09). Fournir `clang-scan-deps` de la même version. |
| clang-tidy | Même version majeure que Clang | Analyse statique ; doit lire les modules compilés par ce Clang. La configuration contrôle la version du binaire retenu et refuse une version majeure différente. |
| clang-format | 21.1.8 exactement | Formatage selon [`.clang-format`](../../.clang-format) ; deux versions différentes peuvent formater différemment |

Ces planchers sont alignés sur ceux de mddlog afin qu'une intégration éventuelle n'oblige pas à les relever ; ils seront réexaminés par l'ADR [#16](https://github.com/camille-martin-paris/clepsydre/issues/16).

Le langage est C++23 strict (`CMAKE_CXX_STANDARD 23`, sans extensions). Le code du projet est organisé en modules nommés (`export module clepsydre.…`). Les en-têtes de la bibliothèque standard sont inclus dans le fragment global des modules ; `import std;` n'est pas utilisé tant que sa prise en charge par CMake reste expérimentale.

Toute autre famille de compilateurs (MSVC, compilateurs embarqués) est refusée à la configuration. La chaîne croisée de la cible embarquée sera ajoutée après le choix de l'architecture de calcul ([#14](https://github.com/camille-martin-paris/clepsydre/issues/14)).

### Versions vérifiées

| Date | Système | Compilateurs | Résultat |
| --- | --- | --- | --- |
| 2026-10-09 | Ubuntu 26.04 | GCC 16.1, Clang 20, 21 et 22, clang-tidy 20 et 21, CMake 4.2, Ninja 1.13 | Configuration, compilation, édition de liens et tests réussis |
| 2026-10-09 | Ubuntu 26.04 | GCC 15.2 | Refusé à la configuration, comme attendu |

La CI vérifie les compilateurs planchers à chaque pull request, dans des images épinglées par empreinte : `gcc:16.1.0` (CMake 4.2.3 téléchargé et vérifié par SHA-256) et `ubuntu:26.04` (Clang 20, CMake 4.2.3). Voir [Intégration continue](#intégration-continue).

## Commandes

Les préréglages de [`CMakePresets.json`](../../CMakePresets.json) placent chaque configuration dans `build/<préréglage>` :

```bash
cmake --workflow --preset gcc     # configuration, compilation, édition de liens, tests
cmake --workflow --preset clang
```

Étape par étape :

```bash
cmake --preset gcc                                    # configuration
cmake --build --preset gcc --target clepsydre_compile # compilation seule, sans édition de liens
cmake --build --preset gcc                            # édition de liens des exécutables
ctest --preset gcc                                    # exécution des tests
```

Une configuration réussie ne vaut pas build réussi : chaque étape peut échouer indépendamment.

Les préréglages appellent `g++` et `clang++`. Pour un compilateur suffixé par sa version, le remplacer en ligne de commande, par exemple `cmake --preset clang -DCMAKE_CXX_COMPILER=clang++-20`, ou dans un `CMakeUserPresets.json` local, ignoré par Git.

| Préréglage | Compilateur | Type de build | Particularité |
| --- | --- | --- | --- |
| `gcc` | GCC | Debug | |
| `clang` | Clang | Debug | |
| `clang-tidy` | Clang | Debug | clang-tidy exécuté sur chaque unité de traduction |
| `gcc-release` | GCC | RelWithDebInfo | |
| `gcc-asan`, `clang-asan` | GCC, Clang | Debug | AddressSanitizer et UndefinedBehaviorSanitizer |
| `gcc-tsan`, `clang-tsan` | GCC, Clang | Debug | ThreadSanitizer |

## Qualité

- **Avertissements** : l'ensemble défini dans [`cmake/ClepsydreToolchain.cmake`](../../cmake/ClepsydreToolchain.cmake) est traité comme des erreurs (`CLEPSYDRE_WARNINGS_AS_ERRORS`, activé par défaut).
- **Formatage** : `cmake --build --preset gcc --target clepsydre_format_check` échoue si un fichier n'est pas formaté ; `clepsydre_format` le reformate. La CI utilise clang-format 21.1.8 ; pour obtenir la même version localement : `uvx --from clang-format==21.1.8 clang-format -i <fichiers>`.
- **Sanitizers** : `CLEPSYDRE_SANITIZERS` accepte `address`, `undefined` et `thread`, séparés par des virgules ; `thread` et `address` sont incompatibles. Toute erreur détectée interrompt le programme (`-fno-sanitize-recover=all`), donc fait échouer le test.
- **Analyse statique** : `cmake --workflow` n'est pas disponible pour `clang-tidy` ; utiliser `cmake --preset clang-tidy && cmake --build --preset clang-tidy`. Toute remarque est une erreur. Les vérifications exclues sont justifiées dans [`.clang-tidy`](../../.clang-tidy).

### Limites connues de clang-tidy avec les modules

- `readability-redundant-declaration` signale à tort les `operator new` et `operator delete` déclarés par la bibliothèque standard dans le fragment global d'un module : vérification désactivée.
- `modernize-use-std-print` provoque un plantage de clang-tidy 20 et 21 sur un appel à `std::fprintf` dans une unité qui importe un module. Le code de test utilise `std::println` et `std::fputs`, ce qui évite le cas.

## Tests

Les tests sont des exécutables enregistrés dans CTest par `clepsydre_add_test` (voir [`tests/CMakeLists.txt`](../../tests/CMakeLists.txt)). Un test réussit s'il effectue au moins une vérification et qu'aucune n'échoue. Le support minimal [`tests/support/expect.hpp`](../../tests/support/expect.hpp) évite toute dépendance externe ; le choix d'un cadriciel de test sera consigné avec l'épinglage des dépendances ([#22](https://github.com/camille-martin-paris/clepsydre/issues/22)).

## Intégration continue

Le workflow [`.github/workflows/ci.yml`](../../.github/workflows/ci.yml) s'exécute sur chaque pull request et sur chaque poussée vers `develop` et `main`.

| Tâche | Contenu | Phases publiées séparément |
| --- | --- | --- |
| `build (<compilateur>, <variante>)` | GCC 14 et Clang 18 (versions minimales), chacun en `debug`, `asan-ubsan` et `tsan` : six combinaisons | Configuration, compilation, édition de liens, tests ; un tableau de résultats par tâche dans le résumé de l'exécution |
| `format` | clang-format 21.1.8 sur tous les fichiers C++ suivis par Git | — |
| `clang-tidy` | Clang 18 et clang-tidy 18, toute remarque bloquante | Configuration, analyse |
| `licences` | `reuse lint` (reuse 6.2.0) | — |
| `registry` ([`registry.yml`](../../.github/workflows/registry.yml)) | Tests du vérificateur et cohérence du registre de traçabilité | Tests, vérification |

La matrice de traçabilité générée s'ajoutera à la tâche `registry` ([#11](https://github.com/camille-martin-paris/clepsydre/issues/11)).

Réglages d'exécution des sanitizers en CI : `halt_on_error=1` pour les trois, détection des fuites mémoire avec AddressSanitizer. Sur Ubuntu 24.04, ThreadSanitizer demande de réduire l'entropie ASLR (`vm.mmap_rnd_bits=28`), sans quoi il refuse de démarrer.

### Contre-épreuves des sanitizers

Pour vérifier que les variantes à sanitizers détectent bien les défauts, un programme d'essai temporaire (non versionné) a été exécuté le 2026-10-09 avec GCC 16.1 et Clang 20.1 :

| Défaut injecté | Variante | Diagnostic | Code de sortie |
| --- | --- | --- | --- |
| Lecture hors d'un `std::vector` | `asan-ubsan` | `AddressSanitizer: heap-buffer-overflow` | 1 |
| Dépassement d'un entier signé | `asan-ubsan` | `runtime error: signed integer overflow` | 1 |
| Incrément concurrent sans synchronisation | `tsan` | `ThreadSanitizer: data race` | 66 |
