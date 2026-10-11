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
| 3 (suite) | Injection de défauts : capteurs, pilote, seringue, air, alimentation | À venir |
| 4 | Démonstration interactive et limites complètes du modèle | À venir |

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
| `clepsydre.sim.processor` | Processeur simulé (`Processor`) qui exécute un logiciel sur son propre compteur, avec ses défauts : blocage, réinitialisation, arrêt de l'oscillateur |

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
| Compteur de pas | Pas exécutés par le moteur | Vu par le processeur de commande |
| Force du pousseur | Force de contact entre le pousseur et le piston | Quantifiée à `forceResolution` ; sert à la détection d'occlusion (CTRL-007) |
| Position du piston | Déplacement du piston depuis la seringue pleine | Quantifiée à `positionResolution` ; capteur linéaire propre au canal de sécurité, distinct du comptage de pas (ADR 0003, SYS-REQ-001). Il mesure le piston et non le pousseur : en occlusion, la compression du joint fait avancer le pousseur sans déplacer le piston |
| Diamètre de la seringue | Diamètre extérieur du corps | Identification de la seringue (SYS-REQ-047) |
| Capture du piston | Piston capturé ou libéré | SYS-REQ-006 |

Les grandeurs réelles (volume délivré, pression, positions) sont accessibles aux tests. Le logiciel embarqué n'y aura pas accès.

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

Les tests [`sim_syringe_pump_test.cpp`](../../tests/sim_syringe_pump_test.cpp), [`sim_clock_test.cpp`](../../tests/sim_clock_test.cpp), [`sim_serial_link_test.cpp`](../../tests/sim_serial_link_test.cpp), [`sim_simulation_test.cpp`](../../tests/sim_simulation_test.cpp), [`sim_link_faults_test.cpp`](../../tests/sim_link_faults_test.cpp) et [`sim_processor_test.cpp`](../../tests/sim_processor_test.cpp) vérifient que le modèle reproduit les comportements qu'il prétend reproduire. Ils ne vérifient **aucune exigence de la pompe**.

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

Dans les lignes qui mettent deux processeurs en jeu, ce sont des bouchons de test qui n'échangent que des signaux de vie numérotés : les logiciels de la pompe n'existent pas encore. Ces lignes montrent que chaque défaut injecté est observable ; la détection et la réaction relèvent des logiciels et de leurs propres vérifications.

Ces résultats illustrent le modèle. Ils ne valent pas mesure du dispositif.

## Limites du modèle

Les limites suivantes sont connues. L'étape 4 les complétera.

- **Mécanique** :
  - piston sans masse et pousseur rigide ;
  - frottement de Coulomb seulement, sans dépendance à la vitesse, à la température ni au temps de repos ;
  - joint élastique linéaire.
- **Hydraulique** :
  - compliance et résistance constantes, liquide incompressible assimilé à de l'eau ;
  - pas d'inertie du liquide ;
  - pas d'air dans la ligne, prévu à l'étape 3.
- **Moteur** : il suit toujours la commande. La perte de pas, le couple limité et le calage seront des défauts injectés à l'étape 3.
- **Capteurs** : capteurs idéaux, à la quantification près, sans bruit ni dérive.
- **Horloges** : dérive constante, sans dépendance à la température ni gigue. Un oscillateur ne peut pas aller deux fois trop vite ou plus.
- **Liaison** : sans injecteur, ligne parfaite. Avec un injecteur, une trame part quand son délimiteur est confié, et une trame retardée est relâchée au plus tard au pas suivant de l'ordonnanceur. File d'émission sans limite de taille.
- **Processeurs** : le compteur de démarrage ne devient jamais illisible, et un processeur bloqué ne coupe pas encore le moteur ; ces défauts et leurs effets viendront avec le pilote et la suite de l'étape 3.
- **Ordonnancement** : un processeur simulé ne réagit qu'aux frontières des quanta ; son temps d'exécution n'est pas modélisé.
- **Paramètres** : ordres de grandeur, non mesurés.

## Usage comme moyen de vérification

Le simulateur portera les vérifications de niveau « système logiciel » ([stratégie de vérification](../../software_development_file/verification-strategy.md#niveaux-de-vérification)). Il devra alors être qualifié comme outil de vérification ([#89](https://github.com/camille-martin-paris/clepsydre/issues/89)). Cette page et ses tests en sont une première pièce, pas la qualification.
