# Analyse préliminaire des dangers

| Champ | Valeur |
| --- | --- |
| Statut | Révision C soumise à relecture ; la révision B est acceptée pour confirmer la classe C ; mesures proposées restant à vérifier |
| Révision | C |
| Issue | [#9](https://github.com/camille-martin-paris/clepsydre/issues/9) |
| Méthode | [Plan de gestion des risques](risk-management-plan.md) ; ISO 14971:2019, ISO/TR 24971:2020 |

> [!CAUTION]
> Clepsydre ne doit pas être utilisé sur un être humain ni sur un animal. Cette analyse identifie des dangers et **propose** des mesures ; aucune mesure n'est encore mise en œuvre ni vérifiée.

## Portée et limites

- **Objet** : les fonctions décrites par l'[usage prévu](../intended-use.md) et les scénarios S1 à S7 de la [spécification d'utilisation](use-specification.md).
- **Niveau** : préliminaire. Depuis la révision C, l'analyse tient compte du principe de pompage et de l'architecture de calcul retenus : pousse-seringue ([ADR 0002](../adr/0002-principe-de-pompage.md)) et deux processeurs, commande et sécurité ([ADR 0003](../adr/0003-architecture-de-calcul-et-canal-de-securite.md)). Seule la **gravité** est estimée ; la probabilité le sera dans l'analyse détaillée, conformément au plan.
- **Sources** : dangers demandés par l'issue #9, points d'attention des scénarios d'utilisation, problèmes connus des pompes à perfusion (livre blanc FDA 2010, cité dans la spécification d'utilisation), catégories de dangers de l'ISO 14971 (énergie, fonction, information, utilisation, environnement).
- **Valeurs** : les seuils des exigences (délais, volumes, précision) sont proposés par la [spécification des exigences](requirements-specification.md#valeurs-proposées) et restent à confirmer par les essais ([#73](https://github.com/camille-martin-paris/clepsydre/issues/73), [#74](https://github.com/camille-martin-paris/clepsydre/issues/74)).

## Dangers, situations dangereuses et dommages

| Risque | Danger | Séquence d'événements | Situation dangereuse | Dommage | Gravité | Logiciel contributeur | Scénarios | Mesures |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| RISK-001 | Énergie : débit délivré supérieur au débit programmé | Défaut de l'asservissement, de l'actionneur ou de la mesure de position ; défaillance du logiciel de commande ; modèle de seringue mal identifié (section erronée) ; emballement du moteur avant la coupure ; programmation modifiée par une commande défaillante après la confirmation | Le patient reçoit le médicament plus vite que prescrit | Surdosage | G5 | Oui | S4 | CTRL-001, CTRL-002, CTRL-014, CTRL-024, CTRL-025, CTRL-026 |
| RISK-002 | Fonction : débit délivré inférieur au programmé ou arrêt non détecté | Actionneur bloqué, seringue mal installée ou mal identifiée, défaut logiciel ; retard au démarrage à bas débit (jeux mécaniques, frottement du joint du piston) ; la pompe indique une perfusion en cours | Le patient ne reçoit pas le traitement sans que le soignant le sache | Sous-dosage, interruption de thérapie | G4 | Oui | S1, S4 | CTRL-002, CTRL-003, CTRL-005, CTRL-026, CTRL-027 |
| RISK-003 | Énergie : écoulement libre par gravité | Seringue installée ou retirée prolongateur non clampé ; piston non retenu par le pousseur ou seringue non fixée, pompe placée au-dessus du patient (siphonage) ; vis entraînée par le piston | Le contenu du réservoir s'écoule sans contrôle vers le patient | Surdosage massif | G5 | Non | S1, S6 | CTRL-004, CTRL-005 |
| RISK-004 | Air dans la ligne | Seringue ou prolongateur incomplètement purgés, dégazage, raccord non étanche ; détection d'air défaillante | De l'air est perfusé dans le système veineux | Embolie gazeuse | G5 | Oui | S2, S5 | CTRL-006 |
| RISK-005 | Occlusion non détectée ou détectée tardivement | Ligne clampée, coudée ou cathéter obstrué en aval ; seuil de force trop élevé, ou délai allongé par la section et l'élasticité de la seringue ; capteur de force défaillant | Le patient ne reçoit plus le traitement pendant une durée prolongée | Interruption de thérapie | G4 | Oui | S5 | CTRL-007 |
| RISK-006 | Énergie : bolus libéré à la levée d'une occlusion | Pression accumulée dans la seringue et la ligne pendant l'occlusion, d'autant plus que la seringue est élastique ; levée de l'occlusion sans recul du pousseur | Le volume accumulé est perfusé en quelques secondes | Surdosage | G5 | Oui | S5 | CTRL-008 |
| RISK-007 | Utilisation : erreur de saisie lors de la programmation | Décimale mal placée, zéro ajouté, rebond de touche, valeur saisie dans le mauvais champ ; récapitulatif confirmé machinalement ; affichage figé ou erroné | La pompe perfuse à un débit ou un volume différent de la prescription | Surdosage ou sous-dosage | G5 | Oui | S3 | CTRL-009, CTRL-010 |
| RISK-008 | Utilisation : confusion d'unités ou de médicament | Unité ambiguë à l'écran, mauvais médicament choisi dans la bibliothèque, concentration erronée | La dose délivrée diffère de la dose prescrite d'un facteur important | Surdosage ou sous-dosage | G5 | Oui | S3 | CTRL-010, CTRL-011 |
| RISK-009 | Énergie : bolus au-delà des limites | Bolus manuel de volume ou de débit excessif ; purge lancée alors que la ligne est raccordée au patient | Un volume important est perfusé en peu de temps | Surdosage | G5 | Oui | S2, S4 | CTRL-010, CTRL-012 |
| RISK-010 | Énergie électrique : perte d'alimentation pendant la perfusion | Débranchement, coupure secteur, batterie dégradée ou épuisée ; absence d'alarme d'autonomie ; perte de toute alimentation des processeurs pendant la perfusion | La perfusion s'arrête sans que le soignant soit prévenu à temps | Interruption de thérapie | G4 | Oui | S4 | CTRL-013, CTRL-016 |
| RISK-011 | Information : perte, altération ou retour arrière du journal d'événements | Coupure pendant une écriture, corruption du support, restauration d'un état antérieur, modification délibérée | Après un incident, la séquence réelle des programmations, alarmes et actions ne peut pas être établie | Cause d'incident non identifiée, récurrence chez d'autres patients | G3 | Oui | S7 | CTRL-015 |
| RISK-012 | Défaillance du processeur ou du logiciel de commande | Blocage, réinitialisation, corruption de mémoire, erreur de conception logicielle | L'actionneur est commandé de façon incontrôlée, ou s'arrête sans alarme | Surdosage ou interruption de thérapie | G5 | Oui | S4 | CTRL-001, CTRL-014, CTRL-016, CTRL-024, CTRL-025 |
| RISK-013 | Fausse alarme interrompant la thérapie | Détection trop sensible, mesure bruitée, défaut de capteur interprété comme une condition d'alarme | La perfusion s'arrête alors qu'elle se déroulait correctement | Interruption de thérapie | G4 | Oui | S5 | CTRL-017 |
| RISK-014 | Utilisation : alarme non perçue ou ignorée | Alarme trop faible, confondue avec celle d'un appareil voisin, ou ignorée après de nombreuses alarmes non pertinentes | Une situation dangereuse signalée par la pompe n'est pas traitée | Dommage de la situation non traitée (jusqu'au décès) | G5 | Oui | S5 | CTRL-016, CTRL-017 |
| RISK-015 | Utilisation : modification non autorisée ou involontaire | Patient confus, visiteur ou personne non habilitée qui appuie sur les touches ; accès malveillant par une interface | La perfusion est modifiée ou arrêtée à l'insu du soignant | Surdosage ou interruption de thérapie | G5 | Oui | S4 | CTRL-018 |
| RISK-016 | Utilisation : programmation de la mauvaise pompe | Plusieurs pompes côte à côte pour un même patient ou dans une même chambre | Le débit prévu pour un médicament est appliqué à un autre | Surdosage ou sous-dosage | G5 | Oui | S3, S4 | CTRL-019 |
| RISK-017 | Énergie électrique : courant de fuite ou contact avec une tension dangereuse | Défaut d'isolement, boîtier endommagé, alimentation non conforme | Un courant traverse le patient ou le soignant | Électrisation, électrocution | G5 | Non | S1 | CTRL-020 |
| RISK-018 | Environnement : pénétration de liquide | Projection de perfusat ou de produit de nettoyage sur le boîtier | L'électronique ou le clavier se dégrade, provoquant un comportement erroné | Surdosage ou interruption de thérapie | G4 | Non | S4, S7 | CTRL-021 |
| RISK-019 | Information : bibliothèque de médicaments erronée | Erreur de configuration diffusée sur plusieurs pompes ; version non contrôlée | Les limites de dose protègent mal ou bloquent à tort | Surdosage ou interruption de thérapie | G5 | Oui | S7 | CTRL-018, CTRL-022 |
| RISK-020 | Information : logiciel ou mise à jour corrompu ou non autorisé | Mise à jour interrompue, image altérée, logiciel non publié installé | La pompe fonctionne avec un logiciel non vérifié | Surdosage ou interruption de thérapie | G5 | Oui | S7 | CTRL-022 |
| RISK-021 | Utilisation clinique d'un prototype non évalué | Le projet étant public, un prototype est employé sur un patient | Un patient est perfusé par un dispositif dont la sécurité n'est pas démontrée | Tout dommage ci-dessus | G5 | Non | S1 | CTRL-023 |

## Mesures de maîtrise proposées

Types, dans l'ordre de priorité du plan : **Conception** (conception intrinsèquement sûre), **Protection** (mesure de protection dans le dispositif), **Information** (information de sécurité).

| Mesure | Type | Description | Risques maîtrisés | Exigences |
| --- | --- | --- | --- | --- |
| CTRL-001 | Protection | Supervision indépendante du volume délivré par un canal de sécurité distinct du canal de commande (processeur, horloge et capteur de position du piston propres), qui coupe le moteur par une autorisation dynamique et place la pompe en état sûr en cas d'écart ou de vitesse hors borne | RISK-001, RISK-012 | SYS-REQ-001, SYS-REQ-002, SYS-REQ-045, SYS-REQ-046 |
| CTRL-002 | Conception | Asservissement du débit fondé sur la position du pousseur et la section du modèle de seringue identifié, avec précision de débit vérifiée sur banc pour chaque seringue | RISK-001, RISK-002 | SYS-REQ-003 |
| CTRL-003 | Protection | Détection d'un actionneur qui ne délivre pas alors qu'une perfusion est en cours | RISK-002 | SYS-REQ-004 |
| CTRL-004 | Conception | Retenue mécanique du piston par le pousseur et fixation de la seringue dans le berceau, avec vis d'entraînement irréversible, contre l'écoulement libre et le siphonage | RISK-003 | SYS-REQ-005 |
| CTRL-005 | Protection | Détection de la seringue absente ou mal positionnée et du piston non capturé, avec refus du démarrage ou arrêt et alarme | RISK-002, RISK-003 | SYS-REQ-006 |
| CTRL-006 | Protection | Détection d'air sur le prolongateur, unitaire et cumulée, avec arrêt et alarme | RISK-004 | SYS-REQ-007 |
| CTRL-007 | Protection | Détection d'occlusion aval par la force sur le pousseur, avec seuils et délais établis pour chaque modèle de seringue | RISK-005 | SYS-REQ-008 |
| CTRL-008 | Conception | Recul du pousseur d'une course calculée pour le modèle de seringue à la détection d'une occlusion, pour limiter le bolus à sa levée | RISK-006 | SYS-REQ-009 |
| CTRL-009 | Conception | Saisie protégée : filtrage des rebonds de touche, format numérique sans ambiguïté, récapitulatif à confirmer explicitement | RISK-007 | SYS-REQ-010, SYS-REQ-011 |
| CTRL-010 | Protection | Bibliothèque de médicaments avec limites dures bloquantes et limites souples à confirmer | RISK-007, RISK-008, RISK-009 | SYS-REQ-012 |
| CTRL-011 | Conception | Affichage de l'unité avec chaque valeur, et du médicament et de la concentration pendant toute la perfusion | RISK-008 | SYS-REQ-013 |
| CTRL-012 | Conception | Bolus et purge distincts, bornés chacun par leurs propres limites de volume et de débit ; purge refusée une fois la perfusion démarrée ; purge subordonnée à la condition « ligne non raccordée au patient », refusée si cette condition n'est pas établie et arrêtée si sa violation est détectée ou signalée | RISK-009 | SYS-REQ-014, SYS-REQ-028 |
| CTRL-013 | Protection | Batterie avec bascule secteur/batterie sans interruption, alarmes d'autonomie faible et épuisée, alarme de perte d'alimentation | RISK-010 | SYS-REQ-015, SYS-REQ-016 |
| CTRL-014 | Protection | Chien de garde indépendant et autotests du processeur et des mémoires, avec état sûr en cas d'échec | RISK-001, RISK-012 | SYS-REQ-017 |
| CTRL-015 | Conception | Journal d'événements à écriture confirmée sur support durable, avec détection d'altération et de retour arrière | RISK-011 | SYS-REQ-018, SYS-REQ-019 |
| CTRL-016 | Protection | Alarme technique de repli par un circuit autonome sur réserve propre, armé par le canal de sécurité pendant la perfusion et déclenché par la perte de son signal de vie, sans processeur alimenté ; testée à chaque démarrage | RISK-010, RISK-012, RISK-014 | SYS-REQ-020, SYS-REQ-046 |
| CTRL-017 | Conception | Catalogue d'alarmes avec priorités et signaux distinctifs, conditions confirmées avant déclenchement et taux de fausses alarmes mesuré | RISK-013, RISK-014 | SYS-REQ-021, SYS-REQ-022 |
| CTRL-018 | Protection | Verrouillage de l'interface et authentification des fonctions protégées par rôle | RISK-015, RISK-019 | SYS-REQ-023 |
| CTRL-019 | Conception | Identification visible de la pompe et de la perfusion en cours, lisible à distance | RISK-016 | SYS-REQ-024 |
| CTRL-020 | Conception | Conception électrique selon l'IEC 60601-1 : isolement, courants de fuite, alimentation | RISK-017 | HW-REQ-001 |
| CTRL-021 | Conception | Boîtier protégé contre la pénétration des liquides et compatible avec les produits de nettoyage | RISK-018 | HW-REQ-002 |
| CTRL-022 | Protection | Contrôle d'intégrité et d'origine de la bibliothèque de médicaments et du logiciel avant leur utilisation | RISK-019, RISK-020 | SYS-REQ-025, SYS-REQ-026 |
| CTRL-023 | Information | Avertissement de prototype non utilisable sur un patient, sur l'appareil, à son démarrage et dans chaque version publiée | RISK-021 | SYS-REQ-027 |
| CTRL-024 | Conception | Vitesse maximale du pousseur bornée par la conception mécanique et électronique, indépendamment du logiciel, pour limiter le volume délivré lors d'un emballement avant l'état sûr | RISK-001, RISK-012 | SYS-REQ-043 |
| CTRL-025 | Protection | Programmation confirmée figée dans le canal de sécurité, qui lit lui-même les touches de confirmation et n'admet que les changements de mode listés | RISK-001, RISK-012 | SYS-REQ-044, SW-REQ-013 |
| CTRL-026 | Protection | Identification du modèle de seringue : choix dans une liste vérifiée, contrôlé par la mesure du diamètre du corps ; désaccord bloquant | RISK-001, RISK-002 | SYS-REQ-047 |
| CTRL-027 | Conception | Rattrapage des jeux mécaniques avant le démarrage, avec délai au premier écoulement borné et mesuré pour chaque seringue | RISK-002 | SYS-REQ-048 |

## Exigences issues des mesures de maîtrise

Ces exigences, au statut `draft`, traduisent les mesures proposées. La spécification des exigences ([#10](https://github.com/camille-martin-paris/clepsydre/issues/10)) les complétera, fixera leurs seuils et les reliera aux besoins UN-nn.

> [!NOTE]
> Le tableau reproduit les énoncés de la révision A de cette analyse. Les énoncés en vigueur, avec leurs seuils, sont ceux du registre et de la [spécification des exigences](requirements-specification.md). Les exigences ajoutées par la révision C, SYS-REQ-043 à SYS-REQ-048, n'y figurent que dans la spécification.

| Exigence | Intitulé | Énoncé (statut : draft) |
| --- | --- | --- |
| SYS-REQ-001 | Supervision indépendante du volume délivré | Un canal de sécurité indépendant du canal de commande doit mesurer le volume délivré et détecter un écart au volume programmé supérieur à un seuil à définir (#10). |
| SYS-REQ-002 | État sûr | Sur détection d'une condition dangereuse, la pompe doit arrêter l'actionneur, maintenir le dispositif anti-écoulement libre actif et déclencher une alarme de priorité haute. |
| SYS-REQ-003 | Précision de débit | La pompe doit délivrer le débit programmé avec une précision à définir (hypothèse H-04 : ± 5 % en régime établi), mesurée selon la méthode de l'IEC 60601-2-24. |
| SYS-REQ-004 | Actionneur inactif | La pompe doit détecter, dans un délai à définir (#10), que l'actionneur ne délivre pas alors qu'une perfusion est en cours, et déclencher une alarme. |
| SYS-REQ-005 | Anti-écoulement libre | Aucun écoulement libre ne doit être possible lorsque le consommable est installé, retiré, ou que la porte est ouverte. |
| SYS-REQ-006 | Porte et consommable | La pompe doit refuser de démarrer, ou arrêter la perfusion et alarmer, si la porte est ouverte ou si le consommable est absent ou mal positionné. |
| SYS-REQ-007 | Air dans la ligne | La pompe doit détecter une bulle d'air de volume supérieur à un seuil à définir, ainsi qu'un volume d'air cumulé supérieur à un seuil à définir, et placer la pompe en état sûr. |
| SYS-REQ-008 | Délai d'alarme d'occlusion | La pompe doit déclencher l'alarme d'occlusion aval dans un délai maximal à définir pour chaque débit de référence, mesuré selon l'IEC 60601-2-24. |
| SYS-REQ-009 | Bolus post-occlusion | Le volume du bolus libéré à la levée d'une occlusion doit rester inférieur à un seuil à définir. |
| SYS-REQ-010 | Rebond de touche | Un appui sur une touche ne doit produire qu'une seule saisie, quels que soient les rebonds du contact. |
| SYS-REQ-011 | Récapitulatif et confirmation | Avant le démarrage, la pompe doit afficher un récapitulatif (médicament, concentration, débit, volume, durée) et exiger une confirmation explicite distincte de la saisie. |
| SYS-REQ-012 | Limites de dose | La pompe doit refuser toute programmation hors des limites dures du médicament et exiger une confirmation spécifique au-delà des limites souples. |
| SYS-REQ-013 | Affichage des unités | Chaque valeur affichée doit être accompagnée de son unité ; le médicament, la concentration et le débit doivent rester affichés pendant la perfusion. |
| SYS-REQ-014 | Bolus et purge bornés | Le bolus et la purge doivent être des commandes distinctes. Le volume et le débit d'un bolus doivent être bornés par les limites configurées pour le médicament ; ceux d'une purge, par des limites propres à la purge, qui ne peuvent servir à administrer un bolus. La purge doit être refusée une fois la perfusion démarrée. |
| SYS-REQ-015 | Bascule d'alimentation | La perte de l'alimentation secteur ne doit pas interrompre ni modifier la perfusion en cours tant que la batterie n'est pas épuisée. |
| SYS-REQ-016 | Alarmes d'alimentation | La pompe doit déclencher une alarme d'autonomie faible au moins à une durée à définir avant l'épuisement, puis une alarme d'autonomie épuisée avant l'arrêt. |
| SYS-REQ-017 | Surveillance du processeur | Un chien de garde indépendant et des autotests du processeur et des mémoires doivent placer la pompe en état sûr en cas de défaillance détectée. |
| SYS-REQ-018 | Persistance du journal | Un événement de sécurité ne doit être considéré comme enregistré qu'après son écriture confirmée sur un support durable. |
| SYS-REQ-019 | Intégrité du journal | Toute altération, suppression ou retour arrière du journal d'événements doit être détectable lors de sa vérification, sur l'appareil ou hors de l'appareil. |
| SYS-REQ-020 | Alarme de repli | Une alarme technique doit retentir même si le processeur principal est défaillant. |
| SYS-REQ-021 | Priorités et signaux d'alarme | Les alarmes doivent être classées par priorité et signalées par des signaux visuels et sonores distincts, selon l'IEC 60601-1-8. |
| SYS-REQ-022 | Fausses alarmes | Le taux de fausses alarmes interrompant la perfusion doit être mesuré et rester inférieur à une valeur à définir. |
| SYS-REQ-023 | Verrouillage et rôles | La pompe doit permettre de verrouiller l'interface pendant une perfusion et réserver les fonctions de configuration aux utilisateurs authentifiés selon leur rôle. |
| SYS-REQ-024 | Identification à distance | Pendant une perfusion, l'état, le médicament et le débit doivent être lisibles à une distance à définir. |
| SYS-REQ-025 | Intégrité de la bibliothèque | La pompe doit vérifier l'intégrité et l'origine de la bibliothèque de médicaments avant de l'utiliser et afficher sa version. |
| SYS-REQ-026 | Intégrité du logiciel | La pompe ne doit exécuter qu'un logiciel dont l'intégrité et l'origine ont été vérifiées, et une mise à jour interrompue ne doit pas laisser la pompe dans un état utilisable non vérifié. |
| SYS-REQ-027 | Avertissement de prototype | L'appareil doit porter l'avertissement « PROTOTYPE — NE PAS UTILISER SUR UN PATIENT » et l'afficher au démarrage jusqu'à acquittement explicite. |
| SYS-REQ-028 | Purge sur ligne non raccordée | Une purge ne doit être lancée que si la condition « ligne non raccordée au patient » a été établie immédiatement avant son lancement, y compris avant le premier démarrage d'une perfusion ; à défaut, la purge doit être refusée. Si la violation de cette condition est détectée ou signalée pendant la purge, celle-ci doit s'arrêter immédiatement, et l'utilisateur doit pouvoir l'arrêter à tout moment. Le moyen d'établir et de surveiller cette condition sera choisi avec l'architecture (#13, #14). |
| HW-REQ-001 | Sécurité électrique | L'isolement et les courants de fuite doivent respecter les exigences de l'IEC 60601-1 applicables, vérifiées par des pré-essais (#76). |
| HW-REQ-002 | Protection contre les liquides | Le boîtier doit atteindre un indice de protection contre les liquides à définir et résister aux produits de nettoyage listés. |

## Constats

1. **Classe de sécurité logicielle.** Le logiciel contribue à 17 des 21 situations dangereuses. Parmi elles, plusieurs peuvent entraîner un décès (G5) : RISK-001, RISK-004, RISK-006, RISK-007, RISK-008, RISK-009, RISK-012, RISK-014 à RISK-016, RISK-019 et RISK-020. Aucune mesure externe au logiciel n'est encore démontrée. L'analyse confirme donc la classe C dans le [dossier de développement](../../software_development_file/safety-classification.md). Elle ne permet de l'abaisser pour aucun élément logiciel tant que la ségrégation et le canal de sécurité indépendant (CTRL-001, CTRL-016) ne sont pas démontrés. La réserve R2 est levée par l'acceptation explicite de cette analyse le 2026-10-09, consignée dans [reviews.md](../../software_development_file/reviews.md).
2. **Mesures structurantes.** CTRL-001 (supervision indépendante), CTRL-014 (chien de garde et autotests) et CTRL-016 (alarme de repli) conditionnent l'architecture de calcul. L'[ADR 0003](../adr/0003-architecture-de-calcul-et-canal-de-securite.md) les met en œuvre par deux processeurs. La révision C en déduit trois mesures : la coupure par autorisation dynamique et ses tests au démarrage (CTRL-001), la vitesse maximale bornée par conception (CTRL-024) et la programmation verrouillée par le canal de sécurité (CTRL-025). L'alarme de repli devient un circuit autonome qui couvre aussi la perte de toute alimentation (CTRL-016, RISK-010).
3. **Mesures dépendant du principe de pompage.** L'[ADR 0002](../adr/0002-principe-de-pompage.md) retient le pousse-seringue. CTRL-004 devient la retenue du piston et la fixation de la seringue, contre le siphonage ; CTRL-008 devient le recul du pousseur. Deux causes propres au pousse-seringue reçoivent leur mesure : la seringue mal identifiée (CTRL-026) et le retard au démarrage (CTRL-027). La détection d'air (CTRL-006) est **maintenue**, sur le prolongateur : les pousse-seringues courants n'en ont souvent pas, mais rien ne justifie ici de réduire la maîtrise de RISK-004 (G5).
4. **Dangers introduits par les mesures.** Une détection trop sensible (CTRL-005 à CTRL-007) peut produire de fausses alarmes, qui interrompent la thérapie (RISK-013) et entretiennent la lassitude des soignants face aux alarmes (RISK-014). CTRL-017 traite ce risque.
5. **Information de sécurité.** CTRL-023 (avertissement de prototype) est la seule mesure de type information ; elle ne remplace aucune autre mesure.
6. **Cause non maîtrisée.** Un affichage figé ou erroné, notamment de la programmation renvoyée par le canal de sécurité, n'est couvert par aucune mesure de l'architecture (ADR 0003, analyse de défaillance unique). Il contribue à RISK-007 et RISK-008 et reste à traiter avec l'interface et l'électronique ([#36](https://github.com/camille-martin-paris/clepsydre/issues/36), [#57](https://github.com/camille-martin-paris/clepsydre/issues/57)).

## Suites

- Estimation de la probabilité et évaluation des risques résiduels après le choix de l'architecture.
- AMDEC de conception et analyse des défaillances du logiciel.
- Vérification de chaque exigence et démonstration de l'efficacité de chaque mesure, inscrites au registre.

## Historique

| Révision | Date | Auteur | Modification |
| --- | --- | --- | --- |
| A | 2026-10-09 | Ambroise Leclerc | Création ; révision corrigée acceptée par Camille Martin dans #94 et intégrée au commit `75169c378cf326937f730a5a436baa69d42734f0` |
| B | 2026-10-09 | Camille Martin | Constat 1 : levée explicite de R2 après acceptation de l'analyse ; classe C confirmée, mesures restant à vérifier ; mise à jour assistée par Codex dans #95 |
| C | 2026-10-09 | Ambroise Leclerc | Application des ADR 0002 et 0003 ([#104](https://github.com/camille-martin-paris/clepsydre/issues/104)) : causes propres au pousse-seringue et à l'architecture ; CTRL-001, 002, 004 à 008 et 016 révisées ; CTRL-024 à CTRL-027 ajoutées ; constats 2, 3 et 6 |
