# Protocole de la liaison entre les processeurs

| Champ | Valeur |
| --- | --- |
| Statut | Conception détaillée, soumise à relecture |
| Issue | [#125](https://github.com/camille-martin-paris/clepsydre/issues/125) |
| Décision d'architecture | [ADR 0006](../adr/0006-interfaces-internes-horloge-et-datation.md#1-liaison-entre-les-processeurs) |
| Exigence | SYS-REQ-053 |
| Code | [`src/protocol/`](../../src/protocol/) : modules `clepsydre.protocol.crc32`, `clepsydre.protocol.cobs`, `clepsydre.protocol.frame` et `clepsydre.protocol.link` |
| Classe de sécurité | C ([classification](../../software_development_file/safety-classification.md)) |

> [!CAUTION]
> Clepsydre ne doit pas être utilisé sur un être humain ni sur un animal.

## Objet

Ce document décrit la conception détaillée du protocole de la liaison entre le processeur de commande et le processeur de sécurité (IEC 62304 §5.4). Le protocole est défini une seule fois et partagé par les deux logiciels et par le simulateur (ADR 0006, « Définition unique »). Il compte deux couches :
- la **couche trame** forme les trames à émettre et reconnaît les trames reçues dans le flux d'octets (modules `crc32`, `cobs` et `frame`) ;
- la **couche de surveillance** numérote les trames émises, cadence le signal de vie et détecte la perte de lien à la réception (module `link`, [Surveillance du lien](#surveillance-du-lien)).

Les charges utiles ne sont pas définies ici, sauf celle du signal de vie et la taille maximale. Les autres viendront avec les fonctions qui les produisent.

## Format d'une trame

Une trame décodée compte au plus **254 octets** :

| Position | Taille | Champ | Contenu |
| --- | --- | --- | --- |
| 0 | 1 | Version | Version du protocole : `1` |
| 1 | 1 | Émetteur | `1` commande, `2` sécurité |
| 2 | 4 | Numéro de démarrage | Numéro de démarrage de l'émetteur, entier non signé en petit-boutiste |
| 6 | 4 | Numéro de transport | Augmenté de 1 à chaque trame émise, retransmissions comprises ; petit-boutiste |
| 10 | 1 | Type | Type de message, de `1` à `9` (tableau ci-dessous) |
| 11 | 1 | Longueur | Taille de la charge utile, de 0 à 238 octets |
| 12 | 0 à 238 | Charge utile | Selon le type |
| 12 + longueur | 4 | CRC | CRC-32C des octets qui précèdent, petit-boutiste |

| Type | Message |
| --- | --- |
| 1 | Signal de vie |
| 2 | Programmation |
| 3 | Écho |
| 4 | Demande de changement de mode |
| 5 | État de supervision |
| 6 | Événement |
| 7 | Accusé d'événement |
| 8 | Ancrage |
| 9 | Test |

Sur la ligne, la trame décodée est encodée en **COBS**, puis suivie d'un **octet nul**, le délimiteur. L'encodage ne contient aucun octet nul : un délimiteur marque donc toujours la fin d'une trame. Une trame de version 1 encodée, délimiteur compris, compte au plus **257 octets** ; une enveloppe, au plus 1 030 octets.

Les entiers sont en petit-boutiste, l'ordre natif des microcontrôleurs envisagés. Le format est fixé octet par octet, sans dépendre de la disposition des structures en mémoire : la sérialisation est écrite champ par champ.

### Enveloppe invariante

Toute version du protocole, présente ou future, respecte la même **enveloppe** :
- COBS et délimiteur nul ;
- version au premier octet ;
- CRC-32C sur les quatre derniers octets ;
- trame décodée de **5 octets** (version et CRC) **à 1 024 octets**.

Un récepteur reconnaît ainsi une trame intègre de n'importe quelle version sans en interpréter le reste, et la signale comme telle. La couche de surveillance en tire la mise hors service prévue par SYS-REQ-053.

Les bornes de la version 1 (en-tête complet, 254 octets au plus) ne s'appliquent **qu'après** le contrôle du CRC et de la version. Une trame intègre d'une autre version, plus courte ou plus longue qu'une trame de version 1, est donc bien signalée comme autre version. L'enveloppe laisse à une version future la place d'allonger ses trames, par exemple pour les événements d'audit (#41), jusqu'à 1 024 octets. Au-delà de 655 octets, la distance de Hamming garantie par le CRC-32C n'est plus que de 4 ; une telle version devra le prendre en compte.

La version n'est lue **qu'après** le contrôle du CRC. Une altération du premier octet est ainsi rejetée comme trame altérée, ce qui entraîne une perte de lien et l'état sûr. Elle ne peut pas passer pour une autre version, ce qui mettrait la pompe hors service à tort.

## Code de détection d'erreur : CRC-32C

**Retenu** : CRC-32C (Castagnoli) :
- polynôme `0x1EDC6F41`, soit `0x82F63B78` en représentation réfléchie ;
- valeur initiale et masque final `0xFFFFFFFF` ;
- entrée et sortie réfléchies.

Valeur de contrôle : `0xE3069283` pour « 123456789 ».

**Motif.** L'ADR 0006 demande « CRC-32 » sans fixer le polynôme. La donnée protégée compte au plus 250 octets, soit 2 000 bits, et la distance de Hamming garantie dépend du polynôme et de cette longueur :

| Polynôme | Distance de Hamming 6 jusqu'à | Distance de Hamming 5 jusqu'à | À 2 000 bits |
| --- | --- | --- | --- |
| CRC-32C (Castagnoli) | 5 243 bits | — | **6** : toute altération de 5 bits au plus est détectée |
| CRC-32 IEEE 802.3 | 268 bits | 2 974 bits | 5 : toute altération de 4 bits au plus est détectée |

Source : P. Koopman, « 32-Bit Cyclic Redundancy Codes for Internet Applications », *International Conference on Dependable Systems and Networks*, 2002. Les deux polynômes détectent aussi toute rafale d'au plus 32 bits.

**Conséquence matérielle.** Certaines unités de calcul de CRC des microcontrôleurs ne proposent que le polynôme IEEE 802.3, d'autres acceptent un polynôme programmable. L'implémentation actuelle est logicielle : une table de 256 entrées calculée à la compilation, soit 1 Kio de mémoire morte, et une itération par octet. Le choix du microcontrôleur (#57, #59) dira si une unité matérielle peut la remplacer. Les vecteurs de contrôle des tests resteront la référence.

## Délimitation : COBS

L'algorithme est celui de Cheshire et Baker (« Consistent Overhead Byte Stuffing », *IEEE/ACM Transactions on Networking*, 1999), dans sa **forme canonique** : un bloc plein de 254 octets qui termine les données n'est suivi d'aucun code.

- **Surcoût** : au plus un octet par bloc de 254 octets, plus un. La borne `cobsMaxEncodedSize(n) = n + n/254 + 1` est utilisée pour dimensionner les tampons. La forme canonique peut être plus courte d'un octet.
- **Refus au décodage** : un octet nul, un bloc qui dépasse la fin des données, un tampon de sortie trop petit. Une suite qui ne peut pas résulter d'un encodage n'est jamais décodée.

## Réception

`FrameReceiver` reçoit les octets un à un. Il les accumule jusqu'au délimiteur, puis décode la trame et renvoie la trame reconnue ou le motif de son rejet. Après chaque délimiteur, il repart d'un tampon vide : un rejet ne fait jamais perdre la synchronisation.

Les octets reçus avant le premier délimiteur, à la mise en route ou après une coupure de la ligne, forment une trame incomplète. Elle est rejetée comme les autres.

| Motif | Cause |
| --- | --- |
| `TooLong` | Enveloppe : plus de 1 029 octets reçus avant le délimiteur (les octets en trop ne sont pas conservés), ou trame décodée de plus de 1 024 octets passée à `parseFrame`. Version 1 : trame intègre de plus de 254 octets. |
| `InvalidCobs` | Pas un encodage COBS valide, ou décodage de plus de 1 024 octets |
| `TooShort` | Enveloppe : moins de 5 octets décodés, par exemple deux délimiteurs consécutifs. Version 1 : trame intègre de moins de 16 octets. |
| `BadCrc` | CRC faux : trame altérée ou tronquée |
| `VersionMismatch` | Trame intègre d'une autre version du protocole, quelle que soit sa taille dans l'enveloppe |
| `LengthMismatch` | Champ de longueur différent de la taille de la charge utile reçue |
| `UnknownSender` | Émetteur autre que `1` ou `2` |
| `UnknownType` | Type hors de `1` à `9` |

Les contrôles se font dans cet ordre : enveloppe (taille, CRC, version), puis version 1 (taille, longueur, émetteur, type). Chaque rejet est remonté à la couche de surveillance. Lien établi, elle le traite comme une perte de lien ; `VersionMismatch` conduit à la mise hors service dans tous les cas (SYS-REQ-053).

Une trame intègre et bien formée peut encore être hors séquence : doublon, réordonnancement, trame retardée. Ce contrôle relève de la surveillance, par le numéro de transport.

## Surveillance du lien

### Émission

`LinkTransmitter` numérote chaque trame émise : 0 pour la première trame après le démarrage, puis +1 à chaque trame, quel que soit son type, modulo 2³². À 20 trames par seconde, le numéro ne revient à 0 qu'après plus de six ans de fonctionnement continu ; le passage par 0 est néanmoins traité.

Le **signal de vie** est dû 50 ms après le précédent (`heartbeatPeriod`), ou dès le démarrage. Le logiciel interroge `heartbeatDue` à chaque cycle. L'émission d'une autre trame ne remplace pas le signal de vie.

Charge utile du signal de vie, 6 octets :

| Position | Taille | Champ |
| --- | --- | --- |
| 0 | 1 | État de l'émetteur, dont les valeurs seront définies par la machine à états (#24) |
| 1 | 1 | `1` si l'émetteur a reçu une trame de l'autre processeur depuis son démarrage, sinon `0` |
| 2 | 4 | Dernier numéro de transport reçu de l'autre processeur, petit-boutiste ; `0` sans réception |

Le décodage refuse toute autre taille, un indicateur autre que 0 ou 1, et un numéro non nul sans réception. Le dernier numéro reçu est transmis comme le prévoit l'ADR 0006 ; son usage, par exemple pour vérifier que l'autre processeur reçoit bien, relève des logiciels.

### Réception

`LinkMonitor` surveille les trames de l'autre processeur sur le temps monotone du processeur récepteur (SW-REQ-018). Il reçoit chaque résultat de la couche trame (`onReceived`) et doit être interrogé à chaque cycle, même sans réception (`poll`). `onReceived` contrôle d'abord le délai : une trame qui arrive après un silence ne masque pas la perte.

| État | Signification | Sortie |
| --- | --- | --- |
| `Waiting` | Aucune trame valide depuis la mise en route | Première trame valide de l'autre processeur : `Up`. 200 ms sans elle : `Lost` (`Timeout`) |
| `Up` | Lien établi | Toute anomalie : `Lost` (tableau suivant) |
| `Lost` | Perte de lien | Première trame valide de l'autre processeur : `Up` (rétablissement) |
| `OutOfService` | Désaccord de version | Aucune : l'état est définitif |

Lien établi, chaque trame est contrôlée dans cet ordre :

| Cause (`LinkFault`) | Condition |
| --- | --- |
| `Timeout` | Aucune trame valide depuis 200 ms (`linkTimeout`) |
| `Rejected` | Trame rejetée par la couche trame ; le motif est conservé (`rejection()`) |
| `WrongSender` | Trame valide dont l'émetteur n'est pas l'autre processeur |
| `BootChanged` | Numéro de démarrage différent de celui de l'établissement |
| `TransportRepeated` | Même numéro de transport que la trame précédente : doublon |
| `TransportBackward` | Numéro en arrière : écart de 2³¹ ou plus, modulo 2³² |
| `TransportGap` | Numéro en avance de plus de 1 : trame manquante |
| `VersionMismatch` | Trame intègre d'une autre version, dans tout état : `OutOfService` |

Avant l'établissement et après une perte, les trames rejetées et celles d'un autre émetteur sont ignorées. À la mise en route, le récepteur peut en effet recevoir une fin de trame. Le **rétablissement** se fait sur la première trame valide de l'autre processeur, dont les numéros servent de référence. C'est ce qui permet la réémission des événements d'audit « après chaque rétablissement du lien » (ADR 0006). Le rétablissement ne relance rien de lui-même : la perte a mis la pompe en état sûr, et une nouvelle programmation est nécessaire (ADR 0003). Le compteur `losses()` permet au logiciel de constater une perte même si le lien s'est rétabli entre deux consultations.

**Délai de détection.**
- Une trame perdue, en double, altérée ou hors séquence est détectée à la réception de la trame suivante, au plus une période de signal de vie plus tard.
- Un silence est détecté 200 ms après la dernière trame valide, à la granularité de l'appel de `poll` près.

La perte de lien est donc constatée en 250 ms au plus après le défaut, ce qui laisse la marge prévue par l'ADR 0006 pour l'état sûr en 1 s (SYS-REQ-002).

## Ressources et traitement des défauts

- **Mémoire** : aucune allocation dynamique ; toutes les tailles sont bornées à la compilation.
  - `FrameReceiver` occupe environ 1 040 octets, dont son tampon de 1 029 octets, dimensionné pour l'enveloppe.
  - `EncodedFrame` occupe environ 270 octets.
  - Le décodage d'une trame utilise 1 024 octets de pile.
  - `Payload` occupe environ 250 octets.
  - `LinkTransmitter` et `LinkMonitor` occupent quelques dizaines d'octets chacun.
- **Exceptions** : toutes les fonctions sont `noexcept` et ne lèvent pas d'exception. Les erreurs sont des valeurs (`std::optional`, `std::expected`).
- **Préconditions** : elles sont rendues impossibles à enfreindre par construction plutôt que vérifiées à l'exécution. Une charge utile ne peut dépasser 238 octets, puisque `Payload::from` refuse au-delà. L'encodage tient toujours dans `EncodedFrame`, ce que vérifie un `static_assert`. `parseFrame` refuse une trame décodée trop longue.
- **Temps d'exécution** : linéaire en nombre d'octets, sans boucle non bornée.

Ces règles anticipent celles du code de sécurité, que CONTRIBUTING.md renvoie aux ADR d'architecture.

## Vérification

Tests unitaires : `tests/protocol_crc32_test.cpp`, `tests/protocol_cobs_test.cpp`, `tests/protocol_frame_test.cpp` et `tests/protocol_link_test.cpp`. Essais d'intégration avec la liaison et les processeurs simulés : `tests/protocol_link_sim_test.cpp`.

| Critère (IEC 62304 §5.5.4, [stratégie de vérification](../../software_development_file/verification-strategy.md#critères-de-vérification-des-unités-classe-c)) | Couverture par les tests |
| --- | --- |
| Conditions aux limites | Charge utile vide et maximale (238 octets) ; refus à 239 ; blocs COBS pleins, presque pleins et suivis d'un octet ; tampons exacts et trop petits d'un octet ; flot plus long que la plus longue enveloppe ; trames intègres d'une autre version de 5 à 1 024 octets, y compris hors des bornes de la version 1 ; trame de version 1 trop courte et trop longue d'un octet ; entiers à `0xFFFFFFFF` |
| Flux de données | Format de la trame vérifié octet par octet ; aller-retour par l'encodage et la réception pour trois trames successives ; chaque type de `1` à `9` |
| Traitement des défauts | Chaque motif de rejet provoqué au moins une fois, émetteur et type inconnus de chaque côté de la plage valide ; reprise de la réception après un rejet, un flot trop long et un début de flux incomplet |
| Séquences d'événements | Trames successives dans un même flux ; octets avant le premier délimiteur |
| Gestion de la mémoire et débordements | Aucune écriture hors des tampons : chaque taille est contrôlée avant l'écriture, et les variantes AddressSanitizer et `_GLIBCXX_DEBUG` exécutent les tests |
| Initialisation des variables | Membres initialisés à leur déclaration ; tampons initialisés |
| Couverture structurelle | 100 % des régions et des branches, sans exclusion (tâche CI `coverage`) |
| Exactitude du CRC | Valeur de contrôle du catalogue et vecteurs de la RFC 3720, annexe B.4 |

Les tests de `protocol_link_test` couvrent :
- le format et le refus des signaux de vie mal formés ;
- la numérotation et la cadence à l'émission ;
- chaque transition du moniteur, chaque cause de perte, y compris de part et d'autre de la demi-plage et au passage par 0 ;
- le rétablissement, la perte constatée à la réception qui suit un silence, et la mise hors service depuis chaque état.

Les essais d'intégration (`protocol_link_sim_test`) relient deux processeurs simulés. La commande émet ses signaux de vie à travers l'injecteur de défauts de la liaison simulée, et la sécurité les surveille. Chaque défaut doit être détecté en 250 ms au plus, avec la cause attendue :

| Défaut injecté | Cause attendue | Après le défaut |
| --- | --- | --- |
| Aucun, pendant 10 s | Aucune perte | Lien établi |
| Trame perdue | `TransportGap` | Lien rétabli |
| Trame en double | `TransportRepeated` | Lien rétabli |
| Trame altérée | `Rejected` | Lien rétabli |
| Trame tronquée | `Rejected` | Lien rétabli |
| Trame retardée de 300 ms | `TransportGap` | Lien rétabli |
| Trame réordonnée | `TransportGap` | Lien rétabli |
| Commande bloquée | `Timeout` | Lien perdu |
| Commande redémarrée | `BootChanged` | Lien rétabli avec le nouveau numéro de démarrage |
| Oscillateur de la commande arrêté | `Timeout` | Lien perdu |
| Commande hors tension | `Timeout` | Lien perdu |

Ces essais vérifient la mise en œuvre de la mesure de la liaison (CTRL-031). Ils ne constituent pas VER-123, qui vérifiera au niveau du système logiciel l'état sûr atteint par les deux logiciels complets.

## Limites

- Le CRC détecte toute altération de 5 bits au plus et toute rafale de 32 bits au plus. Une altération plus étendue peut passer, avec une probabilité de l'ordre de 2⁻³² par trame altérée. La couche de surveillance ajoute le contrôle du numéro de transport.
- La liaison est interne à la carte : le protocole n'authentifie ni ne chiffre les trames. Le modèle de menaces (#67) dira si cela suffit.
- Le moniteur mesure le délai sur le compteur monotone de son propre processeur. Si cet oscillateur s'arrête, le moniteur ne constate plus de délai dépassé. L'autre processeur, lui, constate l'arrêt des signaux de vie, comme le montre l'essai de l'oscillateur arrêté ; le chien de garde externe complète la protection (ADR 0003).
- Le rétablissement se fait sur une seule trame valide. Une liaison instable alterne donc pertes et rétablissements ; chaque perte est comptée et met la pompe en état sûr. Une éventuelle politique de mise hors service sur pertes répétées relève des logiciels.
- Le temps d'exécution n'est pas mesuré sur la cible. L'analyse temporelle (VER-124) le reprendra avec le microcontrôleur retenu.
