# Contribuer à Clepsydre

Merci de votre intérêt pour Clepsydre. Le projet vise une pompe à perfusion : chaque modification doit pouvoir être reliée à une issue, relue et vérifiée. Ce document fixe les règles qui le permettent.

En participant, vous acceptez le [code de conduite](CODE_OF_CONDUCT.md).

> [!CAUTION]
> Clepsydre ne doit pas être utilisé sur un être humain ni sur un animal. Aucune contribution ne doit être présentée comme apte à un usage clinique.

## Avant de commencer

1. Cherchez une issue existante, ou ouvrez-en une avec le modèle adapté : anomalie, exigence, décision ou essai.
2. Pour une modification structurante (architecture, choix d'un composant, changement d'une exigence), ouvrez d'abord une issue de décision : elle aboutira à une ADR dans `docs/adr/`.
3. Indiquez dans l'issue que vous la prenez en charge, pour éviter les travaux en double.

## Modèle de branches

| Branche | Rôle | Qui y écrit |
| --- | --- | --- |
| `develop` | Branche par défaut et d'intégration ; toutes les pull requests la ciblent | Uniquement par pull request fusionnée |
| `main` | Versions publiées ; chaque commit de `main` porte une étiquette `vX.Y.Z` | Uniquement par pull request de publication depuis `develop` |
| `<numéro-d'issue>-<slug>` | Travail sur une issue, par exemple `24-machine-a-etats` | L'auteur de la pull request |

