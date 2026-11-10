# Mise à jour intégrée — à partir de la bêta 0.5.8

Cette fonction est intégrée à la bêta 0.5.8. Depuis la 0.5.7 ou une version antérieure, installer manuellement la 0.5.8 une fois pour en bénéficier lors des prochaines mises à jour.

## Parcours joueur

- Réglages → Mises à jour du simulateur.
- Recherche au démarrage du jeu autonome, désactivable ; recherche manuelle disponible.
- Un bouton apparaît au menu lorsqu’une nouvelle version est disponible. Aucun téléchargement ni fermeture automatique.
- Téléchargement du paquet complet avec progression, directement sur disque ; le joueur peut continuer à jouer.
- Vérification de la taille et du SHA-256 fourni par GitHub.
- Planeur récupéré : « Installer et redémarrer » sauvegarde les réglages, lance l’installeur, puis ferme le simulateur. Le fichier est revérifié avant exécution.
- L’installeur réutilise le dossier existant et l’AppId, exclut les sauvegardes et relance le jeu avec `/RESTARTGAME=1`. Aucun redémarrage de Windows demandé.
- Le mode éditeur permet la recherche et le téléchargement mais interdit l’exécution de l’installeur.

## Contrat de publication

Le client consulte les 100 dernières releases publiques de `Freyr86/ca-plane-rc`. Les bêtas sont incluses. Les versions sont comparées numériquement au format `vMAJEUR.MINEUR.CORRECTIF`. Brouillons, tags non conformes, exécutables d’un autre dépôt et fichiers sans empreinte SHA-256 sont ignorés. Une version identique ou plus ancienne n’est jamais proposée.

Une release doit contenir `CaPlaneRC-X.Y.Z-Windows-Setup.exe` complètement téléversé ; son URL doit être celle de cette release dans le dépôt officiel. GitHub fournit l’empreinte dans `assets[].digest`. `SHA256SUMS.txt` reste utile pour les téléchargements manuels.

Les fichiers téléchargés résident dans le répertoire utilisateur `CaPlaneRC/Updates`. Une interruption peut laisser un fichier incomplet, remplacé lors d’une nouvelle tentative ; il ne peut pas être installé avant validation. Les anciens installateurs ne sont pas purgés automatiquement dans cette première version.

La vérification établit l’intégrité du téléchargement depuis la release GitHub ; ce n’est pas une signature Authenticode. Pas de mises à jour différentielles ni de retour automatique à une version précédente pour l’instant.

## Vérifications

- Tests `SimPlaneur.Updates.ReleaseSelection` : comparaison numérique, absence de rétrogradation, brouillons, URL, empreinte, JSON invalide, SHA-256 connu et fichier modifié.
- Test réseau volontaire : `-ExecCmds="Automation RunTests SimPlaneur.Updates" -UpdateDownloadProof`. Télécharge et vérifie une release publiée, sans exécuter l’installeur.
- Parcours visuel : `-game -SimSmoke -MenuProof -UpdaterUIProof -RenderOffscreen -unattended` ; affiche les états disponible, progression et prêt, vérifie les restrictions d’installation.
- Avant distribution, valider également le remplacement et le redémarrage avec deux builds autonomes successifs sur une installation de test.

### Validation de la 0.5.8 — 10 novembre 2026

Les 49 tests automatisés du simulateur passent. Le parcours visuel de mise à jour passe aussi dans le jeu autonome, y compris les restrictions du bouton d’installation. Le remplacement du build 0.5.7 par le build 0.5.8 a été testé dans un dossier isolé, avec une identité d’installateur de test, sans inscription Windows ni raccourcis : exécutable remplacé, profil Unreal valide conservé, puis redémarrage du nouveau jeu. Le jeu relancé termine les treize étapes du contrôle centrage/résistance. Les captures du [dossier communauté](communaute/v0.5.8/README.md) proviennent de cette version autonome.
