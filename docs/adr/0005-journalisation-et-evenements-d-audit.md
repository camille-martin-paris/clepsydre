# ADR 0005 : journaux de diagnostic, événements d'audit et bibliothèque de journalisation

| Champ | Valeur |
| --- | --- |
| Statut | **Acceptée** le 2026-10-09 par Camille Martin, approbatrice ([approbation](https://github.com/camille-martin-paris/clepsydre/pull/110#pullrequestreview-5474251592), révision `6f9d5e9`) ; retenue par l'auteur, Ambroise Leclerc ([décision du 2026-10-10](https://github.com/camille-martin-paris/clepsydre/pull/114#issuecomment-6099664713)) ; acceptation du choix de mddlog sous les cinq conditions de l'ADR, sans acceptation de l'évaluation SOUP. Statut consigné le 2026-10-10 dans [reviews.md](../../software_development_file/reviews.md) |
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

## Décision retenue sous conditions

**Option A, mddlog 0.3.0, sous les cinq conditions ci-dessous.** Choix retenu par l'auteur, Ambroise Leclerc, le 2026-10-10 ([décision](https://github.com/camille-martin-paris/clepsydre/pull/114#issuecomment-6099664713)), et accepté par Camille Martin ([approbation](https://github.com/camille-martin-paris/clepsydre/pull/110#pullrequestreview-5474251592)). Cette acceptation porte sur le choix sous conditions ; elle ne vaut pas acceptation de l'évaluation SOUP. L'évaluation complète de #44 doit être relue par une personne autre que l'auteur de mddlog avant tout usage dans du code de classe C (voir l'encadré).

### Usage retenu

- **Audit, sur le processeur de commande** : modèle d'événement (`AuditEvent`), anneau borné (`AuditRing`), persistance (`PersistingAuditSink`) sur la mémoire non volatile de la carte, et pilotage (`AuditService`).
- **Flux** : un flux par producteur et par démarrage. Les événements du processeur de sécurité sont transmis par la liaison de l'ADR 0003 et enregistrés dans **leur propre flux**, jamais réattribués à la commande. L'identité des flux et la datation relèvent de l'ADR de #17.
- **Ancrage sur l'appareil** : le **processeur de sécurité** tient le rôle de fournisseur d'ancrage, avec la frontière de confiance définie ci-dessous. La séparation des processeurs ne suffit pas à elle seule : le témoin **calcule lui-même** la chaîne qu'il ancre.
- **Vérification hors de l'appareil** ([#47](https://github.com/camille-martin-paris/clepsydre/issues/47)) : le lecteur de mddlog, sur un poste de maintenance, avec la position retenue d'une lecture à l'autre, pour détecter le retour arrière conjoint du journal et de son ancrage.
- **Diagnostic** : uniquement le cœur sans allocation (`GovernedRecord`, `RingLog`) sur la cible. Les adaptateurs de diagnostic qui allouent (`SimpleLogger`, `TextLogger`) restent réservés au simulateur et aux outils sur hôte.
- **Support de stockage** : mémoire flash NOR externe pour les segments du journal, dimensionnée pour au moins 10 000 événements avec la rotation (SYS-REQ-034), et durée d'écriture d'un segment communiquée à l'ADR de #15 pour dimensionner la réserve de maintien. Le contrat de stockage de mddlog (`StorageMedium`) est implémenté et qualifié par le projet.
- **Politique en cas de refus ou de perte** : jamais silencieuse ; la politique précise de l'appareil (poursuite, alarme technique, refus de démarrer une perfusion) est fixée par [#46](https://github.com/camille-martin-paris/clepsydre/issues/46).

### Frontière de confiance de l'ancrage

Selon l'[ADR-004 de mddlog, décision 7](https://github.com/ambroise-leclerc/mddlog/blob/v0.3.0/docs/adr/ADR-004-audit-persistence-and-tamper-evidence.md), le fournisseur d'ancrage ne vérifie pas lui-même la continuité de la chaîne, et la garantie exclut quiconque peut autoriser l'avancement ou le retrait des ancrages. Si le processeur de commande, qui écrit le journal, transmettait aussi au témoin les positions et condensats à ancrer, il pourrait réécrire le journal puis faire ancrer l'histoire réécrite : la séparation matérielle ne détecterait rien. Le témoin est donc conçu ainsi :

- **Chaîne calculée par le témoin** : chaque événement d'audit, dans son encodage canonique, transite par la liaison de l'ADR de [#17](https://github.com/camille-martin-paris/clepsydre/issues/17) vers le processeur de sécurité. Celui-ci recalcule le chaînage SHA-256 de chaque flux à partir de son propre dernier condensat. L'ancre qu'il conserve est **son** condensat, jamais une valeur fournie par la commande.
- **Avancement** : seul le témoin fait avancer un ancrage, d'un événement à la fois, dans l'ordre des séquences. Un trou, un doublon de contenu différent ou une séquence décroissante est refusé et signalé ; le refus est lui-même un événement de son flux.
- **Retrait** : seule la rotation du journal (#45) le demande, et le témoin n'accepte un retrait que sous la position ancrée et au-delà de la durée de rétention minimale. Il conserve le condensat du préfixe retiré, de sorte qu'une histoire plus ancienne ne puisse pas être substituée.
- **Contrôle croisé** : à chaque démarrage et à intervalles réguliers, la commande relit son journal et le témoin compare sa chaîne à celle du support. Un désaccord est une défaillance technique (alarme) et un verdict « Altered » pour le lecteur hors appareil.

| Défaillance ou atteinte | Couverte ? |
| --- | --- |
| Logiciel ou mémoire de la commande qui altère, tronque ou réécrit le journal après enregistrement | **Oui** : le condensat du témoin diverge |
| Retour arrière du support du journal (restauration d'une ancienne copie) | **Oui** : le témoin conserve une position plus avancée |
| Perte d'un événement entre la commande et le témoin | **Oui** : trou de séquence refusé et signalé |
| Commande qui enregistre dès l'origine un contenu faux (événement inexact, mais chaîné) | **Non** : la chaîne prouve l'absence de modification après enregistrement, pas l'exactitude du fait. Cette atteinte relève des exigences de contenu (SYS-REQ-034) et de la vérification du logiciel de commande |
| Défaillance ou atteinte simultanée des deux processeurs, ou remplacement physique des deux mémoires | **Non** : hors de l'hypothèse de défaillance unique de l'ADR 0003 |
| Retour arrière conjoint du journal et de l'état du témoin | **Partiellement** : détecté par le lecteur hors appareil, mais seulement **au-delà de la dernière position qu'il a retenue** ; l'intervalle depuis sa dernière lecture n'est couvert que par le témoin |

**Hypothèse consignée** : le logiciel du processeur de sécurité et son stockage des ancrages sont de confiance, au même titre que pour la supervision du volume (ADR 0003). Ils sont de classe C. Cette hypothèse borne la promesse : le journal est protégé contre une défaillance unique, pas contre une compromission des deux processeurs.

**Positions retenues hors appareil** (#47) : le poste de maintenance conserve, pour chaque pompe, la dernière position lue de chaque flux, dans un stockage protégé et versionné. Leur intégrité est une condition du contrôle de retour arrière conjoint ; elle relève de la procédure de #47.

**Charge ajoutée au processeur de sécurité** : un SHA-256 par événement et un condensat par flux. Cette charge est faible, mais elle étend son logiciel, qui doit rester simple. Elle est à confirmer par l'analyse temporelle de #29.

### Conditions, à remplir avant tout usage dans du code de classe C

1. **Épinglage** : commit `073761b7a6d5ed29ed87bc37c85967db72386d3c` (étiquette `v0.3.0`), récupéré par `FetchContent` avec `GIT_TAG` de commit complet, comme le contrôle l'exige ([build.md](../development/build.md#dépendances-épinglées)).
2. **Portage sur la cible démontré** : compilation des modules utilisés par la chaîne croisée du processeur de commande et tests du cœur sur la cible ou un émulateur ; absence d'allocation et d'exception dans les modules embarqués, vérifiée par le contrôle de symboles. **Sans cette démonstration, l'option B est reprise**, en conservant le format canonique publié par mddlog pour garder le lecteur hors appareil.
3. **`import std`** : mddlog l'exige. La règle de [build.md](../development/build.md) qui l'écarte tant que sa prise en charge par CMake est expérimentale doit être réexaminée avec #44 : elle est levée pour la consommation de mddlog, ou mddlog doit offrir une variante sans `import std`.
4. **Évaluation SOUP complète** (#44), selon les six points de [soup.md](../../software_development_file/soup.md#évaluation-dun-soup), **relue par une personne autre que l'auteur de mddlog**. Elle couvre le scénario d'une commande qui réécrit le journal et tente de le faire ancrer, ainsi que le fonctionnement de mddlog avec un témoin qui calcule lui-même la chaîne.
5. **Montée de version** vers une 1.0 : dans une PR dédiée, avec réévaluation, puisque l'API et les formats ne sont pas figés.

## Conséquences

- **Registre SOUP** : mddlog est inscrit comme SOUP-001, en version 0.3.0, avec ses anomalies connues à la date de la revue. L'inscription vaut composant retenu sous conditions ; l'évaluation complète suit dans #44.
- **Exigences** : à préciser dans une révision ultérieure de la spécification : support de stockage et volume de rétention ; fournisseur d'ancrage tenu par le processeur de sécurité ; séparation entre audit et diagnostic.
- **Analyse des dangers** : RISK-011 gagne une cause (défaillance du composant tiers de journalisation), maîtrisée par CTRL-015, l'ancrage indépendant et la vérification hors appareil.
- **Issues** : #41 à #50 s'appuient sur ce choix ; #43 garde une journalisation minimale du simulateur, comme point de comparaison pour #44.

## Limites

- Les qualités annoncées de mddlog sont démontrées par ses propres tests sur hôte, pas sur la cible de Clepsydre.
- L'auteur de mddlog est un contributeur de Clepsydre : sans relecture indépendante, l'évaluation n'est pas indépendante.
- Rien n'est signé : la détection d'altération repose sur le témoin et sur le lecteur hors appareil, pas sur une signature ([mddlog #123](https://github.com/ambroise-leclerc/mddlog/issues/123)).
- Le lecteur hors appareil ne protège que les positions qu'il a déjà retenues ; entre deux lectures, seul le témoin protège le journal.

## Références

- [mddlog v0.3.0](https://github.com/ambroise-leclerc/mddlog/tree/v0.3.0), ADR-001 à ADR-006 et rapport de validation de la persistance
- [Composants tiers (SOUP)](../../software_development_file/soup.md)
- [Spécification des exigences](../requirements/requirements-specification.md)
- [Analyse préliminaire des dangers](../requirements/preliminary-hazard-analysis.md), RISK-011