- Créez votre branche depuis `develop` à jour.
- Le slug est en minuscules, sans accents, mots séparés par des tirets.
- Une branche traite une seule issue. Elle est supprimée après fusion.
- Les étiquettes de version suivent le [versionnage sémantique](https://semver.org/lang/fr/) et sont annotées : `git tag -a v0.1.0`.

## Messages de commit

Le format suit [Conventional Commits](https://www.conventionalcommits.org/fr/v1.0.0/), avec une description en français :

```text
<type>(<portée facultative>): <description à l'impératif, sans point final>

<corps facultatif : pourquoi la modification, et ce qu'elle change>

Closes #<numéro>
Signed-off-by: Prénom Nom <adresse@exemple.org>
```

| Type | Usage |
| --- | --- |
| `feat` | Nouvelle fonctionnalité logicielle ou matérielle |
| `fix` | Correction d'une anomalie |
| `docs` | Documentation, exigences, ADR, dossier de développement |
| `test` | Tests, scénarios de vérification, protocoles d'essai |
| `build` | Système de build, dépendances |
| `ci` | Intégration continue |
| `refactor` | Restructuration sans changement de comportement |
| `hw` | Sources matérielles : électronique, mécanique, nomenclature |
| `chore` | Maintenance sans effet sur le produit |

- Ligne de titre de 72 caractères au plus.
- Une référence d'issue par commit : `Closes #n` pour le commit qui la termine, `Refs #n` sinon.
- Un changement incompatible est signalé par `!` après le type et un pied `BREAKING CHANGE:`.

## Certificat d'origine du développeur (DCO)

Le projet retient le [Developer Certificate of Origin 1.1](https://developercertificate.org/) plutôt qu'un accord de licence de contributeur (CLA) :

- le DCO ne transfère aucun droit : chaque contributeur garde ses droits d'auteur et publie sous les licences du projet (EUPL-1.2, CERN-OHL-S v2, CC BY-SA 4.0) ;
- il ne demande ni signature préalable ni gestion d'accords ;
- il trace pour chaque commit l'engagement de son auteur sur l'origine de la contribution, ce qui sert la traçabilité attendue d'un dispositif médical.

Contrepartie : un changement ultérieur de licence demanderait l'accord de tous les contributeurs concernés. L'EUPL-1.2 prévoit déjà la compatibilité avec plusieurs licences, ce qui limite ce besoin.

Chaque commit porte la ligne `Signed-off-by` ; `git commit -s` l'ajoute. Elle certifie que vous avez le droit de soumettre la contribution sous la licence du projet, selon les termes du DCO.

L'adresse de la ligne `Signed-off-by` doit être celle de l'auteur du commit. Pour ne pas publier votre adresse personnelle, utilisez l'adresse `noreply` fournie par GitHub, pour l'auteur comme pour la signature. Personne ne signe pour une autre personne : un commit dont l'auteur n'a pas certifié l'origine est réécrit par cet auteur avant la fusion.

### Commits antérieurs à l'adoption du DCO

Les commits suivants précèdent l'adoption du DCO et ne portent pas `Signed-off-by` :

| Commit | Autrice | Objet |
| --- | --- | --- |
| `483840a` | Camille Martin | Création du dépôt et de la licence EUPL-1.2 |
| `3555d22` | Camille Martin | Présentation du projet dans le README |

Sur `develop`, le contenu de `3555d22` est entré avec la fusion de la pull request #83 (commit `7c4f9be`, Ambroise Leclerc, co-écrit par Camille Martin), elle aussi antérieure à l'adoption du DCO et sans `Signed-off-by`.

Ces commits ne sont pas réécrits : `main` et `develop` sont protégées contre les poussées forcées, et une réécriture changerait l'identifiant de toutes les versions qui en dérivent. Leurs auteurs en certifient l'origine selon les termes du DCO par un commentaire dans l'issue [#4](https://github.com/camille-martin-paris/clepsydre/issues/4). Tout commit postérieur à la fusion de ce document doit porter `Signed-off-by`.

## Pull requests

- Ciblez `develop`. Reliez l'issue avec `Closes #n` dans la description.
- Remplissez le [modèle de pull request](.github/pull_request_template.md), en particulier l'effet sur les exigences, les risques et la vérification.
- Gardez la pull request ciblée : une issue, un sujet.
- Mettez à jour la documentation et le registre de traçabilité dans la même pull request que le code.
- Fusion par *squash* ou *rebase* : l'historique de `develop` reste linéaire.

## Revue

- Toute pull request est relue et approuvée par au moins une personne autre que son auteur, dès que le projet compte plus d'une personne habilitée. Tant que la mainteneuse est seule à relire, le cumul des rôles d'auteur et de relecteur est consigné dans le dossier de développement.
- La personne qui relit vérifie : la correspondance avec l'issue, la justesse, les tests, la mise à jour de la documentation et de la traçabilité, et l'absence de revendication d'usage clinique.
- Chaque fil de discussion est résolu avant la fusion ; la personne qui l'a ouvert le clôt ou accepte la réponse.

## Protection des branches

Réglages appliqués à `develop` et à `main` dans les paramètres du dépôt (accès administrateur requis) :

| Règle | `develop` | `main` |
| --- | --- | --- |
| Pull request obligatoire avant fusion | Oui | Oui |
| Nombre d'approbations requises | 1 | 1 |
| Approbations invalidées par un nouveau commit | Oui | Oui |
| Approbation du dernier commit poussé par une autre personne | Oui | Oui |
| Conversations résolues avant fusion | Oui | Oui |
| Vérifications de statut requises (CI verte) | Oui, dès la mise en place de la CI ([#20](https://github.com/camille-martin-paris/clepsydre/issues/20)) | Oui |
| Branche à jour avant fusion | Oui | Oui |
| Historique linéaire | Oui | Oui |
| Poussée forcée et suppression | Interdites | Interdites |
| Règles appliquées aux administrateurs | Oui | Oui |

`develop` est défini comme branche par défaut du dépôt.

## Conventions C++23

Ces conventions s'appliquent dès l'ajout du premier code ([#19](https://github.com/camille-martin-paris/clepsydre/issues/19)). Le formatage est défini par [`.clang-format`](.clang-format) et vérifié en CI.

| Élément | Convention | Exemple |
| --- | --- | --- |
| Espaces de noms | `snake_case`, racine `clepsydre` | `clepsydre::alarms` |
| Types, concepts, énumérations | `PascalCase` | `InfusionState`, `RateSource` |
| Valeurs d'énumération | `PascalCase` | `InfusionState::Running` |
| Fonctions et variables | `camelCase` | `computeStepRate()` |
| Membres privés | `camelCase` suivi de `_` | `rate_` |
| Constantes `constexpr` | `camelCase` | `maxBolusVolume` |
| Paramètres de modèle | `PascalCase` | `template <typename Clock>` |
| Macros (à éviter) | `MAJUSCULES_SNAKE_CASE`, préfixe `CLEPSYDRE_` | `CLEPSYDRE_ASSERT` |
| Fichiers | `snake_case`, `.hpp` et `.cpp` | `infusion_state.hpp` |

Règles générales :

- Une unité physique est portée par le type, pas par le nom : `std::chrono` pour les durées, types dédiés pour les débits et volumes.
- Pas de variable globale mutable ; pas de `using namespace` dans un en-tête.
- Les avertissements du compilateur sont traités comme des erreurs.
- Chaque fichier source porte l'en-tête SPDX défini par l'ADR des licences.
- Les règles propres au code de sécurité (allocation dynamique, exceptions, temps réel) seront fixées par les ADR d'architecture ([#14](https://github.com/camille-martin-paris/clepsydre/issues/14), [#17](https://github.com/camille-martin-paris/clepsydre/issues/17)).

## Conventions matérielles

- Les sources sont dans `hardware/`, au format natif d'outils libres. Le choix des outils et de leur version est consigné par ADR ; leur version est indiquée dans le `README.md` de chaque sous-répertoire.
- Les fichiers de fabrication (Gerber, perçage, STEP, PDF des plans) sont générés à partir des sources et joints aux versions publiées ; ils ne sont pas modifiés à la main.
- Unités du Système international ; cotes en millimètres.
- Les repères des composants (`R12`, `U3`) restent stables d'une révision à l'autre, pour la traçabilité de la nomenclature.
- La nomenclature est un fichier texte (CSV) avec, pour chaque ligne : repères, quantité, valeur, boîtier, fabricant, référence fabricant et alternatives.
- Chaque révision matérielle est identifiée (`A`, `B`, …) sur la carte ou la pièce, et dans le nom de l'étiquette de version.

## Signaler une vulnérabilité

Ne publiez pas de vulnérabilité dans une issue. Utilisez le [signalement privé de GitHub](https://github.com/camille-martin-paris/clepsydre/security/advisories/new).
