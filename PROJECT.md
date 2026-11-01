# Simulateur RC

Ce dépôt est désormais la racine de travail du projet.

## Documents de décision

- `docs/ARCHITECTURE_PROPOSEE.md` : séparation des modules et contrats.
- `docs/PHYSIQUE_CAHIER_DES_CHARGES_V1.md` : périmètre physique et critères de validation de la première version.
- `docs/reference-aircraft/GRAUPNER_TOPY_RECHERCHE.md` : recherche documentaire sur le Topy 4522, candidat de référence.
- `docs/reference-aircraft/TOPY_MESURES_IRL.md` : relevés à effectuer sur l'exemplaire réel.
- `models/topy_4522/reference-data.json` : données publiées en SI et champs de mesure encore inconnus.
- `reports/topy_v0/TOPY_V0_ANALYSE.md` : première polaire estimée à partir des mesures de soufflerie du S4083.
- `reports/topy_v0/JSBSIM_V0_VALIDATION.md` : vérification numérique du modèle JSBSim V0.
- `reports/unreal_prototype/UNREAL_PROTOTYPE_STATUS.md` : état du client Unreal et vérifications possibles avant compilation.
- `docs/prompts/BASE.md` : contexte commun.
- `docs/prompts/PHYSIQUE.md` : chantier physique du vol.
- `docs/prompts/MODELISATION.md` : chantier modèles et formats.
- `docs/prompts/ENVIRONNEMENT.md` : chantier moteur 3D, monde et application.

## Prototype Topy V0

Les dossiers `game`, `models`, `simulation` et `tests` contiennent maintenant un premier Topy provisoire dans Godot/JSBSim. Il sert de référence et de banc d'essai ; il ne représente pas encore l'architecture finale ni une physique validée sur l'appareil réel.

Les dépendances portables volumineuses (`tools`) restent dans l'ancien dossier Dropbox. Les lanceurs `.cmd` de ce dépôt les retrouvent automatiquement et conservent la calibration radio existante.

Cette séparation permet de tester le modèle actuel avant le choix définitif du moteur 3D, du moteur physique et de la chaîne de modélisation.

## Client Unreal expérimental

`unreal/SimPlaneurUE` contient le simulateur Unreal Engine 5.8. Son moteur C++ calcule les six degrés de liberté du Topy à 120 Hz. Les trajectoires JSBSim du prototype servent de référence hors ligne. `Lancer-Unreal.cmd` ouvre le simulateur.
