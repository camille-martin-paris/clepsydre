# ADR 0004 : architecture d'alimentation et de batterie

| Champ | Valeur |
| --- | --- |
| Statut | Proposée, soumise à la décision de l'auteur puis à l'acceptation de l'approbatrice |
| Date | 2026-10-09 |
| Issue | [#15](https://github.com/camille-martin-paris/clepsydre/issues/15) |
| Épique | [#12](https://github.com/camille-martin-paris/clepsydre/issues/12) |
| Dépend de | [ADR 0002](0002-principe-de-pompage.md) (pousse-seringue), [ADR 0003](0003-architecture-de-calcul-et-canal-de-securite.md) (deux processeurs, alarme autonome) |

> [!CAUTION]
> Clepsydre ne doit pas être utilisé sur un être humain ni sur un animal. Les consommations et capacités ci-dessous sont des estimations de conception, à mesurer sur banc ([#62](https://github.com/camille-martin-paris/clepsydre/issues/62)).

## Contexte

Exigences et mesures concernées :

- **SYS-REQ-015** : la perte du secteur ne doit ni interrompre ni modifier la perfusion ; la bascule est signalée.
- **SYS-REQ-016** : alarme d'autonomie faible au moins 30 min avant l'épuisement, alarme haute d'autonomie épuisée au moins 5 min avant l'arrêt.
- **SYS-REQ-033** : au moins 4 h à 25 mL/h sur batterie neuve, autonomie restante affichée.
- **SYS-REQ-020** et l'[ADR 0003](0003-architecture-de-calcul-et-canal-de-securite.md#alarme-de-repli) : circuit d'alarme autonome sur réserve propre, au moins 2 min.
- **SYS-REQ-018, SYS-REQ-034, SYS-REQ-038** : un événement n'est enregistré qu'une fois écrit durablement ; le journal et l'état de perfusion survivent à la perte de toute alimentation ; le volume délivré enregistré n'est jamais sous-estimé.
- **HW-REQ-001** (CTRL-020) : sécurité électrique selon l'IEC 60601-1.
- **RISK-010** (CTRL-013) : perte d'alimentation pendant la perfusion.

L'issue pose trois questions : la bascule sans interruption, l'autonomie et la fin d'autonomie, la réserve d'énergie à la coupure.

## Bilan d'énergie estimé

| Consommateur | Estimation | Commentaire |
| --- | --- | --- |
| Deux microcontrôleurs, capteurs, mémoire | 0,2 à 0,4 W | En fonctionnement continu |
| Écran rétroéclairé | 0,3 à 0,8 W | Réduit en veille d'affichage, jamais éteint pendant une alarme |
| Moteur pas à pas | 0,1 à 0,5 W en moyenne | Faible à bas débit ; courant de maintien réduit entre les pas, la vis irréversible tenant la charge |
| Avertisseurs (alarme) | Ponctuel | — |
| **Total moyen** | **1 à 2 W** | **À mesurer sur banc** |

Quatre heures à 2 W demandent 8 Wh utiles. Avec une marge pour le vieillissement (80 % de capacité en fin de vie), le froid et l'imprécision de la jauge, la batterie visée est d'environ **20 Wh nominaux**.

## Options évaluées

### Entrée secteur

| Option | Évaluation |
| --- | --- |
| **A. Bloc secteur externe médical** (IEC 60601-1, 2 MOPP), basse tension continue en entrée de la pompe | **Proposée** : l'isolement patient-secteur est porté par un composant certifié du commerce, et la pompe ne manipule que de la très basse tension ; c'est la voie la plus sûre et la plus reproductible pour un projet ouvert (HW-REQ-001) |
| B. Alimentation secteur interne | Écartée pour le prototype : l'isolement à 2 MOPP, les lignes de fuite et les essais diélectriques relèveraient de la conception du projet |

### Chimie de la batterie

| Option | Évaluation |
| --- | --- |
| **A. Lithium fer phosphate (LiFePO4)** | **Proposée** : la chimie lithium la moins exposée à l'emballement thermique ; durée de vie en cycles élevée. Sa tension très plate rend la jauge par la tension inutilisable : il faut un comptage coulométrique |
| B. Lithium-ion NMC | Écartée : meilleure densité d'énergie, mais risque d'emballement thermique plus élevé, que la densité n'impose pas pour 20 Wh |
| C. Nickel-métal hydrure | Écartée : faible densité, autodécharge élevée, jauge peu fiable |

## Décision proposée

**Cette décision revient d'abord à l'auteur, puis à l'acceptation de l'approbatrice** ; l'ADR reste au statut « proposée » jusque-là.

### 1. Bascule sans interruption

- **Bloc secteur externe médical** (2 MOPP) fournissant une très basse tension continue.
- **Chemin d'alimentation prioritaire** (*power path*) : le système est alimenté par l'entrée quand elle est présente, sinon par la batterie, à travers des **diodes idéales**. La bascule est donc matérielle et instantanée : aucun logiciel n'intervient pour maintenir la perfusion (SYS-REQ-015).
- Le logiciel ne fait que **constater** la bascule (présence de l'entrée) et la signaler, puis la journaliser.

### 2. Batterie, jauge et fin d'autonomie

- **Batterie LiFePO4 d'environ 20 Wh** en cellules au format standard, avec circuit de protection (surcharge, décharge profonde, surintensité, température) et charge autorisée seulement dans la plage de température des cellules.
- **Jauge coulométrique** (circuit dédié), recalée à pleine charge, avec une **estimation prudente** : l'autonomie affichée et les seuils d'alarme utilisent la capacité restante minorée de l'incertitude de la jauge et la consommation majorée. Une fausse indication de réserve suffisante est le danger principal (RISK-010) ; une fausse indication d'épuisement interrompt la thérapie à tort (RISK-013). La tension de la batterie sert de contrôle indépendant : sous un seuil, l'épuisement est déclaré quelle que soit la jauge.
- **Fin d'autonomie** (SYS-REQ-016) :
  - alarme d'autonomie faible à 30 min estimées ;
  - alarme haute d'autonomie épuisée à 5 min estimées ;
  - à l'épuisement, **arrêt commandé** : état sûr, journalisation de l'arrêt, puis mise hors tension. Le circuit d'alarme autonome, armé pendant la perfusion, retentit alors sur sa propre réserve.
- **État de santé** : la capacité mesurée à chaque recalage est journalisée ; une batterie sous un seuil de capacité est signalée en maintenance (SYS-REQ-041).

### 3. Réserve d'énergie à la coupure

La perte du secteur n'est pas une coupure : la batterie prend le relais. La **coupure** visée ici est la perte simultanée des deux sources : batterie déconnectée ou protégée par son circuit, défaut de connexion, panne du chemin d'alimentation.

- **Détection** : un superviseur de tension, matériel, signale la coupure imminente à chaque processeur par une interruption, avant le seuil de fonctionnement.
- **Réserve de maintien** : des condensateurs (supercondensateurs si nécessaire) sur l'alimentation des processeurs et de la mémoire non volatile. Ils sont dimensionnés pour **terminer l'écriture en cours la plus longue du support retenu** ([#16](https://github.com/camille-martin-paris/clepsydre/issues/16)), journaliser si possible la coupure, puis passer en état sûr, avec une marge d'un facteur 2.
- **Ordre des actions à la coupure** : retrait de l'autorisation moteur (déjà l'état par défaut, ADR 0003), refus de toute nouvelle écriture, achèvement de l'écriture en cours, événement « coupure » si la réserve le permet.
- **Écritures sûres en cas de coupure** : l'état de perfusion et le journal sont écrits de façon à ce qu'une coupure à n'importe quel instant laisse soit l'ancienne, soit la nouvelle version intègre (SYS-REQ-038, SW-REQ-017). La réserve réduit le nombre d'écritures interrompues, mais **la sûreté ne repose pas sur elle**.
- **Réserve de l'alarme autonome** (ADR 0003) : une **pile lithium primaire** dédiée, préférée au supercondensateur car l'énergie d'un supercondensateur de taille raisonnable est trop juste pour 2 min de signal sonore. Sa tension en charge est contrôlée au test de démarrage, et elle est remplacée en maintenance.
- **Horloge** : l'horloge temps réel a sa propre réserve (pile ou supercondensateur), traitée par l'ADR [#17](https://github.com/camille-martin-paris/clepsydre/issues/17).

## Conséquences

### Exigences et analyse des dangers

À traiter après acceptation, dans une révision de l'analyse et de la spécification :

- **nouveau danger** : emballement thermique ou incendie de la batterie (énergie thermique), à ajouter à l'analyse ; mesures : chimie LiFePO4, circuit de protection, charge limitée en température, cellules conformes à l'IEC 62133-2 ;
- **SYS-REQ-015** : bascule matérielle par diodes idéales, sans intervention logicielle ;
- **SYS-REQ-016** : estimation prudente de l'autonomie, contrôle indépendant par la tension, arrêt commandé à l'épuisement ;
- nouvelles exigences : **bloc secteur externe 2 MOPP** ; **jauge coulométrique** et son exactitude ; **détection de coupure imminente** et **réserve de maintien** ; **pile de l'alarme autonome** et son test ;
- **SYS-REQ-020** : la réserve de l'alarme autonome est une pile lithium primaire surveillée.

### Conception

- **Électronique** ([#56](https://github.com/camille-martin-paris/clepsydre/issues/56), [#63](https://github.com/camille-martin-paris/clepsydre/issues/63), [#64](https://github.com/camille-martin-paris/clepsydre/issues/64)) : connecteur et bloc externe, chemin d'alimentation à diodes idéales, chargeur LiFePO4, jauge coulométrique, protection de la batterie, superviseur de tension, réserve de maintien, pile de l'alarme autonome.
- **Logiciel** ([#65](https://github.com/camille-martin-paris/clepsydre/issues/65)) : alarmes d'autonomie, arrêt commandé, gestion de l'interruption de coupure imminente.
- **Bancs** ([#75](https://github.com/camille-martin-paris/clepsydre/issues/75)) : consommation réelle, autonomie, exactitude de la jauge, coupures à des instants aléatoires pendant les écritures.

## Limites

- Consommation estimée, pas mesurée ; la capacité de la batterie sera ajustée après mesure.
- La durée de maintien dépend du support de stockage, choisi avec #16.
- Les pré-essais de sécurité électrique (#76) ne portent que sur la partie très basse tension et sur l'emploi d'un bloc certifié ; ils ne valent pas essai de type.

## Références

- [ADR 0003](0003-architecture-de-calcul-et-canal-de-securite.md), alarme de repli
- [Spécification des exigences](../requirements/requirements-specification.md)
- [Analyse préliminaire des dangers](../requirements/preliminary-hazard-analysis.md), RISK-010
- IEC 60601-1:2005+A1:2012+A2:2020, moyens de protection (MOPP)
- IEC 62133-2:2017, sécurité des accumulateurs lithium portables
