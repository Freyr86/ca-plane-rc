# Ça plane ! 0.5.10 — hotfix de compatibilité graphique

Ce correctif part de la 0.5.9. Les nouveautés en cours de développement sont reportées à la 0.5.11.

- Le mode **DirectX 11 · graphismes réduits · 30 images/s** limite désormais le rendu à 30 images par seconde et active la synchronisation verticale. Il conserve la qualité basse, les nuages volumétriques désactivés et l’herbe à 20 m. Le budget des textures diffusées est limité à 400 Mo ; ce chiffre n’est pas la consommation totale de mémoire graphique.
- Le lancement explicite avec `-dx11` ou `-d3d11` applique maintenant ce profil et choisit la carte graphique selon le même ordre que le lanceur. Ce chemin contournait auparavant le profil réduit.
- Les réglages graphiques restent verrouillés pendant la session de compatibilité pour éviter de réactiver une qualité élevée par inadvertance. Les préférences enregistrées sont conservées pour le prochain lancement normal.
- Le lancement sans interface tient compte des recommandations de mémoire pour choisir le mode réduit. Un moteur graphique explicitement demandé reste respecté.
- La mise à jour intégrée reconnaît désormais les suffixes de correctif `-a` à `-z` : `0.5.10 < 0.5.10-b < 0.5.10-c < 0.5.11`. Les noms de fichiers, URL officielles et empreintes SHA-256 restent contrôlés.

## Installation

La 0.5.10 est proposée par la recherche de mises à jour des versions 0.5.8 et 0.5.9. Elle peut aussi être installée avec **CaPlaneRC-0.5.10-Windows-Setup.exe** depuis la [release officielle](https://github.com/Freyr86/ca-plane-rc/releases/tag/v0.5.10). Depuis la 0.5.7, télécharger l’installateur manuellement.

Pour une ancienne carte graphique ou un gel signalé, choisir **DirectX 11 · graphismes réduits · 30 images/s** dans le lanceur.

## Limites du diagnostic

Un joueur signale un gel complet du PC et des bandes verticales après quelques minutes, avec un i5-4440, 8 Go de RAM, une Radeon R9 200 de 2 Go et un Intel HD 4600. Le correctif réduit la charge et corrige des défauts constatés dans le mode de secours. Il ne prouve pas la cause du gel et ne garantit pas sa disparition sur ce matériel, qui n’est pas disponible pour les essais locaux.

La famille R9 200 relève du support historique chez [AMD](https://www.amd.com/en/resources/support-articles/faqs/GPU-630.html). Le diagnostic d’un plantage graphique peut nécessiter le journal du jeu, la version de Windows et du pilote, ainsi que les événements Windows à l’heure du gel ; voir les [explications d’Epic](https://dev.epicgames.com/documentation/en-us/unreal-engine/dealing-with-a-gpu-crash-when-using-unreal-engine). Aucun changement du registre Windows ni de ses délais de récupération graphique n’est effectué.

## Vérifications du 27 novembre 2026

- Compilation de l’éditeur, du jeu autonome et du lanceur. Suite Unreal sans échec : 50 résultats annoncés, dont un scénario de téléchargement réseau optionnel non exécuté ; les 49 autres tests ont été exécutés.
- Tests du lanceur : cas Radeon R9 200 + Intel HD 4600 simulé, arguments DirectX explicites, sélection de GPU, prérequis et suivi du processus.
- Installation de test isolée, fichiers installés identiques au paquet, version 0.5.10 vérifiée dans la configuration embarquée et désinstallation réussie.
- Jeu installé sur NVIDIA GeForce RTX 2050 : **185 secondes** de contrôle du mode DirectX 11 réduit, puis **70 étapes** du parcours des menus en DirectX 12 normal. Qualité élevée, VSync désactivée, absence de plafond personnel et distance d’herbe à 500 m restent sauvegardées pendant que les valeurs réduites sont actives.
- Comparaison et sélection des versions suffixées, rejet des rétrogradations et des URL invalides ; compilation d’un installateur de test `0.5.10-b` avec version de fichier Windows `0.5.10.2` et rejet d’une discordance entre la version du jeu et celle de l’installateur.

Ces essais locaux ne remplacent pas la validation sur la Radeon du signalement. Le paquet conserve le type de compilation Unreal **Development**, comme la 0.5.9, pour garder les journaux de diagnostic.
