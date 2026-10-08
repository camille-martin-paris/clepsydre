# Maintenance du logiciel

Référence : IEC 62304, §6.

## Retours

Les retours (issues, résultats d'essais, signalements de sécurité, anomalies publiées des SOUP) sont recueillis par les issues GitHub et la veille des vulnérabilités ([#70](https://github.com/camille-martin-paris/clepsydre/issues/70)). Chaque retour est évalué selon le processus de [résolution des problèmes](problem-resolution.md).

## Modifications après publication

Une modification d'une version publiée suit le même processus que le développement : analyse de l'effet sur les exigences, les risques et la classification, puis vérification et nouvelle publication. Une version corrective incrémente le numéro de correctif (`vX.Y.Z+1`).

## Suivi des SOUP

Pour chaque SOUP, les anomalies publiées et les nouvelles versions sont examinées au moins à chaque publication de Clepsydre ; la date de revue est consignée dans [soup.md](soup.md) et `registry/soup.toml`.

## Limite

Le projet n'a pas de fabricant légal ni d'appareil en service : aucune surveillance après commercialisation n'est assurée. Ce rôle reviendrait à un fabricant légal ([#6](https://github.com/camille-martin-paris/clepsydre/issues/6)).
