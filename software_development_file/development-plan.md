# Plan de développement logiciel

| Champ | Valeur |
| --- | --- |
| Révision | A, soumise à relecture |
| Issue | [#5](https://github.com/camille-martin-paris/clepsydre/issues/5) |

## Objet et périmètre

Ce plan s'applique à tout logiciel embarqué dans Clepsydre ou contribuant à sa sécurité : logiciel de commande, superviseur de sécurité, simulateur lorsqu'il sert de moyen de vérification, et outils qui produisent des preuves (vérificateur du registre, scripts d'essai).

Usage visé : [docs/intended-use.md](../docs/intended-use.md). Classe de sécurité proposée : C ([justification](safety-classification.md)).

## Cycle de vie

Le développement est incrémental : chaque jalon livre un ensemble cohérent d'exigences, de conception, de code et de vérifications.

| Jalon | Contenu |
| --- | --- |
| M0 — Fondations | Gouvernance, usage prévu, exigences et analyse de risques initiales, chaîne de build |
| M1 — Simulateur | Machine à états, commande et alarmes vérifiées sur simulateur physique |
| M2 — Banc mécatronique | Commande d'un mécanisme réel sur banc instrumenté |
| M3 — Prototype intégré | Électronique, mécanique, alimentation et logiciel intégrés |
| M4 — Vérification | Campagnes de vérification et rapport par version |

Pour chaque incrément, les activités suivent l'ordre de l'IEC 62304, sans imposer un enchaînement en cascade :

| Activité | Livrable | Emplacement |
| --- | --- | --- |
| Analyse des exigences (§5.2) | Exigences logicielles | `registry/requirements.toml`, [#10](https://github.com/camille-martin-paris/clepsydre/issues/10) |
| Gestion des risques (§7) | Analyse des dangers, mesures de maîtrise | `docs/requirements/`, `registry/risks.toml`, `registry/controls.toml`, [#9](https://github.com/camille-martin-paris/clepsydre/issues/9) |
| Conception de l'architecture (§5.3) | ADR, description de l'architecture, éléments logiciels et leur classe | `docs/adr/`, [#12](https://github.com/camille-martin-paris/clepsydre/issues/12) |
| Conception détaillée (§5.4) | Interfaces des unités logicielles | En-têtes C++ documentés, `docs/` |
| Implémentation et vérification des unités (§5.5) | Code, tests unitaires, analyse statique, revue | `src/`, `tests/` |
| Intégration et tests d'intégration (§5.6) | Tests sur simulateur et sur banc | `tests/`, `examples/` |
| Tests du système logiciel (§5.7) | Rapport de vérification par version | `docs/validation/`, [#77](https://github.com/camille-martin-paris/clepsydre/issues/77) |
| Publication (§5.8) | Version étiquetée, anomalies résiduelles, SBOM | Étiquette Git, notes de version, [#22](https://github.com/camille-martin-paris/clepsydre/issues/22) |

## Normes, méthodes et outils

- Langage : C++23. Conventions : [CONTRIBUTING.md](../CONTRIBUTING.md).
- Build, analyse statique, sanitizers et CI : [#19](https://github.com/camille-martin-paris/clepsydre/issues/19), [#20](https://github.com/camille-martin-paris/clepsydre/issues/20). Les outils et leurs versions sont épinglés.
- Gestion des risques : ISO 14971.
- Revue : chaque modification passe par une pull request relue (voir [rôles](#rôles-et-responsabilités)).

## Rôles et responsabilités

| Rôle | Responsabilité |
| --- | --- |
| Auteur | Rédige un livrable ou une modification et sa vérification ; ouvre la pull request |
| Relecteur | Vérifie la justesse, la complétude et la traçabilité ; approuve ou demande des modifications dans la pull request |
| Approbateur | Accepte un document du dossier ou une version publiée ; consigne la décision dans [reviews.md](reviews.md) |

Règles :

- Le relecteur d'une modification n'en est pas l'auteur.
- L'approbateur d'un document du dossier n'en est pas l'auteur.
- Les rôles sont attribués par livrable et tracés par la pull request (auteur des commits, approbations GitHub) et par [reviews.md](reviews.md).

### Attribution actuelle et cumuls de rôles

| Personne | Rôles tenus |
| --- | --- |
| Mainteneuse du projet ([@camille-martin-paris](https://github.com/camille-martin-paris)) | Approbatrice de tous les livrables ; relectrice ; autrice |
| Contributeurs disposant d'un droit d'écriture | Auteurs ; relecteurs des livrables dont ils ne sont pas auteurs |

**Cumuls consignés.** Le projet compte trop peu de personnes pour séparer complètement les rôles :

1. La mainteneuse cumule les rôles de relectrice et d'approbatrice : une même personne relit et accepte un livrable. Effet : la relecture et l'acceptation ne sont pas indépendantes.
2. Lorsque la mainteneuse est autrice d'un livrable et qu'aucune autre personne habilitée n'est disponible, elle cumule auteur, relecteur et approbateur. Chaque occurrence est signalée dans [reviews.md](reviews.md), avec la réserve « revue non indépendante ».

Ces cumuls sont incompatibles avec une revue indépendante. Un livrable accepté sous cumul devra être relu à nouveau par une personne indépendante avant toute évaluation de conformité.

## Documentation et livrables

Tous les livrables sont des fichiers texte versionnés dans ce dépôt. Une version d'un livrable est identifiée par le commit Git qui la contient ; un livrable accepté est identifié dans [reviews.md](reviews.md) par sa révision.

## Révision du plan

Le plan est mis à jour à chaque changement d'organisation, de cycle de vie ou d'outil ; la mise à jour suit le processus de revue ordinaire.
