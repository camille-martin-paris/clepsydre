# Revues et acceptations

Chaque acceptation d'un document du dossier ou d'une version publiée est consignée ici, avec son auteur, sa date, la révision acceptée et les réserves éventuelles. La pull request correspondante porte la discussion de relecture.

| Date | Document ou version | Révision (commit) | Auteur | Relecteur | Approbateur | Décision | Réserves |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 2026-10-09 | Dossier de développement initial : `software_development_file/`, `tools/check_registry.py` et ses tests, `.github/workflows/registry.yml` | `c06e004` sur `develop` ; contenu identique à la révision approuvée `9765b55` de [#87](https://github.com/camille-martin-paris/clepsydre/pull/87#pullrequestreview-5466991745) | Ambroise Leclerc | Camille Martin | Camille Martin | Acceptée avec réserves | R1 à R3 ci-dessous |

Décisions possibles : acceptée, acceptée avec réserves, refusée. Lorsqu'un assistant logiciel (LLM, outil de revue automatique) a contribué à la rédaction ou à la relecture, l'entrée le précise dans la section « Moyens de rédaction et de revue » : la personne nommée reste responsable du contenu, l'assistant est un moyen. Une acceptation sous cumul de rôles porte la réserve « revue non indépendante » ([plan de développement](development-plan.md#attribution-actuelle-et-cumuls-de-rôles)).

## Réserves

### Dossier de développement initial (2026-10-09)

| Réserve | Objet | Levée prévue |
| --- | --- | --- |
| R1 | Revue non indépendante : la relectrice est aussi l'approbatrice (cumul consigné dans le [plan de développement](development-plan.md#attribution-actuelle-et-cumuls-de-rôles)). | Nouvelle relecture par une personne indépendante avant toute évaluation de conformité |
| R2 | La classe de sécurité C est proposée, pas confirmée. | Analyse préliminaire des dangers [#9](https://github.com/camille-martin-paris/clepsydre/issues/9) |
| R3 | `tools/check_registry.py` n'est pas qualifié comme outil : il contrôle la structure et la complétude du registre, pas la justesse de son contenu ni l'immuabilité des versions SOUP. Cette acceptation ne vaut ni qualification de l'outil ni validation du dispositif. | Qualification des outils, y compris des assistants LLM ([#89](https://github.com/camille-martin-paris/clepsydre/issues/89)), en complément de la chaîne de vérification ([#20](https://github.com/camille-martin-paris/clepsydre/issues/20)) |

## Moyens de rédaction et de revue

### Dossier de développement initial et épique [#1](https://github.com/camille-martin-paris/clepsydre/issues/1) (2026-10-09)

| Activité | Personne responsable | Assistance utilisée |
| --- | --- | --- |
| Rédaction des livrables des PR #83 à #88 | Ambroise Leclerc | Claude Code (modèle Claude Opus 5.5, Anthropic), service distant |
| Relecture et approbation des PR #83 à #88 | Camille Martin | Codex, sous le compte GitHub de Camille Martin |
| Revue automatique complémentaire | — | GitHub Copilot (revue de pull request) ; ses remarques ont été examinées par l'auteur, elles ne comptent pas comme relecture |

Chaque contribution assistée a été relue et acceptée par une personne nommée. Ces assistants ne sont pas qualifiés ; leur usage relève de la réserve R3. Le projet vise à les remplacer par une assistance locale, avec des LLM à poids ouverts dont l'aptitude aux usages du projet est qualifiée ; voir le [plan de développement](development-plan.md#assistance-par-llm).
