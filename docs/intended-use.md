# Usage prévu de Clepsydre

| Champ | Valeur |
| --- | --- |
| Statut | Proposition soumise à la relecture de la mainteneuse |
| Issue | [#2](https://github.com/camille-martin-paris/clepsydre/issues/2) |
| Épique | [#1](https://github.com/camille-martin-paris/clepsydre/issues/1) |

Ce document décrit l'usage **visé** par le projet. Il oriente les besoins utilisateurs, les exigences et l'analyse de risques ; il ne constitue ni une revendication de conformité, ni une autorisation d'emploi. Les éléments marqués « hypothèse » doivent être confirmés ou révisés par le recueil des besoins ([#8](https://github.com/camille-martin-paris/clepsydre/issues/8)) et l'analyse préliminaire des dangers ([#9](https://github.com/camille-martin-paris/clepsydre/issues/9)).

## Avertissement

> [!CAUTION]
> **Clepsydre ne doit pas être utilisé sur un être humain ni sur un animal.**
> Aucune évaluation de conformité n'a abouti pour ce projet. Le logiciel, les plans matériels et la documentation sont fournis à des fins d'étude, de développement et d'essais sur banc uniquement, sans aucune garantie. Ils ne doivent pas servir à administrer un médicament ou un liquide à un patient, ni à prendre une décision de soin.

Ce texte de référence est reproduit, sans modification de sens :

| Support | Emplacement | Responsable de la mise en place |
| --- | --- | --- |
| README | Encart en tête du fichier | Ce document ([#2](https://github.com/camille-martin-paris/clepsydre/issues/2)) |
| Version publiée | Notes de version et archive publiée | Processus de publication (dossier de développement, [#5](https://github.com/camille-martin-paris/clepsydre/issues/5)) |
| Appareil | Étiquette apposée sur le boîtier et écran de démarrage | Conception du boîtier et de l'interface ([#54](https://github.com/camille-martin-paris/clepsydre/issues/54), [#36](https://github.com/camille-martin-paris/clepsydre/issues/36)) |

Sur l'appareil, la forme courte est : « PROTOTYPE — NE PAS UTILISER SUR UN PATIENT ». L'écran de démarrage l'affiche jusqu'à acquittement explicite par l'opérateur.

L'avertissement ne pourra être retiré qu'après l'aboutissement d'une évaluation de conformité menée par un fabricant légal (voir [#6](https://github.com/camille-martin-paris/clepsydre/issues/6)). Ce retrait fera l'objet d'une décision consignée.

## Déclaration d'usage prévu

### Fonction

Clepsydre est une pompe à perfusion destinée à administrer par voie intraveineuse, de façon contrôlée et programmée, un médicament ou une solution liquide, à un débit ou selon un volume définis par l'utilisateur, et à alerter l'utilisateur lorsque la perfusion ne se déroule pas comme programmé (occlusion, air dans la ligne, fin de perfusion, défaillance technique, autonomie faible).

Le principe retenu est le **pousse-seringue** ([ADR 0002](adr/0002-principe-de-pompage.md)) : le médicament est contenu dans une seringue standard de 60 mL au plus, dont la pompe pousse le piston. Clepsydre est donc destiné aux **perfusions de petit volume**.

### Population de patients (hypothèse)

- Adultes, en dehors de la néonatalogie et de la pédiatrie.
- La néonatalogie et la pédiatrie sont exclues du périmètre initial : les débits très faibles et les volumes de bolus admissibles y imposent des performances et des alarmes spécifiques.

### Utilisateurs prévus (hypothèse)

- Professionnels de santé formés à l'emploi des pompes à perfusion : infirmiers et infirmières, médecins.
- Personnel technique biomédical pour l'installation, la configuration de la bibliothèque de médicaments, la maintenance et la calibration.
- Les patients et les aidants ne sont pas des utilisateurs prévus dans le périmètre initial.

### Environnement d'utilisation (hypothèse)

- Établissement de soins : unités d'hospitalisation, hors transport et hors environnements soumis à des contraintes particulières (imagerie par résonance magnétique, caisson hyperbare, atmosphère explosive).
- Le domicile est **hors du périmètre initial** : il ferait du patient ou d'un aidant un utilisateur et modifierait les exigences d'aptitude à l'utilisation, d'alarmes et d'alimentation. Son inclusion éventuelle passera par une révision de ce document.

### Contre-indications et limites (hypothèse)

- Aucune contre-indication clinique n'est revendiquée : le projet ne dispose d'aucune donnée clinique.
- Exclusions de périmètre :
  - perfusions dont la défaillance ne laisse aucun délai de réaction, par exemple les médicaments vasoactifs à demi-vie très courte, tant que les performances de reprise et de précision à bas débit ne sont pas démontrées ;
  - transfusion de sang et de produits sanguins ;
  - nutrition entérale ;
  - voies péridurale et intrathécale ;
  - analgésie contrôlée par le patient (PCA).
- Perfusions de grand volume (hydratation, poches de solutés) : le volume d'une perfusion est limité à celui de la seringue installée (ADR 0002).
- Les consommables compatibles (seringues, prolongateurs) seront limités à une liste explicitement vérifiée ; chaque modèle de seringue de la liste est identifié par la pompe, et seuls les prolongateurs munis d'une valve anti-siphon sont admis.

## Hypothèses d'usage orientant les exigences

Les valeurs ci-dessous sont des hypothèses de travail destinées à dimensionner les premières exigences. Elles seront confirmées, révisées ou rejetées par [#8](https://github.com/camille-martin-paris/clepsydre/issues/8) et [#10](https://github.com/camille-martin-paris/clepsydre/issues/10).

| Identifiant | Hypothèse | Valeur de travail | Effet sur la conception |
| --- | --- | --- | --- |
| H-01 | Types de perfusion | Débit continu ; volume à perfuser sur une durée ; bolus borné à la demande du soignant | Modes de la machine à états ([#24](https://github.com/camille-martin-paris/clepsydre/issues/24), [#25](https://github.com/camille-martin-paris/clepsydre/issues/25)) |
| H-02 | Plage de débit | 0,1 à 200 mL/h, limitée en outre par le modèle de seringue | Pousse-seringue retenu (ADR 0002) ; mécanisme et vitesse maximale ([#52](https://github.com/camille-martin-paris/clepsydre/issues/52)) |
| H-03 | Résolution de programmation du débit | 0,1 mL/h | Interface de programmation et asservissement ([#26](https://github.com/camille-martin-paris/clepsydre/issues/26)) |
| H-04 | Précision de débit visée | ± 5 % en régime établi, mesurée selon la méthode de l'IEC 60601-2-24 | Banc de précision ([#73](https://github.com/camille-martin-paris/clepsydre/issues/73)) |
| H-05 | Volume à perfuser programmable | 0,1 mL au volume utile de la seringue installée (60 mL au plus) | Comptage de volume, alarme de fin de perfusion et position du piston |
| H-06 | Un canal de perfusion par appareil | Pas de perfusion multiple ni de séquencement entre appareils | Simplicité de la machine à états et de l'interface |
| H-07 | Autonomie sur batterie | Au moins 4 h au débit de 25 mL/h | Architecture d'alimentation ([#15](https://github.com/camille-martin-paris/clepsydre/issues/15)) |
| H-08 | Fonctionnement autonome | Aucune connexion réseau requise pour perfuser | Surface d'attaque réduite ([#67](https://github.com/camille-martin-paris/clepsydre/issues/67)) |
| H-09 | Langue de l'interface | Français, puis autres langues | Internationalisation de l'interface |

## Historique

| Révision | Date | Auteur | Modification |
| --- | --- | --- | --- |
| A | 2026-10-09 | Contributeurs Clepsydre | Création, soumise à relecture |
| B | 2026-10-09 | Ambroise Leclerc | Pousse-seringue retenu (ADR 0002) : périmètre restreint aux perfusions de petit volume ; H-02 et H-05 révisées ([#104](https://github.com/camille-martin-paris/clepsydre/issues/104)) |
