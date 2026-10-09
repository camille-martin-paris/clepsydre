# Plan de gestion des risques

| Champ | Valeur |
| --- | --- |
| Statut | Version initiale, soumise à relecture |
| Révision | A |
| Issue | [#9](https://github.com/camille-martin-paris/clepsydre/issues/9) |
| Référence | ISO 14971:2019, §4.4 (plan de gestion des risques) ; ISO/TR 24971:2020 (recommandations d'application). Le texte des normes n'est pas reproduit. |

> [!CAUTION]
> Clepsydre ne doit pas être utilisé sur un être humain ni sur un animal. Ce plan organise la gestion des risques de la **conception** ; il ne démontre aucune sécurité.

## Périmètre

Le plan couvre Clepsydre tel que décrit par l'[usage prévu](../intended-use.md) et la [spécification d'utilisation](use-specification.md) : logiciel, électronique, mécanique, alimentation et interface consommable. Il couvre les phases de conception et de vérification menées par le projet.

Hors périmètre, faute de fabricant légal ([trajectoire réglementaire](../regulatory-roadmap.md)) : production, distribution, installation chez un utilisateur et surveillance après commercialisation. Un futur fabricant devra reprendre ce plan dans son propre système de management de la qualité.

## Responsabilités

Les rôles sont ceux du [plan de développement](../../software_development_file/development-plan.md#rôles-et-responsabilités) :

| Activité | Rôle |
| --- | --- |
| Identification des dangers, estimation et proposition des mesures | Auteur de l'analyse |
| Revue de l'analyse et des mesures | Relecteur |
| Acceptation des critères, des risques résiduels et du rapport de gestion des risques | Approbatrice (mainteneuse) |

Le cumul des rôles de relectrice et d'approbatrice est consigné, comme pour l'ensemble du dossier (réserve R1).

## Critères d'acceptabilité des risques

### Gravité

| Niveau | Désignation | Description du dommage |
| --- | --- | --- |
| G5 | Catastrophique | Décès |
| G4 | Critique | Atteinte permanente ou menaçant le pronostic vital |
| G3 | Sérieuse | Atteinte nécessitant une intervention médicale ou prolongeant l'hospitalisation |
| G2 | Mineure | Atteinte temporaire ne nécessitant pas d'intervention médicale |
| G1 | Négligeable | Gêne ou inconfort passager |

### Probabilité d'occurrence du dommage

| Niveau | Désignation | Ordre de grandeur indicatif, par appareil et par heure d'utilisation |
| --- | --- | --- |
| P5 | Fréquente | ≥ 10⁻³ |
| P4 | Probable | 10⁻⁴ à 10⁻³ |
| P3 | Occasionnelle | 10⁻⁶ à 10⁻⁴ |
| P2 | Rare | 10⁻⁸ à 10⁻⁶ |
| P1 | Improbable | < 10⁻⁸ |

Les ordres de grandeur sont indicatifs et **à confirmer** : le projet ne dispose ni de données de terrain ni de données de fiabilité. Ils seront justifiés lors de l'estimation détaillée.

### Matrice d'acceptabilité

| Gravité \ Probabilité | P1 | P2 | P3 | P4 | P5 |
| --- | --- | --- | --- | --- | --- |
| **G5** | Acceptable après réduction | Non acceptable | Non acceptable | Non acceptable | Non acceptable |
| **G4** | Acceptable | Acceptable après réduction | Non acceptable | Non acceptable | Non acceptable |
| **G3** | Acceptable | Acceptable | Acceptable après réduction | Non acceptable | Non acceptable |
| **G2** | Acceptable | Acceptable | Acceptable | Acceptable après réduction | Non acceptable |
| **G1** | Acceptable | Acceptable | Acceptable | Acceptable | Acceptable après réduction |

« Acceptable après réduction » : le risque n'est acceptable qu'après mise en œuvre de toutes les mesures de maîtrise réalisables. Dans tous les cas, et conformément à l'objectif de réduction des risques « autant que possible » du règlement (UE) 2017/745 (annexe I, point 2), une mesure réalisable qui réduit un risque est mise en œuvre, même si le risque est déjà acceptable.

### Logiciel

Pour une contribution du logiciel à une situation dangereuse, la probabilité d'une défaillance logicielle n'est pas estimée : elle est prise égale à 1, comme le recommande la pratique associée à l'IEC 62304. Le risque est alors maîtrisé par des mesures qui ne dépendent pas du seul logiciel défaillant (supervision indépendante, mesure matérielle, état sûr) ou par la réduction de la gravité.

### Analyse préliminaire

L'analyse préliminaire des dangers estime la **gravité** de chaque dommage, mais pas encore sa probabilité : l'architecture n'est pas choisie ([#12](https://github.com/camille-martin-paris/clepsydre/issues/12)). Toute situation dangereuse de gravité G3 à G5 reçoit au moins une mesure de maîtrise proposée. La probabilité sera estimée lors de l'analyse détaillée, une fois l'architecture connue.

## Maîtrise des risques

Les mesures sont choisies dans l'ordre de priorité de l'ISO 14971 :

1. **conception intrinsèquement sûre** : supprimer le danger ou réduire la probabilité ou la gravité par la conception ;
2. **mesures de protection** dans le dispositif ou dans le processus de fabrication : alarmes, supervision, état sûr ;
3. **informations de sécurité** : avertissements, instructions, formation. Elles ne remplacent jamais une mesure des deux premiers types quand celle-ci est réalisable.

Chaque mesure est mise en œuvre par au moins une exigence. Le [registre de traçabilité](../../software_development_file/registry/) relie risques (`RISK-nnn`), mesures (`CTRL-nnn`), exigences et vérifications ; sa cohérence est contrôlée en CI.

## Vérification

- **Mise en œuvre** : chaque exigence issue d'une mesure est vérifiée (test, analyse, inspection ou démonstration), et la vérification est inscrite au registre.
- **Efficacité** : l'efficacité de chaque mesure est démontrée par un essai ou une analyse qui reproduit la situation dangereuse, par exemple une occlusion provoquée sur banc ([#74](https://github.com/camille-martin-paris/clepsydre/issues/74)) ou un défaut injecté dans le simulateur ([#21](https://github.com/camille-martin-paris/clepsydre/issues/21)).
- **Nouveaux dangers** : chaque mesure est examinée pour les dangers qu'elle pourrait introduire, par exemple une fausse alarme créée par une détection trop sensible.

## Risque résiduel global

Le risque résiduel global ne peut être évalué qu'au regard des bénéfices cliniques. Le projet ne disposant d'aucune donnée clinique, cette évaluation revient à un futur fabricant légal. Le projet documente les risques résiduels de chaque situation dangereuse et leur justification.

## Revue et mise à jour

Le dossier de gestion des risques est révisé :

- à chaque modification de l'usage prévu, de la spécification d'utilisation ou de l'architecture ;
- à chaque anomalie ayant un effet possible sur la sécurité ([résolution des problèmes](../../software_development_file/problem-resolution.md)) ;
- avant chaque version publiée, avec un rapport de gestion des risques accepté par l'approbatrice.

## Documents du dossier de gestion des risques

| Document | Contenu |
| --- | --- |
| Ce plan | Organisation, critères |
| [Analyse préliminaire des dangers](preliminary-hazard-analysis.md) | Dangers, situations dangereuses, dommages, gravité, mesures proposées |
| [Registre](../../software_development_file/registry/) : `risks.toml`, `controls.toml`, `requirements.toml` | Traçabilité lisible par machine |
| Analyses détaillées (à venir) | AMDEC de conception, analyse des défaillances logicielles, après le choix de l'architecture |
| Rapport de gestion des risques (à venir) | Par version publiée |

## Historique

| Révision | Date | Auteur | Modification |
| --- | --- | --- | --- |
| A | 2026-10-09 | Ambroise Leclerc | Création, soumise à relecture |
