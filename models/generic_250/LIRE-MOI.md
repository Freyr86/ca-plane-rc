# Generic 250 — planeur fictif pour essais logiciels

Ouvrir Generic250_Rhino7.3dm directement dans Rhino 7. Archive version 70 verifiee par relecture ; interface Rhino non testee.

- Envergure : 2,50 m ; longueur : 1,51 m.
- Aile trapezoidale : corde racine 250 mm, saumon 150 mm, surface de reference 0,50 m2, diedre 3 degres.
- Fuselage profile, verriere sombre, empennage classique, gouvernes orange.
- Masse supposee pour la simulation : 1,50 kg (non calculee a partir des volumes).
- Profil geometrique approximatif de type NACA 2412 sur l'aile. Aucune polaire calculee.

## Fichiers
Generic250_Rhino7.3dm : 14 maillages fermes, un calque par piece, plus 6 courbes de sections dans un calque masque.
Ce sont des maillages editables, PAS des solides NURBS parametriques ni un plan de fabrication.
Generic250.glb : modele colore pour Godot, pieces distinctes et pivots de gouvernes.
Generic250.obj : copie geometrique avec objets nommes, sans materiaux.
aircraft.json : dimensions, pieces, couleurs et axes des articulations.

Rhino/OBJ : metres, X droite, Y avant, Z haut. GLB/Godot : metres, X droite, Y haut, Z arriere.
Les gouvernes ont un petit jeu visible. Les ailes en plusieurs pieces facilitent le controle des maillages.

## Regenerer
Depuis la racine du projet : tools\python\python.exe simulation\generate_geometry.py
Le script regenere aussi les fichiers dans game/assets. Les changements manuels des exports seront ecrases : conserver une copie avant modification dans Rhino.

## Limites
Les coefficients JSBSim actuels sont provisoires et ne sont pas deduits de ce modele.
La geometrie et la surface de reference sont coherentes avec l'echelle du demonstrateur ; les inerties, le centrage, le decrochage et les performances restent a valider.
Ne pas utiliser pour construire ou regler un appareil reel.
