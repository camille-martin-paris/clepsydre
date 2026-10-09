# Classification de sécurité du logiciel

| Champ | Valeur |
| --- | --- |
| Statut | Confirmée le 2026-10-09 par acceptation de l'analyse préliminaire des dangers ([#9](https://github.com/camille-martin-paris/clepsydre/issues/9), [#94](https://github.com/camille-martin-paris/clepsydre/pull/94), commit `75169c378cf326937f730a5a436baa69d42734f0`) ; réserve R2 levée dans [reviews.md](reviews.md) |
| Référence | IEC 62304:2006+A1:2015, §4.3 |

## Classe confirmée : C

Le système logiciel de Clepsydre est classé **C** jusqu'à ce que l'architecture et l'analyse de risques justifient une autre classe pour certains éléments.

## Justification

1. Le logiciel commande l'actionneur qui délivre le médicament et calcule le volume délivré. Une défaillance peut causer une sur-perfusion (débit trop élevé, écoulement libre, bolus non commandé) ou une sous-perfusion (arrêt non détecté, occlusion non alarmée).
2. Pour de nombreux médicaments administrés par perfusion, une sur- ou sous-perfusion peut entraîner une blessure grave ou un décès. Le dommage possible est donc grave.
3. L'IEC 62304 (§4.3) permet de tenir compte de mesures de maîtrise externes au système logiciel. Le projet prévoit un superviseur de sécurité indépendant ([#29](https://github.com/camille-martin-paris/clepsydre/issues/29), [#59](https://github.com/camille-martin-paris/clepsydre/issues/59)), mais son architecture, son indépendance et son efficacité ne sont pas encore démontrées. Il n'est donc pas pris en compte pour abaisser la classe.
4. En l'absence de mesure de maîtrise externe démontrée, la classe C s'impose.

L'[analyse préliminaire des dangers](../docs/requirements/preliminary-hazard-analysis.md) confirme cette justification : le logiciel contribue à 17 des 21 situations dangereuses, dont plusieurs peuvent entraîner un décès (G5). Aucune mesure externe au logiciel n'est encore démontrée. Camille Martin a explicitement demandé la levée de R2 après l'approbation et la fusion de #94 ; cette décision est exécutée et consignée avec l'assistance de Codex.

## Éléments logiciels

Tant que la ségrégation des éléments logiciels n'est pas décrite et justifiée par l'architecture ([#14](https://github.com/camille-martin-paris/clepsydre/issues/14)), tous les éléments héritent de la classe C (§4.3 d). Une classe inférieure ne pourra être attribuée à un élément (journalisation de diagnostic, interface de configuration, par exemple) qu'avec :

- une ségrégation documentée et vérifiée entre cet élément et les éléments de classe C ;
- une analyse montrant que sa défaillance ne peut pas contribuer à une situation dangereuse inacceptable.

Le simulateur et les outils de vérification ne sont pas embarqués ; ils sont qualifiés comme outils selon leur effet sur les preuves, pas classés.

## Conséquences

La classe C demande l'ensemble des activités de l'IEC 62304, notamment : conception détaillée de chaque unité logicielle (§5.4.2 à §5.4.4), critères d'acceptation des unités complémentaires (§5.5.4) et vérification de l'intégration complète.

## Révision

Cette classification est réexaminée à chaque révision de l'analyse de risques et de l'architecture.
