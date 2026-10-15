# Simulateur physique de la pompe

Ce document décrit le modèle physique qui permet de développer et d'éprouver le logiciel avant le matériel. Issue : [#21](https://github.com/camille-martin-paris/clepsydre/issues/21).

> [!CAUTION]
> Le simulateur ne reproduit que ce que son modèle contient. Il ne remplace pas les essais sur banc ([#73](https://github.com/camille-martin-paris/clepsydre/issues/73), [#74](https://github.com/camille-martin-paris/clepsydre/issues/74), [#75](https://github.com/camille-martin-paris/clepsydre/issues/75)). Les exigences de performance physique se vérifient sur banc ([stratégie de vérification](../../software_development_file/verification-strategy.md#niveau-attendu-par-type-dexigence)).

## État d'avancement

| Étape | Contenu | État |
| --- | --- | --- |
| 1 | Modèle physique du pousse-seringue : mécanisme, seringue, ligne, capteurs | Ce document |
| 2 | Liaison simulée entre les processeurs et horloges indépendantes ([ADR 0006](../adr/0006-interfaces-internes-horloge-et-datation.md)) | Ce document |
| 3 | Injection de défauts : liaison, horloges et processeurs | Ce document |
| 3 (suite) | Injection de défauts : pilote, capteurs, seringue, air, alimentation | Ce document |
| 4 | Démonstration interactive et limites complètes du modèle | Ce document |

## Organisation

Le simulateur est la bibliothèque `clepsydre_sim` ([`src/sim/`](../../src/sim)). Elle s'exécute sur l'hôte, jamais sur la cible.

| Module | Rôle |
| --- | --- |
| `clepsydre.sim.quantities` | Grandeurs physiques typées, en unités SI et en virgule flottante : longueur, volume, force, pression, débit, raideur, compliance, résistance hydraulique, vitesse moteur |
| `clepsydre.sim.syringe_pump` | Modèle du pousse-seringue (`SyringePump`), paramètres du mécanisme, de la seringue et de la ligne, et jeux de paramètres d'exemple |
| `clepsydre.sim.clock` | Temps simulé (`TrueTime`), compteur monotone de chaque processeur (`MonotonicClock`) et horloge temps réel (`CivilClock`) |
| `clepsydre.sim.serial_link` | Liaison série point à point entre les processeurs (`SerialLink`), un sens par `SerialChannel` |
| `clepsydre.sim.simulation` | Ordonnanceur à pas fixe (`Simulation`) qui fait avancer le modèle physique, les processeurs et la liaison sur le même temps simulé |
| `clepsydre.sim.link_faults` | Injecteur de défauts de la liaison (`FaultyChannel`) : pertes, doublons, altérations, troncatures, retards et réordonnancements de trames |
| `clepsydre.sim.processor` | Processeur simulé (`Processor`) qui exécute un logiciel sur son propre compteur, avec ses défauts : blocage, réinitialisation, arrêt de l'oscillateur, mise hors tension |
| `clepsydre.sim.power` | Alimentation (`PowerSupply`) : secteur, batterie, bascule matérielle, superviseur de tension et réserve de maintien (ADR 0004) |

La démonstration interactive est la bibliothèque `clepsydre_sim_console` (module `clepsydre.sim.console`) et l'exécutable `clepsydre-sim` ([`src/sim_console/`](../../src/sim_console)) ; voir [Démonstration](#démonstration).

La virgule flottante convient au modèle, qui représente le monde physique. Le logiciel embarqué utilisera ses propres types, en arithmétique entière ou à virgule fixe (SW-REQ-005).

## Modèle

Le modèle avance par pas de temps fixes, de 1 ms recommandé. Il est **déterministe** : les mêmes commandes donnent les mêmes résultats, au bit près.

### Mécanisme

- **Moteur pas à pas** : il reçoit une vitesse signée en pas par seconde, bornée par `Mechanism::maxStepRate`. Cette borne représente la limite matérielle de SYS-REQ-043. Les pas entiers sont exécutés, et la fraction de pas est reportée au pas de temps suivant.
- **Vis et écrou** : l'écrou avance de `screwLead / stepsPerRevolution` par pas. La vis est irréversible : la charge ne la fait pas tourner.
- **Jeu** : le pousseur est libre dans un jeu `backlash` par rapport à l'écrou. La charge du piston le plaque contre le flanc de l'écrou qui la retient. Au départ, le jeu est entièrement ouvert : c'est le cas le plus défavorable pour le démarrage (CTRL-027).

### Seringue

Notations : x<sub>p</sub> est la position du pousseur, x la position du piston, A la section intérieure, k la raideur du joint, C la compliance, V<sub>s</sub> le volume sorti vers le patient, p<sub>0</sub> la pression initiale.

- **Joint du piston** : c'est un ressort de raideur k entre le pousseur et le liquide. La force de contact vaut F = k (x<sub>p</sub> − x). Un piston non capturé n'est que poussé (F ≥ 0) ; un piston capturé peut aussi être tiré.
- **Pression** : le volume chassé par le piston et non encore sorti est stocké dans la compliance C, celle du corps de la seringue et du prolongateur :

  p = p<sub>0</sub> + (A x − V<sub>s</sub>) / C

- **Frottement** : le piston est sans masse. Il reste immobile tant que la force nette F − A p ne dépasse pas le frottement statique. Il glisse ensuite jusqu'à la position où cette force égale le frottement dynamique. La force nette étant affine en x, cette position se calcule exactement. Le collé-glissé (*stick-slip*) découle de l'écart entre les deux frottements.
- **Course** : le piston est borné entre la seringue pleine et la fin de course, qui correspond au volume utile.

Le joint et le corps forment ensemble une compliance effective C + A²/k. Avec les paramètres d'exemple de la seringue de 50 mL, le joint domine : environ 10 µL/kPa, contre 1,2 µL/kPa pour le corps.

### Ligne et patient

- **Écoulement** : le liquide traverse la résistance hydraulique R de la ligne et du cathéter, vers une pression extérieure qui vaut p<sub>patient</sub> − ρ g h. Ici h est la hauteur de la seringue au-dessus du point d'injection, ρ la masse volumique de l'eau et g l'accélération de la pesanteur. Une hauteur positive rend le siphonage possible (RISK-003).
- **Calcul** : pendant un pas, la compliance se décharge de façon exponentielle, avec une constante de temps R C. Ce calcul exact reste stable quel que soit le pas, y compris en occlusion, où R est très grande.
- **Débit inverse** : il est admis quand la pression de la seringue est inférieure à la pression extérieure.

### Capteurs

| Capteur | Grandeur | Remarque |
| --- | --- | --- |
| Compteur de pas | Pas commandés | Vu par le processeur de commande ; le moteur peut en exécuter d'autres si le pilote est défaillant |
| Force du pousseur | Force de contact entre le pousseur et le piston | Quantifiée à `forceResolution` ; sert à la détection d'occlusion (CTRL-007) |
| Position du piston | Position du piston par rapport au bâti | Quantifiée à `positionResolution` ; capteur linéaire propre au canal de sécurité, distinct du comptage de pas (ADR 0003, SYS-REQ-001). Il mesure le piston et non le pousseur : en occlusion, la compression du joint fait avancer le pousseur sans déplacer le piston |
| Diamètre de la seringue | Diamètre extérieur du corps | Identification de la seringue (SYS-REQ-047) ; aucune valeur sans seringue |
| Capture du piston | Piston capturé ou libéré | SYS-REQ-006 |
| Berceau | Seringue en place, mal placée ou absente | SYS-REQ-006 |
| Détecteur d'air | Bulle devant le détecteur, placé sur la ligne à `airDetectorVolume` de la seringue | SYS-REQ-007 |
| Alimentation | Présence du secteur, tension de la batterie, courant moyen de la batterie sur le dernier pas (positif en décharge, négatif en charge), coupure imminente | SYS-REQ-015, SYS-REQ-049, SYS-REQ-050 ; la réserve de maintien court dès la perte des deux sources, datée au début du pas si elles étaient déjà perdues, ou à l'instant de la coupure par la protection de la batterie |

Les grandeurs réelles (volume délivré, pression, positions) sont accessibles aux tests. Le logiciel embarqué n'y aura pas accès.

### Défauts physiques et matériels

| Élément | Défauts | Effet |
| --- | --- | --- |
| Validation du pilote | Retrait de la validation | Le moteur ne tourne pas, même si le pilote est emballé ; les pas commandés continuent d'être comptés (SYS-REQ-045) |
| Pilote moteur | Pas perdus, calage, emballement | Une fraction des pas n'est pas exécutée ; aucun pas n'est exécuté ; le moteur tourne à la vitesse maximale quelle que soit la commande |
| Capteurs de force, de position du piston et de diamètre | Figé, décalé, perdu | La mesure reste à sa valeur au moment du défaut, est décalée d'une valeur choisie, ou n'est plus rendue |
| Seringue | Mal placée, absente | Corps non tenu : le pousseur entraîne la seringue entière, le capteur de position voit le piston avancer, mais rien n'est délivré. Absente : ni force, ni mesure, ni délivrance ; remise, elle est placée au contact du pousseur |
| Ligne | Bulle d'air | La bulle avance avec le liquide sorti de la seringue, passe devant le détecteur, puis atteint le patient après le volume intérieur de la ligne |
| Alimentation | Perte du secteur, batterie déconnectée, vieillissement | Bascule matérielle sur batterie sans interruption ; perte totale signalée par le superviseur, puis extinction après la réserve de maintien ; capacité réelle réduite |

**Assemblage.** Le modèle physique, l'alimentation et les processeurs sont des pièces distinctes. L'assemblage qui les relie, dans les tests, reproduit le câblage matériel : la validation du pilote exige l'alimentation et la marche des deux processeurs, et une perte totale éteint les processeurs.

### Liaison, horloges et ordonnancement

Ces modules appliquent l'[ADR 0006](../adr/0006-interfaces-internes-horloge-et-datation.md).

- **Temps simulé** : `TrueTime` est la référence du monde simulé, en nanosecondes entières. Le modèle physique, la liaison et l'ordonnanceur l'utilisent ; un logiciel simulé n'y a jamais accès et ne lit que ses propres horloges.
- **Compteur monotone** : chaque processeur a le sien, avec son décalage au départ, son écart de fréquence en parties par milliard et sa période de comptage (1 µs par défaut). L'écart de fréquence est borné à ±(10<sup>9</sup> − 1) parties par milliard (`maxDriftPpb`), de presque arrêté à presque deux fois trop vite ; un écart au-delà est ramené à la borne. Cette borne garantit un calcul en entiers sans débordement, quelle que soit la durée. Les deux compteurs ne se recalent jamais : une horloge fausse d'un côté se voit de l'autre.
- **Horloge temps réel** : celle du processeur de commande, en UTC à la seconde (SYS-REQ-035), avec sa dérive. Elle peut être mise à l'heure par un technicien, sauter, ou s'arrêter quand sa réserve s'épuise ; elle se fige alors et lève l'indicateur d'arrêt de l'oscillateur. Elle peut aussi devenir illisible : la lecture ne rend alors aucune valeur, jamais une heure inventée.
- **Liaison série** : deux sens indépendants, qui transportent des octets au format 8N1 (10 bits par octet). Un octet part quand la ligne est libre, occupe la ligne pendant sa durée d'émission, puis arrive après la latence. L'ordre d'émission est conservé. La liaison ignore les trames : leur format (COBS, CRC-32) relève du module de protocole partagé par les deux logiciels et par le simulateur.
- **Ordonnanceur** : à chaque pas, le temps simulé avance d'un quantum (1 ms par défaut), puis chaque participant est appelé dans l'ordre d'inscription. Cet ordre fixe rend l'ensemble déterministe.

### Processeurs et injection de défauts

- **Processeur** : il exécute un logiciel, fourni sous forme de deux fonctions, l'une au démarrage, l'autre à chaque pas. Le logiciel ne voit que son compteur monotone, qui repart de zéro à chaque démarrage, son numéro de démarrage, persistant, et la liaison. Il ne voit jamais le temps simulé.
- **Défauts du processeur** :
  - blocage : le logiciel cesse de s'exécuter, et le compteur continue d'avancer ;
  - réinitialisation : le processeur redémarre après une durée choisie, avec un nouveau numéro de démarrage, et les octets reçus pendant l'arrêt sont perdus ;
  - arrêt de l'oscillateur : le compteur se fige, y compris après un redémarrage.
- **Défauts de la liaison** : l'injecteur se place devant un sens de la liaison. Il découpe le flux en trames selon le délimiteur de l'ADR 0006 (octet nul de l'encodage COBS), sans en lire le contenu. Chaque règle porte un défaut, une fenêtre d'activité et une probabilité par trame :

  | Défaut | Effet sur la trame |
  | --- | --- |
  | Perte | Elle n'est pas émise |
  | Doublon | Elle est émise deux fois |
  | Altération | Un bit d'un octet de contenu est inversé |
  | Troncature | Sa seconde moitié est perdue, le délimiteur est conservé |
  | Retard | Elle part une durée choisie plus tard ; les trames suivantes peuvent la doubler |
  | Réordonnancement | Elle part juste après la trame suivante, dans l'ordre de remise à l'injecteur, que celle-ci parte aussitôt ou après son propre retard ; une trame perdue ne compte pas |

  Plusieurs défauts peuvent toucher une même trame. Une perte l'emporte sur tout. L'altération et la troncature modifient le contenu, puis un doublon émet deux fois le contenu obtenu. Une trame à la fois retardée et réordonnée part au plus tôt à son échéance, et après la trame suivante. Les départs se font dans l'ordre de leurs instants ; à instant égal, dans l'ordre de remise.

- **Déterminisme** : les tirages viennent d'un générateur dont la suite est fixée par la norme C++ (`std::mt19937_64`), avec une graine choisie. La même graine injecte les mêmes défauts.

### Paramètres d'exemple

`exampleMechanism`, `exampleSyringes` (10, 20 et 50 mL) et `exampleLine` donnent des **ordres de grandeur, non mesurés**. Les noms « exemple 10 mL » et suivants ne désignent aucun produit. Les paramètres réels de chaque seringue de la liste vérifiée viendront des bancs (#73, #74).

## Comportements reproduits

Les tests [`sim_syringe_pump_test.cpp`](../../tests/sim_syringe_pump_test.cpp), [`sim_clock_test.cpp`](../../tests/sim_clock_test.cpp), [`sim_serial_link_test.cpp`](../../tests/sim_serial_link_test.cpp), [`sim_simulation_test.cpp`](../../tests/sim_simulation_test.cpp), [`sim_link_faults_test.cpp`](../../tests/sim_link_faults_test.cpp), [`sim_processor_test.cpp`](../../tests/sim_processor_test.cpp), [`sim_pump_faults_test.cpp`](../../tests/sim_pump_faults_test.cpp) et [`sim_power_test.cpp`](../../tests/sim_power_test.cpp) vérifient que le modèle reproduit les comportements qu'il prétend reproduire. Ils ne vérifient **aucune exigence de la pompe**.

| Comportement | Résultat avec les paramètres d'exemple |
| --- | --- |
| Débit en régime établi égal au débit commandé | 10 mL/h, à 0,5 % près |
| Retard au premier écoulement à bas débit : rattrapage du jeu, puis compression du joint jusqu'au frottement statique | Environ 13 min à 1 mL/h avec la seringue de 50 mL, au-delà des 5 min proposées par SYS-REQ-048 : un rattrapage avant le démarrage (CTRL-027) est nécessaire |
| Montée de la force en occlusion, sans écoulement | Environ +40 N en 2 min à 25 mL/h |
| Bolus à la levée de l'occlusion, borné par le volume accumulé | Environ 0,85 mL sans recul du pousseur ; environ 0,15 mL après 2 s de recul (CTRL-008) |
| Mesure de position du piston en occlusion : elle suit le piston, pas le pousseur, quand le joint se comprime | — |
| Vis irréversible : le pousseur ne recule pas sous la charge | — |
| Siphonage : seringue 1 m au-dessus du patient, joint à faible frottement | Piston retenu : moins de 0,1 mL, limité par le jeu et l'élasticité du joint ; piston libéré : écoulement libre de plus de 20 mL en 10 min |
| Fin de course : volume utile délivré, puis montée de la force | — |
| Vitesse bornée par le matériel ; capteurs quantifiés ; déterminisme | — |
| Dérive des horloges calculée sans erreur d'arrondi ni débordement | 23 ppm sur 30 jours : exactement 59,616 s |
| Compteurs monotones indépendants | ±50 ppm : 60 ms d'écart relatif en 10 min |
| Horloge temps réel : mise à l'heure, saut, arrêt de l'oscillateur | — |
| Liaison au débit de la ligne, ordre conservé, ligne occupée | 86,806 µs par octet à 115 200 bit/s |
| Signaux de vie toutes les 50 ms entre deux processeurs à ±50 ppm, pendant une perfusion de 10 min | Silence le plus long : environ 51 ms, mesuré par chaque processeur sur sa propre horloge |
| Horloge du processeur de sécurité cinq fois trop lente | La commande mesure un silence d'environ 250 ms, au-delà des 200 ms de la perte de lien |
| Chaque défaut de trame, dans sa fenêtre et à sa probabilité ; injecteur sans règle transparent | 10 % de pertes demandées : 9 à 11 % obtenues sur 10 000 trames |
| Pertes et doublons à 1 % pendant 2 min, retrouvés par le processeur récepteur dans les numéros de trame | 25 pertes et 31 doublons injectés, tous retrouvés |
| Trames tronquées retrouvées par leur longueur | 58 sur 1 min, toutes retrouvées |
| Réinitialisation de la sécurité pendant 300 ms | La commande voit changer le numéro de démarrage et mesure un silence d'environ 350 ms |
| Blocage, ou arrêt de l'oscillateur, de la sécurité | La commande mesure un silence qui grandit sans fin |
| Horloge temps réel illisible | Aucune valeur lue ; l'heure continue d'avancer |
| Pilote emballé pendant 10 s, commande à 10 mL/h | Environ 2,8 mL délivrés pour 0,03 mL commandés ; le capteur de position du piston suit le volume poussé, pas les pas commandés ; le retrait de la validation arrête le moteur |
| Un pas sur deux perdu | Débit réduit de moitié : 5 mL/h pour 10 mL/h |
| Moteur calé pendant 10 min | Le piston n'avance plus que de la détente du joint, environ 5 µm, contre 3 mm attendus |
| Capteur de force figé, puis occlusion | La force réelle monte, la mesure non |
| Seringue mal placée, 5 min à 100 mL/h | Le capteur de position voit le piston avancer d'environ 8,3 mL ; moins de 0,1 µL délivré ; le berceau signale la seringue mal placée |
| Bulle de 60 µL | Vue par le détecteur à 100 µL de la seringue, puis entièrement délivrée au patient après 1 mL de ligne |
| Batterie neuve, moteur en marche (330 mA) | Environ 5 h 21 min d'autonomie avec les paramètres d'exemple ; moitié moins à capacité réduite de moitié |
| Perte du secteur | Ni processeur réinitialisé, ni moteur arrêté |
| Perte totale | Coupure imminente signalée pendant la réserve de maintien de 50 ms, puis processeurs et moteur arrêtés ; au retour du secteur, les processeurs redémarrent et le bouchon du logiciel de commande ne relance pas le moteur |

Dans les lignes qui mettent des processeurs en jeu, ce sont des bouchons de test qui n'échangent que des signaux de vie numérotés : les logiciels de la pompe n'existent pas encore. Ces lignes montrent que chaque défaut injecté est observable ; la détection et la réaction relèvent des logiciels et de leurs propres vérifications. Le protocole de la liaison ([#125](https://github.com/camille-martin-paris/clepsydre/issues/125)) est déjà essayé face à ces défauts : `tests/protocol_link_sim_test.cpp` vérifie que chacun est détecté comme perte de lien ([conception détaillée](../design/link-protocol.md#vérification)). L'asservissement du moteur ([#26](https://github.com/camille-martin-paris/clepsydre/issues/26)) est essayé en boucle fermée avec le modèle du pousse-seringue : `tests/control_drive_sim_test.cpp` mesure le délai au premier écoulement et la précision du débit du modèle ([conception détaillée](../design/drive.md#vérification-en-simulation)). La détection d'occlusion et la décompression ([#27](https://github.com/camille-martin-paris/clepsydre/issues/27)) le sont aussi : `tests/control_occlusion_sim_test.cpp` mesure le délai de détection et le bolus à la levée de l'occlusion, avec et sans recul ([conception détaillée](../design/occlusion.md#vérification-en-simulation)).

Ces résultats illustrent le modèle. Ils ne valent pas mesure du dispositif.

## Démonstration

`clepsydre-sim` pilote le pousse-seringue et son alimentation par des commandes texte, injecte les défauts physiques et matériels, et affiche côte à côte les grandeurs réelles et ce que mesurent les capteurs. Il s'exécute sur l'hôte :

```sh
cmake --build --preset gcc
build/gcc/src/sim_console/clepsydre-sim                              # commandes au clavier
build/gcc/src/sim_console/clepsydre-sim examples/sim/occlusion-bolus.txt   # scénario
```

« aide » liste les commandes ; « verifie liste » liste les grandeurs vérifiables. En mode scénario, chaque ligne est recopiée, et l'exécution s'arrête à la première erreur, de commande ou de lecture du scénario (code de sortie 2), ou à la première vérification échouée (code 1).

La console tient le rôle de l'opérateur : il n'y a pas encore de logiciel de pompe. Elle reproduit seulement deux câblages matériels : la validation du moteur exige l'alimentation, et une perte totale d'alimentation remet la commande du moteur à zéro, comme le logiciel de commande devra le faire à son redémarrage (SYS-REQ-038). Les défauts de la liaison et des processeurs n'y figurent pas : ils n'ont d'effet qu'à travers des logiciels, qui viendront en M1.

| Scénario ([`examples/sim/`](../../examples/sim)) | Ce qu'il montre |
| --- | --- |
| `occlusion-bolus.txt` | Montée de la force en occlusion à 25 mL/h ; bolus limité à 0,15 mL par un recul de 2 s |
| `siphonage.txt` | Seringue 1 m au-dessus du patient : retenue par la vis si le piston est capturé, écoulement libre s'il est libéré |
| `pilote-emballe.txt` | Pilote emballé : 2,8 mL en 10 s ; arrêt par le retrait de la validation |
| `coupure-alimentation.txt` | Bascule sur batterie sans interruption ; perte totale, coupure imminente puis arrêt ; pas de reprise au retour du secteur |
| `air.txt` | Bulle de 60 µL vue par le détecteur, puis délivrée au patient |
| `seringue-mal-placee.txt` | Seringue entraînée par le pousseur sans délivrer ; seul le berceau le révèle |

Chaque scénario porte ses propres vérifications. CTest les exécute comme tests de fumée, en CI, pour que la démonstration ne se dégrade pas sans qu'on le voie. Ces vérifications portent sur le modèle et ses paramètres d'exemple, jamais sur la pompe.

## Limites du modèle

Le tableau recense les limites connues, élément par élément. Pour chacune, il indique comment elle peut fausser une vérification menée avec le simulateur, et comment elle sera levée. Une vérification qui dépend d'une limite non levée ne vaut que pour le modèle.

| Élément | Limite | Effet possible sur une vérification | Levée prévue |
| --- | --- | --- | --- |
| Mécanisme | Pousseur et vis rigides ; jeu constant ; pas de couple limité ni de perte de pas spontanée | Délai de démarrage et volume d'occlusion sous-estimés ; un calage par surcharge n'apparaît pas | Mécanisme retenu et banc de précision ([#52](https://github.com/camille-martin-paris/clepsydre/issues/52), [#73](https://github.com/camille-martin-paris/clepsydre/issues/73)) |
| Piston et joint | Piston sans masse ; joint élastique linéaire ; frottement de Coulomb sans dépendance à la vitesse, à la température ni au temps de repos | Collé-glissé, retard au démarrage et seuil de siphonage approximatifs ; écarts selon les seringues réelles | Caractérisation de chaque seringue de la liste vérifiée sur banc ([#73](https://github.com/camille-martin-paris/clepsydre/issues/73), [#74](https://github.com/camille-martin-paris/clepsydre/issues/74)) |
| Hydraulique | Compliance et résistance constantes ; liquide incompressible assimilé à de l'eau ; pas d'inertie ; pas de viscosité dépendant du perfusat | Montée de pression en occlusion et bolus approximatifs, surtout aux débits extrêmes et avec des cathéters fins | Banc d'occlusion selon l'IEC 60601-2-24 ([#74](https://github.com/camille-martin-paris/clepsydre/issues/74)) |
| Air | La bulle n'ajoute pas de compliance ; elle avance sans compression ni dissolution ; pas de micro-bulles ni de dégazage | Délai de détection d'occlusion avec de l'air sous-estimé ; volume d'air cumulé idéalisé | Détecteur retenu et essais ([#58](https://github.com/camille-martin-paris/clepsydre/issues/58)) |
| Capteurs | Idéaux, à la quantification près : sans bruit, dérive, hystérésis ni temps de réponse ; les défauts sont des régimes imposés ; berceau et détecteur d'air sans défaut propre | Seuils de détection trop optimistes ; fausses alarmes absentes | Capteurs retenus, étalonnage et incertitude ([#58](https://github.com/camille-martin-paris/clepsydre/issues/58)) ; défauts du berceau et du détecteur à ajouter avec eux |
| Seringue | Mal placée, elle suit exactement le pousseur ; remise, elle reprend son état | Cas intermédiaires (corps à demi tenu, ailettes mal engagées) absents | Interface consommable ([#53](https://github.com/camille-martin-paris/clepsydre/issues/53)) |
| Pilote moteur | Défauts imposés (pertes, calage, emballement) ; signal dynamique d'autorisation non modélisé : l'assemblage retire la validation | Délai de coupure par le circuit de validation non représenté | Carte de commande et superviseur matériel ([#57](https://github.com/camille-martin-paris/clepsydre/issues/57), [#59](https://github.com/camille-martin-paris/clepsydre/issues/59)) |
| Alimentation | Tension affine en fonction de la charge, sans résistance interne ni température ; consommation constante par régime ; le moteur ne dépend pas de la tension ; réserve de maintien de durée fixe | Autonomie et instant de coupure approximatifs ; chute de tension sous charge absente | Batterie retenue et campagne de coupures ([#63](https://github.com/camille-martin-paris/clepsydre/issues/63), [#64](https://github.com/camille-martin-paris/clepsydre/issues/64), [#75](https://github.com/camille-martin-paris/clepsydre/issues/75)) |
| Horloges | Dérive constante, sans température ni gigue ; oscillateur borné à moins de deux fois la fréquence nominale | Défauts d'horloge extrêmes non représentables | Composants retenus ([#59](https://github.com/camille-martin-paris/clepsydre/issues/59)) ; autre représentation si un tel défaut devient nécessaire |
| Liaison | Sans injecteur, ligne parfaite ; défauts au niveau des trames, sans erreurs de bit au fil de l'eau ; file d'émission illimitée | Saturation de la liaison et erreurs de cadrage absentes | Module de protocole partagé (ADR 0006) et essais d'intégration |
| Processeurs et ordonnancement | Réaction aux seules frontières des quanta (1 ms) ; temps d'exécution, interruptions et ordonnancement réel non modélisés ; compteur de démarrage jamais illisible | Respect des délais (SYS-REQ-002, SYS-REQ-053) non démontrable par le simulateur | Analyse temporelle (VER-124) et mesures sur la cible |
| Paramètres | Ordres de grandeur, non mesurés | Toutes les valeurs numériques sont illustratives | Bancs ([#73](https://github.com/camille-martin-paris/clepsydre/issues/73), [#74](https://github.com/camille-martin-paris/clepsydre/issues/74), [#75](https://github.com/camille-martin-paris/clepsydre/issues/75)) |

### Ce que le simulateur permet de conclure, et ce qu'il ne permet pas

Le simulateur permet :

- d'exercer la logique des logiciels face à des situations reproductibles, y compris celles qu'un banc rend rares ou dangereuses : défauts du pilote, des capteurs, de l'alimentation, des processeurs et de la liaison ;
- de vérifier qu'un défaut est détecté et que la réaction attendue est commandée, au niveau « système logiciel » ;
- de rejouer exactement un scénario, ce qui en fait un outil de non-régression.

Il ne permet pas de conclure :

- sur une **performance physique** : précision de débit, délai d'alarme d'occlusion, bolus, volume d'air, autonomie ;
- sur l'**efficacité d'une mesure matérielle** : validation du pilote, alarme autonome, bascule d'alimentation, superviseur de tension ;
- sur le **respect des délais sur la cible** ;
- sur la **sécurité électrique**, la **compatibilité électromagnétique** ou l'**aptitude à l'utilisation**.

Ces conclusions relèvent des essais sur banc et de la vérification système ([stratégie de vérification](../../software_development_file/verification-strategy.md#niveau-attendu-par-type-dexigence)).

## Usage comme moyen de vérification

Le simulateur portera les vérifications de niveau « système logiciel » ([stratégie de vérification](../../software_development_file/verification-strategy.md#niveaux-de-vérification)). Il devra alors être qualifié comme outil de vérification ([#89](https://github.com/camille-martin-paris/clepsydre/issues/89)). Cette page et ses tests en sont une première pièce, pas la qualification.
