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

## Jouer

L’installateur Windows est proposé dans les [versions téléchargeables](https://github.com/Freyr86/ca-plane-rc/releases). Il contient le jeu autonome : Unreal Editor, Visual Studio et Git ne sont pas nécessaires pour jouer.

Le projet reste un prototype : les réglages physiques sont affinés par comparaison avec des vols réels, sans constituer une validation aérodynamique certifiée.

## Fonctionnalités

- Neuf appareils au hangar, du Snipe 2 de lancer main au Jonker JS-3 de 9 m. Le Flight-Composites Nubis de 5,95 m dispose de volets et d’aérofreins indépendants.
- Radio USB (notamment TX16S), affectations personnalisées et pilotage clavier.
- Masse, centrage avec silhouette interactive, force des commandes de −500 à +500 % par modèle.
- Vent relatif, thermiques, ascendances de relief, boussole et filets d’air.
- Vallée alpine procédurale, prairie en 3D, arbres, lac, nuages et collisions.
- Menu animé, réglages graphiques, noclip et choix du poste de pilotage.

## Commandes principales

| Action | Commande par défaut |
| --- | --- |
| Entrer/sortir du noclip | N |
| Se déplacer avant lancer | WASD / ZQSD / flèches |
| Regarder et orienter le lancer | Souris, avant lancer |
| Monter / descendre en noclip | Espace / Ctrl |
| Accélérer / régler la vitesse du noclip | Maj / molette |
| Lancer le planeur tenu en main | Espace |
| Remettre en main et réactiver la marche | R |
| Profondeur / direction au clavier en vol | Haut–bas / A–D |
| Changer de caméra en vol | C |
| Instruments / filets d’air | H / F |
| Menu | Échap |

En quittant le noclip, le pilote est placé au sol. La marche lente et le regard à la souris restent actifs jusqu’au lancer ; ensuite, le pilote est fixe et la caméra suit le planeur. Les affectations se règlent dans **Périphériques**.

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
```

Les tests couvrent la dynamique, le vent, les gains de commande, le lancer, les menus, le changement de site et la marche avant lancement. Les journaux et captures restent dans les dossiers locaux ignorés par Git.

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
