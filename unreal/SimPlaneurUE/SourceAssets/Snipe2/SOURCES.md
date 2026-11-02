# Snipe 2 Regular, DLG F3K

L’exemplaire documenté par son pilote mesure 1,49 m d’envergure, 1,081 m de longueur et 19,65 dm² de surface alaire. La masse en vol de 249 g sert de réglage initial. Le centrage de 68 mm se situe dans la plage publiée de 66 à 70 mm. [Mesures et réglages de l’exemplaire](https://flyingtom.net/hangar/snipe-2/) · [Notice du Snipe 2](https://mahmoudi-modellsport.eu/wp-content/uploads/2018/02/Snipe2-Instructions_ENnew2.pdf)

Le script `simulation/build_snipe_visual.py` produit le maillage et sa livrée carbone et orange. Le téton de lancer se trouve sur l’aile gauche pour une prise main droite. Les flaperons assurent le roulis et la cambrure symétrique. Leur abaissement sert au freinage. Les débattements symétriques initiaux sont de 4° vers le haut et 40° vers le bas, réglables dans le hangar.

Le prototype anime une rotation de 270° en 0,45 s depuis la hauteur d’épaule (1,45 m). Au lâcher, la vitesse est tangente au mouvement et vaut environ 30 m/s par rapport au sol. Le vent local détermine la vitesse air. Une compensation temporaire de profondeur s’efface à mesure que le planeur ralentit et le pilote conserve la main pendant la montée. Le geste, la compensation, la polaire et les inerties sont des estimations de simulation.
