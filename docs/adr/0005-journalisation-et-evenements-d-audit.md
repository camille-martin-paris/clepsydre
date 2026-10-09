# ADR 0005 : journaux de diagnostic, événements d'audit et bibliothèque de journalisation

| Champ | Valeur |
| --- | --- |
| Statut | Proposée, soumise à la décision de l'auteur puis à l'acceptation de l'approbatrice |
| Date | 2026-10-09 |
| Issue | [#16](https://github.com/camille-martin-paris/clepsydre/issues/16) |
| Épique | [#12](https://github.com/camille-martin-paris/clepsydre/issues/12) ; mise en œuvre : [#41](https://github.com/camille-martin-paris/clepsydre/issues/41) |
| Dépend de | [ADR 0003](0003-architecture-de-calcul-et-canal-de-securite.md) (deux processeurs) ; ADR de [#15](https://github.com/camille-martin-paris/clepsydre/issues/15) (réserve à la coupure) et de [#17](https://github.com/camille-martin-paris/clepsydre/issues/17) (datation, identité des flux), proposées en parallèle |

> [!IMPORTANT]
> **Conflit d'intérêts.** La bibliothèque proposée, [mddlog](https://github.com/ambroise-leclerc/mddlog), a pour auteur Ambroise Leclerc, qui est aussi l'auteur de cette ADR et contributeur de Clepsydre. L'évaluation de ce composant tiers doit donc être relue par une autre personne. Son acceptation éventuelle ne dispense pas de l'évaluation SOUP complète avant l'intégration ([#44](https://github.com/camille-martin-paris/clepsydre/issues/44)).

## Contexte

Le journal d'événements de Clepsydre est une **boîte noire** : après un incident, la séquence réelle des programmations, alarmes et actions doit pouvoir être établie (RISK-011, CTRL-015). Exigences concernées :

- **SYS-REQ-018 / SW-REQ-008** : un événement n'est enregistré qu'après écriture durable confirmée ; tout refus ou toute perte est signalé ;
- **SYS-REQ-019 / SW-REQ-009** : altération, suppression ou retour arrière détectables, sur l'appareil ou hors de l'appareil ; séquence strictement croissante et code de détection chaîné ;
- **SYS-REQ-034** : contenu du journal et conservation d'au moins 10 000 événements, y compris sans alimentation ;
- **SYS-REQ-035, SYS-REQ-037** : datation, consultation de l'historique sur l'appareil.

Critères de choix de l'issue :

1. mémoire bornée, refus explicite plutôt qu'écrasement silencieux ;
2. persistance avec confirmation durable, résistance aux coupures ;
3. détection d'altération et de retour arrière par un ancrage indépendant, vérification hors de l'appareil ;
4. licence compatible, C++23, version épinglable, anomalies connues publiées.

## Séparation des deux journaux

| | Événements d'audit | Journaux de diagnostic |
| --- | --- | --- |
| Rôle | Preuve de ce qui s'est passé : actions, alarmes, configuration, cycle de vie (SYS-REQ-034) | Aide au développement et à la maintenance |
| Exigences | SYS-REQ-018, 019, 034, 035 ; classe C | Aucune exigence de sécurité ; ne servent jamais de preuve |
| Perte | Jamais silencieuse : refus signalé, compteur visible, politique de l'appareil ([#46](https://github.com/camille-martin-paris/clepsydre/issues/46)) | Admise, comptée |
| Persistance | Écriture durable confirmée, chaînage, ancrage | Mémoire volatile bornée ; extraction par le port de maintenance |
| Vocabulaire | Fermé, défini par [#42](https://github.com/camille-martin-paris/clepsydre/issues/42) | Libre |

Un même fait peut produire un événement d'audit et une trace de diagnostic, mais jamais l'un à la place de l'autre.

## Options évaluées

| Critère | A. mddlog 0.3.0 | B. Solution interne | C. Bibliothèques généralistes (spdlog, Quill, journal de Zephyr…) |
| --- | --- | --- | --- |
| 1. Mémoire bornée, refus explicite | Oui : cœur sans allocation, anneaux qui refusent quand ils sont pleins ([ADR-001](https://github.com/ambroise-leclerc/mddlog/blob/v0.3.0/docs/adr/ADR-001-allocation-free-governed-logging-core.md)) | À concevoir | Non en général : files qui écrasent ou allouent |
| 2. Persistance, confirmation durable, coupures | Oui : segments en ajout seul, durabilité confirmée par le support, scénarios de coupure validés sur hôte ([ADR-004](https://github.com/ambroise-leclerc/mddlog/blob/v0.3.0/docs/adr/ADR-004-audit-persistence-and-tamper-evidence.md)) | À concevoir | Non |
| 3. Altération, retour arrière, vérification hors appareil | Oui : chaînage SHA-256 par flux, fournisseur d'ancrage extérieur, lecteur avec verdict par flux, position retenue hors appareil | À concevoir | Non |
| 4. Licence, C++23, épinglage, anomalies | EUPL-1.2 (licence du projet) ; modules C++23 ; étiquette `v0.3.0`, commit `073761b` ; anomalies publiées sur GitHub | EUPL-1.2 ; tout à écrire et à vérifier en classe C | Licences compatibles ; C++17 ou C++20 ; pas d'audit |
| Points faibles | Version antérieure à 1.0 : API et formats non figés ([#121](https://github.com/ambroise-leclerc/mddlog/issues/121)) ; **aucune cible embarquée testée** (Linux et macOS seulement) ; adaptateurs de diagnostic qui allouent ; `import std` requis ; aucun fournisseur d'ancrage indépendant réel livré ; un seul consommateur par chaîne ; conflit d'intérêts | Effort important, sans retour d'usage extérieur | Ne satisfont pas les critères 1 à 3 |

## Décision proposée

**Option A, mddlog, sous conditions.** **Cette décision revient d'abord à l'auteur, puis à l'acceptation de l'approbatrice**, après relecture indépendante de l'évaluation (voir l'encadré).

### Usage retenu

- **Audit, sur le processeur de commande** : modèle d'événement (`AuditEvent`), anneau borné (`AuditRing`), persistance (`PersistingAuditSink`) sur la mémoire non volatile de la carte, et pilotage (`AuditService`).
- **Flux** : un flux par producteur et par démarrage. Les événements du processeur de sécurité sont transmis par la liaison de l'ADR 0003 et enregistrés dans **leur propre flux**, jamais réattribués à la commande. L'identité des flux et la datation relèvent de l'ADR de #17.
- **Ancrage indépendant sur l'appareil** : le **processeur de sécurité** tient le rôle de fournisseur d'ancrage. Il conserve, dans sa propre mémoire non volatile, la position et le condensat atteints par chaque flux. C'est un processeur, une mémoire et un logiciel distincts du journal (ADR 0003), donc un témoin indépendant au sens de l'ADR-004 de mddlog.
- **Vérification hors de l'appareil** ([#47](https://github.com/camille-martin-paris/clepsydre/issues/47)) : le lecteur de mddlog, sur un poste de maintenance, avec la position retenue d'une lecture à l'autre, pour détecter le retour arrière conjoint du journal et de son ancrage.
- **Diagnostic** : uniquement le cœur sans allocation (`GovernedRecord`, `RingLog`) sur la cible. Les adaptateurs de diagnostic qui allouent (`SimpleLogger`, `TextLogger`) restent réservés au simulateur et aux outils sur hôte.
- **Support de stockage** : mémoire flash NOR externe pour les segments du journal, dimensionnée pour au moins 10 000 événements avec la rotation (SYS-REQ-034), et durée d'écriture d'un segment communiquée à l'ADR de #15 pour dimensionner la réserve de maintien. Le contrat de stockage de mddlog (`StorageMedium`) est implémenté et qualifié par le projet.
- **Politique en cas de refus ou de perte** : jamais silencieuse ; la politique précise de l'appareil (poursuite, alarme technique, refus de démarrer une perfusion) est fixée par [#46](https://github.com/camille-martin-paris/clepsydre/issues/46).

### Conditions, à remplir avant tout usage dans du code de classe C

1. **Épinglage** : commit `073761b7a6d5ed29ed87bc37c85967db72386d3c` (étiquette `v0.3.0`), récupéré par `FetchContent` avec `GIT_TAG` de commit complet, comme le contrôle l'exige ([build.md](../development/build.md#dépendances-épinglées)).
2. **Portage sur la cible démontré** : compilation des modules utilisés par la chaîne croisée du processeur de commande et tests du cœur sur la cible ou un émulateur ; absence d'allocation et d'exception dans les modules embarqués, vérifiée par le contrôle de symboles. **Sans cette démonstration, l'option B est reprise**, en conservant le format canonique publié par mddlog pour garder le lecteur hors appareil.
3. **`import std`** : mddlog l'exige. La règle de [build.md](../development/build.md) qui l'écarte tant que sa prise en charge par CMake est expérimentale doit être réexaminée avec #44 : elle est levée pour la consommation de mddlog, ou mddlog doit offrir une variante sans `import std`.
4. **Évaluation SOUP complète** (#44), selon les six points de [soup.md](../../software_development_file/soup.md#évaluation-dun-soup), **relue par une personne autre que l'auteur de mddlog**.
5. **Montée de version** vers une 1.0 : dans une PR dédiée, avec réévaluation, puisque l'API et les formats ne sont pas figés.

## Conséquences

- **Registre SOUP** : mddlog est inscrit comme SOUP-001, en version 0.3.0, avec ses anomalies connues à la date de la revue. L'inscription vaut composant retenu sous conditions ; l'évaluation complète suit dans #44.
- **Exigences** : à préciser dans une révision ultérieure de la spécification : support de stockage et volume de rétention ; fournisseur d'ancrage tenu par le processeur de sécurité ; séparation entre audit et diagnostic.
- **Analyse des dangers** : RISK-011 gagne une cause (défaillance du composant tiers de journalisation), maîtrisée par CTRL-015, l'ancrage indépendant et la vérification hors appareil.
- **Issues** : #41 à #50 s'appuient sur ce choix ; #43 garde une journalisation minimale du simulateur, comme point de comparaison pour #44.

## Limites

- Les qualités annoncées de mddlog sont démontrées par ses propres tests sur hôte, pas sur la cible de Clepsydre.
- L'auteur de mddlog est un contributeur de Clepsydre : sans relecture indépendante, l'évaluation n'est pas indépendante.
- Rien n'est signé : la détection d'altération repose sur l'ancrage indépendant, pas sur une signature ([mddlog #123](https://github.com/ambroise-leclerc/mddlog/issues/123)).

## Références

- [mddlog v0.3.0](https://github.com/ambroise-leclerc/mddlog/tree/v0.3.0), ADR-001 à ADR-006 et rapport de validation de la persistance
- [Composants tiers (SOUP)](../../software_development_file/soup.md)
- [Spécification des exigences](../requirements/requirements-specification.md)
- [Analyse préliminaire des dangers](../requirements/preliminary-hazard-analysis.md), RISK-011
