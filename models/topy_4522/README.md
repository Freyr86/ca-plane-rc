# Graupner Topy 4522

Ce dossier contient les données de départ du futur modèle physique.

- `topy_4522.xml` est le modèle JSBSim V0 généré.
- `assumptions-v0.json` distingue les valeurs publiées des hypothèses à remplacer.
- `Topy4522V0.glb` et `Topy4522V0.obj` sont les exports visuels utilisés par le prototype.
- `Topy4522V0_Rhino7.3dm` est la source compatible avec Rhino 7.
- `visual-model.json` documente les dimensions et simplifications du modèle 3D.
- `reference-data.json` sépare les valeurs publiées, les valeurs dérivées et les mesures encore inconnues.
- `airfoils/s4083-uiuc.dat` contient les coordonnées officielles UIUC du profil candidat S4083.
- `airfoils/S4083A.LFT` et `airfoils/S4083A.DRG` contiennent les mesures de soufflerie UIUC utilisées par l'analyse V0.

Le fichier S4083 est une **hypothèse de travail**. Il ne représente pas encore l'aile du Topy : le profil exact doit être confirmé et son bord de fuite abaissé d'environ 1,5 mm doit être reconstruit d'après l'aile réelle. Ne pas produire de résultats de validation en présentant ce fichier brut comme le profil définitif.

La géométrie visuelle respecte l'envergure, la longueur et les cordes publiées. La forme du fuselage, le dièdre, le V-tail et les détails de gouvernes restent approximatifs en attendant les mesures et photos de l'appareil réel.

Source des coordonnées : [UIUC Airfoil Coordinates Database](https://m-selig.ae.illinois.edu/ads/coord_database.html), fichier `s4083.dat`.

Source des mesures : [UIUC Low-Speed Airfoil Tests, volume 2](https://m-selig.ae.illinois.edu/pd/pub/lsat/vol2/), éprouvette S4083 (A) construite par M. Levoe.
