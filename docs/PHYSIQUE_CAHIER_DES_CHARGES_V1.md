# Cahier des charges physique V1

**Statut :** proposition à valider  
**Objet :** définir ce que le premier moteur de vol crédible doit reproduire, indépendamment du moteur 3D et de la technologie retenue.

## 1. Objectif de la V1

La V1 doit reproduire de manière cohérente le **vol plané normal d'un planeur RC conventionnel** et permettre de comparer automatiquement la simulation à des données documentées ou mesurées.

Elle doit répondre à deux usages :

1. piloter le planeur avec une radio USB et retrouver des réactions plausibles ;
2. mesurer ses performances simulées sans interface graphique.

La V1 ne promet pas une fidélité complète en décrochage profond, vrille, collision ou rupture structurale.

## 2. Planeurs couverts

Le premier modèle validé sera le Graupner Topy, un planeur comportant :

- une aile principale sans ailerons ;
- un empennage en V à deux surfaces mobiles ;
- deux axes de pilotage, profondeur et direction, obtenus par mixage des gouvernes de l'empennage ;
- aucun moteur actif.

Le format doit aussi permettre ensuite un empennage conventionnel, des ailerons, des volets, des aérofreins, un moteur ou une aile volante.

Le **Generic 250 actuel n'est pas le modèle de validation** tant que ses données physiques n'ont pas été établies. Il reste un modèle de test logiciel.

Le **Graupner Topy 4522** est le candidat actuel pour devenir le premier modèle de validation. Son choix définitif dépend de l'identification et des mesures de l'exemplaire réel décrites dans `reference-aircraft/TOPY_MESURES_IRL.md`.

## 3. Domaine de vol validé

Le domaine exact dépendra du planeur de référence. La V1 vise :

- vol subsonique incompressible ;
- lancement en altitude, sans phase de remorquage ;
- vol symétrique et manœuvres modérées ;
- dérapage et roulis usuels ;
- facteur de charge positif dans l'enveloppe normale du modèle ;
- incidences allant du vol accéléré jusqu'au premier décrochage ;
- vent nul ou vent uniforme, avec rafales modérées ;
- atmosphère proche du niveau du sol.

Chaque paquet de planeur devra définir ses bornes en vitesse, incidence, nombre de Reynolds, braquage des gouvernes et centrage. Toute sortie de ce domaine sera signalée dans la télémétrie.

## 4. Phénomènes obligatoires

### 4.1 Corps rigide

- Six degrés de liberté.
- Masse, centre de gravité et tenseur d'inertie complets.
- Position et attitude indépendantes du moteur graphique.
- Vitesses linéaires et angulaires dans des repères explicitement définis.
- Gravité et atmosphère en unités SI.

### 4.2 Aérodynamique

- Portance et traînée dépendant au minimum de l'incidence et du nombre de Reynolds.
- Traînée parasite et traînée induite.
- Moments de tangage.
- Stabilité longitudinale et latérale.
- Amortissements en tangage, roulis et lacet.
- Effet proportionnel des gouvernes présentes sur le modèle.
- Lacet inverse des ailerons lorsqu'ils sont présents.
- Couplages roulis-lacet et dérapage-roulis.
- Effet des volets et aérofreins lorsqu'ils sont présents.
- Transition continue vers un décrochage simple.

Le moteur doit exposer séparément les contributions des principales surfaces afin qu'un comportement étrange soit explicable.

### 4.3 Atmosphère

- Densité, pression et température cohérentes.
- Vecteur de vent tridimensionnel fourni par le module d'environnement.
- Vent uniforme dans les premiers essais.
- Rafales déterministes à partir d'une graine connue.

### 4.4 Commandes

- Entrées normalisées et horodatées.
- Débattements asymétriques possibles.
- Différentiel d'ailerons lorsqu'ils sont présents.
- Neutres, inversions et limites explicites.
- Vitesse réaliste des servos configurable.
- Aucun lissage caché dans le moteur physique.

L'exponentiel de la radio et l'assistance clavier appartiennent au module `runtime`, pas au modèle aérodynamique.

## 5. Décrochage V1

La V1 doit fournir :

- une perte progressive de portance près de l'incidence critique ;
- une augmentation de traînée ;
- une autorité réduite des gouvernes lorsque cela est documenté ;
- une dissymétrie possible si les deux demi-ailes rencontrent des conditions différentes ;
- une récupération plausible lorsque l'incidence diminue.

La V1 ne sera pas déclarée fiable pour :

- la vrille stabilisée ;
- l'autorotation précise ;
- les écoulements massivement décollés ;
- le décrochage dynamique à fort taux d'incidence.

L'interface devra afficher **hors domaine validé** plutôt que masquer cette limite.

### 5.1 Priorité exprimée le 10 novembre 2026 : comportement réel hors enveloppe normale

L’objectif demandé dépasse la V1 : retrouver les comportements propres à chaque planeur au décrochage, en départ dissymétrique, en autorotation, en vrille et lors de la récupération. Le comportement doit découler des efforts aérodynamiques et des inerties. La stabilité numérique et la présence d’une rotation ne suffisent pas à valider le réalisme.

