# Vocabulaire et enregistreur des événements d'audit

| Champ | Valeur |
| --- | --- |
| Statut | Conception détaillée, soumise à relecture |
| Issue | [#136](https://github.com/camille-martin-paris/clepsydre/issues/136) |
| Décision d'architecture | [ADR 0005](../adr/0005-journalisation-et-evenements-d-audit.md#usage-retenu), [ADR 0006](../adr/0006-interfaces-internes-horloge-et-datation.md#3-identité-des-flux-dévénements) |
| Vocabulaire | [`event-vocabulary.md`](../event-vocabulary.md) |
| Exigences | SW-REQ-008, SW-REQ-019, SW-REQ-020, SYS-REQ-034, SYS-REQ-035 |
| Code | [`src/audit/`](../../src/audit/) : modules `clepsydre.audit.vocabulary` et `clepsydre.audit.recorder` |
| Classe de sécurité | C ([classification](../../software_development_file/safety-classification.md)) |

> [!CAUTION]
> Clepsydre ne doit pas être utilisé sur un être humain ni sur un animal.

## Objet

Ce document décrit la conception détaillée des deux unités par lesquelles le logiciel de commande et celui de la sécurité enregistrent leurs événements d'audit (IEC 62304 §5.4) :
- le **vocabulaire** traduit en constantes les tableaux du [vocabulaire des événements](../event-vocabulary.md) ;
- l'**enregistreur** compose les identifiants d'un événement, vérifie qu'il appartient au vocabulaire, puis l'admet dans l'anneau borné de mddlog (`AuditRing`).

La persistance, la transmission entre les processeurs et l'ancrage ne sont pas ici : ils consomment l'anneau ([#45](https://github.com/camille-martin-paris/clepsydre/issues/45), [#47](https://github.com/camille-martin-paris/clepsydre/issues/47)). Sur l'hôte, le simulateur les assure avec les adaptateurs de mddlog ([simulateur](../development/simulator.md#commande-simulée-et-journal-daudit)).

## Vocabulaire

`clepsydre.audit.vocabulary` décrit chaque action par un `ActionSpec` : nom, catégorie, phases admises, producteurs, exigence et risque.
- Les **49 actions** hors alarmes sont les valeurs de l'énumération `Action`, dans l'ordre du document.
- Les **72 actions d'alarme** naissent du produit des 18 conditions (`AlarmCondition`) et des 4 étapes (`AlarmStep`). Leurs noms, `alarm.<condition>.<étape>`, sont composés à la compilation dans une mémoire statique.
- `spec()` rend la description d'une action, `at()` celle de rang donné, pour parcourir les 121 actions, et `find()` celle d'un nom. Une valeur hors des énumérations, un rang au-delà du vocabulaire ou un nom inconnu ne rendent rien.

Les phases et les producteurs sont des ensembles de bits (`PhaseSet`, `ProducerSet`), interrogés par `contains()`.

## Enregistreur

Un `Recorder` sert un producteur pendant un démarrage. Il est construit avec son origine (`Origin`) : le numéro de série de la pompe, le processeur, et le numéro de démarrage persistant (SW-REQ-020).

| Élément | Forme | Source |
| --- | --- | --- |
| Flux | `pump-<série>:<producteur>:boot-<n>`, attribué à l'anneau | ADR 0006 |
| Corrélation | `input()`, `condition()` : un compteur chacun, depuis 1 ; `boot()` : le démarrage lui-même | Vocabulaire, « Corrélation » |
| Cible | `pump-<série>` suivi du suffixe de `Target` | Vocabulaire, « Cibles » |
| Détail | `mono=<ms>`, puis ` clock=doubtful` s'il y a lieu, puis les clés de l'action (`Detail`) | Vocabulaire, « Datation » et « Détail » |

`record()` reçoit l'action, par `Action` ou par condition et étape, et les faits de l'événement (`Entry`) : phase, acteur, cible, corrélation, heure civile, temps monotone, heure douteuse, détail. Il vérifie, dans cet ordre, et s'arrête au premier refus :

| Ordre | Vérification | Issue (`Outcome`) |
| --- | --- | --- |
| 1 | Action du vocabulaire | `UnknownAction` |
| 2 | Phase admise pour l'action | `PhaseNotAdmitted` |
| 3 | Producteur admis pour l'action | `ProducerNotAdmitted` |
| 4 | Cible de l'énumération | `TargetInvalid` |
| 5 | Détail, temps monotone compris, dans les 160 octets de mddlog | `DetailTooLong` |
| 6 | Admission par l'anneau : identifiants valides, flux valide, anneau non plein | `Refused`, avec le motif de mddlog |

Sinon, l'événement est admis (`Admitted`), avec la séquence attribuée par mddlog. Les références d'exigence et de risque viennent du vocabulaire, jamais de l'appelant.

**Refus comptés.** `refused()` compte tous les refus de l'enregistreur ; l'anneau compte les siens quand il est plein. Un refus n'est jamais silencieux : l'appelant consigne la perte par `journal.loss` dès que l'anneau l'admet de nouveau (SW-REQ-008). Le simulateur le fait ([`command.cpp`](../../src/sim_console/command.cpp)) ; la politique de l'appareil relève de [#46](https://github.com/camille-martin-paris/clepsydre/issues/46).

**Rien n'est tronqué.** Un texte borné (`Text`) refuse ce qui dépasse sa limite et se marque en débordement ; un détail débordé est refusé. mddlog refuse de même un identifiant trop long ou invalide, accent compris.

**Origine invalide.** Un numéro de série invalide (vide, plus de 32 caractères, autre chose que des lettres ASCII, des chiffres et le trait d'union) ou un producteur hors énumération rendent l'enregistreur invalide : son anneau n'a pas d'identité, et mddlog refuse tout événement (`InvalidStream`).

## Ressources et traitement des défauts

- **Mémoire** : l'anneau de 64 événements de mddlog, et des textes bornés de 160 octets au plus sur la pile. Aucune allocation dynamique ; seul le cœur de mddlog (`mddlog::core`) est employé.
- **Exceptions** : toutes les fonctions sont `noexcept` et n'en lèvent pas.
- **Fils d'exécution** : un seul fil écrit dans un enregistreur ; un seul consommateur vide son anneau (contrat de mddlog).
- **Fonctions totales** : toute valeur d'énumération, valide ou non, a une issue.

## Vérification

| Critère (IEC 62304 §5.5.4) | Couverture par les tests |
| --- | --- |
| Concordance avec le vocabulaire | [`audit_vocabulary_doc_test`](../../tests/audit_vocabulary_doc_test.cpp) relit les tableaux de `event-vocabulary.md` et compare les 121 actions : catégorie, phases, producteurs, exigence, risque ; chaque référence existe dans le registre. Une contre-épreuve sur une copie altérée du document fait échouer le test |
| Flux de contrôle | [`audit_vocabulary_test`](../../tests/audit_vocabulary_test.cpp) : noms uniques, sans le préfixe réservé `mddlog.`, ordre des alarmes, recherche par nom. [`audit_recorder_test`](../../tests/audit_recorder_test.cpp) : chaque issue de `record()`, chaque cible, identifiants composés |
| Traitement des défauts | Valeurs hors énumération, numéro de série et producteur invalides, acteur accentué, détail trop long seul ou avec le temps monotone, anneau plein sans écrasement, refus comptés |
| Conditions aux limites | Entier de 64 bits le plus grand, zéro, numéro de série de 32 et de 33 caractères, 64 événements admis puis le 65ᵉ refusé |
| Intégration | [`sim_audit_test`](../../tests/sim_audit_test.cpp) : persistance, relecture et vérification par le lecteur de mddlog, séance de la commande simulée |
| Couverture structurelle | 100 % des régions et des branches, sans exclusion (tâche CI `coverage`) |

## Limites

- Le temps monotone et l'heure douteuse sont dans le détail, faute de champs dans mddlog 0.3.0 ([mddlog#154](https://github.com/ambroise-leclerc/mddlog/issues/154), [mddlog#155](https://github.com/ambroise-leclerc/mddlog/issues/155)).
- L'enregistreur ne vérifie pas les clés du détail de chaque action, ni que chaque demande se clôt : ces règles du vocabulaire restent à la charge de l'appelant.
- Le code n'est compilé et testé que sur l'hôte ; son portage sur la cible suit [#135](https://github.com/camille-martin-paris/clepsydre/issues/135).
