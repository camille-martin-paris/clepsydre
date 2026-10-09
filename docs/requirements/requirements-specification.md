# Spécification des exigences système et logicielles

| Champ | Valeur |
| --- | --- |
| Statut | Révision B, soumise à relecture ; toutes les exigences sont au statut `draft` |
| Révision | B |
| Issue | [#10](https://github.com/camille-martin-paris/clepsydre/issues/10) |
| Registre | [`requirements.toml`](../../software_development_file/registry/requirements.toml), [`verifications.toml`](../../software_development_file/registry/verifications.toml) |
| Références | IEC 62304:2006+A1:2015 §5.2 ; IEC 60601-2-24:2012 ; IEC 60601-1-8:2006+A1:2012+A2:2020 ; IEC 60601-1:2005+A1:2012+A2:2020 ; IEC 60529 |

> [!CAUTION]
> Clepsydre ne doit pas être utilisé sur un être humain ni sur un animal. Ces exigences décrivent ce que le prototype doit démontrer ; aucune n'est encore vérifiée.

## Objet

Ce document rassemble les exigences du système Clepsydre (SYS-REQ), de son logiciel (SW-REQ) et de son matériel (HW-REQ). Il complète les 30 exigences issues de l'[analyse préliminaire des dangers](preliminary-hazard-analysis.md) avec les exigences fonctionnelles de l'[usage prévu](../intended-use.md) et de la [spécification d'utilisation](use-specification.md), fixe leurs seuils et indique pour chacune sa source et sa méthode de vérification.

Le registre fait foi : les tableaux ci-dessous en reproduisent les énoncés, et toute modification d'une exigence passe par le registre et par ce document dans la même pull request.

## Conventions

- **Énoncé** : une exigence est formulée avec « doit » et porte une valeur vérifiable. Les valeurs chiffrées sont des **propositions** ; elles sont regroupées avec leur justification dans [Valeurs proposées](#valeurs-proposées) et restent à confirmer par la relecture, les ADR et les essais.
- **Statut** : `draft` tant que l'exigence n'est pas relue et acceptée ; `approved` ensuite. Le vérificateur du registre refuse une exigence `approved` sans vérification.
- **Source** : besoin utilisateur (UN-nn), mesure de maîtrise et risques (CTRL-nnn, RISK-nnn), hypothèse de l'usage prévu (H-nn), norme, ou exigence système dont une exigence logicielle découle. Les besoins UN-nn ne sont pas encore validés auprès d'utilisateurs ([#40](https://github.com/camille-martin-paris/clepsydre/issues/40)) ; une exigence qui en découle hérite de cette limite.
- **Vérification prévue** : méthode (essai, analyse, inspection, démonstration) et identifiant VER-nnn au registre, au statut `planned`. Un identifiant VER-nnn désigne une vérification à mener, **pas un résultat**. Le niveau de chaque vérification suit la méthode, selon la [stratégie de vérification](../../software_development_file/verification-strategy.md#niveaux-de-vérification) ; une exigence système vérifiée sur simulateur est confirmée sur banc.
- **Principe et architecture** : le pousse-seringue ([ADR 0002](../adr/0002-principe-de-pompage.md)) et l'architecture à deux processeurs, commande et sécurité ([ADR 0003](../adr/0003-architecture-de-calcul-et-canal-de-securite.md)), sont retenus. Les valeurs qui dépendent de la seringue s'appliquent à chaque modèle de la liste des seringues vérifiées (SYS-REQ-047).

## Synthèse

| Type | Nombre |
| --- | --- |
| Exigences système (SYS-REQ) | 48 |
| Exigences logicielles (SW-REQ) | 18 |
| Exigences matérielles (HW-REQ) | 2 |
| **Total** | **68** |
| Vérifications prévues (VER, `planned`) | 113 |

Les numéros SYS-REQ-001 à 028 et HW-REQ-001 à 002 viennent de l'analyse préliminaire des dangers. Leurs seuils, auparavant « à définir », sont fixés ici, et leur source mentionne désormais les besoins et les normes. Les numéros suivants sont nouveaux ; SYS-REQ-043 à 048 viennent de la révision C de l'analyse, qui applique les ADR 0002 et 0003.

## Exigences système

### Fonctions de perfusion

| Exigence | Intitulé | Énoncé | Source | Vérification prévue |
| --- | --- | --- | --- | --- |
| SYS-REQ-029 | Modes de perfusion | La pompe doit offrir trois modes : débit continu, volume à perfuser sur une durée programmée, et bolus à la demande du soignant pendant une perfusion en débit continu ; un seul canal de perfusion est géré par appareil. | UN-01 ; usage prévu H-01, H-06 | essai, système logiciel (VER-037) ; essai, banc (VER-090) |
| SYS-REQ-030 | Plage et résolution du débit | Le débit programmable doit couvrir 0,1 à 200 mL/h avec une résolution de 0,1 mL/h, dans la limite du débit maximal du modèle de seringue installé ; toute valeur hors de cette plage doit être refusée. | UN-01 ; usage prévu H-02, H-03 ; ADR 0002 | essai, système logiciel (VER-038) ; essai, banc (VER-091) |
| SYS-REQ-031 | Volume à perfuser | Le volume à perfuser doit être programmable de 0,1 mL au volume utile de la seringue installée, avec une résolution de 0,1 mL ; le volume perfusé et le volume restant doivent être affichés pendant la perfusion. | UN-01, UN-02 ; usage prévu H-05 ; ADR 0002 | essai, système logiciel (VER-039) ; essai, banc (VER-092) |
| SYS-REQ-032 | Fin de perfusion | La pompe doit déclencher une préalarme de fin de perfusion un délai configurable avant la fin (1 à 30 min, 5 min par défaut). À l'atteinte du volume à perfuser, elle doit déclencher l'alarme de fin de perfusion et, selon la bibliothèque, s'arrêter ou poursuivre à un débit de maintien de veine ouverte qui ne peut dépasser ni le débit programmé ni 5 mL/h. La position du piston doit en outre déclencher la préalarme de seringue presque vide, au même délai, et l'alarme de seringue vide, qui arrête la perfusion. | UN-10 ; scénario S6 ; IEC 60601-2-24 ; ADR 0002 | essai, système logiciel (VER-040) ; essai, banc (VER-093) |
| SYS-REQ-028 | Purge sur ligne non raccordée | Une purge ne doit être lancée que si la condition « ligne non raccordée au patient » a été établie immédiatement avant son lancement, y compris avant le premier démarrage d'une perfusion ; à défaut, la purge doit être refusée. Si la violation de cette condition est détectée ou signalée pendant la purge, celle-ci doit s'arrêter immédiatement, et l'utilisateur doit pouvoir l'arrêter à tout moment. Le moyen d'établir et de surveiller cette condition sera choisi avec l'architecture (#13, #14). | Mesure CTRL-012 (RISK-009) ; UN-05 ; scénario S2 | essai, système logiciel (VER-035) ; analyse, système logiciel (VER-036) ; essai, banc (VER-089) |
| SYS-REQ-014 | Bolus et purge bornés | Le bolus et la purge doivent être des commandes distinctes. Le volume et le débit d'un bolus doivent être bornés par les limites configurées pour le médicament ; ceux d'une purge, par des limites propres à la purge, qui ne peuvent servir à administrer un bolus. La purge doit être refusée une fois la perfusion démarrée. | Mesure CTRL-012 (RISK-009) ; UN-03, UN-05 | essai, système logiciel (VER-016) ; essai, banc (VER-083) |
| SYS-REQ-038 | Reprise après réinitialisation | Après une réinitialisation ou une coupure totale d'alimentation survenue pendant une perfusion, la pompe ne doit pas reprendre la perfusion d'elle-même : elle doit déclencher une alarme de priorité haute. Si l'état de perfusion enregistré est intègre, elle doit proposer la perfusion interrompue avec son débit, son volume à perfuser, le volume déjà délivré et le volume restant ; la confirmation par le soignant ne doit relancer que le volume restant, jamais le volume initial entier. Le volume délivré enregistré ne doit jamais être inférieur au volume effectivement délivré, de sorte que le volume restant proposé ne soit jamais surestimé. Si l'état enregistré est absent ou corrompu, aucune reprise ne doit être proposée : la pompe doit indiquer que le volume déjà délivré est inconnu et exiger une nouvelle programmation complète. | RISK-010, RISK-012 ; UN-11 | essai, système logiciel (VER-046 : coupure à différents instants de la délivrance ; état enregistré intègre, absent et corrompu ; la confirmation ne relance que le volume restant) ; analyse, système logiciel (VER-078 : le volume délivré enregistré majore le volume délivré quel que soit l'instant de la coupure) ; essai, banc (VER-097) |

### Précision et délivrance

| Exigence | Intitulé | Énoncé | Source | Vérification prévue |
| --- | --- | --- | --- | --- |
| SYS-REQ-003 | Précision de débit | La pompe doit délivrer le débit programmé avec une erreur de ± 5 % au plus en régime établi, aux débits d'essai de l'IEC 60601-2-24, mesurée selon la méthode de cette norme (courbes en trompette publiées). | Mesure CTRL-002 (RISK-001, RISK-002) ; usage prévu H-04 ; IEC 60601-2-24 | essai, banc (VER-004) |
| SYS-REQ-001 | Supervision indépendante du volume délivré | Un canal de sécurité indépendant du canal de commande doit mesurer le volume délivré par la position du piston, au moyen d'un capteur qui lui est propre, et placer la pompe en état sûr lorsque l'écart au volume attendu dépasse le plus grand de 10 % du volume attendu et 0,5 mL, ou lorsque la vitesse du piston dépasse la borne du mode actif sur une fenêtre de 2 s. | Mesure CTRL-001 (RISK-001, RISK-012) ; ADR 0003 | essai, banc (VER-001) ; analyse, système logiciel (VER-002) |
| SYS-REQ-004 | Actionneur inactif | La pompe doit détecter qu'un actionneur ne délivre pas alors qu'une perfusion est en cours, au plus tard 60 s après le temps nécessaire pour délivrer 0,05 mL au débit programmé, et déclencher une alarme de priorité haute. | Mesure CTRL-003 (RISK-002) | essai, banc (VER-005) |
| SYS-REQ-047 | Identification de la seringue | Le soignant doit choisir le modèle de seringue dans une liste de seringues vérifiées, configurée par un technicien authentifié et protégée comme la bibliothèque de médicaments ; la pompe doit mesurer le diamètre du corps de la seringue et refuser de démarrer s'il ne correspond pas au modèle choisi. | Mesure CTRL-026 (RISK-001, RISK-002) ; ADR 0002 | essai, système logiciel (VER-110) ; essai, banc (VER-113) |
| SYS-REQ-048 | Délai au premier écoulement | La pompe doit rattraper les jeux mécaniques avant le démarrage d'une perfusion ; le délai entre le démarrage et le premier écoulement doit rester inférieur à 5 min à 1 mL/h pour chaque modèle de seringue de la liste vérifiée, mesuré sur banc et publié. | Mesure CTRL-027 (RISK-002) ; ADR 0002 | essai, banc (VER-111) |

### Détections et état sûr

| Exigence | Intitulé | Énoncé | Source | Vérification prévue |
| --- | --- | --- | --- | --- |
| SYS-REQ-002 | État sûr | Sur détection d'une condition dangereuse, la pompe doit, en 1 s au plus, arrêter l'actionneur, maintenir le dispositif anti-écoulement libre actif et déclencher une alarme de priorité haute. | Mesure CTRL-001 (RISK-001, RISK-012) | essai, banc (VER-003) |
| SYS-REQ-005 | Anti-écoulement libre | Aucun écoulement libre ne doit être possible, ni par siphonage. Seringue en place, le piston doit être retenu par le pousseur, la seringue fixée dans le berceau, et la vis d'entraînement ne doit pas pouvoir être entraînée par le piston. Lorsque le piston est libéré ou la seringue retirée, ces protections n'agissant plus, l'écoulement libre doit être empêché par une valve anti-siphon du prolongateur : la liste des consommables vérifiés n'admet que des prolongateurs qui en sont munis. Avant de libérer le piston, la pompe doit en outre demander de clamper le prolongateur. | Mesure CTRL-004 (RISK-003) ; UN-04 ; IEC 60601-2-24 ; ADR 0002, ADR 0003 | essai, banc (VER-006 : seringue en place, piston libéré et seringue retirée, seringue au-dessus du patient, avec chaque prolongateur à valve anti-siphon de la liste) ; inspection, banc (VER-007 : retenue du piston, irréversibilité de la vis, valve anti-siphon des prolongateurs de la liste, message de clampage avant libération) |
| SYS-REQ-006 | Seringue et piston | La pompe doit refuser de démarrer, ou arrêter la perfusion et déclencher une alarme de priorité haute, si la seringue est absente ou mal positionnée dans le berceau, ou si le piston n'est pas capturé par le pousseur. | Mesure CTRL-005 (RISK-002, RISK-003) ; UN-04 | essai, banc (VER-008) |
| SYS-REQ-007 | Air dans la ligne | La pompe doit détecter sur le prolongateur, en aval de la seringue, une bulle d'air unitaire d'au moins 50 µL et un volume d'air cumulé d'au moins 1 mL sur 15 min, puis placer la pompe en état sûr. Ces seuils ne peuvent être configurés qu'à la baisse. | Mesure CTRL-006 (RISK-004) ; UN-06 ; IEC 60601-2-24 | essai, banc (VER-009) |
| SYS-REQ-008 | Délai d'alarme d'occlusion | Au seuil par défaut, la pompe doit déclencher l'alarme d'occlusion aval, détectée par la force sur le pousseur, en 30 min au plus à 1 mL/h et en 2 min au plus à 25 mL/h, pour chaque modèle de seringue de la liste vérifiée ; délais mesurés selon l'IEC 60601-2-24 et publiés avec le seuil, par modèle de seringue. | Mesure CTRL-007 (RISK-005) ; UN-06 ; IEC 60601-2-24 | essai, banc (VER-010) |
| SYS-REQ-009 | Bolus post-occlusion | À la détection d'une occlusion aval, la pompe doit faire reculer le pousseur d'une course calculée pour le modèle de seringue ; le volume libéré à la levée de l'occlusion, mesuré selon l'IEC 60601-2-24 au seuil le plus élevé, doit rester inférieur à 0,2 mL pour chaque modèle de la liste vérifiée. | Mesure CTRL-008 (RISK-006) ; UN-09 ; IEC 60601-2-24 | essai, banc (VER-011) |
| SYS-REQ-017 | Surveillance du processeur | Un chien de garde indépendant et des autotests du processeur et des mémoires doivent placer la pompe en état sûr en cas de défaillance détectée. | Mesure CTRL-014 (RISK-001, RISK-012) | essai, banc (VER-019) ; analyse, système logiciel (VER-020) |

### Alarmes

| Exigence | Intitulé | Énoncé | Source | Vérification prévue |
| --- | --- | --- | --- | --- |
| SYS-REQ-021 | Priorités et signaux d'alarme | Chaque condition d'alarme doit avoir la priorité fixée par le catalogue des alarmes de ce document et être signalée par les signaux visuels et sonores de cette priorité selon l'IEC 60601-1-8 ; une alarme de priorité supérieure ne doit jamais être masquée par une alarme de priorité inférieure. | Mesure CTRL-017 (RISK-013, RISK-014) ; UN-06, UN-07 ; IEC 60601-1-8 | essai, système logiciel (VER-026) ; inspection, système logiciel (VER-027) ; essai, banc (VER-085) |
| SYS-REQ-036 | Pause du signal sonore | Le soignant doit pouvoir suspendre le signal sonore d'une alarme pour 2 min au plus ; la condition reste surveillée et affichée, et le signal reprend à l'échéance si elle persiste ou dès qu'une condition de priorité supérieure apparaît. | UN-08 ; RISK-014 ; IEC 60601-1-8 | essai, système logiciel (VER-044) ; essai, banc (VER-095) |
| SYS-REQ-042 | Rappel de pompe inactive | Une pompe programmée mais non démarrée, ou en pause, depuis 2 min doit déclencher une alarme de rappel de priorité moyenne, jusqu'au démarrage, à la reprise ou à l'annulation de la programmation. | RISK-002, RISK-014 ; UN-06 | essai, système logiciel (VER-077) ; essai, banc (VER-101) |
| SYS-REQ-020 | Alarme de repli | Pendant une perfusion, une alarme technique sonore doit retentir même si le processeur de commande ou le processeur de sécurité est défaillant, ou si aucun processeur n'est alimenté. Elle repose sur un circuit autonome, alimenté par une réserve propre, armé par le canal de sécurité au démarrage de la perfusion et déclenché par la perte de son signal de vie ; elle doit retentir au moins 2 min sur cette réserve. | Mesure CTRL-016 (RISK-010, RISK-012, RISK-014) ; ADR 0003 | essai, banc (VER-025) ; analyse, système logiciel (VER-102) |
| SYS-REQ-022 | Fausses alarmes | Le taux d'alarmes interrompant à tort la perfusion doit rester inférieur à 1 pour 100 h de perfusion, mesuré sur banc et pendant l'évaluation formative. | Mesure CTRL-017 (RISK-013, RISK-014) | essai, banc (VER-028) |

#### Catalogue des alarmes

Priorités au sens de l'IEC 60601-1-8. Le catalogue complet, avec délais de confirmation et actions, sera établi par [#31](https://github.com/camille-martin-paris/clepsydre/issues/31) ; il ne pourra abaisser aucune priorité ci-dessous sans révision de ce document.

| Condition | Priorité | Exigences |
| --- | --- | --- |
| Occlusion aval | Haute | SYS-REQ-008 |
| Air dans la ligne | Haute | SYS-REQ-007 |
| Seringue absente ou mal positionnée, piston non capturé, pendant la perfusion | Haute | SYS-REQ-006 |
| Seringue vide | Haute | SYS-REQ-032 |
| Actionneur inactif | Haute | SYS-REQ-004 |
| Écart ou vitesse hors borne détectés par le canal de sécurité, défaillance technique | Haute | SYS-REQ-001, SYS-REQ-002, SYS-REQ-017, SYS-REQ-044 |
| Alarme technique de repli (circuit autonome) | Haute | SYS-REQ-020 |
| Autonomie épuisée | Haute | SYS-REQ-016 |
| Reprise après réinitialisation pendant une perfusion | Haute | SYS-REQ-038 |
| Fin de perfusion | Moyenne | SYS-REQ-032 |
| Autonomie faible | Moyenne | SYS-REQ-016 |
| Pompe programmée non démarrée, ou en pause, depuis 2 min | Moyenne | SYS-REQ-042 |
| Préalarme de fin de perfusion ou de seringue presque vide | Basse | SYS-REQ-032 |
| Bascule sur batterie | Signal d'information | SYS-REQ-015 |

### Interface et programmation

| Exigence | Intitulé | Énoncé | Source | Vérification prévue |
| --- | --- | --- | --- | --- |
| SYS-REQ-011 | Récapitulatif et confirmation | Avant le démarrage, la pompe doit afficher un récapitulatif (médicament, concentration, débit, volume, durée) et exiger une confirmation explicite distincte de la saisie. | Mesure CTRL-009 (RISK-007) ; UN-01 | essai, système logiciel (VER-013) ; essai, banc (VER-080) |
| SYS-REQ-010 | Rebond de touche | Un appui sur une touche ne doit produire qu'une seule saisie, quels que soient les rebonds du contact. | Mesure CTRL-009 (RISK-007) ; UN-03 | essai, système logiciel (VER-012) ; essai, banc (VER-079) |
| SYS-REQ-012 | Limites de dose | La pompe doit refuser toute programmation hors des limites dures du médicament et exiger une confirmation spécifique au-delà des limites souples. | Mesure CTRL-010 (RISK-007, RISK-008, RISK-009) ; UN-03 | essai, système logiciel (VER-014) ; essai, banc (VER-081) |
| SYS-REQ-013 | Affichage des unités | Chaque valeur affichée doit être accompagnée de son unité ; le médicament, la concentration et le débit doivent rester affichés pendant la perfusion. | Mesure CTRL-011 (RISK-008) ; UN-02 | inspection, système logiciel (VER-015) ; inspection, banc (VER-082) |
| SYS-REQ-024 | Identification à distance | Pendant une perfusion, l'état de la pompe, le médicament et le débit doivent être lisibles à 3 m sous l'éclairage d'une chambre d'hospitalisation. | Mesure CTRL-019 (RISK-016) ; UN-02 | essai, banc (VER-030) |
| SYS-REQ-037 | Consultation de l'historique | La pompe doit permettre de consulter sur l'appareil, sans authentification, les programmations, alarmes et actions des 24 dernières heures, datées. | UN-13, UN-16 | essai, système logiciel (VER-045) ; essai, banc (VER-096) |
| SYS-REQ-023 | Verrouillage et rôles | La pompe doit permettre de verrouiller l'interface pendant une perfusion et réserver les fonctions de configuration et de maintenance aux utilisateurs authentifiés selon leur rôle. | Mesure CTRL-018 (RISK-015, RISK-019) ; UN-12, UN-14 | essai, système logiciel (VER-029) ; essai, banc (VER-086) |
| SYS-REQ-039 | Langue de l'interface | Tous les textes de l'interface et des messages d'alarme doivent être disponibles en français. | Usage prévu H-09 | inspection, système logiciel (VER-047) ; inspection, banc (VER-098) |
| SYS-REQ-027 | Avertissement de prototype | L'appareil doit porter l'avertissement « PROTOTYPE — NE PAS UTILISER SUR UN PATIENT » et l'afficher au démarrage jusqu'à acquittement explicite. | Mesure CTRL-023 (RISK-021) ; usage prévu, avertissement | inspection, banc (VER-033) ; essai, banc (VER-034) |

### Alimentation

| Exigence | Intitulé | Énoncé | Source | Vérification prévue |
| --- | --- | --- | --- | --- |
| SYS-REQ-015 | Bascule d'alimentation | La perte de l'alimentation secteur ne doit pas interrompre ni modifier la perfusion en cours tant que la batterie n'est pas épuisée ; la bascule doit être signalée. | Mesure CTRL-013 (RISK-010) ; UN-11 | essai, banc (VER-017) |
| SYS-REQ-033 | Autonomie | Avec une batterie neuve chargée, la pompe doit perfuser au moins 4 h à 25 mL/h sur batterie, et afficher l'autonomie restante estimée. | UN-11 ; usage prévu H-07 | essai, banc (VER-041) |
| SYS-REQ-016 | Alarmes d'alimentation | La pompe doit déclencher une alarme d'autonomie faible au moins 30 min avant l'épuisement, puis une alarme de priorité haute d'autonomie épuisée au moins 5 min avant l'arrêt de la perfusion. | Mesure CTRL-013 (RISK-010) ; UN-11 ; IEC 60601-2-24 | essai, banc (VER-018) |

### Journal d'événements et datation

| Exigence | Intitulé | Énoncé | Source | Vérification prévue |
| --- | --- | --- | --- | --- |
| SYS-REQ-034 | Contenu du journal | La pompe doit journaliser chaque programmation et modification, démarrage, arrêt, bolus, purge, alarme (déclenchement, pause, acquittement, fin), changement d'alimentation, défaillance détectée et modification de configuration, avec sa date, son auteur ou son rôle lorsqu'il est connu, et conserver au moins les 10 000 derniers événements, y compris sans alimentation. | UN-14, UN-16 ; RISK-011 ; ADR #16 | essai, banc (VER-042) |
| SYS-REQ-018 | Persistance du journal | Un événement de sécurité ne doit être considéré comme enregistré qu'après son écriture confirmée sur un support durable. | Mesure CTRL-015 (RISK-011) ; UN-16 | essai, banc (VER-021) ; analyse, système logiciel (VER-022) |
| SYS-REQ-019 | Intégrité du journal | Toute altération, suppression ou retour arrière du journal d'événements doit être détectable lors de sa vérification, sur l'appareil ou hors de l'appareil. | Mesure CTRL-015 (RISK-011) ; UN-16 | essai, système logiciel (VER-023) ; analyse, système logiciel (VER-024) ; essai, banc (VER-084) |
| SYS-REQ-035 | Datation | Chaque événement doit porter une date avec une résolution de 1 s et un numéro d'ordre strictement croissant ; l'horloge doit dériver de moins de 1 min par mois, et toute mise à l'heure doit être journalisée. | UN-16 ; RISK-011 ; ADR #17 | essai, système logiciel (VER-043) ; essai, banc (VER-094) |

### Sûreté de fonctionnement, intégrité et sécurité

| Exigence | Intitulé | Énoncé | Source | Vérification prévue |
| --- | --- | --- | --- | --- |
| SYS-REQ-043 | Vitesse maximale bornée par conception | La vitesse maximale du pousseur doit être bornée par la conception mécanique et électronique, indépendamment du logiciel, de sorte que le volume délivré entre le début d'un emballement et l'état sûr reste inférieur à 0,5 mL avec la plus grande seringue de la liste vérifiée. | Mesure CTRL-024 (RISK-001, RISK-012) ; ADR 0003 | analyse, système logiciel (VER-103) ; essai, banc (VER-104) |
| SYS-REQ-044 | Programmation verrouillée par le canal de sécurité | Le canal de sécurité doit surveiller la programmation qu'il a renvoyée pour affichage au récapitulatif et que le soignant a confirmée. Il doit lire lui-même les touches Démarrer, Confirmer et Arrêt, et n'accepter une programmation que sur un appui sur Confirmer qu'il observe dans les 30 s suivant le renvoi de la copie. Il doit refuser toute nouvelle programmation tant qu'il autorise le moteur. Pendant une perfusion, il ne doit admettre que : la pause et l'arrêt, à tout moment ; la reprise, sur un appui sur Démarrer qu'il observe ; le bolus, sur un appui sur Confirmer qu'il observe, borné par la programmation confirmée ; le passage prévu au maintien de veine ouverte, déclenché par le volume qu'il mesure. Toute autre demande de la commande doit être refusée. | Mesure CTRL-025 (RISK-001, RISK-012) ; ADR 0003 | essai, système logiciel (VER-105 : refus d'une programmation, d'une reprise ou d'un bolus demandés par la commande sans appui observé par le canal de sécurité, ou confirmés hors délai ; refus d'une nouvelle programmation pendant la perfusion) ; analyse, système logiciel (VER-106) ; essai, banc (VER-112 : mêmes refus que VER-105, sur la cible matérielle, avec les touches lues par le canal de sécurité) |
| SYS-REQ-045 | Coupure par autorisation dynamique | Le pilote moteur ne doit être validé que par la combinaison matérielle d'une autorisation du canal de commande et d'une autorisation dynamique du canal de sécurité ; un processeur bloqué, réinitialisé ou hors tension doit retirer son autorisation, et l'état par défaut de la validation doit être « non validé ». | Mesure CTRL-001 (RISK-001, RISK-012) ; ADR 0003 | essai, banc (VER-107) ; analyse, système logiciel (VER-108) |
| SYS-REQ-046 | Tests de la coupure et de l'alarme autonome | À chaque démarrage, la pompe doit vérifier que chacun des deux canaux peut couper le moteur et que le circuit d'alarme autonome retentit ; tout échec doit la mettre hors service. | Mesures CTRL-001, CTRL-016 (RISK-012, RISK-014) ; ADR 0003 | essai, banc (VER-109) |
| SYS-REQ-025 | Intégrité de la bibliothèque | La pompe doit vérifier l'intégrité et l'origine de la bibliothèque de médicaments avant de l'utiliser et afficher sa version. | Mesure CTRL-022 (RISK-019, RISK-020) ; UN-14 | essai, système logiciel (VER-031) ; essai, banc (VER-087) |
| SYS-REQ-026 | Intégrité du logiciel | La pompe ne doit exécuter qu'un logiciel dont l'intégrité et l'origine ont été vérifiées, et une mise à jour interrompue ne doit pas laisser la pompe dans un état utilisable non vérifié. | Mesure CTRL-022 (RISK-019, RISK-020) | essai, système logiciel (VER-032) ; essai, banc (VER-088) |
| SYS-REQ-040 | Fonctionnement sans réseau | La pompe doit pouvoir perfuser sans aucune connexion réseau, et aucune interface de communication ne doit permettre de modifier une perfusion en cours. | Usage prévu H-08 ; RISK-015 | essai, système logiciel (VER-048) ; inspection, système logiciel (VER-049) ; essai, banc (VER-099) |
| SYS-REQ-041 | Contrôle et mise hors service | Un technicien authentifié doit pouvoir exécuter un contrôle de fonctionnement (capteurs, signaux d'alarme, précision) et mettre la pompe hors service ; une pompe hors service doit refuser toute perfusion jusqu'à sa remise en service, journalisée. | UN-15 ; scénario S7 | essai, système logiciel (VER-050) ; essai, banc (VER-100) |

## Exigences matérielles

| Exigence | Intitulé | Énoncé | Source | Vérification prévue |
| --- | --- | --- | --- | --- |
| HW-REQ-001 | Sécurité électrique | L'isolement et les courants de fuite doivent respecter les exigences de l'IEC 60601-1 applicables, vérifiées par des pré-essais (#76). | Mesure CTRL-020 (RISK-017) ; IEC 60601-1 | essai, pré-essais normatifs (VER-075) |
| HW-REQ-002 | Protection contre les liquides | Le boîtier doit atteindre l'indice de protection IPX4 et résister, sans dégradation de ses fonctions ni de ses marquages, aux produits de nettoyage et de désinfection listés dans les instructions. | Mesure CTRL-021 (RISK-018) ; UN-17 ; IEC 60529 | essai, pré-essais normatifs (VER-076) |

## Exigences logicielles

Exigences du logiciel embarqué au sens de l'IEC 62304 §5.2, dérivées des exigences système. Le logiciel est de classe C ([classification](../../software_development_file/safety-classification.md)). Les exigences logicielles propres au processeur de sécurité seront détaillées avec son implémentation ([#29](https://github.com/camille-martin-paris/clepsydre/issues/29)) ; ses exigences système sont SYS-REQ-001, 020 et 043 à 046.

| Exigence | Intitulé | Énoncé | Source | Vérification prévue |
| --- | --- | --- | --- | --- |
| SW-REQ-001 | Machine à états | Le logiciel de commande doit être une machine à états explicite (arrêt, programmation, prête, perfusion, pause, alarme, état sûr, maintenance, hors service) ; toute transition non prévue doit être refusée et journalisée. | SYS-REQ-002, SYS-REQ-029, SYS-REQ-038 ; #24 | essai, intégration (VER-051) ; analyse, unitaire (VER-052) |
| SW-REQ-002 | Validation des valeurs programmées | Le logiciel doit vérifier chaque valeur programmée par rapport aux plages de SYS-REQ-030 et SYS-REQ-031 et aux limites de la bibliothèque avant de l'accepter, et refuser explicitement toute valeur hors limites. | SYS-REQ-012, SYS-REQ-030, SYS-REQ-031 | essai, unitaire (VER-053) |
| SW-REQ-003 | Format numérique sans ambiguïté | Le logiciel doit afficher et saisir les nombres sans zéro final après la virgule et avec un zéro avant la virgule pour les valeurs inférieures à 1 ; une saisie comportant plus d'un séparateur décimal ou plus de décimales que la résolution doit être refusée. | SYS-REQ-010, SYS-REQ-013 ; mesure CTRL-009 | essai, unitaire (VER-054) |
| SW-REQ-004 | Filtrage des rebonds | Le logiciel ne doit valider un changement d'état d'une touche qu'après 20 ms de stabilité, et une touche maintenue ne doit pas répéter une saisie numérique. | SYS-REQ-010 | essai, unitaire (VER-055) |
| SW-REQ-005 | Calcul du volume sans dérive | Le logiciel doit calculer le volume délivré et le volume restant en arithmétique entière ou à virgule fixe, sans erreur d'arrondi cumulée supérieure à 0,01 mL sur toute la perfusion. | SYS-REQ-003, SYS-REQ-031 | essai, unitaire (VER-056) ; analyse, unitaire (VER-057) |
| SW-REQ-006 | Consigne d'actionneur bornée | Avant chaque envoi à l'actionneur, le logiciel doit vérifier que la consigne ne dépasse pas la borne du mode actif : en perfusion, le débit programmé ou calculé, puis le débit de maintien de veine ouverte après la fin de perfusion ; en bolus, le débit de bolus autorisé par les limites du médicament ; en purge, le débit autorisé pour la purge. Chaque borne est plafonnée par le débit maximal de la pompe, et seule la borne du mode actif s'applique ; hors de ces modes, toute consigne non nulle est hors bornes. Le mode et sa borne doivent changer ensemble, sans état intermédiaire où une consigne serait vérifiée contre la borne d'un autre mode. Une consigne hors bornes doit placer la pompe en état sûr. | SYS-REQ-001, SYS-REQ-014, SYS-REQ-029, SYS-REQ-030, SYS-REQ-032 | essai, unitaire (VER-058 : en débit continu, en volume sur durée, en maintien de veine ouverte, en bolus et en purge, et à chaque transition entre ces modes, y compris retour du bolus au débit continu) |
| SW-REQ-007 | Catalogue des alarmes | Le logiciel doit dériver toutes les alarmes d'un catalogue unique donnant, pour chaque condition, sa priorité, son délai de confirmation et l'action associée ; une condition doit être signalée 1 s au plus après son délai de confirmation. | SYS-REQ-021, SYS-REQ-022, SYS-REQ-036 ; #31 | essai, intégration (VER-059) ; inspection, unitaire (VER-060) |
| SW-REQ-008 | Enregistrement confirmé | Le service de journalisation ne doit signaler un événement comme enregistré qu'après confirmation de son écriture durable ; tout refus ou perte d'événement doit être signalé à l'appelant et traité selon une politique définie. | SYS-REQ-018, SYS-REQ-034 ; #45, #46 | essai, intégration (VER-061) |
| SW-REQ-009 | Détection d'altération du journal | Chaque entrée du journal doit porter un numéro de séquence strictement croissant et un code de détection d'altération couvrant son contenu et l'entrée précédente. | SYS-REQ-019, SYS-REQ-035 ; #47 | essai, unitaire (VER-062) ; analyse, unitaire (VER-063) |
| SW-REQ-010 | Autotests au démarrage | Au démarrage, le logiciel doit vérifier l'intégrité de la mémoire programme et des données persistantes, la mémoire vive, l'horloge, les capteurs et les signaux d'alarme ; tout échec doit mettre la pompe hors service avec un message de défaillance. | SYS-REQ-017, SYS-REQ-026 | essai, unitaire (VER-064) |
| SW-REQ-011 | Autotests périodiques | Pendant le fonctionnement, le logiciel doit revérifier l'intégrité de la mémoire programme au moins une fois par heure et la plausibilité des capteurs à chaque cycle de surveillance ; tout échec doit placer la pompe en état sûr. | SYS-REQ-017 | essai, unitaire (VER-065) |
| SW-REQ-012 | Service du chien de garde | Le chien de garde ne doit être rafraîchi qu'après vérification que chaque tâche de commande et de surveillance a terminé son cycle dans le délai prévu, jamais depuis une interruption périodique. | SYS-REQ-017 | analyse, unitaire (VER-066) ; essai, unitaire (VER-067) |
| SW-REQ-013 | Cohérence commande et supervision | Le logiciel de commande doit transmettre la programmation au canal de sécurité avant la confirmation, par une interface définie, et afficher au récapitulatif la copie renvoyée par ce canal ; tout désaccord détecté entre les deux canaux doit placer la pompe en état sûr. | SYS-REQ-001, SYS-REQ-002, SYS-REQ-044 ; ADR 0003 | essai, intégration (VER-068) |
| SW-REQ-014 | Chargement de la bibliothèque | Le logiciel ne doit charger une bibliothèque de médicaments qu'après vérification de sa signature et de son intégrité ; en cas d'échec, il doit conserver la bibliothèque valide précédente et journaliser le refus. | SYS-REQ-025 | essai, unitaire (VER-069) |
| SW-REQ-015 | Installation d'une mise à jour | Le logiciel ne doit installer une mise à jour qu'après vérification de sa signature, et l'installation doit être atomique : une interruption doit laisser la version précédente vérifiée exécutable. | SYS-REQ-026 ; #68 | essai, unitaire (VER-070) |
| SW-REQ-016 | Authentification et verrouillage | Les fonctions de configuration et de maintenance doivent exiger un code propre à chaque rôle ; la session doit se fermer après 2 min d'inactivité et chaque ouverture ou échec d'authentification doit être journalisé. | SYS-REQ-023 ; #39 | essai, unitaire (VER-071) |
| SW-REQ-017 | Données persistantes protégées | Les paramètres, la bibliothèque et l'état de perfusion enregistrés doivent être protégés par un code de détection d'erreur ; des données corrompues ne doivent jamais être utilisées. | SYS-REQ-017, SYS-REQ-025, SYS-REQ-038 | essai, unitaire (VER-072) |
| SW-REQ-018 | Temps monotone | Les délais de sécurité (confirmation d'alarme, pause, détections) doivent être mesurés sur une base de temps monotone, insensible aux mises à l'heure de l'horloge. | SYS-REQ-035 ; #49 | essai, unitaire (VER-073) ; analyse, unitaire (VER-074) |

## Valeurs proposées

Chaque valeur ci-dessous est une proposition de départ. Elle doit être confirmée, révisée ou rejetée par la relecture de ce document, puis par l'ADR ou l'essai indiqué. Une valeur révisée est modifiée dans le registre et ici, dans la même pull request.

| Exigence | Valeur proposée | Justification |
| --- | --- | --- |
| SYS-REQ-001 | Écart > max(10 %, 0,5 mL) ; vitesse hors borne sur une fenêtre de 2 s | L'écart doit rester au-delà de la précision de SYS-REQ-003 (± 5 %) pour éviter les fausses alarmes ; la fenêtre de vitesse détecte un emballement avant l'écart cumulé (ADR 0003). À confirmer avec la résolution du capteur de position. |
| SYS-REQ-002 | État sûr en 1 s au plus | Ordre de grandeur d'une boucle de surveillance embarquée ; à confirmer par l'analyse temporelle (#14). |
| SYS-REQ-003 | ± 5 % en régime établi | Hypothèse H-04 de l'usage prévu ; ordre de grandeur des pompes du marché, à confirmer sur banc (#73). |
| SYS-REQ-004 | 60 s après le temps de délivrance de 0,05 mL | Rend le délai proportionné au débit ; à 25 mL/h, environ 67 s. |
| SYS-REQ-005 | Valve anti-siphon sur chaque prolongateur admis | Seule protection qui agit encore lorsque le piston est libéré ou la seringue retirée. Principe retenu par l'auteur le 2026-10-09 ; la pression d'ouverture, qui doit dépasser la hauteur de siphonage maximale, reste à fixer avec la liste des consommables. |
| SYS-REQ-007 | 50 µL unitaire ; 1 mL cumulé sur 15 min | Valeurs usuelles des détecteurs d'air ; détecteur placé sur le prolongateur (ADR 0002). La mesure est maintenue par la révision C de l'analyse (RISK-004) ; aucune réduction sans justification. |
| SYS-REQ-008 | 30 min à 1 mL/h ; 2 min à 25 mL/h, pour chaque seringue | Débits d'essai usuels de l'IEC 60601-2-24, qui demande de publier ces délais sans fixer de limite. Avec un pousse-seringue, le délai croît avec la section et l'élasticité de la seringue : à mesurer par modèle (#74). |
| SYS-REQ-009 | < 0,2 mL, pour chaque seringue | Valeur proposée avec décompression par recul du pousseur (CTRL-008) ; à confirmer sur banc pour chaque modèle (#74). |
| SYS-REQ-016 | 30 min avant épuisement ; 5 min avant l'arrêt | Laisse le temps de rebrancher ou de remplacer la pompe ; à confirmer avec l'IEC 60601-2-24 et l'ADR #15. |
| SYS-REQ-020 | Alarme autonome d'au moins 2 min sur réserve | Laisse le temps d'entendre et de localiser l'alarme ; à dimensionner avec l'ADR #15. |
| SYS-REQ-022 | < 1 pour 100 h de perfusion | Objectif initial à mesurer sur banc (#74) et en évaluation formative (#40). |
| SYS-REQ-024 | Lisible à 3 m | Distance d'un poste de soin au lit dans une chambre ; à valider en évaluation formative (#40). |
| SYS-REQ-032 | Préalarme 1 à 30 min, 5 min par défaut ; maintien de veine ouverte ≤ min(débit programmé, 5 mL/h) | Laisse préparer la poche suivante ; le débit de maintien de veine ouverte reste borné par la prescription. |
| SYS-REQ-033 | 4 h à 25 mL/h | Hypothèse H-07 de l'usage prévu. |
| SYS-REQ-034 | 10 000 événements | Couvre plusieurs semaines d'usage intensif ; à confirmer par l'ADR #16. |
| SYS-REQ-035 | Résolution 1 s ; dérive < 1 min par mois | Ordre de grandeur d'une horloge temps réel à quartz courante ; à confirmer par l'ADR #17. |
| SYS-REQ-036 | Pause sonore de 2 min au plus | Durée usuelle de pause des signaux sonores ; l'IEC 60601-1-8 demande qu'elle soit définie, à confirmer. |
| SYS-REQ-042 | Rappel à 2 min d'une pompe non démarrée ou en pause | Évite une interruption de thérapie oubliée ; durée à valider en évaluation formative (#40). |
| SYS-REQ-043 | < 0,5 mL délivrés entre emballement et état sûr | Aligné sur le seuil d'écart de SYS-REQ-001 ; fixe la vitesse maximale avec le délai de SYS-REQ-002 (ADR 0003). |
| SYS-REQ-044 | Confirmation observée dans les 30 s suivant le renvoi de la copie | Laisse lire le récapitulatif sans qu'une confirmation tardive s'applique à une copie ancienne. Valeur retenue par l'auteur le 2026-10-09 ; reste à valider en évaluation formative (#40). |
| SYS-REQ-048 | Premier écoulement en moins de 5 min à 1 mL/h | Ordre de grandeur à comparer à l'état de l'art des pousse-seringues ; à mesurer sur banc pour chaque seringue (#73). |
| SW-REQ-004 | 20 ms de stabilité | Valeur courante pour des contacts mécaniques ; à ajuster au clavier retenu. |
| SW-REQ-005 | Erreur cumulée < 0,01 mL sur toute la perfusion | Négligeable devant la résolution de 0,1 mL. |
| SW-REQ-011 | Mémoire programme vérifiée au moins une fois par heure | À confirmer par l'analyse de la couverture de diagnostic (#14). |
| SW-REQ-016 | Fermeture de session après 2 min d'inactivité | Limite l'exposition d'une session ouverte au lit du patient. |
| HW-REQ-002 | IPX4 | Protection contre les projections d'eau ; à confirmer avec la conception du boîtier (#54). |

## Couverture des besoins utilisateurs

Chaque besoin UN-01 à UN-17 de la [spécification d'utilisation](use-specification.md) est repris par au moins une exigence :

| Besoin | Exigences |
| --- | --- |
| UN-01 | SYS-REQ-011, SYS-REQ-029, SYS-REQ-030, SYS-REQ-031 |
| UN-02 | SYS-REQ-013, SYS-REQ-024, SYS-REQ-031 |
| UN-03 | SYS-REQ-010, SYS-REQ-012, SYS-REQ-014 |
| UN-04 | SYS-REQ-005, SYS-REQ-006 |
| UN-05 | SYS-REQ-014, SYS-REQ-028 |
| UN-06 | SYS-REQ-007, SYS-REQ-008, SYS-REQ-021, SYS-REQ-042 |
| UN-07 | SYS-REQ-021 |
| UN-08 | SYS-REQ-036 |
| UN-09 | SYS-REQ-009 |
| UN-10 | SYS-REQ-032 |
| UN-11 | SYS-REQ-015, SYS-REQ-016, SYS-REQ-033, SYS-REQ-038 |
| UN-12 | SYS-REQ-023 |
| UN-13 | SYS-REQ-037 |
| UN-14 | SYS-REQ-023, SYS-REQ-025, SYS-REQ-034 |
| UN-15 | SYS-REQ-041 |
| UN-16 | SYS-REQ-018, SYS-REQ-019, SYS-REQ-034, SYS-REQ-035, SYS-REQ-037 |
| UN-17 | HW-REQ-002 |

## Limites

- Aucune exigence n'est vérifiée ; les VER-nnn sont prévues, pas réalisées.
- Les valeurs proposées ne s'appuient ni sur des essais du projet ni sur des données cliniques.
- L'aptitude à l'utilisation (IEC 62366-1) et la cybersécurité (modèle de menaces, [#67](https://github.com/camille-martin-paris/clepsydre/issues/67)) ajouteront des exigences.
- Les exigences matérielles se limitent à celles des mesures de maîtrise ; la mécanique et l'électronique seront précisées avec les épiques correspondantes.

## Historique

| Révision | Date | Auteur | Modification |
| --- | --- | --- | --- |
| A | 2026-10-09 | Ambroise Leclerc | Création ; approuvée dans [#96](https://github.com/camille-martin-paris/clepsydre/pull/96) |
| B | 2026-10-09 | Ambroise Leclerc | Application des ADR 0002 et 0003 ([#104](https://github.com/camille-martin-paris/clepsydre/issues/104)) : SYS-REQ-001, 005 à 009, 020, 030 à 032, SW-REQ-005 et SW-REQ-013 révisées ; SYS-REQ-043 à 048 ajoutées |
