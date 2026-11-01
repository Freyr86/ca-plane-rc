# Prototype Unreal — état de validation

**Statut : prêt à compiler.**

- Association moteur : Unreal Engine 5.8
- Fichiers source C++ : 9
- Dimensions OBJ Unreal X/Y/Z : 86.00 / 119.00 / 16.84 cm
- Boucle UDP réelle : modèle `topy_4522` reçu correctement depuis JSBSim

## Vérifié sans Unreal

- structure et configuration du projet ;
- orientation et échelle centimétrique du Topy ;
- protocole de commandes/télémétrie sur UDP ;
- présence de la scène procédurale, du Pawn de vol et du HUD.

## À vérifier après installation du moteur

- Unreal Header Tool et compilation C++ ;
- import OBJ par le script Editor Python ;
- rendu, conventions de rotation et performances ;
- packaging d'un exécutable autonome.
