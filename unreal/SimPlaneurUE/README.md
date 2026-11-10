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

Après un posé ou un choc, la caméra de poursuite reste sur le modèle pendant le roulage, la glissade ou la chute. Le retour à la vue pilote attend l’immobilisation du corps physique, y compris après un choc dans un arbre. En vue sol, le regard continue aussi à suivre le modèle jusqu’à cet arrêt. Une commande de déplacement du pilote pendant cette phase libère le suivi et permet de marcher et de regarder librement ; l’arrêt ultérieur du planeur ne reprend pas la main sur ce regard. La pause conserve cet état, et la récupération du planeur le réinitialise.

## Masse et centrage

La vue de profil incline le nez vers le bas quand le centrage est avancé et vers le haut quand il est reculé. Cette inclinaison est un repère pédagogique par rapport au réglage d’origine, pas une prédiction de l’assiette en vol. Les boutons **Avancer · nez** et **Reculer · queue** agissent par millimètre. Le déplacement sur la silhouette et sur son zoom utilise l’échelle de la vue saisie. Enregistrer avec **Appliquer et sélectionner**.

Le VJX dispose d’une plage expérimentale de 88 à 128 mm pour explorer la frontière de stabilité de son modèle numérique, avec 98 mm comme origine. Cette plage décrit le simulateur. Le test `SimPlaneur.Physics.FlightEnvelope` compare les équilibres droit et dos à 20 m/s ainsi que la stabilité à plusieurs centrages. L’équilibre vertical instantané d’un planeur sans moteur ne constitue pas un palier entretenu.

Les neuf planeurs activent `separated_wing_flow`. Les seuils positifs et négatifs proviennent des pics de portance de leur propre polaire. Après le pic de portance, les deux demi-ailes échantillonnent séparément la vitesse locale, la rotation et les rafales. Leurs portances et traînées peuvent produire le départ d’une aile et l’autorotation. L’efficacité de l’empennage diminue progressivement à forte incidence. Sur le LongBow, cette réduction concerne les élevons et préserve la contribution statique de l’aile. C’est une approximation à deux panneaux et à polaires statiques, à confronter aux essais des pilotes. Les coefficients de réactivité des commandes restent ceux du modèle. Le mécanisme de couplage entre décrochage dissymétrique, roulis et lacet est décrit par la [CAA de Nouvelle-Zélande](https://www.aviation.govt.nz/licensing-and-certification/pilots/flight-training/flight-instructor-guide/wing-drop-stalling/).

Le retour du propriétaire du Topy (10 novembre 2026) décrit un décrochage difficile à provoquer : le nez s’abaisse, la vitesse revient, avec des oscillations latérales et une stabilisation attribuée au dièdre. Le Topy conserve donc davantage d’amortissement en roulis après le pic (`post_stall_roll_blend: 0.25`) et d’efficacité d’empennage (`stalled_tail_efficiency: 0.85`). Ces valeurs constituent un premier réglage qualitatif, pas des coefficients mesurés. Les autres planeurs conservent leurs propres polaires, inerties et dérivées de stabilité ; généraliser le calcul ne signifie pas leur imposer le même décrochage.

**Grandes incidences : nouvelle implémentation expérimentale.** Les neuf modèles utilisent désormais une extension continue des polaires sur 360°, à partir de leurs données existantes. Les valeurs à l’intérieur de la table sont conservées ; au-delà, un raccord rejoint une approximation de plaque plane (traînée de face 1,28), avec portance nulle à 90° et écoulement inverse continu. Cette extension est estimée, pas mesurée. Les forces d’ailerons sont ramenées dans le repère du planeur après calcul de l’incidence effective, afin d’éviter une composante de puissance parasite.

Le Fox ajoute `post_stall_tail` : les efforts de l’empennage horizontal et de la dérive proviennent de l’écoulement local, incluant la rotation du planeur. La dérive utilise le flux dans son plan corde/normale ; la descente suivant son envergure ne lui donne plus artificiellement toute la pression dynamique. Le sillage conserve la réduction horizontale existante jusqu’à 35 % d’efficacité. Les anciens efforts de commande et moments remplacés sont soustraits pour éviter de les compter deux fois. Les bras de 2 m, hauteur horizontale de 0,15 m et hauteur de dérive de 0,35 m sont des estimations cohérentes avec la géométrie visuelle. Les surfaces aérodynamiques équivalentes sont identifiées à partir des dérivées existantes ; elles ne sont pas des surfaces géométriques mesurées. Ce calcul d’empennage est activé uniquement sur le Fox ; son extension aux autres configurations exige leurs paramètres et leurs tests propres.

`SimPlaneur.Physics.FlightDeparture` vérifie maintenant une entrée par les commandes seules depuis 3,5° d’assiette, 14 m/s et une rotation initiale nulle, aux centrages 160 et 170 mm, dans les deux sens. Profondeur à cabrer et direction à fond, ailerons neutres, maintiennent une vrille oscillatoire avec phases aplaties sur trente secondes. Sur les dix dernières secondes du test natif du 10 novembre 2026, l’incidence moyenne est de 78–79°, l’assiette absolue moyenne de 16–17° et la rotation verticale moyenne de 1,82–2,13 rad/s. Après cinq secondes de légère profondeur à piquer et direction opposée, puis cinq secondes au neutre, l’incidence revient sous 1° et la rotation verticale sous 0,06 rad/s. Ces mesures démontrent une sortie de vrille, pas une remise en palier : la ressource reste à effectuer. Les essais utilisent le Fox de 24 kg, gains à 100 %, sans vent ni moteur.

Le diagnostic complémentaire `SimPlaneur.Physics.FoxSpinDiagnostics` conserve 36 entrées à direction 80 %, dont certaines deviennent une spirale ou un vol tournant. Son critère strict exige quatre secondes consécutives d’aplatissement ; une vrille oscillatoire peut ne pas le satisfaire. Son succès teste la finitude et la symétrie, et le journal conserve `flight_test_validated=0`. Le [manuel du Fox grandeur, §3.5](https://sagach.ch/wp-content/uploads/2024/03/FlughandbuchFox_BO-28-2023-MDM-1-FOX_ENrev8-komprimiert.pdf) distingue vrille aplatie et vrille plate liée à un centrage excessivement arrière ; il ne calibre pas le Fox RC de 7 m. La réaction précise aux forts ailerons, le sillage et les inerties restent à confronter à des références réelles. Le fabricant du modèle cité par le pilote est toujours inconnu : cette version constitue une amélioration physique qualitative, pas une validation de fidélité à cet appareil.

Dans **Paramètres → Résistance des planeurs**, **Réaliste** applique les seuils du modèle selon le choc et la masse. **Résistant** double les vitesses normales d’impact tolérées, soit quatre fois l’énergie, tout en conservant les dégâts de contact sur le dos et dans l’eau. **Indestructible** supprime les dégâts aux impacts suivants. Les collisions et le roulage restent actifs et un contact termine le vol aérodynamique. Le choix est mémorisé globalement et s’applique aux prochains impacts.

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
