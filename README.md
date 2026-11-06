# Ça plane ! — simulateur de planeur RC

Prototype Windows de vol de pente sous Unreal Engine 5.8, avec moteur de vol C++ natif à 120 Hz. Le simulateur fonctionne sans service JSBSim externe.

![La prairie du lac](reports/unreal_prototype/vallee-3d-sol.jpg)

### Les planeurs

| MDM-1 Fox · 7 m | Jonker JS-3 · 9 m |
| --- | --- |
| [![MDM-1 Fox en vol](reports/unreal_prototype/galerie/fox.jpg)](reports/unreal_prototype/galerie/fox.jpg) | [![Jonker JS-3 en vol](reports/unreal_prototype/galerie/js3.jpg)](reports/unreal_prototype/galerie/js3.jpg) |
| **Snipe 2 · 1,49 m** | **Flight-Composites Nubis · 5,95 m** |
| [![Snipe 2 en vol](reports/unreal_prototype/galerie/snipe.jpg)](reports/unreal_prototype/galerie/snipe.jpg) | [![Nubis dans le hangar](reports/unreal_prototype/galerie/nubis-hangar.png)](reports/unreal_prototype/galerie/nubis-hangar.png) |

### Du hangar au terrain

Choisissez votre planeur et votre site depuis le menu animé, puis retrouvez la vallée alpine et sa piste en herbe. Cliquez sur une capture pour l’agrandir.

| Menu d’accueil avec le Nubis | Piste en herbe du troisième site |
| --- | --- |
| [![Menu d’accueil de Ça plane !](reports/unreal_prototype/galerie/nubis-accueil.png)](reports/unreal_prototype/galerie/nubis-accueil.png) | [![Piste en herbe dans la vallée alpine](reports/unreal_prototype/galerie/site-03-piste.png)](reports/unreal_prototype/galerie/site-03-piste.png) |

### Nouveau site : le Belvédère du cirque

Décollez au bord du grand cirque de falaises, à quatre mètres du muret de pierres sèches. Le site dispose d’une table, de deux bancs et d’une manche à air, avec un passage dégagé vers la corniche.

[![Nouveau décollage au Belvédère du cirque](docs/design/creux-du-van/site-belvedere-proche-muret.png)](docs/design/creux-du-van/site-belvedere-proche-muret.png)

## Jouer

