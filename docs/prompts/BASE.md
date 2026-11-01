# Base commune — SimPlaneurPerso

## Vision

Construire un simulateur moderne de planeurs radiocommandés, proche du comportement réel, permettant d'importer des modèles et d'examiner leurs capacités de vol.

## État du prototype

- Projet principal : racine du dépôt Git.
- Ancien prototype exécutable et dépendances portables : dossier local `tools/` (non versionné).
- Prototype Windows jouable avec rendu Godot et moteur JSBSim relié en local.
- Planeur fictif Generic 250 : 2,50 m d'envergure et masse supposée de 1,50 kg.
- Source compatible Rhino 7, exports OBJ et GLB, pièces et gouvernes séparées.
- Radio RM TX16s reconnue et calibration fonctionnelle.
- Commandes clavier progressives et interpolation visuelle ajoutées.
- XFLR5 est disponible pour les expérimentations aérodynamiques.
- Les tests logiciels existants passent.

## Limite principale actuelle

La physique semble étrange. Les coefficients JSBSim sont synthétiques et n'ont pas été dérivés puis validés à partir du Generic 250. Le prototype prouve la chaîne technique, pas le réalisme aérodynamique.

## Décisions déjà prises

- Séparer modèle visuel, modèle physique et environnement.
- Conserver une physique à pas fixe, indépendante de l'affichage.
- Ne pas laisser le moteur 3D appliquer une seconde physique au planeur.
- Conserver les unités, axes, versions de formats et sources de données de manière explicite.
- Signaler clairement toute donnée supposée ou non validée.
- Préférer les outils open source, tout en acceptant Rhino 7 déjà possédé par l'utilisateur.
- Valider le comportement sur des mesures réelles avant d'annoncer une fidélité élevée.

## Règles pour les trois chantiers

- Ne pas choisir une technologie uniquement parce qu'elle est déjà présente dans le prototype.
- Comparer les solutions sur des critères concrets avant de migrer.
- Ne pas modifier silencieusement un contrat partagé.
- Ajouter des tests proportionnés aux risques physiques et aux conversions d'unités.
- Documenter les domaines où les résultats sont fiables et ceux où ils ne le sont pas.
- Garder une tranche verticale exécutable pendant les évolutions.

## Livrable attendu de la prochaine étape

Une comparaison argumentée des composants possibles, puis une décision pour chaque couche : moteur 3D, moteur physique, analyse aérodynamique, format de modélisation/import et représentation de l'environnement.

