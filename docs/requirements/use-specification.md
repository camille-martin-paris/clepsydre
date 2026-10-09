# Spécification d'utilisation

| Champ | Valeur |
| --- | --- |
| Statut | Version initiale, soumise à relecture |
| Révision | B |
| Issue | [#8](https://github.com/camille-martin-paris/clepsydre/issues/8) |
| Références | [Usage prévu](../intended-use.md) ; IEC 62366-1:2015+A1:2020, §5.1 (spécification d'utilisation) |

> [!CAUTION]
> Clepsydre ne doit pas être utilisé sur un être humain ni sur un animal. Ce document décrit l'utilisation **visée** afin d'orienter la conception ; il ne décrit pas un produit disponible.

## Statut des besoins

Aucun entretien ni aucune observation d'utilisateurs n'a encore été mené. Les profils, besoins et scénarios ci-dessous sont **déduits** de l'usage prévu, de la pratique décrite par des sources publiques et des problèmes connus des pompes à perfusion [FDA 2010]. Chaque besoin porte le statut « à valider ». Sa validation auprès d'utilisateurs représentatifs est prévue par l'évaluation formative de l'aptitude à l'utilisation ([#40](https://github.com/camille-martin-paris/clepsydre/issues/40)). Un besoin validé, révisé ou rejeté donne lieu à une nouvelle révision de ce document.

## Éléments de la spécification d'utilisation

| Élément | Contenu | Source |
| --- | --- | --- |
| Indication médicale | Administration par voie intraveineuse, contrôlée et programmée, d'un médicament ou d'une solution, à un débit ou selon un volume prescrits | [Usage prévu, fonction](../intended-use.md#fonction) |
| Population de patients | Adultes ; néonatalogie et pédiatrie exclues du périmètre initial | [Usage prévu](../intended-use.md#population-de-patients-hypothèse) |
| Partie du corps | Système veineux, par l'intermédiaire d'un accès veineux et d'une tubulure ; la pompe elle-même n'est pas en contact avec le patient | Usage prévu |
| Profils d'utilisateurs | IDE (P1), médecin prescripteur (P2), technicien biomédical (P3) ; patient à domicile (P4) hors périmètre initial | [Profils](#profils-dutilisateurs) |
| Environnement d'utilisation | Unités d'hospitalisation d'un établissement de soins | [Environnement](#environnement-dutilisation) |
| Principe de fonctionnement | Pousse-seringue ([ADR 0002](../adr/0002-principe-de-pompage.md)) : un pousseur déplace le piston d'une seringue standard de 60 mL au plus ; le logiciel commande l'actionneur, surveille les capteurs et déclenche les alarmes | Usage prévu |

## Profils d'utilisateurs

| Profil | Rôle | Formation et expérience supposées | Interactions avec la pompe | Caractéristiques à prendre en compte |
| --- | --- | --- | --- | --- |
| **P1 — IDE (infirmier ou infirmière diplômé·e d'État)** | Utilisateur principal : installe, programme, surveille, répond aux alarmes | Formation initiale aux perfusions ; formation au modèle de pompe en service ; usage quotidien, souvent de plusieurs modèles | Toutes les tâches de soin, de l'installation à la fin de perfusion | Interruptions fréquentes, charge de travail élevée, gants, travail de nuit sous faible éclairage, plusieurs pompes par patient ou par chambre, habitudes acquises sur d'autres modèles |
| **P2 — Médecin prescripteur** | Prescrit le médicament, la dose et le débit ; intervient rarement sur la pompe | Formation médicale ; peu ou pas de formation au modèle | Lecture de l'état et de l'historique, éventuellement modification en situation d'urgence | Usage occasionnel : l'interface doit être compréhensible sans pratique récente |
| **P3 — Technicien biomédical** | Met en service, configure la bibliothèque de médicaments, entretient, contrôle, étalonne | Formation technique ; formation constructeur au modèle | Configuration, maintenance, mise à jour, consultation des journaux | Accès à des fonctions protégées ; travail hors présence du patient |
| **P4 — Patient à domicile** | *Hors périmètre initial* | — | — | Inclure ce profil ferait du patient ou d'un aidant un utilisateur non professionnel. Il faudrait alors réviser l'usage prévu, les alarmes, l'alimentation et l'évaluation de l'aptitude à l'utilisation. |

### Personnes présentes sans être utilisatrices

- **Patient hospitalisé** : il peut toucher la pompe, appuyer sur une touche ou tirer sur la tubulure, volontairement ou non. Il peut être confus ou somnolent. Il perçoit les alarmes et peut en être gêné.
- **Visiteurs et autres soignants** : mêmes risques d'interaction involontaire.

Ces personnes justifient le verrouillage de l'interface ([#39](https://github.com/camille-martin-paris/clepsydre/issues/39)) et la protection contre les manipulations involontaires.

## Environnement d'utilisation

| Caractéristique | Hypothèse | Conséquence pour la conception |
| --- | --- | --- |
| Lieu | Chambre d'hospitalisation, poste de soins ; pompe fixée à une potence ou posée près du lit | Fixation sur potence, stabilité, encombrement |
| Éclairage | Du plein jour à la quasi-obscurité (nuit) | Affichage lisible dans les deux cas, sans éblouir le patient la nuit |
| Bruit | Ambiance sonore variable ; autres alarmes d'appareils voisins | Alarmes distinguables et localisables ([#32](https://github.com/camille-martin-paris/clepsydre/issues/32)) |
| Hygiène | Nettoyage et désinfection fréquents ; projections de liquides | Boîtier nettoyable, protégé contre les liquides ([#54](https://github.com/camille-martin-paris/clepsydre/issues/54)) |
| Alimentation | Secteur avec coupures possibles ; déplacements du patient sur batterie | Autonomie et bascule secteur/batterie ([#63](https://github.com/camille-martin-paris/clepsydre/issues/63)) |
| Mouvements | Transferts du lit au fauteuil, chocs légers, chute possible de la pompe | Robustesse mécanique |
| Exclusions | Transport en ambulance ou hélicoptère, IRM, caisson hyperbare, atmosphère explosive | Hors usage prévu |

## Besoins des utilisateurs

Statut de chaque besoin : **à valider** (voir [Statut des besoins](#statut-des-besoins)). Les identifiants `UN-nn` seront repris par les exigences ([#10](https://github.com/camille-martin-paris/clepsydre/issues/10)).

| Identifiant | Profils | Besoin | Origine |
| --- | --- | --- | --- |
| UN-01 | P1 | Programmer une perfusion conforme à la prescription en un nombre limité d'étapes, avec un récapitulatif à confirmer avant le démarrage | Scénario S3 ; erreurs de programmation [FDA 2010] |
| UN-02 | P1, P2 | Voir sans ambiguïté le médicament, le débit, le volume perfusé, le volume restant et l'unité de chaque valeur | Confusion d'unités [FDA 2010] |
| UN-03 | P1 | Être protégé contre une saisie erronée : touche répétée involontairement, décimale mal placée, valeur hors des limites du médicament | Rebond de touche [FDA 2010] ; bibliothèque de médicaments ([#37](https://github.com/camille-martin-paris/clepsydre/issues/37)) |
| UN-04 | P1 | Installer la seringue sans risque d'écoulement libre, y compris en la retirant ou en libérant le piston | Scénarios S1 et S2 |
| UN-05 | P1 | Purger la ligne de son air avant de la raccorder au patient | Scénario S2 |
| UN-06 | P1 | Être alerté rapidement et de façon compréhensible d'une occlusion, d'air dans la ligne, d'une fin de perfusion, d'une batterie faible ou d'une défaillance, avec la cause et l'action attendue | Scénario S5 |
| UN-07 | P1 | Distinguer les alarmes de cette pompe de celles des appareils voisins, et leur priorité | Environnement |
| UN-08 | P1 | Mettre une alarme en pause sans perdre la surveillance de la situation qui l'a déclenchée | Scénario S5 |
| UN-09 | P1 | Lever une occlusion sans que le patient reçoive un bolus non souhaité | Scénario S5 |
| UN-10 | P1 | Être prévenu avant la fin de la perfusion, pour préparer la suite | Scénario S6 |
| UN-11 | P1 | Poursuivre la perfusion pendant un déplacement ou une coupure de courant, en connaissant l'autonomie restante | Environnement |
| UN-12 | P1, P3 | Empêcher qu'une personne non habilitée ou un geste involontaire modifie la perfusion | Personnes présentes |
| UN-13 | P2 | Comprendre l'état de la perfusion et son historique récent sans formation spécifique | Profil P2 |
| UN-14 | P3 | Configurer la bibliothèque de médicaments et ses limites, et tracer chaque modification | Scénario S7 |
| UN-15 | P3 | Vérifier le bon fonctionnement et la précision de la pompe, et la mettre hors service en cas de défaut | Scénario S7 |
| UN-16 | P1, P3 | Retrouver après un incident la séquence des programmations, alarmes et actions, de façon fiable | Analyse d'incident ([#41](https://github.com/camille-martin-paris/clepsydre/issues/41)) |
| UN-17 | P1 | Nettoyer et désinfecter la pompe entre deux patients | Environnement |

## Scénarios d'utilisation

Chaque scénario décrit le déroulement nominal, puis les situations susceptibles de conduire à une erreur d'utilisation (« points d'attention »). Ces points d'attention alimentent l'analyse des dangers ([#9](https://github.com/camille-martin-paris/clepsydre/issues/9)).

### S1 — Installation

| | |
| --- | --- |
| Profil | P1 |
| Préalables | Pompe contrôlée et en service ; prescription disponible ; consommable compatible |
| Déroulement | 1. Fixer la pompe sur la potence et la brancher. 2. Mettre en marche ; l'autotest s'exécute et l'avertissement de prototype s'affiche. 3. Installer la seringue remplie et purgée dans le berceau, fermer la bride et engager le piston dans le pousseur. 4. Choisir le modèle de seringue dans la liste ; la pompe contrôle sa présence, son diamètre et la capture du piston. |
| Points d'attention | Seringue mal positionnée, non listée ou d'un autre modèle que celui choisi ; piston non engagé dans le pousseur (siphonage) ; écoulement libre pendant l'installation si le prolongateur n'est pas clampé ; autotest en échec ignoré ; pompe d'un autre service avec une autre configuration. |

### S2 — Amorçage (purge)

| | |
| --- | --- |
| Profil | P1 |
| Préalables | S1 terminé ; ligne non raccordée au patient |
| Déroulement | 1. Lancer la purge. 2. Observer l'élimination de l'air jusqu'à l'extrémité de la ligne. 3. Arrêter la purge. 4. Raccorder la ligne à l'accès veineux. |
| Points d'attention | Purge lancée alors que la ligne est déjà raccordée (bolus) ; air résiduel ; purge interrompue et oubliée. |

### S3 — Programmation

| | |
| --- | --- |
| Profils | P1 ; P2 en lecture |
| Préalables | S2 terminé ; prescription disponible |
| Déroulement | 1. Choisir le médicament dans la bibliothèque, ou un mode sans médicament nommé. 2. Saisir le débit et le volume à perfuser, ou la dose selon le mode. 3. Lire le récapitulatif (médicament, concentration, débit, volume, durée calculée). 4. Confirmer. |
| Points d'attention | Erreur de saisie : décimale, zéro ajouté, rebond de touche ; confusion d'unités (mL/h, mg/h) ; mauvais médicament choisi dans la liste ; dépassement d'une limite souple confirmé sans y prêter attention ; confirmation machinale du récapitulatif ; programmation de la mauvaise pompe quand plusieurs sont côte à côte. |

### S4 — Perfusion

| | |
| --- | --- |
| Profil | P1 |
| Préalables | S3 confirmé |
| Déroulement | 1. Démarrer. 2. Vérifier l'indication de perfusion en cours. 3. Surveiller périodiquement : débit, volume perfusé, volume restant, autonomie. 4. Si la prescription change : arrêter, modifier, confirmer, reprendre. 5. Si nécessaire, administrer un bolus borné. |
| Points d'attention | Pompe programmée mais non démarrée ; modification appliquée à la mauvaise ligne ; bolus demandé au lieu d'une modification de débit ; verrouillage oublié ; pompe débranchée sans que l'IDE s'en aperçoive. |

### S5 — Alarme

| | |
| --- | --- |
| Profil | P1 |
| Préalables | Perfusion en cours ou en pause |
| Déroulement | 1. Percevoir l'alarme et la localiser. 2. Lire la cause et l'action proposée. 3. Mettre l'alarme en pause si nécessaire. 4. Corriger la cause (lever l'occlusion, purger l'air, changer la seringue, brancher le secteur). 5. Reprendre la perfusion. |
| Points d'attention | Alarme non perçue ou confondue avec celle d'un appareil voisin ; alarmes trop fréquentes qui conduisent à les ignorer ; cause mal comprise ; bolus libéré à la levée d'une occlusion ; perfusion non reprise après la pause ; fausse alarme qui interrompt une thérapie nécessaire. |

### S6 — Fin de perfusion

| | |
| --- | --- |
| Profil | P1 |
| Préalables | Volume programmé bientôt atteint |
| Déroulement | 1. Recevoir l'alerte de fin imminente. 2. Préparer la suite : nouvelle seringue, ou arrêt. 3. À la fin, la pompe passe en débit de maintien de veine (si prévu) ou s'arrête et alarme. 4. Arrêter, déconnecter, retirer le consommable sans écoulement libre. |
| Points d'attention | Alerte de fin ignorée ; débit de maintien non souhaité ; consommable retiré clamp ouvert. |

### S7 — Maintenance

| | |
| --- | --- |
| Profil | P3 |
| Préalables | Pompe hors service clinique, sans patient |
| Déroulement | 1. Accéder au mode technique avec authentification. 2. Consulter les journaux et les compteurs. 3. Contrôler la précision du débit et les capteurs, étalonner si nécessaire. 4. Mettre à jour la bibliothèque de médicaments ou le logiciel. 5. Remettre en service ou déclarer hors service. |
| Points d'attention | Bibliothèque erronée diffusée sur plusieurs pompes ; étalonnage non enregistré ; mise à jour interrompue ; pompe défectueuse remise en service ; mode technique laissé ouvert. |

## Fonctions principales d'utilisation (provisoire)

Les fonctions d'utilisation liées à la sécurité, au sens de l'IEC 62366-1, seront désignées après l'analyse des dangers ([#9](https://github.com/camille-martin-paris/clepsydre/issues/9)). Candidates : installation du consommable (S1), programmation et confirmation (S3), réponse aux alarmes (S5), configuration de la bibliothèque (S7).

## Références

- [FDA 2010] FDA, Center for Devices and Radiological Health, *White Paper: Infusion Pump Improvement Initiative*, avril 2010 : https://www.fda.gov/medical-devices/infusion-pumps/white-paper-infusion-pump-improvement-initiative. Problèmes cités : défauts logiciels (rebond de touche, alarmes absentes ou intempestives), interfaces ambiguës (confusion d'unités), défaillances mécaniques et électriques.
- IEC 62366-1:2015+A1:2020, §5.1. Le texte de la norme n'est pas reproduit.

## Historique

| Révision | Date | Auteur | Modification |
| --- | --- | --- | --- |
| A | 2026-10-09 | Ambroise Leclerc | Création, soumise à relecture |
| B | 2026-10-09 | Ambroise Leclerc | Pousse-seringue retenu (ADR 0002) : seringue comme consommable ; UN-04 et scénarios S1, S5 et S6 révisés ([#104](https://github.com/camille-martin-paris/clepsydre/issues/104)) |
