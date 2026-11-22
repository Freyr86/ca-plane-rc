# Préparation communautaire — Ça plane ! 0.5.9

Dossier préparé le 22 novembre 2026 pour le chat **Ça plane ! — Communauté et communication**. Préparer un post court à relire par Ludovic ; aucun envoi ou post social n’est autorisé par ce dossier.

## État de diffusion

La publication GitHub de la **0.5.9** a été demandée par Ludovic le 22 novembre 2026, pour tester les mises à jour intégrées depuis sa version précédente. Vérifier le statut de la release avant d’annoncer « disponible ».

Lien prévu après publication : <https://github.com/Freyr86/ca-plane-rc/releases/tag/v0.5.9>.

Validation finale : compilation Windows réussie, tests natifs du lanceur réussis, parcours des menus terminé (70 étapes), tutoriel APUS terminé dans le jeu installé, contrôle des binaires et désinstallation de test réussis. Le formulaire public répond et la lecture anonyme des rapports est refusée. Ces vérifications ne remplacent pas les essais sur les configurations des joueurs.

## Informations pour le post

**Un démarrage mieux accompagné.** Le lanceur contrôle les capacités nécessaires avant Unreal et explique les incompatibilités détectées. Il peut proposer DirectX 11 avec des graphismes réduits. Ne pas annoncer une compatibilité avec tous les PC : la création réelle d’un périphérique DirectX est vérifiée, mais les performances et tous les pilotes ne peuvent pas être garantis. Le PC Intel HD 4600 à l’origine du retour reste à tester par son propriétaire.

**Un moyen simple de signaler un problème.** Le joueur n’a pas besoin de compte GitHub ou ChatGPT. Il relit les informations avant d’envoyer son rapport depuis le [formulaire d’assistance](https://ca-plane-assistance.endlessbow16.chatgpt.site/). Le lien est disponible dans le jeu, le lanceur et le menu Démarrer de Windows ; il reste donc utilisable sans démarrer le simulateur. Les rapports sont consultables dans le tableau privé du responsable, pas dans des issues GitHub publiques. Aucun journal complet n’est transmis automatiquement.

**Le K3600 retrouve son roulis.** Les ailerons et leur trim n’étaient pas activés correctement ; les commandes sont maintenant indépendantes de la présence d’une dérive. Le coefficient de roulis reste un réglage de simulation à affiner avec les pilotes, pas une validation du modèle réel.

**Un deuxième tutoriel avec l’APUS.** TUTO → Utiliser l’élastique guide de la pose de l’ancrage au rangement, avec repères et jauge de traction. La leçon utilise l’APUS sur la piste en herbe. Une récupération après crash permet de réessayer sans reposer l’installation. Les réglages et installations habituels sont restaurés à la sortie. TUTO → Mon 1er vol conserve le Topy au chalet.

**Rappeler les mises à jour intégrées.** Elles existent depuis la 0.5.8 : recherche automatique désactivable, téléchargement puis installation à la demande. La 0.5.9 sera détectable après sa publication. Les utilisateurs de la 0.5.7 ou d’une version antérieure doivent installer manuellement la nouvelle version une première fois.

## Captures du simulateur

Privilégier la capture APUS pour illustrer le post ; la capture du menu montre les deux tutoriels. Images capturées dans le jeu compilé, sans retouche.

![APUS prêt à lancer, jauge de traction et repère d’ancrage](apus-elastique-tension.png)

![Menu proposant les deux tutoriels](menu-deux-tutoriels.png)

## Ce qui appartient déjà à la 0.5.8

La vue de profil du centrage, le travail commun sur le décrochage et les fortes incidences, les ajustements du Fox, les trois niveaux de résistance et le suivi caméra jusqu’à l’arrêt sont déjà dans la 0.5.8. Ils peuvent être rappelés, mais ne doivent pas être présentés comme des nouveautés de la 0.5.9. [Dossier précédent et captures du centrage](../v0.5.8/README.md).

La télécabine est uniquement notée pour une future implémentation ; elle n’est pas présente dans cette version. Aucune annonce de livraison macOS ou d’anglais n’est incluse.

## Références

- [Notes de version pour GitHub](NOTES_VERSION.md)
- [Détail du contrôle et du signalement](../../COMPATIBILITE_ET_SIGNALEMENTS.md)
- [Tutoriel APUS](../../TUTORIEL_ELASTIQUE.md)
- [Correction K3600](../../reference-aircraft/K3600.md)
- [Mises à jour intégrées](../../MISES_A_JOUR.md)
