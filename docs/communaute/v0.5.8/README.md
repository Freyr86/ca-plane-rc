# Ça plane ! — bêta 0.5.8

Matériel pour préparer une publication communautaire. Aucun post social n’est publié par ce dossier.

[Version Windows et téléchargement](https://github.com/Freyr86/ca-plane-rc/releases/tag/v0.5.8)

## Réglage du point d’équilibre

Une nouvelle vue de profil rend le réglage du centrage plus facile à comprendre. Les boutons **Avancer · nez** et **Reculer · queue** déplacent le centre de gravité au millimètre. Le déplacement directement sur la silhouette fonctionne aussi, avec une échelle adaptée à la vue agrandie. Valider avec **Appliquer et sélectionner**.

L’inclinaison du profil est un repère visuel par rapport au centrage d’origine. Ce n’est ni une prévision de l’assiette en vol, ni une recommandation de centrage pour un modèle réel.

### Centrage avancé

Le repère de profil baisse le nez pour montrer le déplacement vers l’avant.

![Centrage avancé](centrage-avant.png)

### Centrage d’origine

La vue de profil fournit le repère de départ pour comparer les réglages.

![Centrage d’origine](centrage-reference.png)

### Centrage reculé

Le repère de profil relève le nez pour montrer le déplacement vers l’arrière.

![Centrage reculé](centrage-arriere.png)

## Physique : ce qui change

- **Décrochage et départ d’une aile :** les deux demi-ailes réagissent séparément à leur écoulement local. Le calcul tient compte du mouvement de rotation du planeur et permet un décrochage dissymétrique.
- **Fortes incidences :** les calculs aérodynamiques ont été prolongés sur un tour complet pour mieux traiter les situations qui sortent du vol habituel, y compris l’écoulement inversé.
- **Fox :** travail spécifique sur les efforts de l’empennage et l’efficacité des gouvernes à forte incidence. Les essais numériques obtiennent une entrée en vrille par les commandes, des phases aplaties et une sortie avec des commandes correctrices. Cela reste une vrille oscillatoire : aucune promesse de vrille parfaitement plate ni de reproduction exacte d’un Fox RC précis.
- **Centrage et vol dos :** vérification de l’application du centrage et comparaison de la stabilité et de la profondeur nécessaire en vol droit et dos, à vitesse comparable. Le VJX permet d’explorer davantage le centrage dans le simulateur.
- **Topy :** réglage distinct pour conserver un décrochage doux, avec baisse du nez et reprise de vitesse, conformément au retour du pilote. Chaque appareil garde ses particularités.

Le socle aérodynamique commun est appliqué aux neuf configurations de planeurs. Le nouveau calcul local d’empennage du Fox reste spécifique au Fox : les autres empennages doivent être ajustés selon leur géométrie. Ces améliorations sont vérifiées par des tests numériques ; l’affinage avec les retours de pilotes continue.

## Résistance et caméra

Trois niveaux sont proposés : **Réaliste**, **Résistant** et **Indestructible**. Le mode Résistant augmente la tolérance aux impacts ordinaires ; le mode Indestructible supprime les nouveaux dégâts, tout en conservant les collisions.

En caméra suivie, le retour au pilote au sol attend l’arrêt du planeur. Après un choc, notamment dans un arbre, le regard continue de suivre la chute ou le mouvement jusqu’à l’immobilisation. Un déplacement volontaire du pilote libère ce suivi.

## Mise à jour automatique intégrée

Le simulateur peut rechercher automatiquement une nouvelle version au démarrage. Cette recherche est désactivable et un bouton de recherche manuelle reste accessible dans **Réglages → Mises à jour du simulateur**.

Lorsqu’une version est disponible, le joueur peut la télécharger depuis le jeu. Le fichier est vérifié avant installation. Une fois le planeur récupéré, **Installer et redémarrer** lance la mise à jour puis relance le simulateur, en conservant les réglages. Le téléchargement et la fermeture du jeu restent à l’initiative du joueur.

**Première installation nécessaire :** depuis la 0.5.7 ou une version antérieure, télécharger et installer manuellement la 0.5.8 une fois. Les prochaines versions pourront ensuite être récupérées depuis le simulateur.

## Proposition de texte à relire

> Une nouvelle bêta de Ça plane ! est disponible 🙂
>
> Vos retours de pilotes font avancer le simulateur ! Cette mise à jour retravaille la physique du décrochage, les fortes incidences et les vrilles, avec un travail particulier sur le Fox. Le Topy conserve son comportement plus doux. Le centrage et le vol dos ont aussi été vérifiés, et une vue de profil rend le réglage du point d’équilibre bien plus lisible.
>
> Vous pouvez maintenant choisir entre trois niveaux de résistance : Réaliste, Résistant et Indestructible. La caméra continue également de suivre le planeur après un posé ou un choc, jusqu’à son arrêt.
>
> Autre nouveauté : les mises à jour intégrées ! Après une première installation manuelle de cette version, le simulateur pourra vous signaler les suivantes et vous permettre de les télécharger et de les installer depuis les réglages.
>
> Le travail sur la physique continue : vos essais et vos comparaisons avec vos planeurs réels sont précieux. Pour un retour utile, indiquez le modèle, le centrage et ce que vous observez. Merci à toutes et tous !
>
> Téléchargement : https://github.com/Freyr86/ca-plane-rc/releases/tag/v0.5.8

Pour accompagner le post : utiliser les trois captures ci-dessus, dans l’ordre avant / origine / arrière. Ce sont des captures du simulateur, sans retouche de l’interface.
