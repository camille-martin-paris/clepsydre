# ADR 0001 : licences des fichiers matériels et de la documentation

| Champ | Valeur |
| --- | --- |
| Statut | **Acceptée** le 2026-10-09 par Camille Martin, approbatrice ([approbation](https://github.com/camille-martin-paris/clepsydre/pull/84#pullrequestreview-5466991305), révision `9b038d1`). Statut consigné le 2026-10-10 dans [reviews.md](../../software_development_file/reviews.md) |
| Date | 2026-10-09 |
| Issue | [#3](https://github.com/camille-martin-paris/clepsydre/issues/3) |

## Contexte

Le logiciel de Clepsydre est distribué sous [EUPL-1.2](../../LICENSE), licence à réciprocité : toute version modifiée et distribuée du logiciel reste sous EUPL-1.2 ou sous une licence compatible de son annexe.

Le projet publiera aussi des sources matérielles (schémas, circuits imprimés, modèles mécaniques, nomenclature) et une documentation de conception (exigences, analyses de risques, protocoles et résultats d'essais). Ces contenus ne sont pas du logiciel : l'EUPL-1.2 n'y est pas adaptée, notamment parce qu'elle ne traite ni de la fabrication d'objets physiques ni de la notion de « sources » d'un matériel.

Le choix doit :

1. garder l'esprit de réciprocité de l'EUPL-1.2 : une amélioration distribuée d'une pompe à perfusion doit rester ouverte, car sa sécurité dépend de la possibilité de l'étudier ;
2. protéger la traçabilité : un dérivé doit pouvoir être distingué de l'original ;
3. être reconnu par l'écosystème open hardware, en vue d'une éventuelle certification OSHWA ([#82](https://github.com/camille-martin-paris/clepsydre/issues/82)) ;
4. rester lisible par les outils (identifiants SPDX).

## Options évaluées

### Matériel (électronique et mécanique)

| Option | Réciprocité | Points forts | Points faibles |
| --- | --- | --- | --- |
| CERN-OHL-S v2 | Forte : tout le matériel distribué qui intègre la conception doit en publier les sources | Cohérente avec l'EUPL-1.2 ; impose la publication des sources des modifications, utile à la revue de sécurité ; impose le marquage des modifications | Peut dissuader un industriel de combiner la conception avec des blocs propriétaires |
| CERN-OHL-W v2 | Faible : réciprocité limitée aux fichiers sous licence, pas aux blocs ajoutés | Permet d'intégrer des composants propriétaires | Une version modifiée peut fermer une partie critique pour la sécurité |
| CERN-OHL-P v2 | Aucune (permissive) | Adoption la plus large | Aucune garantie que les améliorations restent publiques |

### Documentation

| Option | Réciprocité | Points forts | Points faibles |
| --- | --- | --- | --- |
| CC BY-SA 4.0 | Partage à l'identique | Les documents dérivés restent ouverts ; cohérente avec l'EUPL-1.2 et la CERN-OHL-S ; compatibilité unidirectionnelle vers la GPLv3 | Contraignante pour réutiliser un extrait dans un document sous une autre licence |
| CC BY 4.0 | Aucune | Réutilisation la plus simple | Un dossier dérivé peut être fermé |

## Décision

| Contenu | Emplacement | Licence | Identifiant SPDX |
| --- | --- | --- | --- |
| Logiciel, scripts, configuration de build et de CI | `src/`, `tests/`, `examples/`, `tools/`, `.github/`, fichiers de build | EUPL-1.2 (inchangée) | `EUPL-1.2` |
| Sources matérielles : électronique, mécanique, nomenclature | `hardware/` | CERN-OHL-S v2 | `CERN-OHL-S-2.0` |
| Documentation | `docs/`, `software_development_file/`, `README.md` et autres fichiers Markdown à la racine | CC BY-SA 4.0 | `CC-BY-SA-4.0` |

Les extraits de code présents dans la documentation sont aussi disponibles sous EUPL-1.2, afin qu'ils puissent être repris dans le logiciel.

Les textes intégraux sont dans `LICENSES/`, nommés par identifiant SPDX. Le fichier `LICENSE` à la racine reste celui de l'EUPL-1.2, licence du logiciel, pour la détection automatique par GitHub.

## Marquage des fichiers

La licence de chaque fichier est déclarée selon la [spécification REUSE 3.3](https://reuse.software/spec-3.3/) :

- `REUSE.toml` associe chaque répertoire à sa licence ; c'est la source de vérité pour les fichiers qui ne peuvent pas porter d'en-tête (formats binaires de CAO, images, nomenclatures exportées).
- Les fichiers texte qui acceptent les commentaires (C++, CMake, scripts, KiCad) portent en plus un en-tête :

  <!-- REUSE-IgnoreStart -->
  ```cpp
  // SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
  // SPDX-License-Identifier: EUPL-1.2
  ```
  <!-- REUSE-IgnoreEnd -->

- Un fichier sous une licence différente de celle de son répertoire (composant tiers, par exemple) porte son propre en-tête, qui l'emporte sur `REUSE.toml`.

La conformité pourra être vérifiée en CI avec `reuse lint` ([#20](https://github.com/camille-martin-paris/clepsydre/issues/20)).

## Conséquences

- Toute personne qui distribue un matériel fabriqué à partir de Clepsydre, modifié ou non, doit en rendre les sources disponibles et indiquer ses modifications (CERN-OHL-S v2, sections 3 et 4).
- Les composants tiers intégrés au matériel (empreintes, modèles 3D de fabricants) doivent avoir une licence compatible ou être identifiés comme « composants disponibles » au sens de la CERN-OHL-S v2.
- La réutilisation de la documentation impose l'attribution et le partage à l'identique.
- L'EUPL-1.2 cite dans son annexe la CC BY-SA 3.0, pas la 4.0. Cette différence est sans effet tant que la documentation et le logiciel restent des œuvres distinctes ; tout projet de fusion d'un document dans le logiciel devra la réexaminer.
- Aucune de ces licences ne vaut autorisation d'usage clinique ; voir l'usage prévu ([#2](https://github.com/camille-martin-paris/clepsydre/issues/2)).

## Références

- [EUPL-1.2](https://interoperable-europe.ec.europa.eu/collection/eupl/eupl-text-eupl-12) et son annexe des licences compatibles
- [CERN Open Hardware Licence v2](https://ohwr.org/project/cernohl/-/wikis/home) et son guide d'utilisation
- [Creative Commons BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/legalcode.fr)
- [Spécification REUSE 3.3](https://reuse.software/spec-3.3/)
