# Résolution des problèmes

Référence : IEC 62304, §9.

## Signalement

Tout problème (anomalie du logiciel, du matériel ou de la documentation, résultat d'essai non conforme, anomalie d'un SOUP) est consigné dans une issue créée avec le modèle « Anomalie ». Les vulnérabilités suivent le signalement privé décrit dans [CONTRIBUTING.md](../CONTRIBUTING.md).

## Traitement

1. **Qualification** : reproduction, révision concernée, domaine.
2. **Analyse de criticité** : effet possible sur la sécurité, en lien avec l'analyse de risques ; ajout éventuel d'un risque au registre.
3. **Décision** : correction, report justifié ou rejet justifié, consigné dans l'issue.
4. **Correction** : pull request liée à l'issue, avec les tests qui démontrent la correction et l'absence de régression.
5. **Vérification** : la correction est vérifiée avant clôture de l'issue.
6. **Tendances** : les anomalies sont revues à chaque publication pour repérer des causes communes.

## Anomalies résiduelles

Les anomalies connues non corrigées au moment d'une publication sont listées dans les notes de version, avec la justification de leur acceptation au regard des risques.
