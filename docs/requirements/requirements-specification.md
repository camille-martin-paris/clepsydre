# Spécification des exigences système et logicielles

| Champ | Valeur |
| --- | --- |
| Statut | Version initiale, soumise à relecture ; toutes les exigences sont au statut `draft` |
| Révision | A |
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
- **Vérification prévue** : méthode (essai, analyse, inspection, démonstration) et identifiant VER-nnn au registre, au statut `planned`. Un identifiant VER-nnn désigne une vérification à mener, **pas un résultat**. Le niveau (unitaire, intégration, système, banc) et les critères de réussite relèvent de la stratégie de vérification ([#72](https://github.com/camille-martin-paris/clepsydre/issues/72)).
- **Dépendance au principe de pompage** : certaines valeurs dépendent du choix entre pompe volumétrique et pousse-seringue ([#13](https://github.com/camille-martin-paris/clepsydre/issues/13)). L'énoncé le précise ; l'ADR fixera la variante retenue.

## Synthèse

| Type | Nombre |
| --- | --- |
| Exigences système (SYS-REQ) | 42 |
| Exigences logicielles (SW-REQ) | 18 |
| Exigences matérielles (HW-REQ) | 2 |
| **Total** | **62** |
| Vérifications prévues (VER, `planned`) | 77 |

Les numéros SYS-REQ-001 à 028 et HW-REQ-001 à 002 viennent de l'analyse préliminaire des dangers. Leurs seuils, auparavant « à définir », sont fixés ici, et leur source mentionne désormais les besoins et les normes. Les numéros suivants sont nouveaux.

## Exigences système

### Fonctions de perfusion

| Exigence | Intitulé | Énoncé | Source | Vérification prévue |
| --- | --- | --- | --- | --- |
| SYS-REQ-029 | Modes de perfusion | La pompe doit offrir trois modes : débit continu, volume à perfuser sur une durée programmée, et bolus à la demande du soignant pendant une perfusion en débit continu ; un seul canal de perfusion est géré par appareil. | UN-01 ; usage prévu H-01, H-06 | essai (VER-037) |
| SYS-REQ-030 | Plage et résolution du débit | Le débit programmable doit couvrir 0,1 à 999 mL/h si le principe volumétrique est retenu, ou 0,1 à 200 mL/h pour un pousse-seringue (ADR #13), avec une résolution de 0,1 mL/h ; toute valeur hors de cette plage doit être refusée. | UN-01 ; usage prévu H-02, H-03 | essai (VER-038) |
| SYS-REQ-031 | Volume à perfuser | Le volume à perfuser doit être programmable de 0,1 à 9 999 mL avec une résolution de 0,1 mL ; le volume perfusé et le volume restant doivent être affichés pendant la perfusion. | UN-01, UN-02 ; usage prévu H-05 | essai (VER-039) |
| SYS-REQ-032 | Fin de perfusion | La pompe doit déclencher une préalarme de fin de perfusion un délai configurable avant la fin (1 à 30 min, 5 min par défaut). À l'atteinte du volume à perfuser, elle doit déclencher l'alarme de fin de perfusion et, selon la bibliothèque, s'arrêter ou poursuivre à un débit de maintien de veine ouverte qui ne peut dépasser ni le débit programmé ni 5 mL/h. | UN-10 ; scénario S6 ; IEC 60601-2-24 | essai (VER-040) |
| SYS-REQ-028 | Purge sur ligne non raccordée | Une purge ne doit être lancée que si la condition « ligne non raccordée au patient » a été établie immédiatement avant son lancement, y compris avant le premier démarrage d'une perfusion ; à défaut, la purge doit être refusée. Si la violation de cette condition est détectée ou signalée pendant la purge, celle-ci doit s'arrêter immédiatement, et l'utilisateur doit pouvoir l'arrêter à tout moment. Le moyen d'établir et de surveiller cette condition sera choisi avec l'architecture (#13, #14). | Mesure CTRL-012 (RISK-009) ; UN-05 ; scénario S2 | essai (VER-035) ; analyse (VER-036) |
| SYS-REQ-014 | Bolus et purge bornés | Le bolus et la purge doivent être des commandes distinctes. Le volume et le débit d'un bolus doivent être bornés par les limites configurées pour le médicament ; ceux d'une purge, par des limites propres à la purge, qui ne peuvent servir à administrer un bolus. La purge doit être refusée une fois la perfusion démarrée. | Mesure CTRL-012 (RISK-009) ; UN-03, UN-05 | essai (VER-016) |
| SYS-REQ-038 | Reprise après réinitialisation | Après une réinitialisation ou une coupure totale d'alimentation survenue pendant une perfusion, la pompe ne doit pas reprendre la perfusion d'elle-même : elle doit déclencher une alarme de priorité haute et proposer la dernière programmation, qui ne reprend qu'après confirmation par le soignant. | RISK-010, RISK-012 ; UN-11 | essai (VER-046) |

### Précision et délivrance

| Exigence | Intitulé | Énoncé | Source | Vérification prévue |
| --- | --- | --- | --- | --- |
| SYS-REQ-003 | Précision de débit | La pompe doit délivrer le débit programmé avec une erreur de ± 5 % au plus en régime établi, aux débits d'essai de l'IEC 60601-2-24, mesurée selon la méthode de cette norme (courbes en trompette publiées). | Mesure CTRL-002 (RISK-001, RISK-002) ; usage prévu H-04 ; IEC 60601-2-24 | essai (VER-004) |
| SYS-REQ-001 | Supervision indépendante du volume délivré | Un canal de sécurité indépendant du canal de commande doit mesurer le volume délivré et placer la pompe en état sûr lorsque l'écart au volume attendu dépasse le plus grand de 10 % du volume attendu et 0,5 mL. | Mesure CTRL-001 (RISK-001, RISK-012) | essai (VER-001) ; analyse (VER-002) |
| SYS-REQ-004 | Actionneur inactif | La pompe doit détecter qu'un actionneur ne délivre pas alors qu'une perfusion est en cours, au plus tard 60 s après le temps nécessaire pour délivrer 0,05 mL au débit programmé, et déclencher une alarme de priorité haute. | Mesure CTRL-003 (RISK-002) | essai (VER-005) |

### Détections et état sûr

| Exigence | Intitulé | Énoncé | Source | Vérification prévue |
| --- | --- | --- | --- | --- |
| SYS-REQ-002 | État sûr | Sur détection d'une condition dangereuse, la pompe doit, en 1 s au plus, arrêter l'actionneur, maintenir le dispositif anti-écoulement libre actif et déclencher une alarme de priorité haute. | Mesure CTRL-001 (RISK-001, RISK-012) | essai (VER-003) |
| SYS-REQ-005 | Anti-écoulement libre | Aucun écoulement libre ne doit être possible lorsque le consommable est installé, retiré, ou que la porte est ouverte. | Mesure CTRL-004 (RISK-003) ; UN-04 ; IEC 60601-2-24 | essai (VER-006) ; inspection (VER-007) |
| SYS-REQ-006 | Porte et consommable | La pompe doit refuser de démarrer, ou arrêter la perfusion et déclencher une alarme de priorité haute, si la porte est ouverte ou si le consommable est absent ou mal positionné. | Mesure CTRL-005 (RISK-002, RISK-003) ; UN-04 | essai (VER-008) |
| SYS-REQ-007 | Air dans la ligne | La pompe doit détecter une bulle d'air unitaire d'au moins 50 µL et un volume d'air cumulé d'au moins 1 mL sur 15 min, puis placer la pompe en état sûr. Ces seuils ne peuvent être configurés qu'à la baisse. | Mesure CTRL-006 (RISK-004) ; UN-06 ; IEC 60601-2-24 | essai (VER-009) |
| SYS-REQ-008 | Délai d'alarme d'occlusion | Au seuil de pression par défaut, la pompe doit déclencher l'alarme d'occlusion aval en 30 min au plus à 1 mL/h et en 2 min au plus à 25 mL/h, délais mesurés selon l'IEC 60601-2-24 et publiés avec le seuil de pression. | Mesure CTRL-007 (RISK-005) ; UN-06 ; IEC 60601-2-24 | essai (VER-010) |
| SYS-REQ-009 | Bolus post-occlusion | Le volume libéré à la levée d'une occlusion aval, mesuré selon l'IEC 60601-2-24 au seuil de pression le plus élevé, doit rester inférieur à 0,2 mL. | Mesure CTRL-008 (RISK-006) ; UN-09 ; IEC 60601-2-24 | essai (VER-011) |
| SYS-REQ-017 | Surveillance du processeur | Un chien de garde indépendant et des autotests du processeur et des mémoires doivent placer la pompe en état sûr en cas de défaillance détectée. | Mesure CTRL-014 (RISK-001, RISK-012) | essai (VER-019) ; analyse (VER-020) |

### Alarmes

| Exigence | Intitulé | Énoncé | Source | Vérification prévue |
| --- | --- | --- | --- | --- |
| SYS-REQ-021 | Priorités et signaux d'alarme | Chaque condition d'alarme doit avoir la priorité fixée par le catalogue des alarmes de ce document et être signalée par les signaux visuels et sonores de cette priorité selon l'IEC 60601-1-8 ; une alarme de priorité supérieure ne doit jamais être masquée par une alarme de priorité inférieure. | Mesure CTRL-017 (RISK-013, RISK-014) ; UN-06, UN-07 ; IEC 60601-1-8 | essai (VER-026) ; inspection (VER-027) |
| SYS-REQ-036 | Pause du signal sonore | Le soignant doit pouvoir suspendre le signal sonore d'une alarme pour 2 min au plus ; la condition reste surveillée et affichée, et le signal reprend à l'échéance si elle persiste ou dès qu'une condition de priorité supérieure apparaît. | UN-08 ; RISK-014 ; IEC 60601-1-8 | essai (VER-044) |
| SYS-REQ-042 | Rappel de pompe inactive | Une pompe programmée mais non démarrée, ou en pause, depuis 2 min doit déclencher une alarme de rappel de priorité moyenne, jusqu'au démarrage, à la reprise ou à l'annulation de la programmation. | RISK-002, RISK-014 ; UN-06 | essai (VER-077) |
| SYS-REQ-020 | Alarme de repli | Une alarme technique sonore doit retentir même si le processeur principal est défaillant. | Mesure CTRL-016 (RISK-012, RISK-014) | essai (VER-025) |
| SYS-REQ-022 | Fausses alarmes | Le taux d'alarmes interrompant à tort la perfusion doit rester inférieur à 1 pour 100 h de perfusion, mesuré sur banc et pendant l'évaluation formative. | Mesure CTRL-017 (RISK-013, RISK-014) | essai (VER-028) |

#### Catalogue des alarmes

Priorités au sens de l'IEC 60601-1-8. Le catalogue complet, avec délais de confirmation et actions, sera établi par [#31](https://github.com/camille-martin-paris/clepsydre/issues/31) ; il ne pourra abaisser aucune priorité ci-dessous sans révision de ce document.

| Condition | Priorité | Exigences |
| --- | --- | --- |
| Occlusion aval | Haute | SYS-REQ-008 |
| Air dans la ligne | Haute | SYS-REQ-007 |
| Porte ouverte ou consommable absent pendant la perfusion | Haute | SYS-REQ-006 |
| Actionneur inactif | Haute | SYS-REQ-004 |
| Écart détecté par le canal de sécurité, défaillance technique | Haute | SYS-REQ-001, SYS-REQ-002, SYS-REQ-017 |
| Autonomie épuisée | Haute | SYS-REQ-016 |
| Reprise après réinitialisation pendant une perfusion | Haute | SYS-REQ-038 |
| Fin de perfusion | Moyenne | SYS-REQ-032 |
| Autonomie faible | Moyenne | SYS-REQ-016 |
| Pompe programmée non démarrée, ou en pause, depuis 2 min | Moyenne | SYS-REQ-042 |
| Préalarme de fin de perfusion | Basse | SYS-REQ-032 |
| Bascule sur batterie | Signal d'information | SYS-REQ-015 |

### Interface et programmation

| Exigence | Intitulé | Énoncé | Source | Vérification prévue |
| --- | --- | --- | --- | --- |
| SYS-REQ-011 | Récapitulatif et confirmation | Avant le démarrage, la pompe doit afficher un récapitulatif (médicament, concentration, débit, volume, durée) et exiger une confirmation explicite distincte de la saisie. | Mesure CTRL-009 (RISK-007) ; UN-01 | essai (VER-013) |
| SYS-REQ-010 | Rebond de touche | Un appui sur une touche ne doit produire qu'une seule saisie, quels que soient les rebonds du contact. | Mesure CTRL-009 (RISK-007) ; UN-03 | essai (VER-012) |
| SYS-REQ-012 | Limites de dose | La pompe doit refuser toute programmation hors des limites dures du médicament et exiger une confirmation spécifique au-delà des limites souples. | Mesure CTRL-010 (RISK-007, RISK-008, RISK-009) ; UN-03 | essai (VER-014) |
| SYS-REQ-013 | Affichage des unités | Chaque valeur affichée doit être accompagnée de son unité ; le médicament, la concentration et le débit doivent rester affichés pendant la perfusion. | Mesure CTRL-011 (RISK-008) ; UN-02 | inspection (VER-015) |
| SYS-REQ-024 | Identification à distance | Pendant une perfusion, l'état de la pompe, le médicament et le débit doivent être lisibles à 3 m sous l'éclairage d'une chambre d'hospitalisation. | Mesure CTRL-019 (RISK-016) ; UN-02 | essai (VER-030) |
| SYS-REQ-037 | Consultation de l'historique | La pompe doit permettre de consulter sur l'appareil, sans authentification, les programmations, alarmes et actions des 24 dernières heures, datées. | UN-13, UN-16 | essai (VER-045) |
| SYS-REQ-023 | Verrouillage et rôles | La pompe doit permettre de verrouiller l'interface pendant une perfusion et réserver les fonctions de configuration et de maintenance aux utilisateurs authentifiés selon leur rôle. | Mesure CTRL-018 (RISK-015, RISK-019) ; UN-12, UN-14 | essai (VER-029) |
| SYS-REQ-039 | Langue de l'interface | Tous les textes de l'interface et des messages d'alarme doivent être disponibles en français. | Usage prévu H-09 | inspection (VER-047) |
| SYS-REQ-027 | Avertissement de prototype | L'appareil doit porter l'avertissement « PROTOTYPE — NE PAS UTILISER SUR UN PATIENT » et l'afficher au démarrage jusqu'à acquittement explicite. | Mesure CTRL-023 (RISK-021) ; usage prévu, avertissement | inspection (VER-033) ; essai (VER-034) |

### Alimentation

| Exigence | Intitulé | Énoncé | Source | Vérification prévue |
| --- | --- | --- | --- | --- |
| SYS-REQ-015 | Bascule d'alimentation | La perte de l'alimentation secteur ne doit pas interrompre ni modifier la perfusion en cours tant que la batterie n'est pas épuisée ; la bascule doit être signalée. | Mesure CTRL-013 (RISK-010) ; UN-11 | essai (VER-017) |
| SYS-REQ-033 | Autonomie | Avec une batterie neuve chargée, la pompe doit perfuser au moins 4 h à 25 mL/h sur batterie, et afficher l'autonomie restante estimée. | UN-11 ; usage prévu H-07 | essai (VER-041) |
| SYS-REQ-016 | Alarmes d'alimentation | La pompe doit déclencher une alarme d'autonomie faible au moins 30 min avant l'épuisement, puis une alarme de priorité haute d'autonomie épuisée au moins 5 min avant l'arrêt de la perfusion. | Mesure CTRL-013 (RISK-010) ; UN-11 ; IEC 60601-2-24 | essai (VER-018) |

### Journal d'événements et datation

| Exigence | Intitulé | Énoncé | Source | Vérification prévue |
| --- | --- | --- | --- | --- |
| SYS-REQ-034 | Contenu du journal | La pompe doit journaliser chaque programmation et modification, démarrage, arrêt, bolus, purge, alarme (déclenchement, pause, acquittement, fin), changement d'alimentation, défaillance détectée et modification de configuration, avec sa date, son auteur ou son rôle lorsqu'il est connu, et conserver au moins les 10 000 derniers événements, y compris sans alimentation. | UN-14, UN-16 ; RISK-011 ; ADR #16 | essai (VER-042) |
| SYS-REQ-018 | Persistance du journal | Un événement de sécurité ne doit être considéré comme enregistré qu'après son écriture confirmée sur un support durable. | Mesure CTRL-015 (RISK-011) ; UN-16 | essai (VER-021) ; analyse (VER-022) |
| SYS-REQ-019 | Intégrité du journal | Toute altération, suppression ou retour arrière du journal d'événements doit être détectable lors de sa vérification, sur l'appareil ou hors de l'appareil. | Mesure CTRL-015 (RISK-011) ; UN-16 | essai (VER-023) ; analyse (VER-024) |
| SYS-REQ-035 | Datation | Chaque événement doit porter une date avec une résolution de 1 s et un numéro d'ordre strictement croissant ; l'horloge doit dériver de moins de 1 min par mois, et toute mise à l'heure doit être journalisée. | UN-16 ; RISK-011 ; ADR #17 | essai (VER-043) |

### Sûreté de fonctionnement, intégrité et sécurité

| Exigence | Intitulé | Énoncé | Source | Vérification prévue |
| --- | --- | --- | --- | --- |
| SYS-REQ-025 | Intégrité de la bibliothèque | La pompe doit vérifier l'intégrité et l'origine de la bibliothèque de médicaments avant de l'utiliser et afficher sa version. | Mesure CTRL-022 (RISK-019, RISK-020) ; UN-14 | essai (VER-031) |
| SYS-REQ-026 | Intégrité du logiciel | La pompe ne doit exécuter qu'un logiciel dont l'intégrité et l'origine ont été vérifiées, et une mise à jour interrompue ne doit pas laisser la pompe dans un état utilisable non vérifié. | Mesure CTRL-022 (RISK-019, RISK-020) | essai (VER-032) |
| SYS-REQ-040 | Fonctionnement sans réseau | La pompe doit pouvoir perfuser sans aucune connexion réseau, et aucune interface de communication ne doit permettre de modifier une perfusion en cours. | Usage prévu H-08 ; RISK-015 | essai (VER-048) ; inspection (VER-049) |
| SYS-REQ-041 | Contrôle et mise hors service | Un technicien authentifié doit pouvoir exécuter un contrôle de fonctionnement (capteurs, signaux d'alarme, précision) et mettre la pompe hors service ; une pompe hors service doit refuser toute perfusion jusqu'à sa remise en service, journalisée. | UN-15 ; scénario S7 | essai (VER-050) |

## Exigences matérielles

| Exigence | Intitulé | Énoncé | Source | Vérification prévue |
| --- | --- | --- | --- | --- |
| HW-REQ-001 | Sécurité électrique | L'isolement et les courants de fuite doivent respecter les exigences de l'IEC 60601-1 applicables, vérifiées par des pré-essais (#76). | Mesure CTRL-020 (RISK-017) ; IEC 60601-1 | essai (VER-075) |
| HW-REQ-002 | Protection contre les liquides | Le boîtier doit atteindre l'indice de protection IPX4 et résister, sans dégradation de ses fonctions ni de ses marquages, aux produits de nettoyage et de désinfection listés dans les instructions. | Mesure CTRL-021 (RISK-018) ; UN-17 ; IEC 60529 | essai (VER-076) |

## Exigences logicielles

Exigences du logiciel embarqué au sens de l'IEC 62304 §5.2, dérivées des exigences système. Le logiciel est de classe C ([classification](../../software_development_file/safety-classification.md)). Les exigences propres au canal de sécurité indépendant seront précisées après l'ADR [#14](https://github.com/camille-martin-paris/clepsydre/issues/14).

| Exigence | Intitulé | Énoncé | Source | Vérification prévue |
| --- | --- | --- | --- | --- |
| SW-REQ-001 | Machine à états | Le logiciel de commande doit être une machine à états explicite (arrêt, programmation, prête, perfusion, pause, alarme, état sûr, maintenance, hors service) ; toute transition non prévue doit être refusée et journalisée. | SYS-REQ-002, SYS-REQ-029, SYS-REQ-038 ; #24 | essai (VER-051) ; analyse (VER-052) |
| SW-REQ-002 | Validation des valeurs programmées | Le logiciel doit vérifier chaque valeur programmée par rapport aux plages de SYS-REQ-030 et SYS-REQ-031 et aux limites de la bibliothèque avant de l'accepter, et refuser explicitement toute valeur hors limites. | SYS-REQ-012, SYS-REQ-030, SYS-REQ-031 | essai (VER-053) |
| SW-REQ-003 | Format numérique sans ambiguïté | Le logiciel doit afficher et saisir les nombres sans zéro final après la virgule et avec un zéro avant la virgule pour les valeurs inférieures à 1 ; une saisie comportant plus d'un séparateur décimal ou plus de décimales que la résolution doit être refusée. | SYS-REQ-010, SYS-REQ-013 ; mesure CTRL-009 | essai (VER-054) |
| SW-REQ-004 | Filtrage des rebonds | Le logiciel ne doit valider un changement d'état d'une touche qu'après 20 ms de stabilité, et une touche maintenue ne doit pas répéter une saisie numérique. | SYS-REQ-010 | essai (VER-055) |
| SW-REQ-005 | Calcul du volume sans dérive | Le logiciel doit calculer le volume délivré et le volume restant en arithmétique entière ou à virgule fixe, sans erreur d'arrondi cumulée supérieure à 0,01 mL sur 9 999 mL. | SYS-REQ-003, SYS-REQ-031 | essai (VER-056) ; analyse (VER-057) |
| SW-REQ-006 | Consigne d'actionneur bornée | Avant chaque envoi à l'actionneur, le logiciel doit vérifier que la consigne ne dépasse ni le débit programmé, ni le débit de bolus ou de purge autorisé, ni le débit maximal de la pompe ; une consigne hors bornes doit placer la pompe en état sûr. | SYS-REQ-001, SYS-REQ-014, SYS-REQ-030 | essai (VER-058) |
| SW-REQ-007 | Catalogue des alarmes | Le logiciel doit dériver toutes les alarmes d'un catalogue unique donnant, pour chaque condition, sa priorité, son délai de confirmation et l'action associée ; une condition doit être signalée 1 s au plus après son délai de confirmation. | SYS-REQ-021, SYS-REQ-022, SYS-REQ-036 ; #31 | essai (VER-059) ; inspection (VER-060) |
| SW-REQ-008 | Enregistrement confirmé | Le service de journalisation ne doit signaler un événement comme enregistré qu'après confirmation de son écriture durable ; tout refus ou perte d'événement doit être signalé à l'appelant et traité selon une politique définie. | SYS-REQ-018, SYS-REQ-034 ; #45, #46 | essai (VER-061) |
| SW-REQ-009 | Détection d'altération du journal | Chaque entrée du journal doit porter un numéro de séquence strictement croissant et un code de détection d'altération couvrant son contenu et l'entrée précédente. | SYS-REQ-019, SYS-REQ-035 ; #47 | essai (VER-062) ; analyse (VER-063) |
| SW-REQ-010 | Autotests au démarrage | Au démarrage, le logiciel doit vérifier l'intégrité de la mémoire programme et des données persistantes, la mémoire vive, l'horloge, les capteurs et les signaux d'alarme ; tout échec doit mettre la pompe hors service avec un message de défaillance. | SYS-REQ-017, SYS-REQ-026 | essai (VER-064) |
| SW-REQ-011 | Autotests périodiques | Pendant le fonctionnement, le logiciel doit revérifier l'intégrité de la mémoire programme au moins une fois par heure et la plausibilité des capteurs à chaque cycle de surveillance ; tout échec doit placer la pompe en état sûr. | SYS-REQ-017 | essai (VER-065) |
| SW-REQ-012 | Service du chien de garde | Le chien de garde ne doit être rafraîchi qu'après vérification que chaque tâche de commande et de surveillance a terminé son cycle dans le délai prévu, jamais depuis une interruption périodique. | SYS-REQ-017 | analyse (VER-066) ; essai (VER-067) |
| SW-REQ-013 | Cohérence commande et supervision | Le logiciel de commande doit transmettre au canal de sécurité la programmation en vigueur par une interface définie ; tout désaccord détecté entre les deux canaux doit placer la pompe en état sûr. | SYS-REQ-001, SYS-REQ-002 ; ADR #14 | essai (VER-068) |
| SW-REQ-014 | Chargement de la bibliothèque | Le logiciel ne doit charger une bibliothèque de médicaments qu'après vérification de sa signature et de son intégrité ; en cas d'échec, il doit conserver la bibliothèque valide précédente et journaliser le refus. | SYS-REQ-025 | essai (VER-069) |
| SW-REQ-015 | Installation d'une mise à jour | Le logiciel ne doit installer une mise à jour qu'après vérification de sa signature, et l'installation doit être atomique : une interruption doit laisser la version précédente vérifiée exécutable. | SYS-REQ-026 ; #68 | essai (VER-070) |
| SW-REQ-016 | Authentification et verrouillage | Les fonctions de configuration et de maintenance doivent exiger un code propre à chaque rôle ; la session doit se fermer après 2 min d'inactivité et chaque ouverture ou échec d'authentification doit être journalisé. | SYS-REQ-023 ; #39 | essai (VER-071) |
| SW-REQ-017 | Données persistantes protégées | Les paramètres, la bibliothèque et l'état de perfusion enregistrés doivent être protégés par un code de détection d'erreur ; des données corrompues ne doivent jamais être utilisées. | SYS-REQ-017, SYS-REQ-025, SYS-REQ-038 | essai (VER-072) |
| SW-REQ-018 | Temps monotone | Les délais de sécurité (confirmation d'alarme, pause, détections) doivent être mesurés sur une base de temps monotone, insensible aux mises à l'heure de l'horloge. | SYS-REQ-035 ; #49 | essai (VER-073) ; analyse (VER-074) |

## Valeurs proposées

Chaque valeur ci-dessous est une proposition de départ. Elle doit être confirmée, révisée ou rejetée par la relecture de ce document, puis par l'ADR ou l'essai indiqué. Une valeur révisée est modifiée dans le registre et ici, dans la même pull request.

| Exigence | Valeur proposée | Justification |
| --- | --- | --- |
| SYS-REQ-001 | Écart > max(10 %, 0,5 mL) | Doit rester au-delà de la précision de SYS-REQ-003 (± 5 %) pour éviter les fausses alarmes ; à revoir avec l'architecture du canal de sécurité (#14). |
| SYS-REQ-002 | État sûr en 1 s au plus | Ordre de grandeur d'une boucle de surveillance embarquée ; à confirmer par l'analyse temporelle (#14). |
| SYS-REQ-003 | ± 5 % en régime établi | Hypothèse H-04 de l'usage prévu ; ordre de grandeur des pompes du marché, à confirmer sur banc (#73). |
| SYS-REQ-004 | 60 s après le temps de délivrance de 0,05 mL | Rend le délai proportionné au débit ; à 25 mL/h, environ 67 s. |
| SYS-REQ-007 | 50 µL unitaire ; 1 mL cumulé sur 15 min | Valeurs usuelles des détecteurs d'air de pompes volumétriques ; applicabilité au pousse-seringue à revoir avec #13. |
| SYS-REQ-008 | 30 min à 1 mL/h ; 2 min à 25 mL/h | Débits d'essai usuels de l'IEC 60601-2-24, qui demande de publier ces délais sans fixer de limite ; valeurs à comparer à l'état de l'art (#13, #74). |
| SYS-REQ-009 | < 0,2 mL | Valeur proposée pour une pompe avec décompression (CTRL-008) ; à confirmer sur banc (#74). |
| SYS-REQ-016 | 30 min avant épuisement ; 5 min avant l'arrêt | Laisse le temps de rebrancher ou de remplacer la pompe ; à confirmer avec l'IEC 60601-2-24 et l'ADR #15. |
| SYS-REQ-022 | < 1 pour 100 h de perfusion | Objectif initial à mesurer sur banc (#74) et en évaluation formative (#40). |
| SYS-REQ-024 | Lisible à 3 m | Distance d'un poste de soin au lit dans une chambre ; à valider en évaluation formative (#40). |
| SYS-REQ-032 | Préalarme 1 à 30 min, 5 min par défaut ; maintien de veine ouverte ≤ min(débit programmé, 5 mL/h) | Laisse préparer la poche suivante ; le débit de maintien de veine ouverte reste borné par la prescription. |
| SYS-REQ-033 | 4 h à 25 mL/h | Hypothèse H-07 de l'usage prévu. |
| SYS-REQ-034 | 10 000 événements | Couvre plusieurs semaines d'usage intensif ; à confirmer par l'ADR #16. |
| SYS-REQ-035 | Résolution 1 s ; dérive < 1 min par mois | Ordre de grandeur d'une horloge temps réel à quartz courante ; à confirmer par l'ADR #17. |
| SYS-REQ-036 | Pause sonore de 2 min au plus | Durée usuelle de pause des signaux sonores ; l'IEC 60601-1-8 demande qu'elle soit définie, à confirmer. |
| SYS-REQ-042 | Rappel à 2 min d'une pompe non démarrée ou en pause | Évite une interruption de thérapie oubliée ; durée à valider en évaluation formative (#40). |
| SW-REQ-004 | 20 ms de stabilité | Valeur courante pour des contacts mécaniques ; à ajuster au clavier retenu. |
| SW-REQ-005 | Erreur cumulée < 0,01 mL sur 9 999 mL | Négligeable devant la résolution de 0,1 mL. |
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
| A | 2026-10-09 | Ambroise Leclerc | Création, soumise à relecture |
