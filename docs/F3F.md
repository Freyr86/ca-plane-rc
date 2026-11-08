# Entraînement F3F

Référence : FAI Sporting Code, volume F3 Soaring, édition du 1er juin 2026, section F3F :
https://www.fai.org/sites/default/files/2026-06/CIAM-F3-soaring-2026-June.pdf

## Utilisation

- Récupérer le planeur, puis **F9** pour sélectionner le F3F. Les installations de treuil/élastique restent en place ; leur interaction E est suspendue pendant ce mode.
- **E** pose A sur le repère au sol. Marcher vers l'emplacement souhaité de B. Son repère reste exactement à 100 m de A en distance horizontale. Se rapprocher à moins de 4 m et appuyer sur **E** pour confirmer.
- Pendant la pose, **G** choisit l'affichage des portes et **F10** inverse le côté de vol. La flèche montre le côté du parcours ; la ligne rouge est la séparation de sécurité. Placer la ligne A–B le long de la pente et le côté vol face au vent.
- Les deux plans verticaux sont parallèles, à 90° de A–B. Les différences de hauteur du terrain ne les inclinent pas. Leur cadre est une aide visuelle, pas une limite de passage.
- La validation de B enregistre le parcours, le côté de vol et la visibilité des portes. Ces choix restent fixes jusqu'au rangement/repositionnement.
- Un cercle doré de 3 m de rayon repère le centre au sol pendant la marche en mode F3F, même lorsque les portes sont masquées. Il suit le relief et reste lisible dans l'herbe.
- Rejoindre ce cercle au milieu du parcours, **E** prépare la tentative et sort le planeur du sac. La tolérance de départ reste de 15 m autour du centre. **Espace**, ou la commande de lancer assignée, lance à la main dans les 30 secondes.
- Entrer par A vers B. Après 30 secondes depuis le lâcher, le chrono commence même si l'entrée n'a pas encore eu lieu ; l'entrée par A reste obligatoire.
- Effectuer 10 branches alternées. Chaque passage valable déclenche un bip. Le chrono et les branches sont visibles dans le HUD ; le résultat reste affiché après la manche.
- Récupérer le planeur et refaire **E** dans la zone de départ pour une nouvelle tentative. Le parcours est conservé entre les sessions et visible sur la carte **F2**.
- **F9** quitte le mode. **Maj+F9**, à moins de 4 m d'un pylône, range le parcours. Un seul parcours personnel est installé à la fois.

## Règles reproduites et limites du mode

Le mode est un entraînement individuel, pas un logiciel de championnat homologué. Il applique le parcours de 100 m, les 10 branches, l'entrée A vers B, le délai de lancer et d'entrée, les passages de l'enveloppe du modèle, le contrôle de hauteur après la sortie, et le chronométrage interpolé au pas physique de 120 Hz. Il refuse une masse supérieure à 5 kg et invalide l'usage du moteur, un contact avant la fin ou l'exploration noclip.

Un rayon vers le modèle vérifie la visibilité depuis chaque base : une entrée masquée à A invalide la tentative ; un virage masqué est annoncé quand le modèle redevient visible à l’extérieur. Les points de contrôle utilisent l’enveloppe simplifiée du fuselage et des ailes, pas chaque triangle du maillage.

Les franchissements vers le côté de sécurité sont signalés à raison de 100 points chacun. Ces points restent séparés du temps : ils ne sont pas artificiellement convertis en secondes. Le calcul du score 1000 × meilleur temps / temps du pilote nécessite un groupe de concurrents et n'est pas inventé en solo.

La météo est mesurée au milieu des bases à 10 m au-dessus du terrain. Un vent hors 3–25 m/s ou décalé de plus de 45° pendant 20 secondes, ainsi que la pluie ou la nuit au-delà de 30 minutes après le coucher du soleil, signale la possibilité d'un revol. La météo et ses turbulences continuent à évoluer normalement ; retenter ne reproduit pas exactement leur historique.

La réception officielle dans une aire d'atterrissage, les pénalités de contact avec des personnes, les groupes, manches et classements, les officiels, le contrôle complet des dimensions/surfaces/charge alaire et du rayon du nez ne sont pas simulés. Les aides du simulateur (portes, télémétrie, vario, caméras) restent disponibles pour apprendre : elles ne constituent pas une configuration de compétition FAI. Le résultat affiché est donc un **temps d'entraînement**, pas une homologation FAI.
