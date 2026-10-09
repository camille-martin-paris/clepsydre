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

Seules les opérations attestées par l'historique Git et GitHub sont consignées ici.

| Opération | Contributions concernées | Responsable | Moyen |
| --- | --- | --- | --- |
| Rédaction, commits et ouverture des PR | Livrables des PR #83 à #88, sauf le commit ci-dessous | Ambroise Leclerc, à sa demande et sous son compte | Claude Code (modèle Claude Opus 5.5, Anthropic), service distant |
| Rédaction du contact privé du code de conduite | PR #85 : commit `5106f4a`, devenu `a7eae8b` après rebase puis `6cefba0` sur `develop` | Camille Martin, à sa demande et sous son compte | Codex |
| Réponses aux remarques de relecture, corrections, rebase | PR #83 à #88 | Ambroise Leclerc, sous son compte | Claude Code (modèle Claude Opus 5.5, Anthropic), service distant |
| Analyse de relecture publiée | PR #83 à #88 | Camille Martin, sous son compte | Codex |
| Revue automatique complémentaire | PR #83 à #88 | — | GitHub Copilot (revue de pull request) ; ne compte pas comme relecture |
| Approbations | PR #83 à #88 | Camille Martin, à sa demande explicite et sous son compte | Codex |
| Fusion | #83, sous le compte `ambroise-leclerc` | Ambroise Leclerc | Non consigné |
| Fusions | #84 à #88, sous le compte `camille-martin-paris` | Camille Martin, à sa demande explicite | Codex |

Ces traces établissent la responsabilité et l'autorisation des personnes nommées. Elles ne prouvent pas, à elles seules, une lecture personnelle de chaque contribution par son auteur ou par la mainteneuse : les attestations de lecture personnelle sont consignées ci-dessous. Une personne qui a effectué cette lecture peut l'attester en ajoutant ici une entrée datée, avec les contributions concernées.

#### Attestations de lecture personnelle

| Date | Personne | Contributions | Attestation | Limite |
| --- | --- | --- | --- | --- |
| 2026-10-09 | Ambroise Leclerc | Contributions assistées des PR #83 à #88 | « Je confirme que chaque contribution assistée a été relue et acceptée par moi ou par Camille. » | L'attestation ne précise pas, pour chaque contribution, laquelle des deux personnes l'a lue ; voir l'entrée suivante pour Camille Martin. |
| 2026-10-09 | Camille Martin | Dossier initial et PR #83 à #88 | « Je confirme avoir personnellement relu et accepté les contributions concernées par le dossier initial et les PR #83 à #88. Cette confirmation atteste ma lecture personnelle, en complément des analyses assistées par Codex et des approbations exécutées à ma demande. » ([commentaire sur #90](https://github.com/camille-martin-paris/clepsydre/pull/90#issuecomment-6077223597)) | La décision d'acceptation du dossier reste assortie des réserves R1 à R3. |

Ces assistants ne sont pas qualifiés ; leur usage relève de la réserve R3. Le projet vise à les remplacer par une assistance locale, avec des LLM à poids ouverts dont l'aptitude aux usages du projet est qualifiée ; voir le [plan de développement](development-plan.md#assistance-par-llm).
