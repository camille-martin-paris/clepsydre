# Dossier de développement logiciel

Ce dossier organise le développement du logiciel de Clepsydre selon les processus de l'IEC 62304. Il prépare le cadre ; il ne revendique aucune conformité. Les preuves (exigences, analyses de risques, résultats de vérification) sont produites par les autres épiques et reliées par le registre de traçabilité.

> [!CAUTION]
> Clepsydre ne doit pas être utilisé sur un être humain ni sur un animal.

| Document | Contenu | Référence IEC 62304 |
| --- | --- | --- |
| [Plan de développement](development-plan.md) | Cycle de vie, livrables, rôles et cumuls de rôles | §5.1 |
| [Classification de sécurité](safety-classification.md) | Classe proposée et justification | §4.3 |
| [Gestion de configuration](configuration-management.md) | Identification, versions, contrôle des modifications | §8 |
| [Résolution des problèmes](problem-resolution.md) | Traitement des anomalies | §9 |
| [Maintenance](maintenance-plan.md) | Retours, modifications après publication | §6 |
| [Composants tiers (SOUP)](soup.md) | Évaluation et suivi des composants tiers | §5.3.3, §5.3.4, §7.1.2, §8.1.2 |
| [Registre de traçabilité](registry/) | Exigences, risques, mesures de maîtrise, vérifications, SOUP | §5.1.1, §7.3.3 |
| [Stratégie de vérification](verification-strategy.md) | Niveaux, méthodes, critères de réussite et de couverture, indépendance, conservation des preuves | §5.1.6, §5.5 à §5.7 |
| [Revues et acceptations](reviews.md) | Décisions d'acceptation des documents | §5.1.1, §8.2.3 |

Les numéros de paragraphe renvoient à l'IEC 62304:2006+A1:2015. Le texte de la norme n'est pas reproduit ici.

## Registre de traçabilité

Le registre est un ensemble de fichiers TOML lisibles par machine :

```text
risque (RISK) ──maîtrisé par──▶ mesure (CTRL) ──mise en œuvre par──▶ exigence (REQ) ◀──vérifie── vérification (VER)
                                                                         ▲
                                                     composant tiers (SOUP) ──doit satisfaire──┘
```

[`tools/check_registry.py`](../tools/check_registry.py) contrôle les identifiants, les références et la complétude des chaînes ; la CI l'exécute sur chaque pull request :

```bash
python3 tools/check_registry.py
python3 -m unittest discover -s tools/tests
```

La matrice de traçabilité publiée sera générée à partir de ce registre ([#11](https://github.com/camille-martin-paris/clepsydre/issues/11)).
