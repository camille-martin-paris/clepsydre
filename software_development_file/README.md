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

[`tools/check_registry.py`](../tools/check_registry.py) contrôle le registre ; la CI l'exécute sur chaque pull request et échoue sur la moindre erreur :

| Contrôle | Erreur signalée |
| --- | --- |
| Identifiants | Motif invalide, doublon |
| Champs | Champ obligatoire absent, mal typé ou vide ; champ inconnu ; statut, méthode ou niveau inconnu |
| Références cassées | Mesure, vérification ou SOUP qui renvoie à un élément inexistant ; source d'exigence qui cite un identifiant du registre inexistant ; référence de preuve qui n'est pas un fichier du dépôt (absente, répertoire, chemin absolu, sortie du dépôt y compris par lien symbolique, ancre seule) |
| Éléments orphelins | Risque sans mesure de maîtrise, sauf s'il est `accepted` ; exigence sans vérification, sauf si elle est `obsolete` |
| Chaînes incomplètes | Risque `controlled` sans mesure ; exigence `approved` sans vérification ; vérification `passed` ou `failed` sans référence de preuve ; exigence système ou matérielle sans vérification sur banc ni en pré-essais |
| SOUP | Version non épinglée |

[`tools/traceability_matrix.py`](../tools/traceability_matrix.py) génère la matrice de traçabilité en Markdown, uniquement si le registre est cohérent. Elle relie risques, mesures, exigences et vérifications, et donne l'état de vérification de chaque exigence. **La présence d'un lien n'est pas un résultat d'essai** : une vérification `planned` est seulement prévue, et une exigence n'est « vérifiée » que si toutes ses vérifications sont `passed`, chacune avec une référence de preuve. La CI publie la matrice dans le résumé de l'exécution et comme artefact ; la matrice d'une version publiée est reprise dans son rapport de vérification ([#77](https://github.com/camille-martin-paris/clepsydre/issues/77)).

```bash
python3 tools/check_registry.py
python3 tools/traceability_matrix.py -o matrice.md
python3 -m unittest discover -s tools/tests
```

Ces outils contrôlent la structure et la complétude du registre, pas la justesse de son contenu ; ils ne sont pas qualifiés (réserve R3, [#89](https://github.com/camille-martin-paris/clepsydre/issues/89)).
