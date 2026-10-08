# Clepsydre

**Projet de pompe à perfusion en C++23, open source et open hardware.**

Clepsydre vise à développer une pompe à perfusion dont le logiciel, les plans matériels et la documentation de conception sont ouverts, versionnés et reproductibles.

- **Logiciel en C++23** : commande de la pompe, gestion des états et des alarmes, diagnostic et traçabilité.
- **Open source** : code et documentation accessibles pour permettre l'étude, la revue et la contribution.
- **Open hardware** : objectif de publier les schémas électroniques, les plans mécaniques, la nomenclature et les instructions d'assemblage.

> [!CAUTION]
> **Clepsydre ne doit pas être utilisé sur un être humain ni sur un animal.**
> Aucune évaluation de conformité n'a abouti pour ce projet. Le logiciel, les plans matériels et la documentation sont fournis à des fins d'étude, de développement et d'essais sur banc uniquement, sans aucune garantie. Voir l'[usage prévu](docs/intended-use.md).

[État du projet](#état-du-projet) · [Organisation](#organisation-proposée) · [Prise en main](#prise-en-main) · [Feuille de route](#feuille-de-route) · [Licence et participation](#licence-et-participation)

## État du projet

Le dépôt est à son stade initial : il contient ce README et la licence. Le logiciel de commande, les fichiers matériels et les procédures de vérification restent à développer.

La simulation constitue une première étape pour explorer les comportements de la pompe et préparer les essais. L'intégration de [mddlog](https://github.com/ambroise-leclerc/mddlog), bibliothèque de journalisation en C++23, est une piste pour les diagnostics et les événements d'audit ; elle n'est pas encore réalisée.

## Périmètre visé

| Domaine | Objectif |
| --- | --- |
| Commande | Définir les états de fonctionnement, le pilotage de l'actionneur et les paramètres de perfusion. |
| Surveillance | Définir les capteurs, les conditions d'alarme et les réponses aux défaillances. |
| Traçabilité | Distinguer les journaux de diagnostic des événements d'audit. |
| Matériel | Documenter l'électronique, la mécanique, la nomenclature et l'assemblage. |
| Vérification | Relier les exigences aux tests logiciels, aux simulations et aux essais sur banc. |

Ces éléments décrivent les objectifs du projet, pas des fonctionnalités déjà livrées.

## Organisation proposée

L'organisation s'inspire de [mddlog](https://github.com/ambroise-leclerc/mddlog) : présentation des capacités, décisions d'architecture explicites, documentation de développement et vérifications reproductibles. L'arborescence suivante servira de guide au fur et à mesure de l'ajout des premiers livrables.

```text
clepsydre/
├── src/                       # Logiciel C++23
├── tests/                     # Tests et scénarios de vérification
├── examples/                  # Simulations et exemples d'utilisation
├── hardware/                  # Sources de conception matérielle
│   ├── electronics/           # Schémas et circuits imprimés
│   ├── mechanics/             # Plans et modèles mécaniques
│   └── bom/                   # Nomenclature des composants
├── docs/
│   ├── adr/                   # Décisions d'architecture
│   ├── requirements/          # Exigences et traçabilité
│   └── validation/            # Protocoles et résultats d'essais
├── software_development_file/ # Dossier de développement logiciel
├── README.md
└── LICENSE
```

Les répertoires sont créés au fur et à mesure, avec leur contenu.

## Prise en main

Récupérer le dépôt avec GitHub CLI :

```bash
gh repo clone camille-martin-paris/clepsydre
cd clepsydre
```

Le langage cible est **C++23**. La chaîne de compilation, les plateformes prises en charge et les commandes de test seront documentées lors de l'ajout du premier code. Aucune procédure de compilation n'est disponible à ce stade.

## Feuille de route

1. Définir le périmètre de la pompe, les exigences et les critères d'acceptation.
2. Consigner les choix d'architecture logicielle et matérielle.
3. Développer une simulation en C++23 et les premiers tests de comportement.
4. Publier les sources matérielles, la nomenclature et les instructions d'assemblage.
5. Intégrer le prototype et documenter les essais sur banc ainsi que leurs résultats.

## Vérification et validation

Les exigences, les décisions de conception et les résultats d'essais seront versionnés afin de rendre les choix et leur vérification consultables. Les capacités démontrées et les limites seront documentées pour chaque étape.

Le projet est en développement ; aucune certification ni validation pour un usage clinique n'est revendiquée.

## Licence et participation

Le dépôt contient la [licence publique de l'Union européenne 1.2 (EUPL-1.2)](LICENSE). Le projet poursuit une démarche **open source et open hardware** ; les conditions de licence propres aux futurs fichiers matériels seront précisées lors de leur publication.

Pour proposer une amélioration, signaler un problème ou discuter d'un choix de conception, utilisez les [issues GitHub](https://github.com/camille-martin-paris/clepsydre/issues). Les contributions peuvent porter sur le logiciel C++23, le matériel, la documentation ou les essais.

Les règles de branche, de commit et de revue sont décrites dans [CONTRIBUTING.md](CONTRIBUTING.md). Les contributions sont soumises au [code de conduite](CODE_OF_CONDUCT.md) et au [certificat d'origine du développeur](https://developercertificate.org/) (`git commit -s`).
