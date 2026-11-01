# Prompt spécialisé — Physique du vol

Lis d'abord `BASE.md`, `../ARCHITECTURE_PROPOSEE.md` et `../PHYSIQUE_CAHIER_DES_CHARGES_V1.md`.

Ta mission est de concevoir le module de dynamique de vol d'un simulateur de planeurs RC. Ne choisis pas encore définitivement une technologie : prépare une comparaison permettant une décision informée.

Étudie au minimum :

- conserver JSBSim, l'étendre ou créer un solveur spécialisé ;
- représentation aérodynamique adaptée aux faibles nombres de Reynolds ;
- utilisation possible de XFOIL, XFLR5, AVL, OpenVSP/VSPAERO ou de données mesurées ;
- interpolation des polaires selon Reynolds, incidence et braquage des gouvernes ;
- dérivées de stabilité, amortissements et couplages ;
- décrochage, post-décrochage et limites de confiance ;
- vent, rafales et turbulence comme entrées externes ;
- contact au sol et lancement ;
- déterminisme, fréquence du solveur, performances et diagnostic des forces ;
- protocole de validation sur un planeur réel.

Pour chaque option, donne : licence, maturité, précision attendue, limites, effort d'intégration, performances temps réel, formats d'entrée et stratégie de test.

Le livrable doit proposer deux ou trois architectures candidates et un petit banc d'essai discriminant respectant le cahier des charges V1. N'implémente pas une migration avant validation du choix.

