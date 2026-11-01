# Ça plane ! — simulateur Unreal

Le moteur C++ du simulateur calcule le vol du Graupner Topy 4522 à 120 Hz. Le modèle en unités SI se trouve dans `Content/FlightModels/Topy4522.json`. Il reprend les polaires et les réglages d’empennage du prototype piloté à la TX16S. Par rapport à ce prototype, les inerties du réglage actuel sont réduites de 8 % en roulis, 6 % en lacet et 3 % en tangage.

## Contenu du prototype

- calcul local des forces et des moments, vent relatif et mixage de l’empennage en V
- interpolation de la pose entre deux pas de physique
- clavier progressif : haut/bas pour la profondeur, A/D pour la direction ;
- caméra sol, poursuite et poursuite large avec C pendant le vol ; marche et regard à la souris avant le lancer ;
- Topy procédural de secours ;
- Topy en FBX avec sa livrée Meshy et trois niveaux de détail pour le vol ;
- pente procédurale, vallée, champs, arbres, rochers, atmosphère, brouillard et nuages volumétriques ;
- surface de contact du relief et du lac, cylindre par arbre et enveloppe convexe des rochers ;
- HUD avec vitesse, altitude, taux de chute et fréquence de calcul du vol.

## Exploration noclip

**Explorer la scène · noclip** dans le menu, ou **N** en vol, détache la caméra et suspend le planeur et le vent. Souris pour regarder ; **ZQSD**, **WASD** ou les flèches pour se déplacer ; **Espace** pour monter et **Ctrl** pour descendre. **Maj** multiplie la vitesse par dix. La **molette** règle la vitesse de base de 2 à 500 m/s (30 m/s au départ). La caméra traverse librement le relief et les objets. Le HUD affiche la position en mètres et la distance horizontale au pilote.

**N** installe le pilote sur le sol à la position horizontale explorée et passe en **mode marche**, planeur en main. **WASD / ZQSD** ou les flèches permettent de marcher à 1,2 m/s pour affiner le placement ; **J/L** restent des raccourcis de pas latéral avant le lancer. La marche suit le sol physique, évite l’eau et les dénivelés infranchissables et s’arrête contre les arbres et les rochers. La souris permet de regarder librement ; le cap horizontal du regard oriente le planeur.

**Espace** déclenche le lancer et verrouille immédiatement la marche et le regard à la souris. La caméra suit ensuite le planeur et le pilote reste fixe. **R** remet le planeur en main au même endroit et réactive le mode marche. **N** permet à tout moment de repartir en noclip. Le site reste actif pendant la session, même en changeant de modèle. Au-dessus de l’eau ou hors du terrain, le mode noclip reste ouvert avec une indication pour choisir un autre emplacement. **Échap** ouvre le menu.

## Prérequis

- Unreal Engine 5.8 installé par Epic Games Launcher ;
- Visual Studio Build Tools 2022 avec C++ et Windows SDK pour compiler le projet.

## Lancement

Double-cliquer sur `Lancer-Unreal.cmd` à la racine du dépôt. Au premier démarrage, la compilation des shaders et du projet peut prendre plusieurs minutes.

Dans **Graphismes**, chaque case affiche ou masque l’herbe ou les filets d’air. Son curseur règle la portée depuis la caméra entre 20 et 500 m. **Appliquer** enregistre les deux réglages. Le raccourci **F** commande aussi la case des filets.

L’accueil survole la prairie du lac. Le hangar permet de sélectionner le planeur. **Prendre l’air** place le modèle en attente à droite du pilote, à hauteur de main. **Espace** déclenche un geste de lancer droit de 0,30 s, puis une libération à environ 8 m/s vers la pente, légèrement vers le haut. Le vent relatif et la physique prennent alors le relais. **R** remet le planeur en main, y compris après un contact, sans lancer automatiquement. Le lancer est attribuable dans **Périphériques → Simulateur → Lancer à la main**. **Échap** rouvre le menu et suspend la physique et le geste jusqu’à la reprise.

Dans **Périphériques**, choisir la radio ou le clavier, une famille, puis une fonction. Cliquer sur **Attribuer** et déplacer la commande vers le sens positif désiré. Les axes, curseurs, boutons et directions de chapeau exposés à Windows sont détectés. Le profil clavier accepte deux touches pour les commandes à deux sens. **Échap** annule l’apprentissage. Les affectations, inversions et neutres sont mémorisés séparément pour chaque radio. Une entrée déjà utilisée peut être déplacée ou partagée explicitement.

Les manches de direction et de profondeur du Topy conservent leurs affectations initiales. Ses trims acceptent un axe ou deux boutons par pas de 2 %, dans une plage de ±25 %. Les petits débattements réduisent les ordres à 70 %. Les raccourcis de caméra, d’instruments et de relance sont aussi attribuables. **Échap** reste disponible pour ouvrir le menu.

