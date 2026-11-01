# Prompt spécialisé — Environnement, moteur 3D et application

Lis d'abord `BASE.md` et `../ARCHITECTURE_PROPOSEE.md`.

Ta mission est de définir le monde de vol et la couche applicative du simulateur. Compare les moteurs avant de retenir Godot, Unreal Engine ou une autre solution.

Étudie au minimum :

- Godot et Unreal Engine pour le rendu, le terrain, les outils, les licences et le packaging ;
- intégration d'un moteur physique externe à pas fixe ;
- interpolation visuelle sans modifier l'état physique ;
- terrain réel ou synthétique, échelle, origine flottante et collisions ;
- vent uniforme, turbulence, thermiques et vent de pente ;
- méthode reliant relief et champ de vent, avec niveaux de fidélité ;
- caméra pilote au sol, suivi du modèle et perception de la distance ;
- radio USB, latence d'entrée, calibration et mixages ;
- performances CPU/GPU, télémétrie et outils de diagnostic ;
- scénarios, météo, audio et expérience utilisateur.

Le livrable doit comparer deux ou trois architectures, préciser les frontières entre `environment`, `runtime` et `flight-physics`, puis proposer une tranche verticale mesurable. Ne duplique pas la dynamique du planeur dans le moteur 3D.