Le moteur Unreal calcule séparément deux demi-ailes et étend désormais les polaires sur 360° avec un raccord continu vers une approximation de plaque plane. Le Fox remplace progressivement ses dérivées de tangage/lacet après décrochage par des efforts locaux d’empennage et de dérive, incluant la vitesse de rotation. Les contributions remplacées sont retirées du calcul global. Les tests `SimPlaneur.Physics.PostStallPolar` et `SimPlaneur.Physics.FlightDeparture` vérifient continuité, dissipation, autorité à forte incidence, entrée par commandes seules en vrille oscillatoire avec phases aplaties, puis récupération. Ce sont des validations numériques d’un modèle estimé ; elles ne démontrent pas sa fidélité à un Fox réel identifié. Le diagnostic `SimPlaneur.Physics.FoxSpinDiagnostics` conserve aussi les cas qui deviennent des spirales ou ne restent pas aplatis.

Les étapes techniques et leur état sont :

1. Extension continue des polaires réalisée pour les neuf modèles. Les coefficients extrapolés restent estimés ; leur calibration et la signalisation du domaine de confiance restent à compléter.
2. Décrire les portions d’aile, leur vrillage, leur dièdre et leurs gouvernes ; calculer l’écoulement local en tenant compte de la rotation et des rafales.
3. Calcul local d’empennage horizontal et de dérive réalisé pour le Fox, avec sillage simplifié et retrait des contributions globales remplacées. La modélisation séparée du fuselage et les autres configurations d’empennage restent à développer.
4. Représenter et identifier les moments et amortissements à grande incidence. Les coefficients du vol normal ne constituent pas une extrapolation validée.
5. Vérifier les entrées et sorties à plusieurs centrages, masses et commandes. Distinguer abattée symétrique, départ d’aile, rotation transitoire, spirale, vrille entretenue et vrille aplatie. Mesurer incidence, assiette, axe et vitesse de rotation, vitesse de chute et délai de récupération sur plusieurs tours.
6. Comparer chaque modèle aux références disponibles et conserver l’incertitude quand une donnée manque. Une description de pilote est une contrainte qualitative ; une mesure de vol peut identifier des paramètres. La généralisation aux autres planeurs reste conditionnée à ces vérifications.

Pour le Topy, la référence qualitative donnée par son propriétaire est une difficulté à décrocher, une abattée suivie d’une reprise de vitesse et un louvoiement gauche/droite avec retour à la stabilité. Le louvoiement précis n’a pas encore été validé. Pour le Fox, le fabricant et la version réelle cités par le pilote sont inconnus ; le modèle numérique de 7 m et 24 kg reste une référence générique. Les paramètres géométriques, les débattements et les inerties nécessaires doivent être documentés ou marqués comme estimations.

