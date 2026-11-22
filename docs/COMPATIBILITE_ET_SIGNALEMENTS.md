# Compatibilité et signalements — version 0.5.9

## Avant de lancer le simulateur

Les raccourcis de l'installateur ouvrent `CaPlaneRCLauncher.exe`. Le contrôle s'exécute avant Unreal et vérifie Windows 10/11 64 bits, SSE4.2, la mémoire vive, les cartes graphiques et leurs pilotes. Il tente réellement la création d'un périphérique DirectX 11 et DirectX 12 au niveau de fonctionnalités 11.0. Le projet distribue des shaders SM5 : le contrôle n'exige donc pas SM6 ou le ray tracing.

Une fonction indispensable absente empêche le lancement et affiche une explication. Les seuils de mémoire produisent des conseils, pas une interdiction arbitraire des cartes intégrées. DirectX 12 est proposé s'il est disponible ; le mode DirectX 11 réduit la qualité, les nuages volumétriques et la distance de l'herbe pour cette session. Il ne réécrit pas les préférences enregistrées. La sélection graphique utilise le même ordre explicite de préférence de performance que le moteur.

Le contrôle indique une capacité de démarrage, pas une garantie de fluidité ou l'absence de tout problème de pilote. Un cas Windows 10 / i5-4670 / 16 Go / Intel HD 4600 a motivé le mode de secours ; cette machine précise n'a pas été testée. Il faut encore confronter ce mode au retour du joueur.

Un diagnostic matériel minimal reste sur le PC dans `%LOCALAPPDATA%\CaPlaneRC\Diagnostics\compatibility.txt`. Le lanceur surveille directement le vrai processus `SimPlaneurUE/Binaries/Win64/SimPlaneurUE.exe` et propose de préparer un rapport s’il se termine en erreur. Le bootstrap à la racine peut renvoyer un succès même lorsque le moteur échoue ; il n’est donc pas utilisé pour ce suivi. Une panne du lanceur lui-même ne peut pas être interceptée par ce mécanisme.

Le lanceur contrôle aussi les bibliothèques Microsoft Visual C++ x64. Si elles sont absentes, anciennes ou endommagées, le joueur peut choisir d’ouvrir le redistribuable fourni avec le jeu. L’installation affiche son interface Microsoft et peut demander une autorisation administrateur. Aucun téléchargement, lancement automatique en mode unattended ou redémarrage automatique de Windows n’est effectué.

## Envoyer et consulter un rapport

- Joueurs : <https://ca-plane-assistance.endlessbow16.chatgpt.site/>, sans inscription.
- Responsable : <https://ca-plane-assistance.endlessbow16.chatgpt.site/admin>, avec connexion ChatGPT au compte propriétaire configuré.
- Dans le jeu : Réglages → Signaler un problème.
- Sans démarrer le jeu : raccourci « Ça plane ! — Signaler un problème » dans le menu Démarrer, ou `CaPlaneRCLauncher.exe --report`.

Le formulaire permet de décrire le problème, sa catégorie, le planeur/site, les étapes et le résultat attendu. Le diagnostic proposé comporte la version, Windows, le processeur, les cartes et pilotes, la mémoire, les capacités DirectX, les avertissements et le code de sortie. Le joueur peut retirer cette configuration et doit confirmer l'envoi. Aucun journal complet, chemin utilisateur ou secret n'est joint automatiquement. Le diagnostic arrive dans un fragment d'URL, lu localement puis effacé de l'adresse ; il n'est envoyé au service qu'à la validation du formulaire.

Après l'envoi, un lien individuel permet de consulter uniquement le statut : Nouveau, En cours, Résolu ou À préciser. Le tableau du responsable affiche les rapports par pages de 50, avec recherche et filtre sur la page affichée, détail du diagnostic, statut et notes privées.

## Hébergement et accès aux données

Le service est publié avec le plugin Sites, sur Cloudflare. Les rapports sont enregistrés dans la base Cloudflare D1 liée au site, pas sur le PC du développeur ni dans des issues GitHub. Le pays exact de stockage n'est pas confirmé par les informations disponibles. Il n'y a pas de purge automatique des rapports dans cette première version.

