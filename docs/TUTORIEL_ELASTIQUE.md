# Tutoriel de lancement à l’élastique — APUS

Accessible dans **TUTO → Utiliser l’élastique**, à partir de la version 0.5.9.

La leçon utilise l’APUS 4 à sa masse et à son centrage standard, avec ses ailerons et sa profondeur. Il n’a pas de dérive mobile. Le départ se trouve sur la piste en herbe, avec un vent faible de face et sans thermiques.

![L’APUS accroché, avec le repère d’ancrage et la jauge de traction](tutorials/elastic/apus-tension.png)

## Parcours dans le simulateur

1. Sélectionner l’élastique (F4 par défaut).
2. Regarder le sol près du repère d’ancrage et appuyer brièvement sur E lorsque le repère de pose est vert.
3. Rejoindre le départ indiqué, à environ 30 m, puis appuyer sur E pour poser la ligne détendue.
4. Sortir et accrocher l’APUS près du départ avec E ou la commande du sac (F3 par défaut).
5. Reculer face à l’ancrage pour tendre la ligne. La jauge montre la traction rapportée au poids du modèle ; la cible pédagogique est comprise entre 2 et 4 fois ce poids.
6. Lancer lorsque la tension et l’orientation sont bonnes (Espace par défaut).
7. Accompagner doucement la montée. Larguer avec la commande affectée, ou E ; le largage automatique reste actif.
8. Poursuivre en vol plané, préparer l’approche et atterrir. Aucun temps de vol minimum n’est imposé.
9. Rejoindre le modèle et le ranger dans le sac.
10. Revenir au départ ou à l’ancrage et maintenir E une seconde pour ranger l’installation.
11. Recommencer ou quitter depuis TUTO.

Les consignes affichent les raccourcis attribués dans les réglages. La jauge et les distances sont des repères pour cette leçon du simulateur.

En cas de crash ou de perte de vue prolongée, **Récupérer** (F8 par défaut) replace le joueur au départ avec l’APUS dans le sac. L’installation reste en place pour un nouvel essai. La progression s’arrête lorsque le menu est ouvert.

L’installation du tutoriel reste temporaire. Les installations sauvegardées ne sont ni remplacées ni écrasées ; le modèle, le poste du pilote, la météo et le matériel précédents sont restaurés à la sortie. Le tutoriel **Mon 1er vol** conserve le Topy.

## Vérification technique

Le scénario `-SimSmoke -BungeeTutorialProof` utilise les interactions du jeu pour parcourir la leçon, contrôle les départs refusés sans tension ou dans le mauvais axe, la reprise après crash, la pause et la restauration du matériel. Le posé final est injecté dans ce scénario pour vérifier la récupération et le rangement ; il ne constitue pas une validation du pilotage de l’approche.

Le test physique `SimPlaneur.Physics.BungeeFlight` couvre le Topy et l’APUS : dans son scénario de référence, l’APUS monte d’environ 29 m et la ligne se libère automatiquement. `SimPlaneur.Physics.BungeeForce` contrôle la force de traction. Le scénario `-SimSmoke -TutorialProof` couvre la régression du premier tutoriel.
