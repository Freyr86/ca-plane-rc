# Son du simulateur RC — plan de référence

Statut : proposition à reprendre lors d’une prochaine session. Priorité au pilotage depuis le sol et à la fluidité sur le PC de référence.

## Organisation

Prévoir quatre volumes indépendants et un volume général. La caméra active représente les oreilles du pilote. Distinguer les sons émis par le terrain, ceux émis par le modèle et ceux entendus directement par le pilote. Réutiliser les données déjà calculées par le simulateur.

### 1. Musique

Commencer par une courte composition instrumentale originale ou sous licence compatible, douce et aérienne, en boucle dans le menu. Prévoir un fondu au départ du vol et au retour au menu, ainsi qu’une coupure indépendante. Plus tard, étudier une playlist de fichiers locaux : choix d’un dossier, lecture en continu et passage au morceau suivant si un fichier manque. Vérifier MP3 et Ogg dans une version Windows empaquetée avant d’annoncer les formats pris en charge. Garder YouTube et Spotify hors du premier périmètre ; réévaluer leurs possibilités et leurs conditions uniquement si une intégration devient souhaitée.

### 2. Ambiance du site

Utiliser une couche de souffle et une couche de végétation à proximité de la caméra. Mélanger prairie et feuillage selon la couverture locale, avec une transition douce en lisière. Moduler le souffle avec le vent et les rafales au point d’écoute. Un thermique peut modifier le souffle si le champ d’air local varie ; un thermique rencontré par un planeur éloigné ne doit pas faire gronder la terrasse. Pour une caméra embarquée, utiliser le flux d’air relatif à l’appareil. Éviter une source par arbre ou par touffe.

### 3. Planeur

Donner la priorité au passage rapide : placer le sifflement sur le modèle, régler sa force et son timbre selon la vitesse par rapport à l’air, puis appliquer l’atténuation avec la distance au pilote. Évaluer le Doppler à l’écoute d’un aller-retour. Pour les servos, déclencher un son discret sur le déplacement effectif de la gouverne, avec un seuil contre le tremblement des manches. Un braquage maintenu doit redevenir silencieux. Prévoir ultérieurement un son moteur par appareil motorisé, piloté par son régime lorsque la propulsion existera.

### 4. Télémétrie

Prévoir une voie audio côté pilote, audible quelle que soit la distance au modèle et désactivable. Lorsque cette étape sera retenue, dériver le vario du taux de montée filtré du planeur, avec cadence variable et zone silencieuse près de zéro. Distinguer montée du modèle et vitesse ascendante de l’air. Réserver l’alerte batterie à la future simulation électrique : seuil, marge de réarmement et répétitions espacées. Le premier essai pourra utiliser les haut-parleurs du PC. La liaison avec la TX16S reste une décision séparée.

## Étapes proposées

1. **Mesurer le point de départ.** Relever les temps de trame sans audio sur un parcours reproductible : menu, lancement, passage près du pilote, éloignement et lisière au site 2.
2. **Écouter un seul passage.** Prototyper uniquement le sifflement 3D. Comparer un passage lent du Topy et un passage rapide du K3600, puis refaire le trajet dans l’autre sens. Depuis le sol, on doit entendre l’approche, le côté du passage et l’éloignement.
3. **Habiller le site et le menu.** Ajouter le vent, la transition herbe/arbres, puis la musique et les volumes. Vérifier les fondus et l’absence de boucles superposées après plusieurs allers-retours au menu.
4. **Ajouter le détail utile.** Tester les servos à côté du pilote, puis le vario PC en ascendance et en descente. Playlist locale, moteur et batterie pourront être traités séparément selon les besoins.

## Budget et validation

Pour le premier essai, viser au plus quatre voix continues simultanées : vent, végétation, sifflement et musique pendant un fondu. Utiliser les outils audio natifs d’Unreal, quelques boucles et une mise à jour indicative des paramètres à 10 Hz avec interpolation. Couper les sources inaudibles. Reporter occlusion détaillée et effets coûteux jusqu’à ce qu’un essai d’écoute les justifie.

Comparer sur le même PC, avec les mêmes réglages et le même trajet, audio actif puis coupé. Relever le temps de trame médian, le 95e percentile, les voix actives et les éventuels craquements. Si le surcoût dépasse la variabilité des mesures ou si le pilotage saccade, réduire d’abord le nombre de voix et la fréquence des mises à jour. Valider également au casque et sur les haut-parleurs du PC : le vent doit laisser entendre un passage proche et les alertes doivent rester distinctes.
