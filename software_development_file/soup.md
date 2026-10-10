# Composants logiciels tiers (SOUP)

Références : IEC 62304, §5.3.3, §5.3.4, §7.1.2, §8.1.2.

Un SOUP est un composant logiciel embarqué dans Clepsydre qui n'a pas été développé selon le dossier de développement du projet : bibliothèque tierce, bibliothèque standard, système d'exploitation, pilote fourni par un fabricant.

## Composants retenus

La source lisible par machine est [`registry/soup.toml`](registry/soup.toml) ; le tableau ci-dessous en donne la vue lisible.

| Identifiant | Composant | Version épinglée | Usage | Exigences de fonctionnement et de performance | Anomalies connues (date de revue) |
| --- | --- | --- | --- | --- | --- |
| SOUP-001 | [mddlog](https://github.com/ambroise-leclerc/mddlog), EUPL-1.2, projet mddlog (Ambroise Leclerc) | 0.3.0, commit `073761b7a6d5ed29ed87bc37c85967db72386d3c` | Audit et cœur de diagnostic sur le processeur de commande ([ADR 0005](../docs/adr/0005-journalisation-et-evenements-d-audit.md)) ; **retenu sous conditions**, évaluation complète dans [#44](https://github.com/camille-martin-paris/clepsydre/issues/44) | SYS-REQ-018, SYS-REQ-019, SYS-REQ-034, SYS-REQ-052, SW-REQ-008, SW-REQ-009, SW-REQ-019 | Aucune anomalie étiquetée bug ouverte ; mddlog #147 (TSan, libc++ 21, chemin non retenu) ; antérieure à 1.0 ; aucune cible embarquée testée ; pas de fournisseur d'ancrage réel livré (2026-10-09) |

## Composants envisagés

| Composant | Usage envisagé | Décision attendue |
| --- | --- | --- |
| Bibliothèque standard C++23 de la chaîne retenue | Ensemble du logiciel | Chaîne de build [#19](https://github.com/camille-martin-paris/clepsydre/issues/19) |

## Évaluation d'un SOUP

Avant inscription au registre, une pull request documente pour le composant :

1. l'identification : nom, version exacte, fournisseur, licence ;
2. l'usage : éléments logiciels qui l'utilisent, fonctions employées ;
3. les exigences de fonctionnement et de performance attendues, sous forme d'exigences du registre ;
4. le matériel et le logiciel nécessaires à son fonctionnement ;
5. les anomalies publiées pertinentes, leur effet possible sur la sécurité et la date de leur revue ;
6. les mesures de maîtrise des risques liées à une défaillance du composant.

Limite du contrôle automatique : `tools/check_registry.py` vérifie seulement la syntaxe de la version (ni intervalle, ni joker, ni alias de branche ou de canal comme `latest`, `main`, `release` ou `stable`). Il ne garantit pas qu'une étiquette désigne toujours le même contenu, car elle peut être déplacée en amont. La revue SOUP vérifie l'immuabilité, par exemple par une empreinte ou un commit précis.

Les versions sont épinglées dans la configuration de build ([build.md](../docs/development/build.md#dépendances-épinglées)). Le registre donne pour chaque SOUP son fournisseur (`supplier`), sa licence en expression SPDX (`license`) et, s'il est connu, son identifiant Package URL (`purl`) : `tools/sbom.py` les reprend dans la SBOM. Une mise à jour d'un SOUP suit la même évaluation.
