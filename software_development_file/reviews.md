# Revues et acceptations

Chaque acceptation d'un document du dossier ou d'une version publiée est consignée ici, avec son auteur, sa date, la révision acceptée et les réserves éventuelles. La pull request correspondante porte la discussion de relecture.

| Date | Document ou version | Révision (commit) | Auteur | Relecteur | Approbateur | Décision | Réserves |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 2026-10-09 | Dossier de développement initial : `software_development_file/`, `tools/check_registry.py` et ses tests, `.github/workflows/registry.yml` | `c06e004` sur `develop` ; contenu identique à la révision approuvée `9765b55` de [#87](https://github.com/camille-martin-paris/clepsydre/pull/87#pullrequestreview-5466991745) | Ambroise Leclerc | Camille Martin | Camille Martin | Acceptée avec réserves | R1 à R3 ci-dessous |
| 2026-10-09 | Plan de gestion des risques et analyse préliminaire des dangers ; confirmation de la classe C | `75169c378cf326937f730a5a436baa69d42734f0` sur `develop`, fusion de [#94](https://github.com/camille-martin-paris/clepsydre/pull/94) | Ambroise Leclerc | Camille Martin, revue assistée par Codex | Camille Martin, décision explicite exécutée par Codex | Acceptée pour confirmer la classe C ; R2 levée | R1 et R3 restent ouvertes ; les mesures de maîtrise proposées restent à vérifier |
| 2026-10-09 | ADR 0002 : principe de pompage, `docs/adr/0002-principe-de-pompage.md` | `bd0188b9201b6d720c4f43dbec66a22f241c92c6` de [#100](https://github.com/camille-martin-paris/clepsydre/pull/100#pullrequestreview-5471704019) ; seul le statut est aligné ensuite sur l'acceptation | Ambroise Leclerc | Camille Martin, revue assistée par Codex | Camille Martin, acceptation explicite exécutée par Codex | Acceptée | Revue non indépendante (cumul relectrice et approbatrice, voir R1) ; usage prévu, exigences et analyse des dangers à réviser selon l'ADR ; aucune validation sur banc |

Décisions possibles : acceptée, acceptée avec réserves, refusée. Lorsqu'un assistant logiciel (LLM, outil de revue automatique) a contribué à la rédaction ou à la relecture, l'entrée le précise dans la section « Moyens de rédaction et de revue » : la personne nommée reste responsable du contenu, l'assistant est un moyen. Une acceptation sous cumul de rôles porte la réserve « revue non indépendante » ([plan de développement](development-plan.md#attribution-actuelle-et-cumuls-de-rôles)).

## Réserves

### Dossier de développement initial (2026-10-09)

| Réserve | Objet | Levée prévue |
| --- | --- | --- |
| R1 | Revue non indépendante : la relectrice est aussi l'approbatrice (cumul consigné dans le [plan de développement](development-plan.md#attribution-actuelle-et-cumuls-de-rôles)). | Nouvelle relecture par une personne indépendante avant toute évaluation de conformité |
| R2 — levée le 2026-10-09 | La classe de sécurité C, initialement proposée, est confirmée par l'analyse préliminaire des dangers. | Acceptation explicite par Camille Martin de l'analyse [#9](https://github.com/camille-martin-paris/clepsydre/issues/9), intégrée par [#94](https://github.com/camille-martin-paris/clepsydre/pull/94) au commit `75169c378cf326937f730a5a436baa69d42734f0` |
| R3 | `tools/check_registry.py` n'est pas qualifié comme outil : il contrôle la structure et la complétude du registre, pas la justesse de son contenu ni l'immuabilité des versions SOUP. Cette acceptation ne vaut ni qualification de l'outil ni validation du dispositif. | Qualification des outils, y compris des assistants LLM ([#89](https://github.com/camille-martin-paris/clepsydre/issues/89)), en complément de la chaîne de vérification ([#20](https://github.com/camille-martin-paris/clepsydre/issues/20)) |

## Moyens de rédaction et de revue

### Décisions d'architecture : ADR 0002 [#100](https://github.com/camille-martin-paris/clepsydre/pull/100) (2026-10-09)

| Opération | Contributions concernées | Responsable | Moyen |
| --- | --- | --- | --- |
| Rédaction et corrections | ADR 0002 | Ambroise Leclerc, sous son compte | Claude Code (Claude Opus 5.5, Anthropic), selon la déclaration d'assistance de la PR |
| Choix de l'option retenue | ADR 0002, option A | Ambroise Leclerc, auteur, décision explicite | Claude Code, consignation de la décision |
| Analyse de relecture et acceptation | ADR 0002, révision `bd0188b` | Camille Martin, décision explicite d'acceptation | Codex, analyse et exécution de l'approbation ([approbation](https://github.com/camille-martin-paris/clepsydre/pull/100#pullrequestreview-5471704019)) |
| Revue automatique complémentaire | #100 | — | GitHub Copilot ; ne compte pas comme relecture |

Ces opérations ne constituent pas une attestation de lecture personnelle ; une telle attestation doit être formulée par la personne concernée.

### Analyse préliminaire des dangers [#94](https://github.com/camille-martin-paris/clepsydre/pull/94) et levée de R2 [#95](https://github.com/camille-martin-paris/clepsydre/pull/95) (2026-10-09)

| Opération | Contributions concernées | Responsable | Moyen |
| --- | --- | --- | --- |
| Rédaction et corrections | #94 : plan de gestion des risques, analyse préliminaire des dangers et registre | Ambroise Leclerc, sous son compte | Claude Code (Claude Opus 5.5, Anthropic), selon la déclaration d'assistance de #94 |
| Analyse de relecture, approbations et fusion | #94 | Camille Martin, à sa demande et sous son compte | Codex |
| Revue automatique complémentaire | #94 | — | GitHub Copilot ; ne compte pas comme relecture |
| Décision de lever R2 | Analyse acceptée au commit `75169c378cf326937f730a5a436baa69d42734f0` | Camille Martin, décision explicite | Codex, exécution et consignation de la décision |
| Rédaction et corrections documentaires | #95 : revues, classification et historique de l'analyse | Camille Martin, à sa demande et sous son compte | Codex |
| Relecture et propositions de compléments | #95, révision `9070b55` | Ambroise Leclerc, sous son compte | Claude Code (Claude Opus 5.5, Anthropic), selon sa [relecture](https://github.com/camille-martin-paris/clepsydre/pull/95#pullrequestreview-5469111528) |

Ces opérations ne constituent pas une attestation de lecture personnelle des contributions de #94 ou #95. Une telle attestation doit être formulée par la personne concernée ; aucune n'est déduite de l'utilisation d'un assistant ou de l'exécution d'une approbation.

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

### Acceptation des ADR 0002 et 0003 (2026-10-09)

Camille Martin a demandé explicitement l’acceptation des ADR des PR #100 et #101 après publication des corrections. Codex a vérifié les changements et exécuté les approbations sous son compte. La rédaction et les corrections sont attribuées à Ambroise Leclerc, assisté par Claude Code selon les descriptions des PR. Cette décision accepte les principes d’architecture et leurs conséquences documentées ; les preuves d’efficacité et d’indépendance restent à établir. Elle n’atteste pas une lecture personnelle et ne vaut pas validation du dispositif. Le cumul relectrice/approbatrice reste consigné sous R1, et l’assistance non qualifiée relève de R3.
