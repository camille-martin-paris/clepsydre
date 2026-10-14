# Modes de perfusion et bolus borné

| Champ | Valeur |
| --- | --- |
| Statut | Conception détaillée, soumise à relecture |
| Issue | [#25](https://github.com/camille-martin-paris/clepsydre/issues/25) |
| Exigences | SW-REQ-002, SW-REQ-005, SW-REQ-006 ; SYS-REQ-012, SYS-REQ-014, SYS-REQ-029 à SYS-REQ-032 ([spécification](../requirements/requirements-specification.md)) |
| Code | [`src/control/`](../../src/control/) : module `clepsydre.control.infusion` |
| Classe de sécurité | C ([classification](../../software_development_file/safety-classification.md)) |

> [!CAUTION]
> Clepsydre ne doit pas être utilisé sur un être humain ni sur un animal.

## Objet

Ce document décrit l'unité des modes de perfusion du logiciel de commande (IEC 62304 §5.4). Elle remplit trois rôles :
- **valider** la programmation et le bolus avant leur acceptation, et refuser explicitement toute valeur hors limites (SW-REQ-002) ;
- donner la **borne de consigne** du mode actif de l'actionneur (SW-REQ-006) ;
- **décompter** les volumes délivrés en arithmétique entière, sans dérive (SW-REQ-005), et signaler à la [machine à états](state-machine.md) la fin du volume à perfuser et la fin du bolus.

Comme la machine à états, l'unité ne commande rien elle-même. Elle ne fait ni allocation dynamique ni exception.

## Décisions retenues par l'auteur

Ambroise Leclerc a retenu ces deux décisions le 2026-10-14, avant la conception :

1. **Volume sur durée : débit arrondi par défaut, arrondi signalé.** Le débit calculé (volume ÷ durée) tombe rarement sur la résolution de 0,1 mL/h : 10 mL en 3 h donnent 3,33… mL/h.
   - Le débit retenu est arrondi par défaut à 0,1 mL/h. Il ne dépasse donc jamais le débit prescrit.
   - La programmation acceptée porte le débit retenu, la durée qui en résulte (ici 3 h 1 min 50 s) et l'indicateur `rateRoundedDown`. Le récapitulatif (SYS-REQ-011) les affiche : la correction est explicite, pas silencieuse.
   - Les deux options écartées étaient de refuser tout débit inexact, ce qui rendait impossibles la plupart des prescriptions, et d'appliquer un débit interne plus fin que celui affiché (SYS-REQ-013).
2. **Limites souples classées dès #25.** La validation refuse au-delà des limites dures. Au-delà des limites souples, elle accepte et signale qu'une confirmation spécifique est exigée (SYS-REQ-012) : `rateSoftLimit` pour le débit, `volumeAboveSoftLimit` pour le bolus. Le parcours de confirmation relève de #36 et #37.

## Grandeurs

| Type | Unité | Résolution programmable | Plage |
| --- | --- | --- | --- |
| `Volume` | µL, entier de 32 bits | 0,1 mL (100 µL) | 0,1 mL au volume utile de la seringue (SYS-REQ-031) |
| `Rate` | µL/h, entier de 32 bits | 0,1 mL/h (100 µL/h) | 0,1 à 200 mL/h, et au plus le débit maximal de la seringue (SYS-REQ-030) |

Les valeurs internes sont plus fines que la résolution. Une valeur saisie hors résolution est refusée, jamais arrondie.

## Entrées

- **Seringue** (`Syringe`) : volume utile et débit maximal du modèle choisi dans la liste vérifiée (SYS-REQ-047).
- **Limites du médicament** (`DrugLimits`) :
  - limites dures et souples du débit ;
  - limites souples et dures du volume de bolus, le volume dur nul interdisant le bolus ;
  - débit maximal du bolus ;
  - fin de perfusion (arrêt ou maintien de veine ouverte) et débit du maintien.

  Les limites sont exprimées en débit. Leur conversion depuis des limites de dose, selon la concentration, relève de la bibliothèque (#37).
- **Programmation saisie** (`ProgramRequest`) :
  - le mode ;
  - le débit, lu seulement en débit continu ;
  - le volume à perfuser, exigé dans les deux modes ;
  - la durée en minutes, lue seulement en volume sur durée.

`validLimits()` vérifie d'abord la cohérence des limites et de la seringue :
- plages et résolutions ;
- limites dures dans la plage de la pompe ;
- limites dures qui englobent les limites souples ;
- maintien de veine ouverte de 0,1 à 5 mL/h ;
- seringue de volume et de débit non nuls.

Une incohérence fait refuser toute programmation et tout bolus (`InvalidLimits`). La vérification de la signature et de l'intégrité de la bibliothèque reste l'affaire de son chargement (SW-REQ-014).

## Validation de la programmation

`validateProgram()` vérifie les valeurs dans cet ordre et refuse la première qui sort des limites, avec son motif (`Refusal`) :

| Ordre | Vérification | Motif du refus |
| --- | --- | --- |
| 1 | Limites et seringue cohérentes | `InvalidLimits` |
| 2 | Mode valide | `InvalidMode` |
| 3 | Volume : résolution, 0,1 mL au moins, volume utile de la seringue au plus | `VolumeResolution`, `VolumeBelowMinimum`, `VolumeAboveSyringe` |
| 4 | Débit continu : débit à la résolution | `RateResolution` |
| 4 | Volume sur durée : durée strictement positive, débit calculé de 200 mL/h au plus | `DurationMissing`, `RateAbovePumpMaximum` |
| 5 | Débit : 0,1 mL/h au moins, 200 mL/h au plus, débit maximal de la seringue au plus | `RateBelowPumpMinimum`, `RateAbovePumpMaximum`, `RateAboveSyringeMaximum` |
| 6 | Débit dans les limites dures du médicament | `RateBelowHardLimit`, `RateAboveHardLimit` |

Les valeurs numériques des motifs sont stables : elles seront journalisées (#42).

La programmation acceptée (`Program`) contient ce que le récapitulatif doit afficher :
- le mode, le débit retenu et le volume ;
- la durée au débit retenu, arrondie par excès à la seconde ;
- l'indicateur d'arrondi et la position par rapport aux limites souples ;
- la fin de perfusion et le débit de maintien de veine ouverte.

En volume sur durée, le débit calculé est arrondi par défaut. Un débit arrondi à zéro, par exemple 0,1 mL en 100 h, est refusé (`RateBelowPumpMinimum`).

**Maintien de veine ouverte (SYS-REQ-032).** Son débit est celui de la bibliothèque, plafonné par le débit retenu : il ne dépasse ni le débit programmé ni 5 mL/h. Il est nul quand la bibliothèque prévoit l'arrêt en fin de perfusion.

## Validation du bolus

`validateBolus()` reçoit le volume et le débit demandés, la programmation acceptée, les limites et la seringue :

| Ordre | Vérification | Motif du refus |
| --- | --- | --- |
| 1 | Limites et seringue cohérentes | `InvalidLimits` |
| 2 | Débit continu (SYS-REQ-029), bolus permis pour le médicament | `BolusNotAllowed` |
| 3 | Volume : résolution, 0,1 mL au moins, volume utile de la seringue au plus | `VolumeResolution`, `VolumeBelowMinimum`, `VolumeAboveSyringe` |
| 4 | Volume dans la limite dure du médicament (SYS-REQ-014) | `BolusVolumeAboveHardLimit` |
| 5 | Débit : résolution, plage de la pompe, débit maximal de la seringue | `RateResolution`, `RateBelowPumpMinimum`, `RateAbovePumpMaximum`, `RateAboveSyringeMaximum` |
| 6 | Débit dans la limite dure du médicament (SYS-REQ-014) | `BolusRateAboveHardLimit` |

Un bolus au-delà de la limite souple de volume est accepté, avec `volumeAboveSoftLimit`. Le bolus est compté dans le volume perfusé. Son volume n'est pas limité au volume restant : si le volume à perfuser est atteint pendant le bolus, le bolus s'arrête là (voir [Décompte](#décompte-des-volumes)).

## Borne de consigne

`setpointBound()` donne la borne du mode de l'actionneur, que la machine à états fixe avec l'état (`actuatorMode()`). Le mode et sa borne changent donc ensemble (SW-REQ-006) :

| Mode | Borne |
| --- | --- |
| `Infusion` | Débit retenu par la programmation |
| `Bolus` | Débit maximal du bolus pour le médicament |
| `Kvo` | Débit de maintien de veine ouverte de la programmation |
| `None`, `Purge`, valeur invalide | Nulle : toute consigne non nulle est hors bornes |

Chaque borne est plafonnée par le débit maximal de la pompe. La borne du bolus vient des limites du médicament et non du bolus demandé : un bolus altéré après sa validation reste borné. La borne de la purge viendra avec l'unité de purge (SYS-REQ-014, SYS-REQ-028). D'ici là, toute consigne de purge est hors bornes.

## Décompte des volumes

`Delivery` est construit avec la programmation acceptée et les limites. La boucle de commande appelle `advance(mode, elapsed)` à chaque cycle :

1. Un intervalle négatif ou supérieur à `maxStep` (60 s) signifie que la boucle a manqué ses échéances. L'appel renvoie `DangerDetected` sans rien délivrer.
2. La consigne du mode (`setpoint()`) est comparée à sa borne **avant** d'être appliquée. Une consigne hors bornes renvoie `DangerDetected` sans rien délivrer (SW-REQ-006).
3. Le volume délivré est l'intégrale exacte de la consigne sur l'intervalle, en µL·ms/h. Il est tronqué au volume restant à perfuser et, en bolus, au volume restant du bolus : le pousseur ne dépasse jamais l'un ni l'autre.
4. L'appel renvoie l'événement à appliquer à la machine :

| Mode | Volume décompté | Événement renvoyé |
| --- | --- | --- |
| `Infusion` | Volume perfusé | `VolumeReached` ou `VolumeReachedKvo` à l'atteinte du volume, selon la bibliothèque |
| `Bolus` | Volume perfusé et volume du bolus | Fin du volume si elle est atteinte, sinon `BolusEnded` à la fin du bolus. Les deux tombent-elles au même instant, la fin du volume l'emporte, et la machine refuse ensuite une fin de bolus |
| `Bolus` sans bolus validé | Aucun | `DangerDetected` |
| `Kvo` | Volume de maintien de veine ouverte, à part | Aucun |
| `None`, `Purge` | Aucun | Aucun |
| Valeur invalide | Aucun | `DangerDetected` |

**Bolus.** L'appelant enregistre le bolus validé par `startBolus()` quand la machine accepte `BolusRequested`. Tout cycle hors du mode `Bolus` l'abandonne. Un bolus interrompu par une pause, la fin du volume ou un arrêt n'est donc jamais repris, conformément à la machine à états.

**Absence de dérive (SW-REQ-005).** Les volumes sont tenus en µL·ms/h, sur des entiers de 64 bits : un microlitre vaut 3 600 000 unités. Le produit d'un débit entier en µL/h par un intervalle entier en ms est exact, et aucun reste n'est perdu d'un cycle à l'autre. Les volumes lus (`infused()`, `remaining()`, `bolusDelivered()`, `kvoDelivered()`) sont arrondis par défaut au microlitre. L'erreur reste donc inférieure à 0,001 mL sur toute la perfusion, pour une exigence de 0,01 mL. Au débit maximal et au pas maximal, un cycle ajoute 1,2 × 10¹⁰ unités, ce qui laisse une marge de plusieurs siècles avant tout débordement.

Le décompte porte sur le volume **commandé**. Le volume réellement délivré par le pousseur relève de l'asservissement (#26), et sa supervision indépendante du canal de sécurité (SYS-REQ-001).

## Ressources et traitement des défauts

- **Mémoire** : `Delivery` occupe quelques dizaines d'octets ; aucune allocation dynamique.
- **Exceptions** : toutes les fonctions sont `noexcept`. Les résultats de validation sont des `std::expected`.
- **Fonctions totales** : toute valeur d'énumération invalide (mode, fin de perfusion, mode de l'actionneur) donne un refus, une borne nulle ou `DangerDetected`.
- **Temps d'exécution** : constant, sans boucle.

## Vérification

Les tests de `tests/control_infusion_test.cpp` construisent leurs attendus indépendamment du code.

| Critère (IEC 62304 §5.5.4) | Couverture par les tests |
| --- | --- |
| Valeurs et limites | Chaque motif de refus de la programmation et du bolus, en débit continu et en volume sur durée. Limites exactes acceptées : 0,1 mL, volume utile, limites dures du débit, limite souple et dure du bolus, débit maximal du bolus, maintien de veine ouverte à 5 mL/h |
| Cohérence des entrées | Dix-huit bibliothèques ou seringues incohérentes, chacune refusée (`InvalidLimits`) |
| Arrondi | Débit exact sans arrondi ; 10 mL en 3 h retenus à 3,3 mL/h, arrondi signalé, durée de 10 910 s ; durée arrondie par excès en débit continu |
| Limites souples | Débit sous la limite souple basse, au-delà de la limite souple haute ; bolus au-delà de la limite souple |
| Bornes de consigne | Borne de chaque mode, nulle hors perfusion et pour une valeur invalide, plafonnée à 200 mL/h ; consigne hors bornes jamais appliquée (programmation ou bolus altérés) |
| Décompte | Volume atteint à la seconde près ; pas tronqué au volume restant ; 200 000 pas irréguliers égaux à l'intégrale exacte ; bolus compté dans le volume perfusé ; volume atteint pendant le bolus, avant ou en même temps que sa fin ; bolus abandonné par une pause, le maintien de veine ouverte ou la purge ; intervalles nul, maximal, négatif et trop long |
| Intégration avec la machine | Perfusion, bolus, retour au débit programmé et maintien de veine ouverte : chaque événement renvoyé est accepté par la machine |
| Couverture structurelle | 100 % des régions et des branches, sans exclusion (tâche CI `coverage`) |

Ces tests vérifient l'unité. Ils préparent les vérifications unitaires prévues de SW-REQ-002 (VER-053), SW-REQ-005 (VER-056, VER-057) et SW-REQ-006 (VER-058), qui restent à mener et à consigner. Les vérifications système de SYS-REQ-029 à SYS-REQ-032 (VER-037 à VER-040) viendront avec le logiciel de commande assemblé.

## Limites

- **Limites de dose.** Les limites sont exprimées en débit. Les unités de dose, la concentration et le poids relèvent de la bibliothèque (#37).
- **Confirmation.** La confirmation au-delà des limites souples et le récapitulatif relèvent du parcours de programmation (#36).
- **Volume exigé.** Le volume à perfuser est exigé dans les deux modes, pour que la fin de perfusion soit toujours définie. L'interface pourra proposer par défaut le volume utile de la seringue.
- **Préalarmes.** Les préalarmes de fin de perfusion et de seringue presque vide (SYS-REQ-032) relèvent des alarmes (#30). Elles s'appuieront sur `remaining()` et la durée restante.
- **Purge.** Sa borne et ses limites propres viendront avec son unité.
- **Volume délivré.** Le décompte suit le volume commandé : la précision du débit relève de l'asservissement (#26), la réservation durable avant délivrance (SYS-REQ-051) de la persistance de l'état de perfusion.