L’installateur Windows est proposé dans les [versions téléchargeables](https://github.com/Freyr86/ca-plane-rc/releases). Il contient le jeu autonome : Unreal Editor, Visual Studio et Git ne sont pas nécessaires pour jouer.

Le projet reste un prototype : les réglages physiques sont affinés par comparaison avec des vols réels, sans constituer une validation aérodynamique certifiée.

## Fonctionnalités

- Neuf appareils au hangar, du Snipe 2 de lancer main au Jonker JS-3 de 9 m. Le Flight-Composites Nubis de 5,95 m dispose de volets et d’aérofreins indépendants.
- Radio USB (notamment TX16S), affectations personnalisées et pilotage clavier.
- Masse, centrage avec silhouette interactive, force des commandes de −500 à +500 % par modèle.
- Vent relatif, thermiques, ascendances de relief, boussole et filets d’air.
- Météo réglable : nuages volumétriques, stratus de pente, averses locales, éclairs et tonnerre ; herbe et arbres animés par le vent.
- Vallée alpine procédurale, prairie en 3D, arbres, lac, nuages et collisions.
- Menu animé, réglages graphiques, noclip et choix du poste de pilotage.

## Commandes principales

| Action | Commande par défaut |
| --- | --- |
| Entrer/sortir du noclip | F12 |
| Se déplacer au sol, y compris pendant le vol | WASD / ZQSD / flèches |
| Regarder et orienter le lancer | Souris, avant lancer |
| Monter / descendre en noclip | Espace / Ctrl |
| Accélérer / régler la vitesse du noclip | Maj / molette |
| Lancer le planeur tenu en main | Espace |
| Récupérer le planeur immédiatement | F8 |
| Ranger / sortir / récupérer le planeur proche | F3 |
| Sélectionner l’élastique | F4 |
| Sélectionner le treuil | F1 |
| Poser, accrocher ou détacher le matériel | E |
| Ranger l’installation près de ses extrémités | Maintenir E une seconde |
| Courir à pied, avec ou sans planeur | Maj |
| Carte / vario | F2 / F6 |
| Pilotage | Radio, ou touches distinctes affectées dans Périphériques |
| Changer de caméra en vol | F7 |
| Filets d’air | F5 |
| Menu | Échap |

En quittant le noclip, le pilote est placé au sol. Avant le lancer, la souris oriente le regard. Pendant le vol, la caméra suit automatiquement le planeur et le pilote peut continuer à marcher avec WASD / ZQSD / les flèches, relativement à la direction horizontale de la caméra. Les touches de marche sont réservées au déplacement : elles n'actionnent pas les gouvernes, même si une ancienne affectation les utilise. La radio reste indépendante ; pour piloter au clavier, affecter d'autres touches dans **Périphériques**.

La marche avance à 5,5 m/s et la course avec Maj à 16,25 m/s, soit +150 % par rapport aux vitesses initiales. Les contrôles du sol et des obstacles sont effectués par petits pas pour conserver les collisions pendant la course.

Pendant le vol, le déplacement est limité à **4,4 m/s**, soit 80 % de la marche normale, y compris avec Maj. Les mêmes contrôles du terrain, de l'eau et des obstacles s'appliquent. Les déplacements s'arrêtent dans le menu et pendant le lancer ou la chute après impact.

Les petites marches jusqu'à **55 cm** se franchissent à la montée comme à la descente, notamment au pied de l'escalier du chalet. Lorsqu'un rebord bloque le déplacement direct, le passage est vérifié en trois mouvements (montée, avance, descente), avec contrôle des collisions et de l'espace au-dessus du pilote. Les murs, plafonds trop bas et dénivelés plus importants restent bloquants.

Les instruments de vol (boussole, vent, vario, cadran de traction et état des dégâts) sont affichés à **70 %** de leur taille précédente, avec les mêmes formes, couleurs et positions d'ancrage. Le bandeau des touches conserve sa taille.

Le bandeau des touches et les astuces s’effacent progressivement : opacité de 20 % en vol, 40 % au sol ou en exploration, et 90 % pendant une interaction. Après une touche de fonction, un lancement ou une interaction avec le matériel, ils restent lisibles pendant 2,5 secondes avant de s’atténuer. Marcher, regarder et piloter ne réactivent pas le bandeau.

La boussole reprend le cadran crème et les panneaux latéraux de la proposition « Instrument de vol », avec les polices du menu, des valeurs agrandies et un vario séparé. Les points cardinaux et les deux aiguilles de vent restent orientés selon la caméra. Le panneau d’instruments de diagnostic est masqué et conservé dans le code ; F1 sélectionne désormais le treuil.

## Météo et ambiance

Depuis **Échap → Météo**, choisir une ambiance : Dégagé, Éclaircies, Couvert, Brumeux, Stratus de pente, Averses locales ou Orage. L’heure déplace le soleil et module l’activité thermique. La force et la direction du vent, les thermiques et les turbulences restent réglables indépendamment. **Appliquer la météo** met à jour le monde et enregistre les réglages pour la prochaine session.

La **base des nuages** se règle de 80 à 2 500 m d’altitude : c’est une altitude par rapport au niveau de référence du monde, pas une hauteur au-dessus du pilote. La couverture nuageuse et l’intensité de pluie se règlent de 0 à 100 %. L’option de nuages bas ajoute des nappes au contact des reliefs, particulièrement visibles avec les ambiances Brumeux et Stratus de pente. La couverture à zéro retire les nuages, la pluie et les éclairs.

Les averses et les orages occupent des zones du monde qui dérivent lentement avec le vent : on peut quitter une averse et retrouver une zone sèche. Les rideaux de pluie se voient à distance ; les gouttes proches et le son de pluie suivent la position de la caméra, avec une transition progressive. En orage, les éclairs précèdent le tonnerre, dont le retard et l’atténuation dépendent de la distance. Le froissement de la végétation varie avec le vent, la proximité du sol et la densité des arbres. L’herbe et le feuillage bougent avec le vent ; leurs collisions restent stables.

Cette météo reste une simulation simplifiée pour le vol RC : les zones de pluie sont déterministes, le brouillard de pente est visuel et il n’y a pas de véritable calcul CFD de l’atmosphère. La pluie ne simule pas encore le mouillage des ailes ni son effet sur leurs performances. Les distances de rendu de l’herbe et des filets d’air ne limitent pas le champ de vent utilisé par le planeur.

![Éclaircies sur la vallée](docs/design/meteo/eclaircies.png)

[Stratus au cirque](docs/design/meteo/stratus-cirque.png) · [Averse locale](docs/design/meteo/averse-vallee.png) · [Éclair d’orage](docs/design/meteo/orage-eclair.png) · [Herbe au vent](docs/design/meteo/herbe-au-vent.png)

## Cirque des falaises

Le cirque inspiré du Creux du Van se trouve autour de **X 2680 / Y 1600 m**, au nord-est de la prairie. Son ouverture regarde le sud-ouest. Le repère « Cirque des falaises » est visible sur la carte F2. Les falaises calcaires, leurs contreforts et les langues d'éboulis entourent une combe boisée ; le plateau supérieur reste herbeux. Le relief se raccorde progressivement aux montagnes existantes dans un rayon de 1,1 km.

Le **site 04 — Belvédère du cirque**, sélectionnable dans le menu, place le pilote à **4 m du muret**, côté plateau, face à son ouverture principale. Le site a été rapproché de la corniche depuis son repère initial X 3037 / Y 2260 m. Deux bancs en bois se font face de chaque côté de la table, à côté du passage laissé libre pour le pilote ; la même manche à air animée que sur les autres sites complète le mobilier. Un muret de pierres sèches d'environ 75 cm suit la corniche, à deux mètres en retrait côté plateau. Des passages d'environ trois mètres permettent d'aller au-delà ; leurs abords sont dégagés des arbres. Le mobilier, le mât et le muret ont des collisions.

Le sol, les collisions et l'aérologie utilisent le même relief. La paroi est maillée à 1 m, le reste du secteur à 5 m, avec raccord au terrain extérieur de 25 m. Le secteur détaillé dispose de ses propres limites de rendu ; pierres et arbres sont instanciés, et les petits éléments disparaissent progressivement à distance. Les matériaux sont reconstruits par `unreal/SimPlaneurUE/Scripts/build_cirque_materials.py`. Le contrôle `simulation/launcher_unreal.ps1 --smoke --offscreen --cirque-proof` vérifie le sol, les raccords et le noclip, et produit quatre captures.

## Lancement à l’élastique

Choisir l’élastique avec F4 range le planeur et affiche un repère de pose. E pose l’ancrage. Marcher de 5 à 90 m, puis E fixe le point de départ. La ligne reste alors posée à cet endroit. E, ou la sortie du planeur avec F3 près du départ, accroche le planeur. La ligne peut être encore détendue au départ. On peut ensuite marcher ou courir avec Maj pour doser la tension et ajuster l’angle. Le cadran à aiguille affiche la force en newtons, avec une plage indicative en vert. Orienter le planeur vers l’ancrage puis lancer avec Espace. Le largage se fait automatiquement à la détente ou à 75° d’élévation de la ligne par rapport au sol. E permet aussi de larguer manuellement. La couleur de l’élastique et de sa ligne se choisit dans Réglages → Matériel de lancement et reste mémorisée.

Après l’atterrissage, rejoindre le planeur et le récupérer avec F3, revenir au repère de départ et refaire E pour repartir. Le dernier point de départ utilisé est enregistré à chaque lancement, avec l’ancrage, et restauré à la prochaine session. Un appui long sur E près de l’une des deux extrémités range l’installation entière. F8 permet de récupérer immédiatement le planeur en main. À l’atterrissage, le regard reste orienté vers le planeur lorsque la marche reprend.

Plusieurs installations peuvent rester dans le monde. F4 permet d’en préparer une nouvelle. À la validation de son point de départ, elle remplace les installations du même type dont l’ancrage se trouve dans un rayon horizontal de 500 m. Le plafond est de dix élastiques : un onzième retire le moins utilisé, compté par décollages effectifs ; à égalité, le moins récemment utilisé est retiré. Un message confirme l’installation et les remplacements. Annuler la pose conserve le matériel existant. Les positions, couleurs et compteurs sont sauvegardés ; l’ancienne installation unique est migrée automatiquement.

La carte F2 affiche les installations, les lignes et les points de départ au zoom rapproché. Les deux extrémités restent repérées au sol. Les treuils disposent de leur propre quota de dix installations et des mêmes règles de remplacement dans un rayon de 500 m.

La première version utilise un élastique simplifié dont la raideur est choisie selon la masse du planeur lors de l’installation. Il exerce une force au centre de masse. La ligne et les repères sont visuels.

## Lancement au treuil

F1 range le planeur et sélectionne une nouvelle installation. E pose le treuil sur le repère au sol. Dérouler entre **40 et 400 m**, puis E pose le départ. E ou F3 près du départ accroche le planeur. On peut marcher et courir pour ajuster la position ; le départ effectivement utilisé est mémorisé au lancement. Orienter le planeur vers le treuil puis **Espace** démarre la traction progressive. Cabrez progressivement avec la profondeur pour monter ; **E** largue manuellement. Le crochet se libère aussi automatiquement à 75° d’élévation, lors d’une détente durable du câble ou en fin de treuillée.

Dans Réglages → Matériel de lancement, la puissance maximale se règle de **25 à 150 %**. Sans affectation supplémentaire, la traction démarre automatiquement à cette puissance. Pour la doser avec la radio, affecter **Puissance du treuil** à un axe ou un interrupteur dans Périphériques. Cette voie est indépendante des gaz moteur. Le cadran affiche la tension en newtons et la puissance commandée. Le tambour du treuil tourne pendant l’enroulement.

Après récupération du planeur, revenir au départ et accrocher pour recommencer. Maintenir E une seconde près de la machine ou du départ range toute l’installation. Positions, couleur et nombre de lancements sont conservés ; la carte F2 montre les treuils avec leur nom et leur ligne.

Cette première version simule un treuil direct, sans poulie de renvoi, dimensionné automatiquement selon la masse du planeur au décollage. La vitesse d’enroulement diminue avec la charge et un câble élastique transmet la force au centre de masse ; aucun pilote automatique ne commande le planeur. La position réelle du crochet et ses moments de tangage restent une simplification à affiner. La machine possède des collisions simples ; le câble est visuel et son trajet doit être dégagé au départ.

## Contacts et dommages

Au premier contact, le planeur conserve sa vitesse et sa rotation et passe à une simulation de corps rigide : glissade, rebond, basculement et chute jusqu'au repos. Les commandes et le moteur sont coupés pendant cette phase. Le planeur reste entier pour le moment.

Le HUD distingue **DÉGÂTS** (orange) et **CRASH** (rouge), dès l'impact. Les seuils initiaux utilisent la vitesse perpendiculaire à la surface : 2,5 m/s pour les dégâts, 6 m/s pour le crash ; un obstacle dur peut endommager dès 1,5 m/s. Un contact inversé endommage, un contact avec l'eau compte comme crash. Un choc secondaire peut aggraver l'état. Ces seuils restent à ajuster aux essais, les ruptures propres à chaque modèle viendront plus tard.

Une fois immobilisé, le planeur peut être récupéré ; F8 permet de le réinitialiser immédiatement. La récupération efface les dommages. Ouvrir le menu suspend le mouvement après impact.

Chaque fiche de vol contient une résistance structurelle `impact_resistance`, initialement estimée : Topy 0,70 ; K3600 1,00 ; VJX 1,35 ; Fox 1,15 ; JS-3 1,00 ; Snipe 0,80 ; LongBow 1,05 ; APUS 1,40 ; Nubis 1,10. Elle multiplie l'énergie d'impact tolérée à la masse de référence : les seuils de vitesse sont multipliés par la racine carrée de résistance × masse de référence / masse actuelle. L'ajout de poids augmente ainsi la fragilité à vitesse égale. Ces valeurs de gameplay ne sont pas des mesures constructeur.

## Développer depuis les sources

Prérequis : Windows 64 bits, Git avec **Git LFS**, Unreal Engine **5.8** (avec modèles et packs de fonctionnalités), Visual Studio 2022 Build Tools avec C++ et SDK Windows.

```powershell
git lfs install
git clone https://github.com/Freyr86/ca-plane-rc.git
cd ca-plane-rc
git lfs pull
.\Installer-Unreal.cmd
.\Lancer-Unreal.cmd
```

`Installer-Unreal.cmd` compile le projet et importe les planeurs depuis leurs FBX/textures. Les ressources ArchVis sont copiées depuis l’installation locale d’Unreal ; elles ne sont pas redistribuées comme sources dans ce dépôt. Le premier lancement peut prendre plusieurs minutes pour préparer les shaders.

Le guide détaillé est dans [unreal/SimPlaneurUE/README.md](unreal/SimPlaneurUE/README.md).

## Tests

```powershell
.\Lancer-Unreal.cmd --test-physics
.\Lancer-Unreal.cmd --smoke --offscreen --menu-proof
.\Lancer-Unreal.cmd --smoke --offscreen --bungee-proof
.\Lancer-Unreal.cmd --smoke --offscreen --weather-proof
```

Les tests couvrent la dynamique, le vent, les gains de commande, le lancer, les menus, le changement de site et la marche avant lancement. Les journaux et captures restent dans les dossiers locaux ignorés par Git.

La preuve météo parcourt les éclaircies, les stratus, une averse et une zone sèche, l’orage, le vent dans l’herbe et le menu. Elle vérifie également le mélange sonore local et produit dix captures. Les ressources météo se régénèrent avec les scripts Unreal Python `build_weather_materials.py`, `build_weather_audio.py` et `build_vegetation_wind.py`, dans `unreal/SimPlaneurUE/Scripts/`. Les sons sont synthétisés par le projet. Les nuages utilisent une instance dynamique du [matériau volumétrique fourni par Unreal](https://dev.epicgames.com/documentation/en-us/unreal-engine/volumetric-cloud-material-in-unreal-engine).

## Organisation

- `unreal/SimPlaneurUE/Source/` : simulation, radio, rendu procédural et interface C++.
- `unreal/SimPlaneurUE/Content/FlightModels/` : paramètres physiques JSON.
- `unreal/SimPlaneurUE/SourceAssets/` : modèles et textures sources, gérés avec Git LFS.
- `simulation/` : lanceurs, outils et bancs de comparaison.
- `packaging/` : fabrication de l’installateur Windows.
- `game/`, `models/`, `tests/` : prototype historique Godot/JSBSim et tests associés.
- `docs/`, `reports/` : décisions, hypothèses et résultats de validation.

## Ressources

Les sources du décor sont documentées dans [SOURCES.md](unreal/SimPlaneurUE/SourceAssets/Meadow/SOURCES.md). Les visuels du Topy et du K3600 proviennent d’exports Meshy. Le Nubis a été redessiné d’après les dimensions et les photos du constructeur. Sa fiche distingue les mesures publiées des réglages estimés pour le simulateur. [Références du Nubis](docs/reference-aircraft/NUBIS.md)

Unreal Engine et les ressources tierces conservent leurs conditions propres ; la visibilité publique du dépôt ne constitue pas une licence générale de redistribution de ces éléments.
