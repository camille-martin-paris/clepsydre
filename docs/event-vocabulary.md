# Vocabulaire des événements d'audit

| Champ | Valeur |
| --- | --- |
| Statut | Proposé, soumis à relecture |
| Issue | [#42](https://github.com/camille-martin-paris/clepsydre/issues/42) |
| Épique | [#41](https://github.com/camille-martin-paris/clepsydre/issues/41) |
| Dépend de | [ADR 0005](adr/0005-journalisation-et-evenements-d-audit.md) (audit et diagnostic séparés, mddlog) ; [ADR 0006](adr/0006-interfaces-internes-horloge-et-datation.md) (flux, corrélation, datation) |
| Exigences | SYS-REQ-034, SYS-REQ-035, SYS-REQ-054, SW-REQ-001, SW-REQ-016, SW-REQ-019, SW-REQ-021 |
| Classe de sécurité | C ([classification](../software_development_file/safety-classification.md)) |

> [!CAUTION]
> Clepsydre ne doit pas être utilisé sur un être humain ni sur un animal.

## Objet

Ce document fixe le **vocabulaire fermé** des événements d'audit de Clepsydre (ADR 0005, SW-REQ-019) : les actions, leurs catégories et leurs phases, et la forme de l'acteur, de la cible, de la corrélation, du flux et du détail. Chaque événement que la pompe enregistre est un `AuditEvent` de mddlog, rempli selon ces règles.

Il ne traite ni de la persistance (#45), ni de la politique face aux refus et pertes (#46), ni de l'ancrage (#47), ni de la datation (#49), ni du catalogue complet des alarmes (#31). Il en reprend seulement ce qui touche au contenu d'un événement.

## Règles générales

1. **Vocabulaire fermé.** Seules les actions des tableaux ci-dessous peuvent être enregistrées. Une action absente est un défaut du logiciel, pas un événement à créer à la volée. Ajouter ou modifier une action passe par une révision de ce document.
2. **Audit et diagnostic séparés** (SW-REQ-019). Une trace de diagnostic n'emploie jamais ce vocabulaire et n'est jamais enregistrée comme événement d'audit. Un même fait peut produire les deux, chacun dans son journal.
3. **Une demande n'est pas une exécution.** Une action demandée par une personne est enregistrée à chaque phase qu'elle traverse. L'effet n'est consigné qu'en phase « exécutée », une fois établi ([Phases](#phases)).
4. **Pas de troncature.** Les identifiants sont refusés par mddlog s'ils sont trop longs, jamais tronqués. Le détail, seul champ que mddlog tronque, est dimensionné pour tenir dans ses 160 octets. Un détail tronqué est un défaut, détecté par les tests.
5. **Caractères.** Les identifiants (action, acteur, cible, références, corrélation, flux) n'emploient que les lettres ASCII, les chiffres et `_ . : / -` : c'est la règle de mddlog. Ils sont donc écrits sans accent (`securite`, `operateur`). Le préfixe `mddlog.` est réservé aux enregistrements de mddlog lui-même et n'est jamais employé.

## Champs d'un événement

| Champ mddlog | Contenu dans Clepsydre | Capacité mddlog |
| --- | --- | --- |
| `streamId` | Flux du producteur et du démarrage ([Flux](#flux)) ; attribué par l'anneau | 96 octets |
| `sequence` | Numéro d'ordre dans le flux, attribué par mddlog, strictement croissant (SYS-REQ-035, SW-REQ-009) | 64 bits |
| `time` | Heure civile UTC, ou « indisponible » (SYS-REQ-054) ([Datation](#datation)) | |
| `category` | Catégorie fixée par l'action ([Catégories](#catégories)) | |
| `phase` | Phase de l'action ([Phases](#phases)) | |
| `action` | Une action des [tableaux](#actions) | 64 octets |
| `actor` | Acteur ([Acteurs](#acteurs)) ; jamais vide | 64 octets |
| `target` | Cible ([Cibles](#cibles)) | 96 octets |
| `correlationId` | Corrélation ([Corrélation](#corrélation)) ; jamais vide | 117 octets |
| `requirementRef`, `riskRef` | Une exigence et un risque, seulement s'ils existent dans le registre ([Références](#références)) | 64 octets chacun |
| `sourceSequence` | Non employé | |
| `detail` | Paires `clé=valeur` fermées par action ([Détail](#détail)) | 160 octets |

## Catégories

Clepsydre emploie les cinq catégories de mddlog. La catégorie est **fixée par l'action** : elle ne dépend ni de l'acteur ni de la phase.

| Catégorie | mddlog | Actions |
| --- | --- | --- |
| Opérateur | `Operator` | Programmation, démarrage, pause, arrêt, bolus, purge, choix de la seringue, acquittements : ce que le soignant demande à la pompe |
| Maîtrise du risque | `RiskControl` | Alarmes, état sûr, défaillances détectées, perte de lien, intégrité du journal : ce que les mesures de maîtrise du risque détectent ou imposent |
| Cycle de vie | `Lifecycle` | Démarrage, autotests, alimentation, arrêt de l'appareil, fins de perfusion, horloge, maintenance |
| Configuration | `Configuration` | Bibliothèque de médicaments, liste des seringues, logiciel, heure, réglages, mise hors service et remise en service |
| Accès | `Access` | Authentification, fin de session, verrouillage de l'interface (SW-REQ-016, SYS-REQ-023) |

L'issue #42 citait quatre catégories. La cinquième, l'accès, vient de mddlog : elle sépare l'authentification, que SW-REQ-016 impose de journaliser, des modifications de configuration qu'elle autorise.

## Phases

| Phase | mddlog | Sens |
| --- | --- | --- |
| Demandée | `Requested` | Une personne ou un processeur demande l'action. Rien n'est encore fait |
| Confirmée | `Confirmed` | Une confirmation distincte de la demande a été obtenue : appui sur Confirmer observé par le canal de sécurité (SYS-REQ-044), confirmation spécifique au-delà d'une limite souple (SYS-REQ-012) |
| Exécutée | `Executed` | L'effet est établi : transition admise par la machine à états, condition détectée, configuration enregistrée |
| Échouée | `Failed` | Une demande est refusée ou n'aboutit pas, ou un contrôle automatique donne un résultat négatif. Le détail en donne la cause ([Échecs](#échecs)) |

Règles :
- **Chaque demande se clôt** par exactement un événement « exécutée » ou « échouée », de même action et de même corrélation. Seule une coupure peut l'empêcher : la frontière du flux se voit alors au démarrage suivant.
- **Une détection n'a pas de demande.** Les actions que la pompe constate elle-même (alarme, défaillance, bascule d'alimentation) n'ont que la phase « exécutée ».
- **Un contrôle automatique n'a pas de demande non plus** (`device.self-test`, `journal.cross-check`) : il est « exécuté » s'il réussit, « échoué » s'il donne un résultat négatif, sans événement « demandé » avant.
- La colonne « Phases » des tableaux donne les phases admises pour chaque action. Une phase non admise est un défaut.

## Acteurs

| Acteur | Sens |
| --- | --- |
| `operateur` | Personne au clavier, non authentifiée. La pompe ne sait pas si c'est une infirmière ou un médecin (P1, P2) : elle ne revendique pas un rôle qu'elle ignore (SYS-REQ-034 : « lorsqu'il est connu ») |
| `role:<rôle>` | Personne authentifiée par le code d'un rôle (SW-REQ-016). Les rôles sont fixés par [#39](https://github.com/camille-martin-paris/clepsydre/issues/39) ; seul `role:technicien` (P3) est employé à ce jour |
| `commande` | Le logiciel du processeur de commande, de lui-même |
| `securite` | Le logiciel du processeur de sécurité, de lui-même |

Un appui observé par le canal de sécurité reste l'action de l'`operateur` : l'acteur dit qui agit, le flux dit qui enregistre.

## Cibles

| Cible | Objet |
| --- | --- |
| `pump-<série>` | La pompe : démarrage, alimentation, maintenance, accès |
| `pump-<série>/channel-1` | Le canal de perfusion : thérapie et alarmes de perfusion. La pompe n'en a qu'un (SYS-REQ-029) ; le numéro garde la forme de l'issue |
| `pump-<série>/<objet>` | Un objet de la pompe : `library`, `syringe-list`, `software`, `clock`, `journal`, `link`, `battery`, `keypad` |

Le numéro de série, programmé à la fabrication (ADR 0006), n'emploie que des lettres ASCII, des chiffres et le trait d'union, en 32 caractères au plus. Le simulateur emploie `SIM` suivi de chiffres.

## Flux

Selon l'ADR 0006 : `pump-<série>:<producteur>:boot-<n>`, avec `<producteur>` ∈ {`commande`, `securite`} et `<n>` le numéro de démarrage persistant du producteur (SW-REQ-020), en décimal sans zéro initial. Au plus 72 octets.

Les événements d'un processeur sont toujours enregistrés dans **son** flux, jamais réattribués à l'autre (ADR 0005). La colonne « Producteur » des tableaux dit quel processeur enregistre l'action : `C` pour la commande, `S` pour la sécurité.

## Corrélation

La corrélation relie les événements d'une même cause. Elle prend l'une de trois formes, toutes préfixées par l'origine : le producteur qui a créé l'identifiant et son numéro de démarrage.

| Forme | Cause | Exemple |
| --- | --- | --- |
| `pump-<série>:<producteur>:boot-<n>:input-<m>` | Une action d'une personne, lue au clavier (ADR 0006) | Programmation : demande, confirmation, exécution |
| `pump-<série>:<producteur>:boot-<n>:cond-<m>` | Une condition détectée | Occlusion : détection, déclenchement de l'alarme, état sûr, fin |
| `pump-<série>:<producteur>:boot-<n>` | Le démarrage lui-même | Autotests, avertissement de prototype |

Règles :
- `<m>` est un compteur propre au producteur et au démarrage, un pour les entrées, un pour les conditions, strictement croissant.
- L'issue #42 proposait `pump-<série>:boot-<n>:input-<m>`. L'ADR 0006, acceptée depuis, y ajoute le producteur, pour qu'un redémarrage d'un seul processeur ne réutilise aucun identifiant : c'est sa forme qui est retenue.
- La forme `cond-<m>` est ajoutée par ce document. Sans elle, les étapes d'une alarme ne seraient reliées que par leur ordre.
- **Chaque action d'une personne a sa propre entrée** `input-<m>`, même quand elle porte sur une condition : chaque pause du signal sonore, chaque acquittement. Une condition persistante peut en recevoir plusieurs, par exemple une nouvelle pause après la reprise du signal à l'échéance (SYS-REQ-036) ; chacune se clôt séparément, et l'unicité des actions de l'ADR 0006 est préservée.
- **Le lien vers la condition** est porté par la clé `cond` du détail, qui donne la corrélation de la condition sans le préfixe de la pompe : `cond=securite:boot-9:cond-3`. Il en va de même pour l'interruption d'un bolus ou d'une purge par l'état sûr ([Thérapie](#thérapie)).
- **Une action transmise d'un processeur à l'autre** garde sa corrélation (ADR 0006) : le canal de sécurité enregistre sous l'identifiant reçu de la commande.
- Au plus 99 octets, avec un numéro de série de 32 caractères et des compteurs de 64 bits ; 61 octets pour la clé `cond`.

## Datation

- `time` porte l'heure civile UTC, ou « indisponible » si l'horloge l'est (SYS-REQ-054). Jamais une heure estimée.
- SYS-REQ-035 demande aussi, pour chaque événement, le temps monotone écoulé depuis le démarrage, et SYS-REQ-054 l'indication « heure douteuse » après un saut. Le modèle d'événement de mddlog 0.3.0 n'a de champ ni pour l'un ni pour l'autre, alors que son ADR-002 les prévoit. Tant que mddlog ne les porte pas, ils figurent **en tête du détail** :
  - `mono=<ms>` : temps monotone du producteur depuis son démarrage, en millisecondes, **dans chaque événement** ;
  - `clock=doubtful` : heure douteuse, entre un saut d'horloge et la mise à l'heure suivante, **seulement dans ce cas**.

  Ces deux évolutions sont demandées en amont : [mddlog#154](https://github.com/ambroise-leclerc/mddlog/issues/154) pour le temps monotone, [mddlog#155](https://github.com/ambroise-leclerc/mddlog/issues/155) pour l'heure douteuse.

## Références

`requirementRef` et `riskRef` ne portent qu'**un** identifiant chacun, et seulement s'il existe dans le [registre](../software_development_file/registry/) à la date de la version du logiciel. Sinon, ils restent vides. Les tableaux donnent la référence de chaque action : l'exigence qui l'impose ou qu'elle met en œuvre, et le risque que maîtrise la mesure dont elle témoigne. Un test vérifiera que chaque référence employée par le code existe dans le registre.

## Détail

Le détail est une suite de paires `clé=valeur` séparées par une espace, en ASCII :
- `mono` d'abord, puis `clock` s'il y a lieu, puis les clés de l'action, dans l'ordre des tableaux ;
- les grandeurs sont des entiers, en unités fixes : `ul` (µL), `ulh` (µL/h), `s`, `ms`, `mv` (mV), `mah` (mAh), comme dans le code ([grandeurs](design/infusion-modes.md#grandeurs)) ;
- les causes et motifs sont les **valeurs numériques stables** des énumérations du code, que leurs documents de conception publient ;
- les versions et empreintes sont des identifiants courts (`v1.2.0`, 16 chiffres hexadécimaux d'empreinte) ;
- un flux cité dans le détail (`stream`) s'écrit sans le préfixe de la pompe, qui est la même : `securite:boot-9`. Les détails tiennent ainsi dans 160 octets avec des compteurs de 64 bits : le plus long, celui de `therapy.bolus-end` interrompu par l'état sûr (`clock`, `cause=syringe-empty` et `cond` compris), fait 150 octets au plus.

Les clés communes :

| Clé | Valeur |
| --- | --- |
| `mono` | Temps monotone du producteur, en ms |
| `clock` | `doubtful` ; absente sinon |
| `refus` | Cause du refus d'une demande ([Échecs](#échecs)) ; seulement en phase « échouée » |
| `state` | Valeur de l'état de la machine à états ([états](design/state-machine.md#états)) au moment de l'événement |
| `cond` | Condition liée à une action, sans le préfixe de la pompe ([Corrélation](#corrélation)) |

### Échecs

La phase « échouée » couvre deux cas, qui ne s'écrivent pas de la même façon.

**Refus d'une demande** (action précédée d'un événement « demandé ») : le détail porte `refus` et sa clé complémentaire **à la place** des clés de l'action, que la demande, enregistrée avant, porte déjà. `refus` est pris dans cette liste fermée :

| `refus` | Cause | Clé complémentaire |
| --- | --- | --- |
| `state` | Demande non admise dans l'état courant (SW-REQ-001) | `state` |
| `validation` | Valeur hors limites ([validation](design/infusion-modes.md#validation-de-la-programmation)) | `motif` : valeur de `Refusal` |
| `safety` | Refusée par le canal de sécurité (SYS-REQ-044) | |
| `timeout` | Confirmation absente dans le délai (30 s, SYS-REQ-044) | |
| `syringe` | Seringue absente, mal positionnée, piston non capturé, ou diamètre différent du modèle choisi (SYS-REQ-006, SYS-REQ-047) | |
| `line` | Ligne non raccordée non établie avant la purge (SYS-REQ-028) | |
| `auth` | Authentification absente ou échouée (SW-REQ-016) | |
| `integrity` | Signature ou intégrité refusée (SW-REQ-014, SW-REQ-015) | |
| `journal` | L'événement de la demande n'a pas pu être enregistré ; la conséquence relève de #46 | |
| `power` | Coupure ou épuisement pendant l'action | |

**Résultat négatif d'un contrôle automatique** (sans événement « demandé ») : le détail **garde** les clés de l'action, puisque aucun événement antérieur ne les porte, et y ajoute `cause`, prise dans la liste propre à l'action ([Cycle de vie](#cycle-de-vie), [Journal](#journal)). Il ne porte pas `refus`.

`maintenance.functional-check` relève des deux cas : refusé avant d'être lancé, il porte `refus` ; lancé puis négatif, il garde `check` et ajoute `cause=fail`.

## Actions

Les actions sont nommées `<domaine>.<objet>`, en minuscules ASCII, avec le trait d'union pour séparer les mots. Les alarmes ajoutent l'étape : `alarm.<condition>.<étape>`.

Abréviations des phases, d'après leur nom dans mddlog : R demandée (`Requested`), C confirmée (`Confirmed`), E exécutée (`Executed`), F échouée (`Failed`).

### Thérapie

Catégorie : opérateur, sauf mention. Cible : `pump-<série>/channel-1`. Acteur : `operateur`, ou `commande` et `securite` pour ce que la pompe fait d'elle-même.

| Action | Phases | Producteur | Exigence | Risque | Clés du détail |
| --- | --- | --- | --- | --- | --- |
| `therapy.program` | R, C, E, F | C, S | SYS-REQ-011 | RISK-007 | `mode`, `rate_ulh`, `volume_ul`, `duration_s`, `drug`, `syringe`, `soft` |
| `therapy.program-cancel` | R, E, F | C | SW-REQ-001 | | `state` |
| `therapy.start` | R, C, E, F | C, S | SYS-REQ-044 | RISK-001 | `state` |
| `therapy.pause` | R, E, F | C, S | SYS-REQ-044 | RISK-015 | `state` |
| `therapy.resume` | R, C, E, F | C, S | SYS-REQ-044 | RISK-015 | `state` |
| `therapy.stop` | R, E, F | C, S | SYS-REQ-044 | RISK-015 | `state`, `delivered_ul` |
| `therapy.bolus` | R, C, E, F | C, S | SYS-REQ-014 | RISK-009 | `volume_ul`, `rate_ulh`, `soft` |
| `therapy.bolus-end` | E | C | SYS-REQ-014 | RISK-009 | `delivered_ul`, `cause`, `cond` |
| `therapy.purge` | R, E, F | C | SYS-REQ-028 | RISK-009 | `volume_ul`, `rate_ulh` |
| `therapy.purge-end` | E | C | SYS-REQ-028 | | `delivered_ul`, `cause`, `cond` |
| `therapy.end` (cycle de vie) | E | C | SYS-REQ-032 | RISK-002 | `cause`, `next`, `delivered_ul` |
| `therapy.recovery` | R, C, E, F | C, S | SYS-REQ-038 | RISK-010 | `remaining_ul`, `rate_ulh` |
| `syringe.select` | R, E, F | C | SYS-REQ-047 | RISK-001 | `syringe`, `diameter_um` |

Valeurs :
- `mode` : valeur de `Mode` (1 débit continu, 2 volume sur durée) ; `soft` : `above` si une limite souple est dépassée, absente sinon ; `drug` : identifiant du médicament dans la bibliothèque ; `syringe` : identifiant du modèle dans la liste vérifiée ; 16 caractères au plus chacun. La version de la bibliothèque n'est pas répétée : elle est consignée à son chargement (`config.library`).
- `cause` de `therapy.bolus-end` : `volume` (bolus délivré), `pause`, `stop`, `end` (volume à perfuser atteint pendant le bolus), `syringe-empty`, `safety` (état sûr ou mise hors service, sur `DangerDetected` ou `FatalFault`) ; de `therapy.purge-end` : `volume`, `stop`, `line` (condition de ligne perdue), `safety`.
- `therapy.bolus-end` et `therapy.purge-end` portent la corrélation de la demande qu'ils terminent (`input-<m>` du bolus ou de la purge) et `delivered_ul`, le volume effectivement délivré. Avec `cause=safety`, `cond` donne la condition qui a provoqué l'état sûr ou la mise hors service. Ils sont enregistrés avant `safety.safe-state` ou `safety.out-of-service`.
- `therapy.end` : `cause` vaut `volume` ou `syringe-empty`, `next` vaut `stop` ou `kvo` (SYS-REQ-032).
- La reprise après une pause (`therapy.resume`) est distincte du démarrage, bien que la machine à états reçoive le même `StartRequested`. L'historique (SYS-REQ-037) doit les distinguer.

**Exemple : bolus au-delà de la limite souple** (scénario 2 de #48). Le soignant demande le bolus (R, `soft=above`), confirme spécifiquement (C), la machine à états l'admet (E). Sans confirmation, le bolus échoue (F, `refus=timeout`) et rien n'est délivré. L'issue #42 citait `alarm.bolus-limit` : le dépassement d'une limite souple n'est pas une condition d'alarme du catalogue, mais une phase de confirmation de `therapy.bolus`. Le dépassement d'une limite dure est un refus (`refus=validation`).

### Alarmes

Catégorie : maîtrise du risque. Cible : `pump-<série>/channel-1` pour les alarmes de perfusion, `pump-<série>` ou l'objet concerné pour les autres. Corrélation : `cond-<m>` de la condition pour le déclenchement et la fin ; `input-<m>` de l'action pour la pause du signal et l'acquittement, qui citent la condition par `cond`.

Chaque condition d'alarme a quatre étapes, soit quatre actions :

| Étape | Action | Phases | Acteur | Clés du détail |
| --- | --- | --- | --- | --- |
| Déclenchement | `alarm.<condition>.raise` | E | `commande` ou `securite` | `priority`, puis les clés de la condition |
| Pause du signal sonore (SYS-REQ-036) | `alarm.<condition>.silence` | R, E, F | `operateur` | `cond`, `until_s` |
| Acquittement | `alarm.<condition>.acknowledge` | R, E, F | `operateur` | `cond` |
| Fin | `alarm.<condition>.clear` | E | `commande` ou `securite` | `cause` |

`priority` vaut `high`, `medium`, `low` ou `info`. `cause` de la fin vaut `resolved` (condition levée) ou `superseded` (remplacée par l'état sûr ou par l'arrêt).

Conditions, d'après le [catalogue des alarmes](requirements/requirements-specification.md#catalogue-des-alarmes) de la spécification :

| `<condition>` | Condition | Exigence | Risque | Clés de la condition |
| --- | --- | --- | --- | --- |
| `occlusion` | Occlusion aval | SYS-REQ-008 | RISK-005 | `force_mn` |
| `air` | Air dans la ligne | SYS-REQ-007 | RISK-004 | `bubble_ul`, `cumulated_ul` |
| `syringe` | Seringue absente ou mal positionnée, piston non capturé | SYS-REQ-006 | RISK-003 | `cause` |
| `syringe-empty` | Seringue vide | SYS-REQ-032 | RISK-002 | |
| `actuator-idle` | Actionneur inactif | SYS-REQ-004 | RISK-002 | |
| `supervision` | Écart ou vitesse hors borne détectés par le canal de sécurité | SYS-REQ-001 | RISK-001 | `expected_ul`, `measured_ul` |
| `technical` | Défaillance technique | SYS-REQ-017 | RISK-012 | `unit`, `cause` |
| `link-loss` | Perte de lien entre les processeurs | SYS-REQ-053 | RISK-012 | `cause` |
| `reservation` | Échec d'une réservation de volume | SYS-REQ-051 | RISK-001 | |
| `battery-depleted` | Autonomie épuisée | SYS-REQ-016 | RISK-010 | `autonomy_s` |
| `reset-recovery` | Reprise après réinitialisation pendant une perfusion | SYS-REQ-038 | RISK-010 | |
| `infusion-end` | Fin de perfusion | SYS-REQ-032 | RISK-002 | |
| `battery-low` | Autonomie faible | SYS-REQ-016 | RISK-010 | `autonomy_s` |
| `idle-reminder` | Pompe programmée non démarrée, ou en pause, depuis 2 min | SYS-REQ-042 | RISK-002 | |
| `infusion-end-prealarm` | Préalarme de fin de perfusion | SYS-REQ-032 | RISK-002 | `remaining_s` |
| `syringe-near-empty` | Préalarme de seringue presque vide | SYS-REQ-032 | RISK-002 | `remaining_s` |
| `clock` | Heure indisponible ou douteuse (signal d'information) | SYS-REQ-054 | RISK-011 | `cause` |
| `journal-witness` | Désaccord entre le journal et son témoin d'ancrage | SYS-REQ-052 | RISK-011 | `stream`, `sequence` |

- L'alarme technique de repli (SYS-REQ-020) n'a pas d'événement : elle retentit quand aucun processeur ne peut plus rien enregistrer. Ses tests au démarrage sont consignés par `device.self-test`.
- La bascule sur batterie, signal d'information du catalogue, est consignée par `power.source`.
- Le catalogue complet (#31) peut ajouter ou scinder des conditions : chaque ajout est une révision de ce tableau.

### Sûreté de fonctionnement

Catégorie : maîtrise du risque. Cible : `pump-<série>`, ou `pump-<série>/channel-1` pour l'état sûr. Corrélation : `cond-<m>`, sauf l'acquittement de l'état sûr, action du soignant : `input-<m>`, avec `cond`.

| Action | Phases | Producteur | Exigence | Risque | Clés du détail |
| --- | --- | --- | --- | --- | --- |
| `safety.safe-state` | E | C, S | SYS-REQ-002 | RISK-001 | `state`, `cause` |
| `safety.safe-state-acknowledge` | R, E, F | C | SW-REQ-001 | RISK-013 | `cond`, `state` |
| `safety.out-of-service` | E | C, S | SW-REQ-001 | RISK-012 | `state`, `cause` |
| `fault.drive` | E | C | SYS-REQ-002 | RISK-012 | `cause` : [défauts de l'entraînement](design/drive.md#défauts) |
| `fault.occlusion-monitor` | E | C | SYS-REQ-002 | RISK-005 | `cause` : [défauts du surveillant](design/occlusion.md#défauts) |
| `fault.state-machine` | E | C | SW-REQ-001 | RISK-012 | `state`, `event` : valeurs reçues, même invalides |
| `fault.self-test` | E | C, S | SW-REQ-011 | RISK-012 | `test` : [liste des tests](#tests) |
| `fault.channel-mismatch` | E | C, S | SW-REQ-013 | RISK-001 | `field` |
| `link.loss` | E | C, S | SYS-REQ-053 | RISK-012 | `cause` : `silence`, `transport`, `boot`, `version` |

- `cause` de `safety.safe-state` et `safety.out-of-service` donne l'action qui l'a provoqué : `cause=alarm.occlusion`, `cause=fault.drive`, `cause=link.loss`… La corrélation est celle de la condition.
- Les valeurs des défauts de l'entraînement et du surveillant d'occlusion se recouvrent (`ForceUnavailable` vaut 4 pour l'un, 3 pour l'autre) : c'est pourquoi ils ont chacun leur action.

### Cycle de vie

Catégorie : cycle de vie, sauf mention. Cible : `pump-<série>`. Corrélation : le démarrage, ou `input-<m>` pour une demande de l'opérateur.

| Action | Phases | Producteur | Exigence | Risque | Clés du détail |
| --- | --- | --- | --- | --- | --- |
| `device.boot` | E | C, S | SW-REQ-020 | RISK-011 | `boot`, `software`, `reset` |
| `device.self-test` | E, F | C, S | SW-REQ-010 | RISK-012 | `test`, et `cause` en cas d'échec |
| `device.prototype-warning` (opérateur) | E | C | SYS-REQ-027 | RISK-021 | |
| `device.shutdown` | R, E | C | SYS-REQ-016 | RISK-010 | `cause` : `operator`, `battery` |
| `device.power-fail` | E | C, S | SYS-REQ-050 | RISK-011 | |
| `power.source` | E | C | SYS-REQ-015 | RISK-010 | `from`, `to` : `mains`, `battery` |
| `power.battery-capacity` | E | C | SYS-REQ-049 | RISK-010 | `capacity_mah`, `nominal_mah` |
| `clock.jump` | E | C | SYS-REQ-054 | RISK-011 | `civil_s`, `mono_s` : avances comparées |
| `clock.unavailable` | E | C | SYS-REQ-054 | RISK-011 | `cause` : `oscillator`, `reserve`, `unreadable` |
| `maintenance.enter` | R, E, F | C | SYS-REQ-023 | RISK-015 | `state` |
| `maintenance.exit` | R, E | C | SYS-REQ-023 | | `cause` : `operator`, `timeout` |
| `maintenance.functional-check` | R, E, F | C | SYS-REQ-041 | RISK-012 | `check` : `sensors`, `alarm-signals`, `accuracy` |

- `device.boot` est le premier événement de chaque flux : `boot` est le numéro de démarrage, `software` la version du logiciel du producteur, `reset` la cause de la réinitialisation précédente si le processeur la connaît (`power`, `watchdog`, `software`, `unknown`).
- `device.power-fail` n'est enregistré que si la réserve de maintien le permet (SYS-REQ-050) ; son absence n'est pas une perte.

#### Tests

`device.self-test` consigne un événement par test du démarrage (SW-REQ-010, SYS-REQ-046) ; `fault.self-test` un test périodique en échec (SW-REQ-011). `test` prend une valeur de cette liste fermée :

| `test` | Test | Producteur |
| --- | --- | --- |
| `program-memory` | Intégrité de la mémoire programme | C, S |
| `ram` | Mémoire vive | C, S |
| `persistent-data` | Intégrité des données persistantes (SW-REQ-017) | C, S |
| `boot-counter` | Compteur de démarrage lisible et intègre (SW-REQ-020) | C, S |
| `clock` | Horloge temps réel | C |
| `sensors` | Plausibilité des capteurs | C, S |
| `alarm-signals` | Signaux d'alarme visuels et sonores | C |
| `cutoff-command` | Coupure du moteur par le canal de commande (SYS-REQ-046) | C |
| `cutoff-safety` | Coupure du moteur par le canal de sécurité (SYS-REQ-046) | S |
| `fallback-alarm` | Circuit d'alarme autonome qui retentit (SYS-REQ-046) | S |
| `fallback-battery` | Tension en charge de la pile du circuit autonome (SYS-REQ-046) | S |
| `protocol-version` | Version du protocole de la liaison (SYS-REQ-053) | C, S |

En cas d'échec, `cause` vaut `fail` (résultat hors tolérance), `timeout` (aucun résultat dans le délai) ou `unavailable` (test impossible : capteur ou périphérique absent). Pour `fallback-battery`, la clé `mv` donne la tension mesurée.


### Configuration

Catégorie : configuration. Acteur : `role:technicien`. Corrélation : `input-<m>`.

| Action | Cible | Phases | Exigence | Risque | Clés du détail |
| --- | --- | --- | --- | --- | --- |
| `config.library` | `pump-<série>/library` | R, E, F | SW-REQ-014 | RISK-019 | `version`, `digest` |
| `config.syringe-list` | `pump-<série>/syringe-list` | R, E, F | SYS-REQ-047 | RISK-001 | `version`, `digest` |
| `config.software` | `pump-<série>/software` | R, E, F | SW-REQ-015 | RISK-020 | `from`, `to`, `digest` |
| `config.clock` | `pump-<série>/clock` | R, E, F | SYS-REQ-035 | RISK-011 | `old_s`, `new_s` : secondes UTC ; `old_s=unavailable` si l'heure l'était |
| `config.setting` | `pump-<série>` | R, E, F | SYS-REQ-023 | RISK-015 | `name`, `old`, `new` |
| `config.out-of-service` | `pump-<série>` | R, E, F | SYS-REQ-041 | RISK-012 | |
| `config.return-to-service` | `pump-<série>` | R, E, F | SYS-REQ-041 | RISK-012 | |

`name` de `config.setting` prend une valeur de cette liste fermée, complétée avec chaque réglage : `occlusion-threshold` (SYS-REQ-008), `air-bubble` et `air-cumulated` (SYS-REQ-007, à la baisse seulement), `prealarm-delay` (SYS-REQ-032).

### Accès

Catégorie : accès. Cible : `pump-<série>`, ou `pump-<série>/keypad` pour le verrouillage. Corrélation : `input-<m>`.

| Action | Phases | Exigence | Risque | Clés du détail |
| --- | --- | --- | --- | --- |
| `access.login` | R, E, F | SW-REQ-016 | RISK-015 | `role` |
| `access.logout` | E | SW-REQ-016 | RISK-015 | `role`, `cause` : `operator`, `timeout` |
| `access.keypad-lock` | R, E, F | SYS-REQ-023 | RISK-015 | |
| `access.keypad-unlock` | R, E, F | SYS-REQ-023 | RISK-015 | |

Un échec d'authentification (`access.login`, F, `refus=auth`) porte l'acteur `operateur` : le rôle n'est pas établi.

### Journal

Catégorie : maîtrise du risque. Cible : `pump-<série>/journal`. Acteur : `commande` ou `securite`.

| Action | Phases | Producteur | Exigence | Risque | Clés du détail |
| --- | --- | --- | --- | --- | --- |
| `journal.loss` | E | C, S | SW-REQ-008 | RISK-011 | `count`, `cause` : `ring-full`, `medium-full`, `write`, `anchor` |
| `journal.stream-gap` | E | C, S | SW-REQ-021 | RISK-011 | `stream`, `from`, `to` : séquences manquantes |
| `journal.witness-refusal` | E | S | SYS-REQ-052 | RISK-011 | `stream`, `sequence`, `cause` : `gap`, `divergent`, `decreasing`, `retention` |
| `journal.cross-check` | E, F | C, S | SYS-REQ-052 | RISK-011 | `stream`, `sequence`, et `cause` en cas d'échec : `digest` (condensats différents à la même séquence), `position` (le support et le témoin ne sont pas à la même séquence), `unreadable` (journal ou état du témoin illisible) |

- `journal.loss` consigne les événements refusés ou perdus depuis le précédent, dès qu'il peut être enregistré : la perte n'est jamais silencieuse (SW-REQ-008). La réponse de l'appareil relève de #46.
- Les frontières, retraits et rotations que tient mddlog lui-même (actions `mddlog.*`) ne sont pas dupliqués ici.

## Exemple

Programmation, puis démarrage, sur la pompe `SIM0001`, au démarrage 12 de la commande et 9 de la sécurité :

| Flux | Séq. | Catégorie | Phase | Action | Acteur | Corrélation | Détail |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `pump-SIM0001:commande:boot-12` | 41 | Opérateur | R | `therapy.program` | `operateur` | `pump-SIM0001:commande:boot-12:input-7` | `mono=81234 mode=1 rate_ulh=12000 volume_ul=50000 duration_s=15000 drug=12 syringe=bd-50` |
| `pump-SIM0001:securite:boot-9` | 18 | Opérateur | C | `therapy.program` | `operateur` | `pump-SIM0001:commande:boot-12:input-7` | `mono=79010` |
| `pump-SIM0001:commande:boot-12` | 42 | Opérateur | E | `therapy.program` | `operateur` | `pump-SIM0001:commande:boot-12:input-7` | `mono=86502 state=4` |
| `pump-SIM0001:commande:boot-12` | 43 | Opérateur | R | `therapy.start` | `operateur` | `pump-SIM0001:commande:boot-12:input-8` | `mono=91877 state=4` |
| `pump-SIM0001:securite:boot-9` | 19 | Opérateur | C | `therapy.start` | `operateur` | `pump-SIM0001:commande:boot-12:input-8` | `mono=89650` |
| `pump-SIM0001:commande:boot-12` | 44 | Opérateur | E | `therapy.start` | `operateur` | `pump-SIM0001:commande:boot-12:input-8` | `mono=91903 state=6` |

La confirmation de la programmation est observée par le canal de sécurité (SYS-REQ-044) : elle est enregistrée dans **son** flux, sous la corrélation créée par la commande. Les temps monotones des deux flux ne sont pas comparables : chaque processeur a le sien (ADR 0006).

## Vérification

À la mise en œuvre, dans le simulateur d'abord ([#136](https://github.com/camille-martin-paris/clepsydre/issues/136)) :
- les actions, catégories et phases admises seront des constantes du code, et un test les comparera à ce document ;
- chaque `requirementRef` et `riskRef` employé sera vérifié dans le registre ;
- le détail de chaque action, aux valeurs les plus longues, sera vérifié sans troncature ;
- une trace de diagnostic présentée comme événement d'audit sera refusée (VER-132).

## Limites

- **mddlog ne porte ni le temps monotone ni l'heure douteuse.** Les deux sont dans le détail, en attendant des champs propres (mddlog#154, mddlog#155) ; un lecteur hors appareil doit donc analyser le détail pour les retrouver.
- **Une seule référence** d'exigence et de risque par événement : le champ de mddlog n'en porte qu'une. La traçabilité complète passe par le registre.
- **Le catalogue des alarmes** n'est pas établi (#31) : la liste des conditions suit la spécification, et la priorité du désaccord avec le témoin reste à fixer.
- **Les rôles** ne sont pas établis (#39) : seul `role:technicien` est employé.
- **Les clés de détail** des bibliothèques (`drug`, `library`, `syringe`) dépendent de leur format, qui n'est pas encore fixé.
