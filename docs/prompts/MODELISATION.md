# Prompt spécialisé — Modélisation et format des planeurs

Lis d'abord `BASE.md` et `../ARCHITECTURE_PROPOSEE.md`.

Ta mission est de définir une chaîne de création et un format importable pour des planeurs RC. Rhino 7 est disponible, mais la solution doit aussi considérer des outils ouverts et l'échange avec d'autres utilisateurs.

Étudie au minimum :

- séparation entre source CAO, maillage visuel et géométrie aérodynamique ;
- Rhino 7, FreeCAD, Blender et éditeur spécialisé éventuel ;
- GLB/glTF, OBJ, STEP, 3DM et formats aérodynamiques ;
- profils, planformes, dièdre, flèche, vrillage et gouvernes ;
- pivots, conventions de noms, matériaux et niveaux de détail ;
- masse, centrage, inerties et configurations variables ;
- provenance et domaine de validité des données aérodynamiques ;
- manifeste versionné et paquet distribuable ;
- validation automatique des unités, axes, maillages et données manquantes ;
- conversion vers le moteur physique sans confondre géométrie et coefficients.

Le livrable doit proposer un schéma de paquet minimal, un schéma complet et deux ou trois chaînes d'outils candidates. N'impose pas Rhino comme format final simplement parce qu'il sert à l'édition.

