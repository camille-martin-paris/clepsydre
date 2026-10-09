# ADR 0002 : principe de pompage

| Champ | Valeur |
| --- | --- |
| Statut | Proposée, soumise à la décision de la mainteneuse |
| Date | 2026-10-09 |
| Issue | [#13](https://github.com/camille-martin-paris/clepsydre/issues/13) |
| Épique | [#12](https://github.com/camille-martin-paris/clepsydre/issues/12) |

> [!CAUTION]
> Clepsydre ne doit pas être utilisé sur un être humain ni sur un animal. Les ordres de grandeur cités ci-dessous sont des estimations de conception, pas des résultats d'essai ; ils seront mesurés sur banc ([#73](https://github.com/camille-martin-paris/clepsydre/issues/73), [#74](https://github.com/camille-martin-paris/clepsydre/issues/74)).

## Contexte

Le principe de pompage détermine le consommable, le mécanisme, les capteurs et une partie des exigences. L'[usage prévu](../intended-use.md) le laisse ouvert, et plusieurs éléments en dépendent :

- **Exigences** : la plage de débit (SYS-REQ-030 : 0,1 à 999 mL/h en volumétrique, 0,1 à 200 mL/h en pousse-seringue), le volume à perfuser (SYS-REQ-031 : jusqu'à 9 999 mL), la détection d'air (SYS-REQ-007), le délai d'alarme d'occlusion (SYS-REQ-008) et le bolus post-occlusion (SYS-REQ-009) ; voir la [spécification](../requirements/requirements-specification.md#valeurs-proposées).
- **Mesures de maîtrise** : l'analyse préliminaire des dangers transmet à cette ADR CTRL-004 (anti-écoulement libre) et CTRL-008 (décompression après occlusion) ([constat 3](../requirements/preliminary-hazard-analysis.md#constats)).
- **Épiques** : mécanique ([#51](https://github.com/camille-martin-paris/clepsydre/issues/51)), électronique ([#56](https://github.com/camille-martin-paris/clepsydre/issues/56)), simulateur ([#21](https://github.com/camille-martin-paris/clepsydre/issues/21)) et bancs.

Le projet est open hardware : le prototype doit pouvoir être reproduit avec des pièces et des consommables qu'on trouve dans le commerce, sans outillage industriel.

## Critères

Critères de l'issue, complétés par l'usage prévu et l'analyse des dangers :

1. **Précision et stabilité du débit** sur la plage visée (H-02 à H-04), y compris au démarrage et à bas débit.
2. **Consommables** standards, disponibles auprès de plusieurs fabricants et utilisables sans modification.
3. **Complexité mécanique, coût et reproductibilité** en open hardware.
4. **Écoulement libre et occlusion** : prévention de l'écoulement libre (RISK-003), détection d'occlusion (RISK-005) et bolus à sa levée (RISK-006).
5. **Détection d'air** (RISK-004).
6. **Couverture de l'usage prévu** : plage de débit et volumes à perfuser.

## Options évaluées

### A. Pousse-seringue

Une vis entraînée par un moteur pas à pas pousse le piston d'une seringue standard (Luer lock, 5 à 60 mL environ) tenue dans un berceau.

- **Précision** : le volume délivré est le produit de la course du piston par la section de la seringue. La précision dépend surtout de la mécanique (pas de vis, jeu, résolution du moteur) et de la tolérance du diamètre de la seringue. Elle est bonne à bas débit, ce qui explique l'usage du pousse-seringue pour les médicaments puissants. Point faible connu : le **retard au démarrage**, dû au rattrapage des jeux et à la mise en mouvement du joint du piston, est d'autant plus long que le débit est faible.
- **Consommables** : seringues et prolongateurs standards, de nombreux fabricants. Chaque modèle de seringue a un diamètre propre : la pompe doit connaître le modèle installé, sinon l'erreur de débit est proportionnelle à l'écart de section.
- **Mécanique** : la plus simple des options. Les pièces sont courantes (vis trapézoïdale ou à billes, guidage linéaire, moteur pas à pas), à la portée d'un atelier équipé d'une imprimante 3D et de quelques usinages simples.
- **Écoulement libre** : le liquide n'avance que si le piston avance. Le danger est le **siphonage** : piston non retenu par la pompe, seringue placée au-dessus du patient. La mesure consiste à détecter que la pompe retient le piston et que la seringue est fixée dans son berceau.
- **Occlusion** : une jauge de force sur le pousseur mesure la force appliquée au piston, d'où l'on déduit la pression. À force égale, la pression baisse quand la section augmente : la sensibilité dépend du modèle de seringue et le délai d'alarme à bas débit s'allonge avec la compliance de la seringue et de la ligne. La **décompression** (CTRL-008) est simple : le pousseur recule d'une course calculée après la détection.
- **Air** : la seringue est remplie et purgée par le soignant. Les pousse-seringues courants n'ont souvent pas de détecteur d'air en ligne. Un détecteur à ultrasons sur le prolongateur reste possible, mais il ajoute un consommable à placer et ses fausses alarmes.
- **Usage prévu** : la plage de débit de H-02 (0,1 à 200 mL/h) est couverte. Le volume par seringue est limité à 60 mL environ : une perfusion de grand volume n'est pas couverte, et SYS-REQ-031 (jusqu'à 9 999 mL) ne l'est pas.

### B. Pompe volumétrique péristaltique linéaire, tubulure standard

Des doigts écrasent successivement une tubulure souple standard (set de perfusion par gravité) pour pousser le liquide.

- **Précision** : le volume par cycle dépend du diamètre intérieur, de l'épaisseur et de l'élasticité de la tubulure. Ces caractéristiques varient d'un fabricant et d'un lot à l'autre et changent avec l'écrasement au fil des heures. Tenir ± 5 % (H-04) avec des tubulures quelconques est improbable sans étalonnage par référence de tubulure.
- **Consommables** : sets par gravité courants, mais la précision impose en pratique de restreindre et d'étalonner les références admises.
- **Mécanique** : plus complexe : came et doigts, porte avec contre-plaque, réglage de l'écrasement.
- **Écoulement libre** : la tubulure n'est fermée que par les doigts. À l'ouverture de la porte, il faut un clamp automatique qui pince la tubulure avant toute ouverture et à chaque retrait. C'est la mesure CTRL-004, et ce mécanisme doit être fiable sur une défaillance unique.
- **Occlusion** : des capteurs de pression en amont et en aval mesurent la déformation de la tubulure. Leur réponse dépend de la tubulure.
- **Air** : un détecteur à ultrasons sur la tubulure, intégré à la porte, est l'usage courant (SYS-REQ-007).
- **Usage prévu** : plage de débit et volumes couverts (H-02, H-05).

### C. Pompe volumétrique à consommable dédié (cassette ou segment calibré)

Une cassette à piston ou un segment de tubulure calibré, propre à la pompe, fixe le volume par cycle.

- **Précision** : bonne, fixée par la géométrie du consommable.
- **Consommables** : à concevoir, fabriquer, stériliser et faire certifier par le projet. C'est hors de portée d'un projet ouvert et contraire au critère 2 : sans fabricant de consommables, le prototype ne peut pas être reproduit.
- **Écoulement libre, occlusion, air** : traités dans la cassette, comme le font les pompes du marché.

### D. Pompe péristaltique rotative

Des galets écrasent une tubulure contre une piste circulaire. Cette option est **écartée d'emblée** : le débit est pulsé, la précision dépend fortement de la tubulure et le principe est réservé, dans les pompes médicales, à la nutrition entérale, exclue de l'usage prévu.

## Synthèse

| Critère | A. Pousse-seringue | B. Péristaltique linéaire, tubulure standard | C. Consommable dédié |
| --- | --- | --- | --- |
| 1. Précision et stabilité | Bonne, retard au démarrage à bas débit | Incertaine, liée à la tubulure | Bonne |
| 2. Consommables standards | Oui, modèle de seringue à identifier | Oui, références à restreindre et étalonner | Non |
| 3. Complexité et reproductibilité | Faible | Moyenne à élevée | Élevée, consommable à produire |
| 4. Écoulement libre et occlusion | Siphonage à prévenir ; décompression simple ; sensibilité dépendant de la seringue | Clamp automatique indispensable ; capteurs de pression sur tubulure | Traités par le consommable |
| 5. Air | Pas de détecteur dans la conception courante ; option sur le prolongateur | Détecteur courant | Traité par le consommable |
| 6. Usage prévu | Débits couverts ; volumes limités à une seringue | Couvert | Couvert |

## Décision proposée

**Option A, pousse-seringue**, pour le prototype de Clepsydre. **Cette décision revient à la mainteneuse** : l'ADR reste au statut « proposée » jusqu'à son acceptation explicite.

Raisons :

- c'est la seule option qui satisfait à la fois les critères 2 et 3 : consommables standards et mécanique reproductible ;
- la précision dépend de la mécanique, que le projet maîtrise et peut mesurer sur banc, plutôt que d'une tubulure, qu'il ne maîtrise pas ;
- l'écoulement libre et le bolus post-occlusion se traitent par des mesures simples : retenue du piston, décompression par recul ;
- l'option B reste possible pour une version ultérieure : elle demanderait une nouvelle ADR, un clamp automatique et un protocole d'étalonnage des tubulures.

Options écartées : **B**, car la précision visée (H-04) n'est pas atteignable sans restreindre et étalonner les tubulures, et parce que le clamp automatique ajoute une mesure de sécurité mécanique complexe ; **C**, car le consommable ne peut pas être produit par le projet ; **D**, pour les raisons données plus haut.

## Conséquences

### Périmètre de l'usage prévu

- **Le périmètre se restreint aux perfusions de petit volume**, contenues dans une seringue de 60 mL au plus : médicaments à débit faible ou moyen. Les perfusions de grand volume (hydratation, poches de 250 mL et plus) sortent du périmètre initial. L'[usage prévu](../intended-use.md) doit être révisé en conséquence, de même que H-02 (0,1 à 200 mL/h seulement) et H-05.
- La spécification d'utilisation remplace « seringue ou cassette » par « seringue » et le scénario S6 (« nouvelle poche ou nouvelle seringue ») par le changement de seringue.

### Exigences à réviser (spécification, révision B)

| Exigence | Révision proposée |
| --- | --- |
| SYS-REQ-030 | Plage 0,1 à 200 mL/h seulement ; débit maximal limité en outre par le modèle de seringue |
| SYS-REQ-031 | Volume à perfuser limité au volume utile de la seringue installée |
| SYS-REQ-032 | Préalarme et fin de perfusion déclenchées aussi par la position du piston (seringue presque vide, seringue vide) |
| SYS-REQ-005, SYS-REQ-006 | « Porte » remplacée par la fixation de la seringue dans le berceau et la retenue du piston ; écoulement libre par siphonage |
| SYS-REQ-007 | Détection d'air à réexaminer dans l'analyse des dangers : aucun relâchement sans justification de la maîtrise de RISK-004 (purge, détecteur sur le prolongateur) |
| SYS-REQ-008 | Délais d'occlusion à fixer par modèle de seringue ; ils dépendent de la section et de la compliance |
| SYS-REQ-009 | Décompression par recul du pousseur |

Nouvelles exigences à prévoir :

- **identification de la seringue** : modèle choisi par le soignant dans une liste de seringues vérifiées, contrôlé par une mesure du diamètre du corps ; un désaccord bloque le démarrage ;
- **liste des seringues vérifiées** : configurée par le technicien biomédical (P3), comme la bibliothèque de médicaments ([#37](https://github.com/camille-martin-paris/clepsydre/issues/37)) ;
- **retard au démarrage** : rattrapage des jeux avant la perfusion, avec un délai maximal au premier écoulement à fixer et à mesurer sur banc ;
- **position du piston** : mesurée indépendamment du comptage des pas du moteur, ce qui offre au canal de sécurité ([#14](https://github.com/camille-martin-paris/clepsydre/issues/14)) une mesure du volume délivré (CTRL-001).

### Analyse des dangers (révision C)

Nouvelles causes à analyser :

- seringue mal identifiée ou de diamètre hors tolérance : erreur de débit (RISK-001, RISK-002) ;
- siphonage : piston non retenu, seringue au-dessus du patient (RISK-003) ;
- retard au démarrage à bas débit : thérapie différée (RISK-002) ;
- compliance de la seringue : délai d'occlusion long à bas débit (RISK-005) et bolus à la levée (RISK-006) ;
- air : maîtrise par la purge seule, ou ajout d'un détecteur (RISK-004).

CTRL-004 devient « retenue du piston et fixation de la seringue, détectées », et CTRL-008 « recul du pousseur après détection d'occlusion ».

### Conception

- **Mécanique** ([#52](https://github.com/camille-martin-paris/clepsydre/issues/52), [#53](https://github.com/camille-martin-paris/clepsydre/issues/53)) : vis, guidage, pousseur avec capture du piston, berceau et bride de la seringue.
- **Capteurs** ([#58](https://github.com/camille-martin-paris/clepsydre/issues/58)) : force sur le pousseur, position du piston, diamètre de la seringue, présence de la seringue et capture du piston, et éventuellement air sur le prolongateur.
- **Commande** ([#26](https://github.com/camille-martin-paris/clepsydre/issues/26), [#27](https://github.com/camille-martin-paris/clepsydre/issues/27), [#28](https://github.com/camille-martin-paris/clepsydre/issues/28)) : asservissement du pas, rattrapage des jeux, occlusion par la force, décompression.
- **Simulateur** ([#21](https://github.com/camille-martin-paris/clepsydre/issues/21)) : modèle d'un pousse-seringue avec seringue compliante, jeu mécanique et ligne ; défauts injectables : occlusion, piston libéré, mauvaise seringue.
- **Bancs** ([#73](https://github.com/camille-martin-paris/clepsydre/issues/73), [#74](https://github.com/camille-martin-paris/clepsydre/issues/74)) : précision et délai au démarrage pour chaque seringue de la liste ; délai d'occlusion et bolus pour chaque seringue.

Ces révisions suivent l'acceptation de cette ADR, dans des pull requests distinctes ; elles ne sont pas faites ici.

## Limites

- Cette analyse s'appuie sur les principes connus des pompes du marché, sans essai du projet et sans accès au texte intégral de l'IEC 60601-2-24.
- Les ordres de grandeur (volume des seringues, comportement à bas débit) sont à confirmer par la conception et les bancs.

## Références

- [Usage prévu](../intended-use.md), hypothèses H-02 à H-05
- [Spécification d'utilisation](../requirements/use-specification.md)
- [Analyse préliminaire des dangers](../requirements/preliminary-hazard-analysis.md), constat 3
- [Spécification des exigences](../requirements/requirements-specification.md)
- IEC 60601-2-24:2012, exigences particulières pour les pompes et régulateurs de perfusion
