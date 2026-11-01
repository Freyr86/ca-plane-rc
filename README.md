# Ça plane ! — simulateur de planeur RC

Prototype Windows de vol de pente sous Unreal Engine 5.8, avec moteur de vol C++ natif à 120 Hz. Le simulateur fonctionne sans service JSBSim externe.

![La prairie du lac](reports/unreal_prototype/vallee-3d-sol.jpg)

## Jouer

L’installateur Windows est proposé dans les [versions téléchargeables](https://github.com/Freyr86/ca-plane-rc/releases). Il contient le jeu autonome : Unreal Editor, Visual Studio et Git ne sont pas nécessaires pour jouer.

Le projet reste un prototype : les réglages physiques sont affinés par comparaison avec des vols réels, sans constituer une validation aérodynamique certifiée.

## Fonctionnalités

- Graupner Topy 4522 et K3600, modèles colorés et fiches physiques séparées.
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

Les sources du décor sont documentées dans [SOURCES.md](unreal/SimPlaneurUE/SourceAssets/Meadow/SOURCES.md). Les modèles colorés des planeurs ont été fournis pour ce projet sous forme d’exports Meshy. Unreal Engine et les ressources tierces conservent leurs conditions propres ; la visibilité publique du dépôt ne constitue pas une licence générale de redistribution de ces éléments.
