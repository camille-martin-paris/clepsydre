# Stratégie de vérification

| Champ | Valeur |
| --- | --- |
| Statut | Version initiale, soumise à relecture |
| Révision | A |
| Issue | [#72](https://github.com/camille-martin-paris/clepsydre/issues/72) |
| Références | IEC 62304:2006+A1:2015 §5.1.6, §5.5 à §5.7, §7.3 ; ISO 14971:2019 §7.2 |

> [!CAUTION]
> Clepsydre ne doit pas être utilisé sur un être humain ni sur un animal. Une vérification réussie montre qu'une exigence est satisfaite dans les conditions de l'essai ; elle ne vaut ni validation clinique ni conformité.

## Objet

Cette stratégie fixe comment le projet démontre que chaque exigence du [registre](registry/requirements.toml) est satisfaite et que chaque mesure de maîtrise des risques est mise en œuvre et efficace. Elle définit les niveaux de vérification, les méthodes, les critères d'entrée, de réussite et de couverture, l'indépendance des revues et la conservation des preuves. Elle s'applique au logiciel embarqué (classe C, voir la [classification](safety-classification.md)), au simulateur lorsqu'il sert de moyen de vérification, et aux essais du système sur banc.

## Vocabulaire

- **Vérification** : confirmation, par des preuves objectives, qu'une exigence est satisfaite.
- **Vérification prévue** : entrée VER-nnn du registre au statut `planned`. Elle désigne une vérification à mener, **pas un résultat**.
- **Preuve** : résultat enregistré (journal d'exécution, rapport d'essai, compte rendu de revue), rattaché à une révision identifiée et conservé selon [Conservation des preuves](#conservation-des-preuves).

## Méthodes

Les méthodes sont celles du champ `method` du registre :

| Méthode | Registre | Usage | Exemple |
| --- | --- | --- | --- |
| Essai | `test` | Exécution avec des entrées définies et comparaison à un résultat attendu | Test unitaire, essai sur simulateur, mesure sur banc |
| Analyse | `analysis` | Raisonnement, calcul ou modélisation sans exécution du produit | Analyse temporelle, analyse statique, analyse de la couverture de diagnostic |
| Inspection | `inspection` | Examen visuel ou documentaire d'un artefact | Lecture d'un écran, revue de code, contrôle d'un marquage |
| Démonstration | `demonstration` | Exécution montrant une fonction, sans mesure quantitative | Parcours d'utilisation commenté |

L'essai est la méthode par défaut. Une autre méthode est retenue lorsqu'un essai est impossible ou insuffisant (absence de défaillance, indépendance, temps de réponse au pire cas) ; elle complète alors l'essai plutôt que de le remplacer.

## Niveaux de vérification

| Niveau | Objet | Environnement | Moyens | Référence IEC 62304 |
| --- | --- | --- | --- | --- |
| Unitaire | Unité logicielle isolée : module C++ ou classe | Poste de développement et CI, sur hôte | Tests CTest, sanitizers, analyse statique, revue de code | §5.5 |
| Intégration | Assemblage d'unités et leurs interfaces : machine à états, alarmes, journal | CI, sur hôte | Tests CTest multi-unités, doublures des pilotes matériels | §5.6 |
| Système logiciel | Logiciel embarqué complet, face à un modèle physique de la pompe avec injection de défauts | Simulateur ([#21](https://github.com/camille-martin-paris/clepsydre/issues/21)), CI | Scénarios automatisés : occlusion, air, coupure, défaillance d'actionneur | §5.7 |
| Système sur banc | Logiciel sur la cible matérielle, avec mécanisme et capteurs réels | Bancs instrumentés ([#73](https://github.com/camille-martin-paris/clepsydre/issues/73), [#74](https://github.com/camille-martin-paris/clepsydre/issues/74), [#75](https://github.com/camille-martin-paris/clepsydre/issues/75)) | Protocoles d'essai versionnés, instruments étalonnés | §5.7 ; ISO 14971 §7.2 |
| Pré-essais normatifs | Sécurité électrique, CEM, protection contre les liquides | Laboratoire ou banc dédié ([#76](https://github.com/camille-martin-paris/clepsydre/issues/76)) | Protocoles inspirés des normes ; pas d'essai de type | — |

L'évaluation formative de l'aptitude à l'utilisation ([#40](https://github.com/camille-martin-paris/clepsydre/issues/40)) relève de l'IEC 62366-1 ; elle ne vérifie pas d'exigence au sens de ce document, mais ses résultats peuvent en réviser.

### Niveau attendu par type d'exigence

Chaque vérification prévue est affectée au niveau le plus bas où elle est probante :

| Type d'exigence | Niveau attendu |
| --- | --- |
| SW-REQ (logicielle) | Unitaire ou intégration ; système logiciel lorsque l'exigence porte sur un comportement d'ensemble |
| SYS-REQ dont la satisfaction dépend du logiciel seul (machine à états, saisie, alarmes, journal) | Système logiciel sur simulateur, puis confirmation sur banc |
| SYS-REQ de performance physique (précision, délais d'occlusion, bolus, air, autonomie) | Système sur banc ; le simulateur ne suffit pas, car il ne reproduit que ce que son modèle contient |
| HW-REQ (matérielle) | Système sur banc ou pré-essais normatifs |

Le niveau de chaque VER-nnn sera inscrit au registre avec la matrice de traçabilité ([#11](https://github.com/camille-martin-paris/clepsydre/issues/11)).

## Critères

### Critères d'entrée

Une vérification n'est exécutée que si :

- l'exigence vérifiée est identifiée au registre ;
- l'élément vérifié est identifié par une révision Git (commit) et, sur banc, par la configuration matérielle et les numéros des instruments ;
- le protocole, le test ou la liste de contrôle et le résultat attendu sont écrits **avant** l'exécution et versionnés.

### Critères de réussite

- Un essai réussit si chaque résultat observé satisfait le critère d'acceptation écrit dans le test ou le protocole ; un test qui n'effectue aucune vérification échoue (voir [`tests/support/expect.hpp`](../tests/support/expect.hpp)).
- Un essai interrompu, non concluant ou exécuté sur une révision non identifiée n'est pas une réussite.
- Une analyse ou une inspection réussit si sa conclusion est consignée, relue et sans réserve bloquante.

### Critères de vérification des unités (classe C)

Pour chaque unité logicielle, la revue et les tests unitaires couvrent, lorsqu'ils s'appliquent, les critères complémentaires de l'IEC 62304 §5.5.4 pour la classe C : séquences d'événements, flux de données et de contrôle, allocation des ressources prévue, traitement des défauts (définition, isolement, reprise), initialisation des variables, autodiagnostics, gestion de la mémoire et ses débordements, et conditions aux limites. La revue de code consigne dans la pull request les critères examinés.

### Couverture

| Couverture | Critère | Mesure |
| --- | --- | --- |
| Exigences | Chaque exigence a au moins une vérification prévue ; aucune exigence `approved` sans vérification | [`tools/check_registry.py`](../tools/check_registry.py), en CI, refuse déjà une exigence `approved` sans vérification ; le contrôle d'une vérification prévue pour chaque exigence vient avec [#11](https://github.com/camille-martin-paris/clepsydre/issues/11) |
| Mesures de maîtrise | Chaque mesure a au moins une exigence, et chaque exigence de mesure au moins une vérification de **mise en œuvre** ; l'**efficacité** de la mesure est démontrée par une vérification au niveau système | Registre, matrice de traçabilité ([#11](https://github.com/camille-martin-paris/clepsydre/issues/11)) |
| Structurelle (code de classe C) | 100 % des instructions et des branches exécutées par les tests unitaires et d'intégration ; toute exception est justifiée dans la pull request (code défensif inatteignable, par exemple) | Instrumentation de couverture en CI, à mettre en place avant le premier code de classe C du jalon M1 |
| Défauts | Chaque condition d'alarme et chaque transition vers l'état sûr est provoquée au moins une fois par injection de défaut | Scénarios du simulateur et du banc |

La couverture structurelle mesure ce que les tests exécutent, pas ce qu'ils vérifient ; elle ne remplace ni la couverture des exigences ni la revue.

### Régression

Chaque pull request exécute l'ensemble des tests automatisés, avec et sans sanitizers, l'analyse statique et le contrôle du registre ([`build.md`](../docs/development/build.md#intégration-continue)). Une modification qui touche une exigence, une mesure ou une unité de classe C fait réexécuter les vérifications manuelles correspondantes avant la publication suivante ; l'analyse d'impact est consignée dans la pull request.

## Indépendance des revues

- Le relecteur d'une modification n'en est pas l'auteur ; le relecteur d'une vérification n'a pas écrit l'élément vérifié ([plan de développement](development-plan.md#rôles-et-responsabilités)).
- Un protocole d'essai sur banc est relu par une personne qui n'a pas conçu l'élément essayé, lorsque l'effectif le permet ; sinon le cumul est consigné comme dans [reviews.md](reviews.md) (réserve R1).
- Un assistant logiciel (LLM, revue automatique) peut aider à rédiger ou à relire ; il ne tient aucun rôle et sa sortie n'est pas une preuve. Une personne nommée relit et répond du résultat ([assistance par LLM](development-plan.md#assistance-par-llm)).
- Les outils qui produisent des preuves (CI, vérificateur de registre, banc) sont qualifiés selon leur effet sur ces preuves ([#89](https://github.com/camille-martin-paris/clepsydre/issues/89)) ; tant qu'ils ne le sont pas, la réserve R3 s'applique.

## Conservation des preuves

- **Identification** : chaque preuve porte la révision vérifiée (commit), la date, la personne ou l'exécution CI responsable, et la configuration (compilateur, image, instruments).
- **Exécutions CI** : les résultats et résumés d'une exécution GitHub Actions ne sont conservés que pour une durée limitée par GitHub. Ils ne sont donc pas la preuve d'archive. Pour chaque version publiée, les journaux de tests, rapports de couverture et résultats d'analyse sont joints à la version et repris dans le rapport de vérification ([#77](https://github.com/camille-martin-paris/clepsydre/issues/77)).
- **Essais sur banc** : protocole, données brutes, résultats et écarts sont versionnés dans le dépôt avec le rapport d'essai.
- **Registre** : une vérification ne passe à `passed` ou `failed` qu'avec un champ `reference` vers la preuve versionnée. Le passage à `passed` d'une vérification se fait dans une pull request relue.
- **Durée** : les preuves d'une version sont conservées au moins aussi longtemps que cette version reste publiée.

## Échecs et anomalies

Un échec de vérification ouvre une anomalie selon le [processus de résolution des problèmes](problem-resolution.md). La vérification reste `failed` jusqu'à ce qu'une nouvelle exécution sur une révision corrigée réussisse ; l'échec antérieur reste consigné. Une anomalie résiduelle à la publication est listée dans les notes de version avec son effet sur la sécurité.

## Rapport de vérification

Chaque version publiée est accompagnée d'un rapport ([#77](https://github.com/camille-martin-paris/clepsydre/issues/77)) qui donne, pour cette révision : la matrice de traçabilité, le statut de chaque vérification avec sa preuve, la couverture mesurée, les anomalies résiduelles et les réserves.

## Révision

Cette stratégie est réexaminée lors de la mise en place du simulateur (M1), des bancs (M2) et de chaque campagne de vérification (M4), et après toute modification de la classification de sécurité.

## Historique

| Révision | Date | Auteur | Modification |
| --- | --- | --- | --- |
| A | 2026-10-09 | Ambroise Leclerc | Création, soumise à relecture |
