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

La CI vérifie les compilateurs planchers à chaque pull request, dans des images épinglées par empreinte : `gcc:16.1.0` (CMake 4.2.3 et Ninja 1.13.2 téléchargés et vérifiés par SHA-256) et `ubuntu:26.04` (Clang 20, CMake 4.2.3 et Ninja 1.13.2, depuis un instantané daté de l'archive). Voir [Intégration continue](#intégration-continue) et [Dépendances épinglées](#dépendances-épinglées).

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

Les tests sont des exécutables enregistrés dans CTest par `clepsydre_add_test` (voir [`tests/CMakeLists.txt`](../../tests/CMakeLists.txt)). Un test réussit s'il effectue au moins une vérification et qu'aucune n'échoue. Le support minimal [`tests/support/expect.hpp`](../../tests/support/expect.hpp) évite toute dépendance externe ; aucun cadriciel de test n'est retenu à ce jour. Un cadriciel ajouté serait une dépendance épinglée selon [Dépendances épinglées](#dépendances-épinglées) ; il n'est pas embarqué et n'est donc pas un SOUP.

## Dépendances épinglées

Chaque dépendance du build et de la CI est épinglée par **révision complète** : deux exécutions sur le même commit utilisent ainsi les mêmes composants ([#22](https://github.com/camille-martin-paris/clepsydre/issues/22)). La tâche `pins` le contrôle à chaque pull request avec [`tools/check_pins.py`](../../tools/check_pins.py) :

| Dépendance | Épinglage | Où |
| --- | --- | --- |
| Action GitHub | Commit complet (40 caractères), version en commentaire | `.github/workflows/` |
| Image de conteneur (dont `fsfe/reuse:6.2.0` pour la tâche `licences`) | Empreinte SHA-256 | `.github/workflows/` |
| Paquets apt (image `ubuntu:26.04`) | Instantané daté de l'archive Ubuntu (`APT_SNAPSHOT`), installé par [`tools/ci/apt-install.sh`](../../tools/ci/apt-install.sh) ; aucun appel direct à `apt` | `ci.yml` |
| Archive téléchargée (CMake, Ninja) | Version exacte ; SHA-256 vérifié par `sha256sum --check` dans le même pas | `ci.yml` |
| Outils Python (clang-format) | Verrou donnant la version exacte et les empreintes de toutes les dépendances, installé par `pip --require-hashes --only-binary=:all:` | [`tools/requirements/`](../../tools/requirements/) |
| Python des tâches | Version complète X.Y.Z dans `actions/setup-python` | `.github/workflows/` |
| Dépendance CMake (`FetchContent`, `ExternalProject`) | `GIT_TAG` de commit complet ou `URL_HASH SHA256` ; aucune à ce jour | `CMakeLists.txt`, `cmake/` |

L'instantané n'est servi qu'en HTTPS, et l'image de base n'a pas de certificats racine. Le script installe donc d'abord `ca-certificates` depuis la poche de publication de la distribution, figée depuis sa sortie. Dans les deux cas, apt authentifie les index par la clé de l'archive Ubuntu.

**Cache des paquets apt** ([#107](https://github.com/camille-martin-paris/clepsydre/issues/107)). Les index et les paquets de l'instantané sont mis en cache par `actions/cache`. La clé combine `APT_SNAPSHOT`, l'empreinte du script et la liste des paquets ; une même clé désigne donc toujours le même contenu. Avec un cache valide, le script installe **sans réseau** : un paquet absent du cache fait basculer vers le téléchargement. Sans cache, chaque téléchargement est tenté plusieurs fois, et un index injoignable fait échouer le script avec un message explicite. Limites :
- une panne de `snapshot.ubuntu.com` bloque encore le premier remplissage du cache, c'est-à-dire une nouvelle valeur de `APT_SNAPSHOT`, une nouvelle liste de paquets ou un cache expiré, car GitHub efface un cache inutilisé pendant 7 jours ;
- les paquets lus dans le cache sont contrôlés par apt contre les index en cache, eux-mêmes authentifiés à leur téléchargement ; un cache n'est écrit que par les workflows du dépôt, dans la portée de sa branche.

**Monter une version** : dans une pull request dédiée, qui ne fait que cela et dont la CI est verte. La description donne la raison, l'ancienne et la nouvelle version, et l'effet sur les SOUP le cas échéant.

- Outils Python : modifier le fichier `.in`, puis régénérer le verrou en excluant les publications de moins de deux semaines, et rétablir l'en-tête du fichier :

  ```bash
  uv pip compile --generate-hashes --exclude-newer <date ISO, il y a 14 jours> \
    --python-version 3.12 --python-platform x86_64-manylinux_2_28 \
    tools/requirements/clang-format.in -o tools/requirements/clang-format.txt
  ```

- Paquets apt : avancer `APT_SNAPSHOT` dans `ci.yml`.
- Archive téléchargée : changer la version et l'empreinte, relevée sur la page de publication amont.

**Limites** :
- l'image des exécuteurs GitHub (`ubuntu-24.04`) évolue sans épinglage possible ; les tâches installent donc elles-mêmes leurs outils, épinglés, ou s'exécutent dans une image épinglée ;
- le contrôle est textuel : il ne remplace pas la relecture d'une montée de version ;
- l'outil reste soumis à la réserve R3 ([#89](https://github.com/camille-martin-paris/clepsydre/issues/89)).

## Nomenclature logicielle (SBOM)

[`tools/sbom.py`](../../tools/sbom.py) génère la SBOM au format CycloneDX 1.6 (JSON). Elle décrit :
- le logiciel embarqué, avec sa licence et son commit ;
- chaque composant tiers (SOUP) du [registre](../../software_development_file/registry/soup.toml), avec sa version, son fournisseur, sa licence et, s'il est connu, son identifiant Package URL.

La SBOM n'est générée que si le registre est cohérent. Elle est reproductible : son horodatage est la date du commit.

- À chaque pull request, la tâche `registry` génère la SBOM de la révision et la publie comme artefact.
- À la poussée d'une étiquette `vX.Y.Z`, le workflow [`version.yml`](../../.github/workflows/version.yml) génère la SBOM de la version, après le contrôle de l'épinglage et du registre. Le processus de publication la joint à la version, avec le rapport de vérification ([#77](https://github.com/camille-martin-paris/clepsydre/issues/77)) : les artefacts d'exécution ne sont conservés que pour une durée limitée.

Les outils de build et de vérification ne figurent pas dans la SBOM, qui décrit le produit ; ils sont épinglés comme indiqué ci-dessus.

## Intégration continue

Le workflow [`.github/workflows/ci.yml`](../../.github/workflows/ci.yml) s'exécute sur chaque pull request et sur chaque poussée vers `develop` et `main`.

| Tâche | Contenu | Phases publiées séparément |
| --- | --- | --- |
| `build (<compilateur>, <variante>)` | GCC 16.1 et Clang 20 (planchers), chacun en `debug`, `asan-ubsan` et `tsan` : six combinaisons, dans les images épinglées | Configuration, compilation, édition de liens, tests ; un tableau de résultats par tâche dans le résumé de l'exécution |
| `format` | clang-format 21.1.8 sur tous les fichiers C++ suivis par Git | — |
| `clang-tidy` | Clang 20 et clang-tidy 20 (image `ubuntu:26.04`), toute remarque bloquante | Configuration, analyse |
| `licences` | `reuse lint` (reuse 6.2.0) | — |
| `pins` | [`tools/check_pins.py`](../../tools/check_pins.py) : chaque dépendance du build et de la CI est épinglée par révision complète | — |
| `registry` ([`registry.yml`](../../.github/workflows/registry.yml)) | Tests des outils du dépôt, cohérence du registre de traçabilité, matrice de traçabilité publiée dans le résumé et comme artefact, SBOM de la révision publiée comme artefact | Tests, vérification, matrice, SBOM |

Chaque dépendance est épinglée ([Dépendances épinglées](#dépendances-épinglées)). Chaque tâche `build` et `clang-tidy` consigne en outre, dans le résumé de l'exécution, la version de CMake et la liste complète des paquets installés avec leur version.

Réglages d'exécution des sanitizers en CI : `halt_on_error=1` pour les trois, détection des fuites mémoire avec AddressSanitizer. Par précaution, les conteneurs sont lancés sans filtre seccomp : ThreadSanitizer peut désactiver l'ASLR par `personality()`, appel que le profil seccomp par défaut de Docker restreint. La nécessité de ce réglage n'a pas été démontrée.

### Contre-épreuves des sanitizers

Pour vérifier que les variantes à sanitizers détectent bien les défauts, un programme d'essai temporaire (non versionné) a été exécuté le 2026-10-09 avec GCC 16.1 et Clang 20.1 :

| Défaut injecté | Variante | Diagnostic | Code de sortie |
| --- | --- | --- | --- |
| Lecture hors d'un `std::vector` | `asan-ubsan` | `AddressSanitizer: heap-buffer-overflow` | 1 |
| Dépassement d'un entier signé | `asan-ubsan` | `runtime error: signed integer overflow` | 1 |
| Incrément concurrent sans synchronisation | `tsan` | `ThreadSanitizer: data race` | 66 |