Le site est en mode public pour recevoir les joueurs anonymes. La lecture et la modification des rapports restent protégées côté serveur : identité ChatGPT obligatoire et adresse correspondant au propriétaire configuré. Les routes de lecture et de modification renvoient 403 sans cette autorisation ; le tableau redirige d'abord vers la connexion. Le lien individuel ne révèle que le statut, jamais le contenu ou les notes.

Le service limite les tailles et les champs acceptés, demande le consentement, utilise un jeton de formulaire signé et applique des quotas d'envoi. La clé de quota est dérivée de l'adresse IP avec un secret et renouvelée chaque jour ; l'adresse IP brute n'est pas enregistrée dans les tables applicatives. Les données techniques de l'hébergeur relèvent de ses propres règles. Les quotas n'éliminent pas tous les abus possibles.

Les secrets `ADMIN_EMAIL` et `ANTISPAM_SECRET`, ainsi que `PUBLIC_ORIGIN`, sont configurés dans Sites. Aucun secret n'est inclus dans le jeu ou les sources. Le code du service possède son propre dépôt Sites ; la copie locale de travail est `work/sites/ca-plane-support`. Identifiant du projet : `appgprj_6afb3419bcec819185da2a5ad3cf462d`.

## Vérifications réalisées le 16 novembre 2026

- Compilation de l'éditeur, du jeu autonome, du lanceur et de l'installateur Windows 0.5.9.
- Tests du contrôle : DirectX 12, DirectX 11 seul, carte incompatible, plusieurs GPU, processeur, Windows et mémoire.
- Démarrage du jeu compilé via le lanceur en DirectX 11 sur Intel Iris Xe, avec mode réduit confirmé dans le journal et parcours de l'interface terminé avec succès.
- Démarrage automatique en DirectX 12 sur NVIDIA RTX 2050, sélection correcte du GPU et même parcours d'interface réussi.
- Correction du défaut « ordinal 345 » rencontré pendant les essais : manifeste Common Controls 6 incorporé dans l'exécutable, puis contrôle du binaire copié dans le paquet et démarrage réussis.
- Tests du service : consentement, filtrage du diagnostic, droits de lecture/modification, suivi limité au statut, répétition d'un envoi sans doublon, taille et quotas. L'émulateur Cloudflare local a parfois redémarré pendant un PATCH ; le test réessaie une seule fois ce message précis, sans masquer les erreurs applicatives.
- En production : un ticket clairement nommé « TEST TECHNIQUE — formulaire sans compte » reçu via le navigateur ; lecture et modification anonymes refusées (403), tableau protégé par redirection vers la connexion (307). La gestion avec le compte réel du propriétaire reste à vérifier après sa connexion ; la logique propriétaire a été testée localement.

## Préparation de release du 22 novembre 2026

La correction du suivi de processus a été vérifiée avec des programmes de test : succès, sortie 3 et code d’exception Windows, arguments contenant des espaces/guillemets et répertoire de travail installé. Les tests couvrent aussi les versions VC++ absente, ancienne, correspondante ou plus récente, ainsi que l’absence de redistribuable et l’interdiction de l’exécuter en mode unattended. L’installation réelle de VC++ sur un Windows qui en est dépourvu reste à vérifier.

La compilation finale, le contrôle du lanceur et une installation avec identité de test séparée ont réussi. Les menus ont été parcourus jusqu’à `MENU_PROOF step=69 pass=1`, et le tutoriel APUS jusqu’à `BUNGEE_TUTORIAL_PROOF complete success=1`, via le lanceur. La désinstallation de test a également réussi. Les anciens raccourcis et attentes de vitesse de marche du scénario de contrôle ont été adaptés aux commandes actuelles. La version contenue dans le paquet a été contrôlée : 0.5.9.

La recherche intégrée de la 0.5.8 découvre la 0.5.9 après sa publication GitHub. L'installateur local est `work/release/CaPlaneRC-0.5.9-Windows-Setup.exe` ; son empreinte est dans `work/release/SHA256SUMS.txt`.
