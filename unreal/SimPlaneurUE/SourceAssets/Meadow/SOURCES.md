# Site alpin

Textures de terrain et plantes de Poly Haven (CC0).
- Sol en 3D, https://polyhaven.com/a/leafy_grass, https://polyhaven.com/a/grass_ground et https://polyhaven.com/a/aerial_grass_rock
- Pissenlit : https://polyhaven.com/a/dandelion_01

La prairie autour du pilote utilise les maillages originaux `CreepingBent`, `BentGrass` et `FloweringFescue`. Le script `build_tussocks.py` reconstruit leurs feuilles et leurs épillets en glTF. Le sol, les touffes et les ombres occupent les mêmes positions en caméra sol et en poursuite. Le matériau `M_LivingGrass` anime les pointes dans la brise et les écarte au voisinage de l'envergure du Topy. Le moteur C++ natif calcule les efforts aérodynamiques.

Les deux caméras partagent le relief de la prairie et des deux massifs, la surface du lac et ses berges. Les prairies traversées en vol bas utilisent les mêmes trois espèces que le poste de pilotage. Le moteur calcule l'atmosphère, les nuages et l'éclairage. Les anciens visuels de référence provenaient du [Montorfano](https://polyhaven.com/a/montorfano) et de [Resting Place 2](https://polyhaven.com/a/resting_place_2).

Le script `build_fir.py` construit les sapins à branches courbées et les silhouettes des bois lointains. L'atlas des rameaux emploie les aiguilles et l'écorce CC0 de [Fir Tree 01](https://polyhaven.com/a/fir_tree_01). Les chênes « HillTree_02 » proviennent du modèle ArchVis fourni avec Unreal Engine 5.8 et les rochers du module PCG Biome Sample d'Epic.

Les variantes glTF individuelles référencent les tampons binaires de leur collection source. Les atlas RGBA combinent la photographie couleur et le masque d'origine. Le script Scripts/build_alpine_site.py importe ces variantes et construit les matériaux du projet.

Le dépôt public ne contient pas les fichiers ArchVis d’Epic : le lanceur les copie depuis l’installation locale d’Unreal Engine. Les rochers sont chargés directement depuis le plugin PCGBiomeSample installé avec le moteur. Ces ressources restent soumises aux conditions de leur fournisseur.

Les trois jeunes pins des lisières utilisent les variantes A, B et C de [Pine Sapling Small](https://polyhaven.com/a/pine_sapling_small) de Poly Haven (CC0), avec les textures 1K et le masque des aiguilles.
