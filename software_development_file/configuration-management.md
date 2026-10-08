# Gestion de configuration

Référence : IEC 62304, §8.

## Éléments de configuration

| Élément | Identification |
| --- | --- |
| Code source, scripts, configuration de build et de CI | Chemin dans le dépôt, commit Git |
| Documents du dossier et exigences | Chemin dans le dépôt, révision de document, commit Git |
| Sources matérielles | Chemin dans `hardware/`, révision matérielle (`A`, `B`…) |
| Composants tiers (SOUP) | Identifiant `SOUP-nnn`, version épinglée ([soup.md](soup.md)) |
| Outils de build et de vérification | Nom et version épinglée dans la configuration de build ou de CI |
| Versions publiées | Étiquette Git annotée `vX.Y.Z` sur `main` |

## Contrôle des modifications

- Toute modification passe par une issue et une pull request vers `develop`, selon CONTRIBUTING.md ([#4](https://github.com/camille-martin-paris/clepsydre/issues/4)).
- La pull request décrit l'effet sur les exigences, les risques et les SOUP ; elle est relue avant fusion.
- `develop` et `main` sont protégées : pas de poussée directe ni forcée.
- L'historique Git (commits signés `Signed-off-by`, pull requests, approbations) constitue l'historique des modifications.

## Identification d'une version

Une version publiée est reconstructible à partir de son étiquette : sources, versions des dépendances et des outils, et instructions de build sont fixées dans le dépôt. Chaque version publiée est accompagnée de ses notes de version, de la liste des anomalies résiduelles, d'une SBOM ([#22](https://github.com/camille-martin-paris/clepsydre/issues/22)) et de l'avertissement d'usage non clinique.

## État de la configuration

L'état de chaque élément est consultable dans le dépôt : historique Git, registre de traçabilité, [reviews.md](reviews.md) pour les acceptations.
