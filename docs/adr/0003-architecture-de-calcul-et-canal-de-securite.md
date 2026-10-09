# ADR 0003 : architecture de calcul et canal de sécurité indépendant

| Champ | Valeur |
| --- | --- |
| Statut | Proposée, soumise à la décision de la mainteneuse |
| Date | 2026-10-09 |
| Issue | [#14](https://github.com/camille-martin-paris/clepsydre/issues/14) |
| Épique | [#12](https://github.com/camille-martin-paris/clepsydre/issues/12) |
| Dépend de | [ADR 0002](0002-principe-de-pompage.md) (pousse-seringue, proposée) |

> [!CAUTION]
> Clepsydre ne doit pas être utilisé sur un être humain ni sur un animal. Cette ADR fixe un principe d'architecture ; l'indépendance et l'efficacité du canal de sécurité restent à démontrer par l'analyse détaillée et par les essais.

## Contexte

L'analyse préliminaire des dangers transmet à cette ADR trois mesures structurantes ([constat 2](../requirements/preliminary-hazard-analysis.md#constats)) :

- **CTRL-001** : supervision du volume délivré par un canal de sécurité distinct du canal de commande (SYS-REQ-001, SYS-REQ-002) ;
- **CTRL-014** : chien de garde indépendant et autotests (SYS-REQ-017) ;
- **CTRL-016** : alarme technique de repli, indépendante du processeur principal (SYS-REQ-020).

Le logiciel est de [classe C](../../software_development_file/safety-classification.md) ; une classe inférieure ne peut être attribuée à un élément qu'avec une ségrégation documentée et vérifiée.

L'issue pose trois questions :

1. la séparation entre la commande temps réel et l'application (interface, journal, communication) ;
2. un superviseur capable d'arrêter le moteur sur une **défaillance unique** de la commande ;
3. le comportement sûr en cas de blocage ou de redémarrage de chaque processeur.

Cette ADR suppose le pousse-seringue proposé par l'[ADR 0002](0002-principe-de-pompage.md). Si une autre option était retenue, la mesure indépendante du volume (position du piston) serait à revoir ; le reste de l'architecture resterait valable.

## Critères

1. **Défaillance unique** : aucune défaillance unique de la commande, matérielle ou logicielle, ne doit conduire à une sur-perfusion ou à un arrêt non signalé sans que la pompe passe en état sûr dans le délai de SYS-REQ-002.
2. **Indépendance** du canal de sécurité : processeur, horloge, capteur de mesure et chemin de coupure distincts de ceux de la commande.
3. **Défaillances latentes** : une défaillance du canal de sécurité qui ne se manifeste pas d'elle-même doit être détectée avant la perfusion suivante.
4. **Simplicité** du canal de sécurité, pour qu'il puisse être vérifié à fond.
5. **Reproductibilité** open hardware, coût, et cohérence avec le simulateur ([#21](https://github.com/camille-martin-paris/clepsydre/issues/21)) : le logiciel de chaque processeur doit pouvoir s'exécuter sur l'hôte face au modèle physique.

## Options évaluées

| Option | Description | Évaluation |
| --- | --- | --- |
| A. Un microcontrôleur et un chien de garde externe | Commande, interface, journal et surveillance sur un même processeur | **Écartée** : pas de canal indépendant ; une erreur logicielle de calcul du débit n'est vue par aucun autre élément (critère 1, CTRL-001) |
| B. Microcontrôleur de commande et microcontrôleur de sécurité | Le premier assure commande, interface et journal ; le second, plus simple, mesure le volume par un capteur propre, peut couper le moteur et porte l'alarme de repli | **Proposée** |
| C. Processeur d'application, microcontrôleur de commande et microcontrôleur de sécurité | Comme B, avec l'interface, le journal et la communication sur un processeur d'application sous Linux | **Écartée pour le prototype** : l'interface affiche et saisit des valeurs de sécurité et reste de classe C ; Linux devient un SOUP volumineux dans un élément de classe C, sans gain de classe ; démarrage, consommation et coût plus élevés |
| D. Microcontrôleur de sûreté à double cœur en lockstep | Un processeur dont deux cœurs exécutent le même code et se comparent | **Écartée comme mesure unique** : détecte les défaillances aléatoires du cœur, mais pas une erreur de spécification ou de logiciel, commune aux deux cœurs, ni la défaillance d'un capteur ; outils souvent propriétaires. Utilisable plus tard comme processeur de commande, en complément de B |
| E. Superviseur en logique matérielle (CPLD ou FPGA) | Comparaison câblée de la position du piston à une consigne | **Écartée** : le calcul du volume attendu pour des programmations variables (modes, bolus, pauses) est complexe en logique câblée et introduit une discipline de conception supplémentaire |

## Décision proposée

**Option B : deux microcontrôleurs, commande et sécurité.** **Cette décision revient à la mainteneuse** ; l'ADR reste au statut « proposée » jusqu'à son acceptation explicite.

```mermaid
flowchart LR
    subgraph CMD["Processeur de commande"]
        SM[Machine à états]
        CTL[Asservissement]
        IHM[Interface]
        LOG[Journal]
    end
    subgraph SEC["Processeur de sécurité"]
        SUP[Supervision du volume]
        RAL[Alarme de repli]
    end
    CMD <-->|liaison avec CRC, numéro de séquence et délai| SEC
    CTL -->|pas, sens| DRV[Pilote moteur]
    CMD -->|autorisation statique| AND{{ET}}
    SEC -->|autorisation dynamique| AND
    AND -->|validation| DRV
    DRV --> MOT[Moteur et vis irréversible]
    MOT --> PIS[Piston]
    PIS -->|force, codeur moteur| CMD
    PIS -->|position par capteur linéaire propre| SEC
    WD1[Chien de garde externe] -.-> CMD
    WD2[Chien de garde externe] -.-> SEC
    SEC --> BUZ[Avertisseur de repli]
```

### Processeur de commande

- Il exécute la machine à états (SW-REQ-001), l'asservissement du moteur, la détection d'occlusion par la force, l'interface, le journal et les autotests.
- **Séparation entre temps réel et application** (question 1) : sur le même processeur, la commande et la surveillance s'exécutent dans des tâches de plus haute priorité que l'interface et le journal. L'unité de protection mémoire, si la cible en dispose, isole les données de commande. Cette séparation ne sert pas à abaisser la classe : l'interface reste de classe C, puisqu'elle affiche et saisit la programmation (RISK-007, RISK-008).
- Il a son propre chien de garde externe à fenêtre, rafraîchi selon SW-REQ-012.

### Processeur de sécurité

- **Mesure propre** : il lit un capteur linéaire de position du piston, distinct du codeur ou du comptage de pas du moteur, et calcule le volume délivré avec la section de la seringue identifiée ([ADR 0002](0002-principe-de-pompage.md#exigences-à-réviser-spécification-révision-b)).
- **Programmation** : à la confirmation du récapitulatif, le processeur de commande lui transmet la programmation (SW-REQ-013). Le processeur de sécurité la renvoie, et c'est **cette copie renvoyée** que l'écran de récapitulatif affiche. Le soignant confirme donc ce que le canal de sécurité surveillera.
- **Supervision** : il compare en continu le volume mesuré au volume attendu (SYS-REQ-001), et la vitesse du piston à la borne du mode actif sur une fenêtre courte, pour détecter un emballement avant que l'écart cumulé n'atteigne le seuil.
- **Coupure** : il commande la validation du pilote moteur par un **signal dynamique**, un créneau que le circuit de validation transforme en niveau. Un processeur bloqué, réinitialisé ou hors tension cesse de produire le créneau et coupe le moteur. La validation exige aussi le signal du processeur de commande (fonction ET matérielle).
- **Alarme de repli** (CTRL-016) : il commande un avertisseur sonore distinct de celui de l'interface, alimenté par une réserve d'énergie propre (à dimensionner avec l'ADR [#15](https://github.com/camille-martin-paris/clepsydre/issues/15)).
- Il a son propre oscillateur et son propre chien de garde externe. Son logiciel est volontairement réduit : pas d'interface, pas de journal complet ; il transmet ses événements au processeur de commande pour journalisation.

### Liaison entre les processeurs

Chaque message porte un code de détection d'erreur, un numéro de séquence et l'identité de l'émetteur ; chaque processeur surveille la réception périodique de l'autre. Un message erroné, manquant ou hors délai est une perte de lien. Le délai de détection doit permettre l'état sûr en 1 s au plus (SYS-REQ-002). Le protocole est défini par l'ADR [#17](https://github.com/camille-martin-paris/clepsydre/issues/17).

### État sûr

État sûr au sens de SYS-REQ-002 : **pilote moteur non validé**, piston retenu (vis **irréversible**, de type trapézoïdal, qui ne peut pas être entraînée par le piston), alarme de priorité haute. L'état par défaut de chaque ligne de validation, processeur réinitialisé ou hors tension, est « non validé » (rappel au niveau inactif).

### Blocage et redémarrage (question 3)

| Événement | Réaction |
| --- | --- |
| Blocage du processeur de commande | Son chien de garde le réinitialise ; sa ligne de validation retombe ; le processeur de sécurité constate la perte de lien et déclenche l'alarme de repli. Au redémarrage, la perfusion ne reprend pas d'elle-même (SYS-REQ-038). |
| Blocage du processeur de sécurité | Le créneau cesse : le moteur est coupé. Le processeur de commande constate la perte de lien, passe en état sûr et alarme. Son chien de garde réinitialise le processeur de sécurité. |
| Redémarrage d'un des deux processeurs | L'autre le constate par la perte de lien, puis par un numéro de démarrage différent. La perfusion ne reprend qu'après une nouvelle confirmation du soignant et une nouvelle transmission de la programmation au processeur de sécurité. |
| Perte totale d'alimentation | Les deux validations retombent ; l'alarme de repli retentit sur sa réserve d'énergie (SYS-REQ-020). |

## Analyse de défaillance unique

Analyse qualitative, au niveau de l'architecture ; elle sera détaillée par une AMDEC avec la conception électronique ([#57](https://github.com/camille-martin-paris/clepsydre/issues/57), [#59](https://github.com/camille-martin-paris/clepsydre/issues/59)).

| Défaillance unique | Effet sans mesure | Détection | Réaction | Reste à démontrer |
| --- | --- | --- | --- | --- |
| Erreur logicielle de la commande : débit calculé faux | Sur- ou sous-perfusion (RISK-001, RISK-002) | Processeur de sécurité : écart entre volume mesuré et attendu, vitesse hors borne | Coupure, alarme | Seuils et délais (SYS-REQ-001) |
| Programmation altérée par la commande avant transmission | Le canal de sécurité surveille une mauvaise consigne | Copie renvoyée affichée au récapitulatif et confirmée par le soignant | Pas de démarrage sans confirmation | Effet d'une altération après la confirmation : couverte par le numéro de séquence et le code de détection |
| Blocage ou emballement du processeur de commande | Moteur incontrôlé ou arrêté (RISK-012) | Chien de garde ; perte de lien ; supervision du volume | Validation retombée, coupure, alarme de repli | Délai de bout en bout ≤ 1 s |
| Horloge du processeur de commande fausse | Débit proportionnellement faux | Processeur de sécurité, sur son oscillateur propre : écart de volume ; dérive des périodes de la liaison | Coupure, alarme | Tolérance des oscillateurs |
| Pilote moteur défaillant : pas en trop, sens inversé, blocage | Sur-perfusion, aspiration ou arrêt | Supervision du volume et de la vitesse ; codeur moteur pour la commande | Coupure de la validation, alarme | Que la coupure de la validation arrête effectivement ce pilote, y compris en défaut |
| Validation du pilote collée à l'état actif | Aucune tant que rien d'autre ne défaille (défaillance latente) | Test de la coupure à chaque démarrage : chaque processeur retire sa validation et vérifie que le moteur ne tourne pas | Pompe hors service | Couverture du test |
| Capteur de force défaillant | Occlusion non détectée (RISK-005) | Plausibilité du capteur ; un piston bloqué fait perdre des pas au moteur, et le processeur de sécurité voit l'écart de volume | Alarme | Délai d'occlusion à bas débit par cette voie de secours |
| Capteur de position du canal de sécurité défaillant | Fausse alarme, ou surveillance aveugle | Plausibilité croisée avec le codeur moteur transmis par la commande ; valeur figée ou hors plage | Alarme (sens sûr) | Mode de défaillance « dérive lente » du capteur retenu |
| Processeur de sécurité défaillant sans arrêt | Surveillance perdue (latente) | Perte de lien ; autotests ; test de coupure au démarrage | État sûr, alarme | — |
| Avertisseur principal muet | Alarme non perçue (RISK-014) | Test des signaux d'alarme au démarrage (SW-REQ-010) | Pompe hors service | Détection en cours de perfusion |
| Avertisseur de repli muet | Défaillance latente | Test au démarrage | Pompe hors service | — |
| Mémoire altérée | Comportement imprévisible | Autotests au démarrage et périodiques (SW-REQ-010, SW-REQ-011), mémoire à correction d'erreur si la cible le permet | État sûr | Couverture de diagnostic (SW-REQ-011) |
| Écran figé ou affichant une valeur fausse | Soignant mal informé (RISK-007, RISK-008) | **Non couverte par cette architecture** | — | À traiter avec l'interface et l'électronique ([#36](https://github.com/camille-martin-paris/clepsydre/issues/36), [#57](https://github.com/camille-martin-paris/clepsydre/issues/57)), par exemple par relecture de l'affichage ou indicateur d'activité |
| Rupture d'alimentation | Arrêt (RISK-010) | Surveillance de l'alimentation | État sûr par défaut, alarme de repli | Réserve d'énergie (#15) |

**Emballement avant détection** : entre le début d'un emballement et la coupure, le moteur peut délivrer un volume. La **vitesse mécanique maximale** du mécanisme doit donc être bornée par conception (rapport de réduction, fréquence de pas limitée par le matériel), de façon que ce volume reste sous une limite à fixer dans la révision des exigences.

**Défaillances doubles** : les défaillances latentes du canal de sécurité (validation collée, avertisseur de repli muet, capteur de position dérivant) sont recherchées à chaque démarrage. Une défaillance double n'est donc pas couverte au cours d'une même perfusion ; la durée maximale d'une perfusion sans nouveau test est à fixer avec l'analyse détaillée.

## Conséquences

### Classification

- Les deux logiciels embarqués sont de **classe C**. Le canal de sécurité est une mesure de maîtrise des risques ; son indépendance n'abaisse la classe d'aucun élément tant qu'elle n'est pas démontrée (vérification, AMDEC).
- Après démonstration, la classe du logiciel de commande pourra être réexaminée selon l'IEC 62304 §4.3 ; le logiciel de sécurité restera de classe C.

### Exigences et analyse des dangers

À traiter après acceptation, dans des pull requests distinctes :

- **SYS-REQ-001** : préciser la mesure (position du piston par un capteur propre) et ajouter la supervision de la vitesse sur une fenêtre courte ;
- nouvelles exigences : **vitesse mécanique maximale bornée par conception** ; **coupure par signal dynamique et fonction ET** ; **état par défaut non validé** ; **test de la coupure et de l'avertisseur de repli à chaque démarrage** ; **récapitulatif affichant la copie renvoyée par le canal de sécurité** ;
- **SW-REQ-013** : préciser l'écho de la programmation ;
- exigences logicielles propres au processeur de sécurité, annoncées par la spécification ;
- analyse des dangers : causes et mesures ci-dessus ; la défaillance de l'affichage reste ouverte.

### Conception

- **Électronique** ([#57](https://github.com/camille-martin-paris/clepsydre/issues/57), [#59](https://github.com/camille-martin-paris/clepsydre/issues/59), [#60](https://github.com/camille-martin-paris/clepsydre/issues/60)) : deux microcontrôleurs, chacun avec son oscillateur et son chien de garde externe ; circuit de validation dynamique et fonction ET ; avertisseur de repli avec sa réserve. La diversité des deux microcontrôleurs (familles ou fabricants différents) réduit les défaillances de cause commune ; elle est recommandée et sera tranchée avec #59.
- **Mécanique** ([#52](https://github.com/camille-martin-paris/clepsydre/issues/52)) : vis irréversible ; capteur linéaire de position du piston pour le canal de sécurité.
- **Logiciel** ([#24](https://github.com/camille-martin-paris/clepsydre/issues/24), [#29](https://github.com/camille-martin-paris/clepsydre/issues/29)) : deux exécutables, chacun avec une couche d'abstraction du matériel, pour s'exécuter sur la cible et sur l'hôte.
- **Simulateur** ([#21](https://github.com/camille-martin-paris/clepsydre/issues/21)) : exécute les deux logiciels face au modèle, avec la liaison simulée ; défauts injectables : blocage et redémarrage de chaque processeur, perte ou altération de messages, dérive d'horloge, défaillance du pilote et des capteurs.
- **Interfaces** ([#17](https://github.com/camille-martin-paris/clepsydre/issues/17)) : protocole de la liaison et délais.

## Limites

- Analyse qualitative ; aucune probabilité ni couverture de diagnostic n'est encore estimée.
- Les délais (détection, coupure, liaison) et le volume maximal avant coupure sont à fixer et à mesurer.
- Le choix des composants n'est pas fait ici.

## Références

- [Analyse préliminaire des dangers](../requirements/preliminary-hazard-analysis.md), constat 2
- [Spécification des exigences](../requirements/requirements-specification.md)
- [Classification de sécurité du logiciel](../../software_development_file/safety-classification.md)
- IEC 62304:2006+A1:2015, §4.3 et §5.3
- IEC 60601-1:2005+A1:2012+A2:2020, notion de condition de premier défaut
