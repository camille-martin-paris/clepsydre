# Trajectoire réglementaire et normes applicables

| Champ | Valeur |
| --- | --- |
| Statut | Proposition soumise à relecture |
| Issue | [#6](https://github.com/camille-martin-paris/clepsydre/issues/6) |
| Date | 2026-10-09 |

> [!IMPORTANT]
> Ce document **ne revendique aucune conformité**. Il identifie la classification probable de Clepsydre et les textes qui orientent sa conception. Il n'est pas un avis juridique ou réglementaire. Chaque affirmation renvoie à une source publique, numérotée `[Sn]`, ou porte la mention **« à confirmer »**.

## Synthèse

| Juridiction | Classification probable | Voie d'accès au marché probable | Statut |
| --- | --- | --- | --- |
| Union européenne (règlement (UE) 2017/745, MDR) | Classe IIb | Évaluation de la conformité avec un organisme notifié | À confirmer par un fabricant légal |
| États-Unis (FDA) | Classe II | Notification préalable 510(k) | À confirmer par un fabricant légal |

## Union européenne

### Qualification et classification

- Une pompe à perfusion est un dispositif médical actif au sens de l'article 2 du MDR [S1].
- **Règle 12** (annexe VIII) : les dispositifs actifs destinés à administrer des médicaments dans le corps sont de classe IIa, sauf si l'administration est potentiellement dangereuse compte tenu de la nature des substances, de la partie du corps concernée et du mode d'administration ; ils relèvent alors de la classe IIb [S1].
- Une pompe destinée à perfuser par voie intraveineuse des médicaments dont le surdosage peut être grave relève de la branche « potentiellement dangereuse » : **classe IIb probable**. Le guide MDCG 2021-24 détaille l'application de la règle 12 [S2]. *L'exemple précis des pompes à perfusion dans ce guide est à confirmer.*
- Le logiciel qui commande le dispositif relève de la même classe que le dispositif (annexe VIII, règle d'application 3.3) [S1]. Il n'est pas qualifié séparément comme logiciel dispositif médical.

### Conséquences d'une classe IIb

- L'évaluation de la conformité fait intervenir un organisme notifié, par exemple selon l'annexe IX (système de management de la qualité et évaluation de la documentation technique) [S1].
- Pour un dispositif actif de classe IIb destiné à administrer un médicament, l'article 54 prévoit une procédure de consultation dans le cadre de l'évaluation clinique [S1]. *Son application et ses exemptions éventuelles sont à confirmer.*
- Le fabricant doit notamment : mettre en œuvre un système de management de la qualité, établir la documentation technique (annexes II et III), mener une évaluation clinique, désigner une personne chargée de veiller au respect de la réglementation (article 15), attribuer un identifiant unique (IUD) et assurer la surveillance après commercialisation [S1].
- Les normes harmonisées publiées au Journal officiel donnent une présomption de conformité aux exigences générales de sécurité et de performances qu'elles couvrent ; la liste est fixée par la décision d'exécution (UE) 2021/1182 et ses modifications [S3]. *La liste des normes ci-dessous effectivement harmonisées au titre du MDR, et leurs éditions, est à confirmer à la date de toute évaluation.*
- La cybersécurité des dispositifs médicaux est traitée par le guide MDCG 2019-16 [S4].

### Fabrication interne par un établissement de santé

L'article 5, paragraphe 5, du MDR permet à un établissement de santé de fabriquer et d'utiliser en interne des dispositifs sans marquage CE, sous conditions, notamment l'absence de dispositif équivalent disponible sur le marché et un système de management de la qualité approprié [S1]. *L'applicabilité de cette voie à un dérivé de Clepsydre est à confirmer au cas par cas ; elle ne lève pas l'avertissement d'usage non clinique du projet.*

## États-Unis

- Les pompes à perfusion relèvent du 21 CFR 880.5725 (*Infusion pump*), classe II [S5]. Le code produit d'une pompe à perfusion générale est FRN [S6] ; *le code applicable dépendra de la conception retenue et est à confirmer.*
- Voie probable : notification préalable 510(k) démontrant l'équivalence substantielle avec un dispositif déjà commercialisé [S6]. La FDA a publié des recommandations propres aux pompes à perfusion pour ces dossiers [S7].
- Le système qualité relève du 21 CFR 820, qui intègre par référence l'ISO 13485:2016 depuis l'entrée en application du règlement QMSR le 2 février 2026 [S8].
- Les exigences de cybersécurité de la section 524B du Federal Food, Drug, and Cosmetic Act s'appliquent aux « cyber devices » [S9]. *Leur application à Clepsydre, qui ne prévoit pas de connexion réseau pour perfuser (hypothèse H-08 de l'usage prévu, [#2](https://github.com/camille-martin-paris/clepsydre/issues/2)), est à confirmer.*

## Normes de référence

Les normes ci-dessous orientent la conception. Leurs éditions sont celles connues à la date du document ; elles sont *à confirmer* auprès des catalogues ISO [S10] et IEC [S11] avant toute utilisation comme référence de vérification.

| Norme | Objet | Édition de référence | Utilisation dans le projet |
| --- | --- | --- | --- |
| ISO 14971 | Gestion des risques des dispositifs médicaux | 2019 | Analyse des dangers ([#9](https://github.com/camille-martin-paris/clepsydre/issues/9)), registre des risques |
| IEC 62304 | Processus du cycle de vie du logiciel | 2006 + A1:2015 | Dossier de développement ([#5](https://github.com/camille-martin-paris/clepsydre/issues/5)) |
| IEC 60601-1 | Sécurité de base et performances essentielles des appareils électromédicaux | 2005 + A1:2012 + A2:2020 | Électronique, alimentation, boîtier ; pré-essais ([#76](https://github.com/camille-martin-paris/clepsydre/issues/76)) |
| IEC 60601-1-2 | Perturbations électromagnétiques (CEM) | 2014 + A1:2020 | Conception électronique ; pré-essais CEM |
| IEC 60601-1-8 | Systèmes d'alarme | 2006 + A1:2012 + A2:2020 | Système d'alarmes ([#30](https://github.com/camille-martin-paris/clepsydre/issues/30)) |
| IEC 60601-2-24 | Exigences particulières des pompes et régulateurs de perfusion | 2012 ; *révision en cours à confirmer* | Précision de débit, occlusion, bolus ([#73](https://github.com/camille-martin-paris/clepsydre/issues/73), [#74](https://github.com/camille-martin-paris/clepsydre/issues/74)) |
| IEC 62366-1 | Ingénierie de l'aptitude à l'utilisation | 2015 + A1:2020 | Interface utilisateur ([#35](https://github.com/camille-martin-paris/clepsydre/issues/35), [#40](https://github.com/camille-martin-paris/clepsydre/issues/40)) |
| IEC 81001-5-1 | Sécurité du logiciel de santé tout au long du cycle de vie | 2021 | Cybersécurité ([#66](https://github.com/camille-martin-paris/clepsydre/issues/66)) |
| ISO 13485 | Systèmes de management de la qualité | 2016 | Responsabilité d'un futur fabricant ; le projet s'en inspire sans la mettre en œuvre |

## Ce qu'un projet ouvert peut et ne peut pas affirmer

Clepsydre n'est pas un fabricant au sens du MDR (article 2, point 30) [S1] : il ne met aucun dispositif sur le marché ni en service.

| Le projet peut affirmer | Le projet ne doit pas affirmer |
| --- | --- |
| Qu'il **s'inspire** d'une norme ou **vise** une exigence, avec le lien vers la preuve correspondante | Qu'il est **conforme** à une norme ou au MDR |
| Les résultats d'un essai précis, avec la révision testée, le protocole et les limites | Qu'il est « certifié », « marqué CE », « approuvé » ou « autorisé » par la FDA |
| Que ses sources matérielles sont publiées sous licence ouverte, et, le cas échéant, certifiées par l'OSHWA | Qu'une certification open hardware vaut aptitude à un usage médical |
| Ce que couvrent et ne couvrent pas ses vérifications | Qu'il est sûr, apte ou destiné à un usage sur un patient |

Les licences du projet excluent toute garantie ; elles ne transfèrent à personne la responsabilité d'un fabricant.

## Rôle d'un futur fabricant légal

Une personne ou une organisation qui voudrait mettre sur le marché un dispositif issu de Clepsydre en deviendrait le fabricant légal. À ce titre, elle :

- assumerait l'ensemble des obligations de l'article 10 du MDR [S1] ou du 21 CFR 807 et 820 [S8], quelle que soit la part de conception reprise du projet ;
- reprendrait sous son propre système de management de la qualité les livrables du projet (exigences, analyses de risques, code, sources matérielles, rapports d'essais), en les revérifiant ; leur caractère public ne les rend pas acceptables en l'état ;
- figerait une configuration identifiée du logiciel et du matériel, et en assurerait la maintenance et la surveillance après commercialisation ;
- traiterait les composants issus du projet comme des éléments à évaluer, au même titre que tout composant tiers (SOUP pour le logiciel).

Le projet vise à faciliter ce travail en fournissant une documentation de conception traçable ; il ne peut pas l'accomplir à la place d'un fabricant.

## Accès aux textes normatifs

- Les règlements et guides publics (MDR, MDCG, CFR, guides de la FDA) sont librement accessibles et peuvent être cités [S1]–[S9].
- Les normes ISO et IEC sont protégées par le droit d'auteur. Le projet **ne reproduit aucun texte, tableau ni figure** de ces normes : il cite leur numéro, leur titre et le numéro de paragraphe, et reformule les exigences en ses propres termes.
- Sources autorisées pour les consulter : boutiques ISO [S10] et IEC [S11], organismes nationaux de normalisation (AFNOR en France [S12]), bibliothèques universitaires abonnées. Les aperçus gratuits de la plateforme ISO OBP couvrent le sommaire et les premières sections. *L'existence d'un accès gratuit en lecture seule aux normes harmonisées en France est à confirmer auprès de l'AFNOR.*
- Une contribution qui s'appuie sur une norme indique l'édition consultée ; elle ne joint pas la norme au dépôt.

## Sources

| Réf. | Source |
| --- | --- |
| S1 | Règlement (UE) 2017/745 relatif aux dispositifs médicaux, version consolidée : https://eur-lex.europa.eu/eli/reg/2017/745/oj |
| S2 | MDCG 2021-24, *Guidance on classification of medical devices* : https://health.ec.europa.eu/medical-devices-sector/new-regulations/guidance-mdcg-endorsed-documents-and-other-guidance_en |
| S3 | Normes harmonisées au titre du MDR, Commission européenne : https://single-market-economy.ec.europa.eu/single-market/european-standards/harmonised-standards/medical-devices_en |
| S4 | MDCG 2019-16, *Guidance on cybersecurity for medical devices* : même page que [S2] |
| S5 | 21 CFR 880.5725, *Infusion pump* : https://www.ecfr.gov/current/title-21/chapter-I/subchapter-H/part-880/subpart-F/section-880.5725 |
| S6 | FDA, base de classification des produits, code FRN : https://www.accessdata.fda.gov/scripts/cdrh/cfdocs/cfPCD/classification.cfm?ID=FRN |
| S7 | FDA, *Infusion Pumps Total Product Life Cycle — Guidance for Industry and FDA Staff* (2 décembre 2014) : https://www.fda.gov/media/78369/download |
| S8 | FDA, *Quality Management System Regulation* (21 CFR 820) : https://www.fda.gov/medical-devices/postmarket-requirements-devices/quality-management-system-regulation-qmsr |
| S9 | FDA, *Cybersecurity in Medical Devices* (section 524B) : https://www.fda.gov/medical-devices/digital-health-center-excellence/cybersecurity |
| S10 | Catalogue ISO : https://www.iso.org/standards.html ; plateforme de consultation en ligne : https://www.iso.org/obp |
| S11 | Boutique IEC : https://webstore.iec.ch |
| S12 | AFNOR, boutique des normes : https://www.boutique.afnor.org |

## Révision

Ce document est réexaminé à chaque changement de l'usage prévu, à chaque nouvelle édition d'une norme citée et au plus tard à chaque jalon du projet.