Pour préparer d’autres planeurs, les familles proposent notamment ailerons, volets, courbure, aérofreins, butterfly, phases de vol, moteur, train, crochet, ballast et quatre voies auxiliaires. Le panneau indique les fonctions disponibles sur le Topy. Une commande proportionnelle peut utiliser un axe, un bouton tout-ou-rien ou deux boutons par crans pour les courses de 0 à 100 %. Une phase sur interrupteur utilise vitesse, croisière, thermique. Les sorties numériques proposent le maintien ou la bascule selon la fonction.

Sur une TX16S en mode joystick USB classique, EdgeTX expose huit axes et jusqu’à vingt-quatre boutons. Un interrupteur physique doit être envoyé sur une voie USB pour être détecté. [Réglage du joystick USB dans EdgeTX](https://manual.edgetx.org/color-radios/model-settings/model-setup/usb-joystick)

Dans **Graphismes**, choisir le mode d’affichage, la résolution, la qualité et la synchronisation verticale, puis **Appliquer**. Le mode sans bordure utilise la résolution du bureau. Un nouveau mode vidéo doit être confirmé sous 15 secondes.

Le hangar présente le même modèle peint qu’en vol. Le FBX Meshy et ses quatre textures PNG sont dans `SourceAssets/Topy`. Le script `Scripts/import_topy.py` prépare le maillage et son matériau. À son chargement, le simulateur oriente le nez vers +X et recale la maquette à 119 cm d’envergure et 86 cm de longueur. Les gouvernes sont intégrées au maillage. Le modèle aérodynamique reste celui du Topy 4522.

## Masse et centrage

Dans **Choisir le planeur**, utiliser les flèches du modèle pour prévisualiser le Topy ou le K3600. **Appliquer et sélectionner** mémorise le modèle et sa configuration. **Retour au menu** retrouve le planeur actif. Les réglages de masse et de centrage sont propres à chaque planeur.

**Manche pour les virages** choisit **Direction → direction** ou **Ailerons → direction**, indépendamment pour chaque modèle. Enregistrer avec **Appliquer et sélectionner**. Le manche choisi utilise son affectation, son inversion et son trim définis dans **Périphériques**. Sur le Topy, cette commande se combine avec la profondeur pour l’empennage en V. Au clavier, les flèches gauche et droite commandent l’entrée ailerons par défaut. Les anciens profils conservent la commande de direction jusqu’à modification de ce réglage.

L’onglet **Avancé** règle séparément l’efficacité de la **direction / lacet**, du **roulis induit** et de la **profondeur**, de **−500 à +500 %**. **100 %** correspond au modèle de référence actuel, **0 %** annule la contribution directe de la commande sur l’axe concerné et une valeur négative inverse cette contribution. Les effets aérodynamiques couplés entre les axes continuent de s’appliquer. Les curseurs avancent par pas de 5 %, la saisie numérique permet un réglage au pourcent près. Les gains sont appliqués après les limites des servos et du mixage. Les valeurs supérieures à 100 % augmentent donc aussi l’autorité à pleine commande. **Rétablir les forces à 100 %** prépare le retour aux valeurs de référence. **Appliquer et sélectionner** enregistre les trois gains avec la configuration du modèle. Changer d’onglet conserve les modifications en cours, tandis que revenir au menu sans appliquer les abandonne.

Le centrage se règle sur des silhouettes vectorielles monochromes simplifiées, vues du dessus. Le nez est en haut. Le point doré indique la position choisie, le cercle blanc le réglage d’origine. Glisser le point sur la vue ou son détail, ou utiliser les flèches pour avancer par pas de 1 mm. Le bouton **Rétablir le centrage d’origine** conserve la masse choisie. Les distances sont mesurées depuis le bord d’attaque à l’emplanture.

Le Topy démarre à 320 g et 61 mm. Le K3600 utilise le FBX fourni, remis à l’échelle de la fiche Hobby Aréna : 3,60 m d’envergure, 1,77 m de longueur et 79,1 dm² de surface alaire pour la physique. Le simulateur utilise la version planeur équipé à 1 600 g avec direction et profondeur. La notice donne 101 à 106 mm de centrage, avec 104 mm retenus par défaut en simulation. La notice et la fiche diffèrent légèrement sur les cotes extérieures. [Fiche du K3600](https://www.hobbyarena.cz/Models/K3600/index.php) · [Notice constructeur](https://www.hobbyarena.cz/Models/K3600/K3600.pdf)

Le constructeur indique un profil de la famille Drela AG. Les polaires et les inerties du modèle numérique restent des estimations. Les anciens chargements de l’Alizé 150 ne sont pas transposés au K3600.

Le visuel du K3600 provient du FBX Meshy **RC Glider Slim Wings** fourni le 1er novembre 2026, enregistré dans `SourceAssets/K3600`. Son point de référence est positionné à 104 mm derrière le bord d’attaque de l’emplanture. La silhouette du menu et le centre de l’enveloppe de collision suivent cette géométrie.

Le réglage de réactivité du K3600 utilise des coefficients de commande de −0,0918 en lacet, −0,046575 en roulis induit et −0,117 en profondeur. Le dernier ajustement augmente l’efficacité de la direction de 70 % et celle du roulis induit de 15 % par rapport au premier réglage vif. Les inerties estimées sont réduites de 25 % en roulis et lacet, de 20 % en tangage, avec 15 % d’amortissement de rotation en moins par rapport au réglage initial. Les servos parcourent la course du neutre à la butée en 0,125 s. La traînée des gouvernes suit l’augmentation de leur efficacité. Ce réglage est à ajuster avec les essais de pilotage.

Le moteur répartit la variation de masse proportionnellement autour du centre de gravité. Le déplacement longitudinal du centrage modifie le bras de levier des forces aérodynamiques.

## Vent et thermiques

Le menu **Météo** règle le vent de référence à 10 m du sol (0–10 m/s), sa provenance, l’activité thermique (0–200 %) et la turbulence (0–100 %). **Appliquer la météo** enregistre les conditions et les applique sans redémarrer. L’heure solaire se règle par demi-heure et reste fixe pendant le vol. Le soleil suit la course d’une journée d’été à 46° nord. Le ciel peut être dégagé, avec éclaircies, couvert ou brumeux. L’ensoleillement module les thermiques et la brume réduit la visibilité. Les conditions de référence sont des éclaircies à 14 h, 4,5 m/s venant du 012°, une activité thermique de 100 % et une turbulence de 15 %. Les réglages des planeurs restent indépendants.

La vitesse moyenne suit une couche de surface logarithmique. La brise monte sur le versant exposé et redescend sous le vent. La réponse au relief combine plusieurs échelles : les petites bosses perdent leur influence avant l’ensemble du versant. Les instruments détaillés (**H**) distinguent la pente du mélange thermique/rafales. Pour essayer la dynamique seule, couper les thermiques et longer la face nord-est de la butte du pilote.

Des sources thermiques déterministes sont réparties sur la carte, en plus des trois sources locales. Elles sont affaiblies en forêt et ne naissent pas dans l’eau. Le noyau ascendant s’élargit et dérive avec le vent ; la subsidence compensatrice est étalée autour. Les cycles sont progressifs, avec une extinction entre 450 et 650 m au-dessus de la source. Relancer le planeur ne réinitialise plus la météo. Les rafales sont continues dans le temps et l’espace, renforcées à basse altitude et sur les faces descendantes. Chaque planeur échantillonne le même air au centre et aux deux bouts d’aile ; les filets et les instruments utilisent ce champ commun.

Ce modèle est une approximation paramétrique de vol à voile, pas une résolution CFD : il ne simule pas la séparation complète des rotors, les ondes de montagne ni la thermodynamique des nuages. La dérive des panaches est bornée par un modèle de mélange. Les principes de pente, noyaux thermiques et subsidence s’appuient sur le [Glider Flying Handbook de la FAA](https://www.faa.gov/regulations_policies/handbooks_manuals/aviation/glider_handbook) ; les paramètres de cette scène restent à confronter aux retours des pilotes RC.

La boussole en haut suit la caméra sol ou poursuite. Le **N** rouge indique le nord de la carte. Les deux aiguilles pointent vers la provenance du flux. **Vent de**, en bleu, donne la direction et la vitesse horizontale du vent local. **Relatif de**, en mauve, indique d’où l’air arrive sur le planeur et sa vitesse totale. **Air** indique la montée de la masse d’air et **Vario** celle du planeur, en m/s.

Les filets bleus suivent le déplacement de l’air dans le décor. Ils deviennent dorés quand il monte. **F** les affiche ou les masque. Cette commande peut être attribuée à un bouton de radio dans **Périphériques → Simulateur**.

## Collisions

À chaque pas, Unreal balaie le fuselage et les deux demi-ailes entre les deux poses. Le moteur immobilise le planeur au premier impact, y compris sous l'altitude zéro au lac. Le HUD indique la surface touchée. R initialise un nouveau vol. Cette version traite le premier contact comme une fin de vol. Le roulage et les rebonds restent à modéliser.

Le test hors écran `Lancer-Unreal.cmd --smoke --offscreen --contact-proof` ajoute un cylindre sur la trajectoire d'une aile et vérifie dans le journal Unreal l'arrêt, la relance et les surfaces du site.

## Comparer les moteurs

Depuis la racine, `Lancer-Unreal.cmd --test-physics` rejoue un plané, un virage à la direction, une impulsion de profondeur et une boucle. Toutes les 0,25 s, le test compare la position, l’attitude et la vitesse air aux trajectoires enregistrées avec JSBSim 1.3.1. Les entrées `NATIVE_COMPARE` du journal donnent les écarts maximaux. Le lancement du jeu et ces tests fonctionnent sans JSBSim. Seule la régénération des trajectoires avec `simulation/reference_native_flight.py` nécessite la bibliothèque.