Références méthodologiques : les travaux [NASA sur le vol après décrochage et les vrilles](https://ntrs.nasa.gov/api/citations/20070005804/downloads/20070005804.pdf) combinent données statiques et dynamiques et soulignent le besoin de validation. Les [essais FASER sur un petit appareil télépiloté](https://ntrs.nasa.gov/citations/20160010112) montrent aussi qu’une recherche d’équilibre peut ne trouver aucune vrille pour la configuration étudiée. Ces méthodes orientent le développement ; leurs coefficients ne sont pas ceux de nos planeurs.

## 6. Sol et lancement

Inclus en V1 :

- lancement directement dans une condition de vol définie ;
- détection du passage sous le terrain ;
- arrêt ou réinitialisation contrôlée après contact.

Reporté :

- roulement réaliste ;
- train d'atterrissage détaillé ;
- lancer main animé ;
- remorquage, treuil et sandow ;
- rebonds et dommages.

## 7. Architecture temporelle

- Pas physique fixe configurable, référence initiale : **120 Hz**.
- Commandes appliquées avec leur horodatage au pas physique approprié.
- Affichage interpolé, sans réinjecter sa pose dans la physique.
- Possibilité d'exécuter plus vite que le temps réel pour les analyses.
- Possibilité de mettre en pause et d'avancer pas à pas.
- Graine contrôlée pour tout phénomène stochastique.

### Critères numériques

- Un même scénario, une même configuration et une même graine produisent le même résultat sur une même version et une même plateforme.
- Entre des calculs à 120 Hz et 240 Hz, les métriques stabilisées ne doivent pas différer de plus de **1 %**, hors événement discontinu documenté.
- Aucun `NaN`, infini ou divergence silencieuse dans le domaine déclaré.
- Le solveur doit soutenir le temps réel avec au moins **4 fois de marge** sur la machine de développement pour un planeur et un environnement V1.

## 8. Conventions communes

### Unités internes

Le contrat public utilise exclusivement le SI :

- mètre, seconde, kilogramme ;
- newton et newton-mètre ;
- radian et radian par seconde ;
- pascal et kelvin.

Toute bibliothèque utilisant des pieds, livres ou degrés effectue ses conversions à une frontière unique, couverte par des tests.

### Repères

- Corps avion : **X vers l'avant, Y vers la droite, Z vers le bas**.
- Attitude canonique stockée sous forme de quaternion normalisé.
- Le repère monde sera fixé dans le contrat `core`; les adaptateurs des moteurs 3D n'en changent jamais la signification.

## 9. Entrées minimales du modèle physique

### Géométrie

- surface, envergure et corde de référence ;
- position et orientation des surfaces portantes ;
- profils et cordes locales ;
- flèche, dièdre et vrillage ;
- géométrie et débattements des gouvernes.

### Masse

- masse totale ;
- centre de gravité ;
- tenseur d'inertie ;
- méthode ou source ayant fourni ces valeurs.

### Aérodynamique

- polaires ou coefficients avec axes et unités ;
- domaine en incidence et Reynolds ;
- dérivées de stabilité et d'amortissement ;
- effets des gouvernes ;
- provenance : mesure, calcul, estimation ou réglage empirique.

Une valeur manquante ne doit jamais être remplacée silencieusement.

## 10. Télémétrie obligatoire

Chaque trame de diagnostic doit pouvoir exposer :

- état complet du corps rigide ;
- incidence, dérapage et nombre de Reynolds ;
- vitesse air et vitesse sol ;
- portance, traînée, force latérale ;
- moments de roulis, tangage et lacet ;
- contribution par aile, empennage, fuselage et gouverne lorsque le modèle le permet ;
- commandes réellement appliquées ;
- vent local ;
- indicateurs de décrochage et de sortie du domaine validé ;
- énergie potentielle, cinétique et dissipation estimée.

## 11. Essais automatiques obligatoires

### Tests élémentaires

- unités et changements de repère ;
- gravité seule ;
- symétrie gauche/droite ;
- signe de chaque commande ;
- conservation de l'état sans force ;
- stabilité du pas d'intégration ;
- valeurs invalides et données manquantes.

### Essais de vol

1. recherche du vol plané stabilisé à plusieurs vitesses ;
2. polaire vitesse/taux de chute ;
3. finesse maximale et vitesse associée ;
4. vitesse de décrochage dans la configuration validée ;
5. réponse à un échelon de profondeur ;
6. réponse à un échelon d'ailerons lorsque le modèle en comporte ;
7. réponse à un échelon de direction, y compris le mixage de l'empennage en V ;
8. stabilité phugoïde et courte période lorsque mesurables ;
9. effet de plusieurs positions du centre de gravité ;
10. réponse à une rafale reproductible.

Tous les essais fonctionnent sans moteur graphique et exportent leurs entrées, résultats et version du modèle.

## 12. Critères de fidélité du planeur de référence

Les tolérances ne s'appliquent qu'aux grandeurs disposant d'une référence suffisamment fiable.

| Grandeur | Cible V1 initiale |
|---|---:|
| Vitesse de meilleur plané | écart ≤ 10 % |
| Finesse maximale | écart ≤ 15 % |
| Taux de chute minimal | écart ≤ 15 % |
| Vitesse de décrochage | écart ≤ 10 % |
| Vitesse de trim pour plusieurs centrages | écart ≤ 10 % |
| Constantes de temps des réponses aux commandes | écart ≤ 20 % |
| Signe des couplages et tendances de stabilité | 100 % correct |

Ces valeurs sont des objectifs de départ. Un résultat ne sera pas déclaré validé si l'incertitude de la mesure réelle est supérieure à la tolérance annoncée.

## 13. Performances ressenties

Le comportement piloté doit également satisfaire :

- aucune marche visible due au rythme de télémétrie ;
- aucune oscillation créée par l'interpolation visuelle ;
- latence mesurée entre l'entrée radio et l'état physique ;
- aucune dépendance du vol à la fréquence d'affichage ;
- réactions continues autour du neutre ;
- commandes et gouvernes visuelles cohérentes en signe et en amplitude.

La sensation au manche complète les mesures, mais ne les remplace pas.

## 14. Hors périmètre V1

- fidélité de vrille ;
- voltige extrême ;
- flutter et déformation structurale ;
- casse et dommages ;
- propulsion électrique validée ;
- eau, pluie, givrage et compressibilité ;
- interaction aérodynamique détaillée avec les obstacles ;
- plusieurs planeurs en interaction ;
- remorquage et treuil.

## 15. Livrables du jalon physique

- contrat d'entrées/sorties du moteur ;
- un planeur de référence documenté ;
- un exécutable ou module de simulation sans rendu ;
- une suite d'essais reproductibles ;
- un rapport comparant simulation et références ;
- une fiche indiquant le domaine validé et les limites ;
- un adaptateur vers le prototype graphique et la T16S.

## 16. Décisions à prendre après validation de ce document

1. Choisir le planeur physique de référence.
2. Déterminer quelles mesures réelles sont accessibles.
3. Comparer JSBSim enrichi, solveur spécialisé et architecture hybride sur le même banc d'essai.
4. Choisir la méthode de production des polaires et dérivées.
5. Fixer la première version du contrat `AircraftPhysics`.

