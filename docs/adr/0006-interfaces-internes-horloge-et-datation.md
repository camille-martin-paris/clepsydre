# ADR 0006 : interfaces internes, horloge et datation des événements

| Champ | Valeur |
| --- | --- |
| Statut | Proposée, soumise à la décision de l'auteur puis à l'acceptation de l'approbatrice |
| Date | 2026-10-09 |
| Issue | [#17](https://github.com/camille-martin-paris/clepsydre/issues/17) |
| Épique | [#12](https://github.com/camille-martin-paris/clepsydre/issues/12) |
| Dépend de | [ADR 0003](0003-architecture-de-calcul-et-canal-de-securite.md) (deux processeurs, liaison) ; ADR de [#16](https://github.com/camille-martin-paris/clepsydre/issues/16) (flux d'audit), proposée en parallèle |

> [!CAUTION]
> Clepsydre ne doit pas être utilisé sur un être humain ni sur un animal. Les périodes et délais ci-dessous sont des propositions, à confirmer par l'analyse temporelle et les essais.

## Contexte

L'[ADR 0003](0003-architecture-de-calcul-et-canal-de-securite.md#liaison-entre-les-processeurs) renvoie à cette ADR le protocole de la liaison entre le processeur de commande et le processeur de sécurité. L'issue pose trois questions :

1. le protocole entre les deux processeurs, avec détection de la perte de lien ;
2. la source d'horloge, sa synchronisation, et le comportement si l'heure est indisponible ou incohérente ;
3. l'identité des flux d'événements par producteur et par démarrage.

Exigences concernées :

- **SYS-REQ-002** : état sûr en 1 s au plus ;
- **SYS-REQ-035** : résolution de 1 s, numéro d'ordre strictement croissant, dérive inférieure à 1 min par mois, mise à l'heure journalisée ;
- **SYS-REQ-040** : fonctionnement sans réseau ;
- **SYS-REQ-044** : programmation verrouillée et appuis observés par le canal de sécurité ;
- **SW-REQ-013** : écho de la programmation ;
- **SW-REQ-018** : délais de sécurité mesurés sur une base de temps monotone.

L'usage prévu exclut toute connexion réseau pour perfuser (H-08).

## 1. Liaison entre les processeurs

### Options

| Option | Évaluation |
| --- | --- |
| **A. Liaison série asynchrone (UART), point à point, bidirectionnelle** | **Proposée** : chaque processeur émet à son rythme, sur sa propre horloge, sans maître ni esclave ; c'est la plus simple à vérifier et à simuler |
| B. SPI | Écartée : le maître cadence l'esclave. Un processeur de sécurité esclave dépendrait de l'horloge de la commande, et un processeur de sécurité maître devrait interroger la commande |
| C. Bus CAN | Écartée pour deux nœuds sur une même carte : robuste, mais plus complexe sans bénéfice ici |

### Trame

| Champ | Contenu |
| --- | --- |
| Délimitation | Encodage COBS : un octet nul marque la fin de trame, une trame tronquée est détectée |
| En-tête | Version du protocole, émetteur (commande ou sécurité), numéro de démarrage de l'émetteur, **numéro de transport** (32 bits, augmenté de 1 à chaque trame émise, retransmissions comprises), type de message, longueur |
| Charge utile | Selon le type ; valeurs entières à virgule fixe, sans flottant |
| Contrôle | CRC-32 sur l'en-tête et la charge utile |

Types de messages :

| Type | Sens | Contenu |
| --- | --- | --- |
| Signal de vie | Les deux | État de l'émetteur, dernier numéro de transport reçu de l'autre |
| Programmation | Commande vers sécurité | Programmation à confirmer (SW-REQ-013) |
| Écho | Sécurité vers commande | Copie de la programmation, à afficher au récapitulatif (SYS-REQ-044) |
| Demande de changement de mode | Commande vers sécurité | Pause, reprise, bolus, arrêt, avec l'identifiant de corrélation de l'action ; la sécurité ne l'admet que selon SYS-REQ-044 |
| État de supervision | Sécurité vers commande | Mode surveillé, volume mesuré, écart, autorisation moteur |
| Événement | Les deux | Événement d'audit dans son encodage canonique, avec son **identité stable** (flux et séquence d'audit). De la sécurité vers la commande : à enregistrer dans le flux de la sécurité. De la commande vers la sécurité : à chaîner par le témoin d'ancrage (ADR de #16) |
| Accusé d'événement | Les deux | Identité du dernier événement reçu sans trou, pour chaque flux ; pour le témoin, avec son verdict de chaînage |
| Ancrage | Les deux | Demande de retrait (commande vers sécurité) ; position et condensat du témoin pour le contrôle croisé (sécurité vers commande) |
| Test | Les deux | Séquence des tests de coupure et d'alarme au démarrage (SYS-REQ-046) |

### Règles

- **Signal de vie toutes les 50 ms** dans chaque sens. **Perte de lien** : aucune trame valide pendant 200 ms, une anomalie du numéro de transport, ou un changement du numéro de démarrage. Elle entraîne l'état sûr selon l'ADR 0003. Le délai laisse la marge nécessaire pour respecter 1 s de bout en bout (SYS-REQ-002).
- **Transport** : chaque trame, retransmission comprise, reçoit le numéro de transport suivant. Un numéro manquant, répété ou décroissant est une perte de lien, quel que soit le type du message : sur une liaison de carte, une trame perdue est un défaut, pas un aléa à rejouer.
- **Pas de retransmission** des messages de sécurité : après une perte de lien, l'état sûr est atteint, et une nouvelle programmation ou une nouvelle demande est nécessaire.
- **Événements d'audit : identité distincte du transport.** L'identité d'un événement est son flux et sa séquence d'audit (ADR de #16), inchangés d'une tentative à l'autre. L'émetteur conserve les événements non acquittés et les réémet, sous de nouveaux numéros de transport, à partir du premier non acquitté, après chaque rétablissement du lien.
  - **Doublon** de même contenu : ignoré, sans conséquence.
  - Même identité, **contenu différent** : refusé et signalé comme altération.
  - **Trou** dans les séquences d'audit d'un flux : les événements suivants restent en attente jusqu'à la réception de l'événement manquant. Si le trou persiste après le rétablissement du lien, la perte est consignée comme frontière du flux et signalée, jamais silencieuse ; sa conséquence pour l'appareil relève de [#46](https://github.com/camille-martin-paris/clepsydre/issues/46). Un trou d'audit n'entraîne pas à lui seul l'état sûr, qui découle déjà de la perte de lien.
- **Version du protocole** vérifiée au démarrage : un désaccord met la pompe hors service.
- **Définition unique** : les types et le format sont définis dans un seul module partagé par les deux logiciels et par le simulateur. Le simulateur injecte des pertes de trames de données et d'accusés, des doublons, des altérations, des retards et des réordonnancements ; chaque cas est vérifié pour le transport (perte de lien) et pour l'audit (aucune perte silencieuse, aucun doublon enregistré).

## 2. Horloge et datation

### Deux bases de temps distinctes

| Base | Usage | Source |
| --- | --- | --- |
| **Temps monotone** | Tous les délais de sécurité : confirmations d'alarme, pause, détections, signal de vie (SW-REQ-018) | Compteur matériel de chaque processeur, sur son propre oscillateur ; jamais réglé, jamais partagé entre les processeurs |
| **Heure civile** | Datation des événements pour les personnes (SYS-REQ-035, SYS-REQ-037) | Horloge temps réel du processeur de commande |

Les deux processeurs ne partagent **aucune** base de temps : une horloge fausse d'un côté est détectée par l'autre (ADR 0003, analyse de défaillance unique).

### Source de l'heure civile

- **Horloge temps réel à compensation de température**, à quelques ppm, pour une dérive très inférieure à 1 min par mois (environ 23 ppm). Elle est sauvegardée par sa propre réserve (pile ou supercondensateur, ADR de [#15](https://github.com/camille-martin-paris/clepsydre/issues/15)) et dispose d'un indicateur d'arrêt de l'oscillateur.
- **Pas de synchronisation par réseau** (H-08, SYS-REQ-040). L'heure est réglée par un **technicien authentifié** (SYS-REQ-023). Chaque mise à l'heure est un événement d'audit qui porte l'ancienne et la nouvelle heure (SYS-REQ-035).
- L'heure est enregistrée en **UTC** ; le fuseau et l'heure d'été ne servent qu'à l'affichage.

### Heure indisponible ou incohérente

- **Indisponible** : indicateur d'arrêt de l'oscillateur levé, réserve épuisée, ou horloge illisible. Les événements portent alors l'indication explicite « heure indisponible », jamais une heure inventée ; ils restent ordonnés par leur numéro de séquence et leur temps monotone depuis le démarrage.
- **Incohérente** : heure antérieure au dernier événement journalisé, ou saut détecté. Toutes les 10 s, l'avance de l'heure civile est comparée à celle du temps monotone ; un écart de plus de 2 s est un **saut**, journalisé comme événement, et les événements suivants portent l'indication « heure douteuse » jusqu'à une mise à l'heure.
- **Effet sur la thérapie** : aucun. La perfusion ne dépend que du temps monotone. Une heure indisponible ou douteuse déclenche un signal d'information et une demande de mise à l'heure en maintenance, jamais une alarme qui interromprait la perfusion (RISK-013).

## 3. Identité des flux d'événements

- **Flux** : `pump-<numéro de série>:<producteur>:boot-<n>`, avec deux producteurs, `commande` et `securite`.
- **Numéro de démarrage** : compteur persistant propre à chaque processeur, incrémenté et **écrit durablement avant tout événement** du démarrage. S'il est illisible ou corrompu, le processeur met la pompe hors service (SW-REQ-010) : réutiliser un numéro rendrait deux flux indiscernables.
- **Numéro de séquence** : attribué par flux, strictement croissant (SYS-REQ-035, SW-REQ-009), par la bibliothèque de journalisation (ADR de #16).
- **Corrélation** : `pump-<série>:<producteur>:boot-<n>:input-<m>` relie une action du soignant à ses effets, selon le vocabulaire de [#42](https://github.com/camille-martin-paris/clepsydre/issues/42). L'identifiant contient **l'origine** : le producteur qui a créé l'action et **son** numéro de démarrage.
  - **La commande crée l'identité des actions du soignant** qu'elle lit dans l'interface. Elle la transmet inchangée dans les messages « Programmation » et « Demande de changement de mode ».
  - Le processeur de sécurité enregistre les effets de ces actions sous l'identifiant reçu. Quand il observe lui-même l'appui qui confirme une demande en attente (SYS-REQ-044), il emploie l'identifiant de cette demande.
  - Un appui observé par la sécurité sans demande en attente (Arrêt, par exemple) reçoit un identifiant d'origine `securite`.
  - Un redémarrage d'un seul processeur ne change donc l'identité d'aucune action déjà créée. Deux actions ne peuvent pas partager un identifiant, puisque l'origine et le démarrage y figurent. Un test de corrélation après le redémarrage d'un seul processeur est prévu dans le simulateur.
- **Numéro de série** : programmé à la fabrication en mémoire non volatile protégée ; il est le même pour les deux processeurs.

## Décision proposée

**Cette décision revient d'abord à l'auteur, puis à l'acceptation de l'approbatrice** ; l'ADR reste au statut « proposée » jusque-là. Elle porte sur les trois points ci-dessus :

1. liaison UART point à point, avec trames COBS et CRC-32 ;
2. deux bases de temps distinctes, et une heure civile TCXO réglée par un technicien ;
3. flux identifiés par producteur et par démarrage, avec compteur de démarrage persistant.

## Conséquences

- **Exigences**, à préciser dans une révision ultérieure de la spécification :
  - période du signal de vie et délai de perte de lien ;
  - absence de retransmission des messages de sécurité ;
  - vérification de la version du protocole ;
  - heure en UTC ;
  - indications « heure indisponible » et « heure douteuse » ;
  - seuil de saut d'horloge ;
  - compteur de démarrage persistant.
- **Analyse des dangers** : RISK-011 gagne trois causes, maîtrisées comme ci-dessus : l'heure indisponible ou fausse, un numéro de démarrage réutilisé, un événement perdu ou dupliqué sur la liaison.
- **Électronique** ([#57](https://github.com/camille-martin-paris/clepsydre/issues/57), [#59](https://github.com/camille-martin-paris/clepsydre/issues/59)) : UART entre les processeurs ; horloge temps réel compensée en température avec sa réserve ; mémoire non volatile pour les compteurs de démarrage et le numéro de série.
- **Logiciel** ([#24](https://github.com/camille-martin-paris/clepsydre/issues/24), [#29](https://github.com/camille-martin-paris/clepsydre/issues/29), [#49](https://github.com/camille-martin-paris/clepsydre/issues/49)) : module de protocole partagé ; datation selon les règles ci-dessus.
- **Simulateur** ([#21](https://github.com/camille-martin-paris/clepsydre/issues/21)) : liaison simulée avec injection de défauts, horloges simulées indépendantes, avec dérive et saut.

## Limites

- Les périodes (50 ms, 200 ms, 10 s, 2 s) sont des propositions, sans analyse temporelle.
- L'exactitude de l'horloge dépend du composant choisi et de la température ; elle sera mesurée.

## Références

- [ADR 0003](0003-architecture-de-calcul-et-canal-de-securite.md)
- [Spécification des exigences](../requirements/requirements-specification.md)
- [Usage prévu](../intended-use.md), hypothèse H-08
