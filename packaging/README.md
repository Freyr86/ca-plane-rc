# Installateur Windows

Le paquet autonome est une version **Development** du prototype, afin de conserver les journaux et les scénarios de vérification. Il ne nécessite pas Unreal Editor sur la machine du joueur.

## Fabrication

Installer Unreal Engine 5.8, Visual Studio Build Tools C++ et Inno Setup 6, puis exécuter depuis la racine :

```powershell
powershell -NoProfile -ExecutionPolicy RemoteSigned -File packaging/build-installer.ps1
```

`-EngineRoot` permet de choisir une autre installation du moteur. `-SkipPackage` réutilise un jeu déjà compilé dans `work/package/Windows`. `-Version` change le numéro de l’installateur.

Résultats dans `work/release` : un programme d’installation et `SHA256SUMS.txt`. Les fichiers générés, les caches et les outils locaux ne sont pas versionnés.

Le dossier de jeu `SimPlaneurUE/Saved` est exclu de l’installeur : les captures, journaux et configurations issus des tests locaux ne sont pas distribués.

L’installation se fait par utilisateur, dans `%LOCALAPPDATA%\Programs\CaPlaneRC` par défaut, avec menu Démarrer, raccourci Bureau optionnel et désinstallation. Le redistribuable Microsoft Visual C++ x64 est embarqué. Le lanceur Ça plane ! vérifie sa version et ses bibliothèques ; s’il manque ou est endommagé, il propose explicitement d’ouvrir l’installateur Microsoft. Aucune installation n’est déclenchée en mode unattended. L’installateur du jeu n’est pas signé par un certificat de signature de code.

## Vérification avant publication

Installer dans un dossier de test, puis exécuter le jeu installé avec `-SimSmoke -MenuProof -RenderOffscreen -unattended -NoSplash` et un fichier `-GameUserSettingsINI=` isolé. Vérifier la dernière ligne `MENU_PROOF step=69 pass=1` et inspecter les captures, notamment `club-meteo.png`. Tester ensuite la désinstallation du dossier de test.

Pour le tutoriel APUS, utiliser `-SimSmoke -BungeeTutorialProof` et attendre `BUNGEE_TUTORIAL_PROOF complete success=1`. Le lanceur surveille directement le binaire `SimPlaneurUE/Binaries/Win64/SimPlaneurUE.exe` ; le bootstrap à la racine peut terminer avec succès même lorsque le moteur échoue. Le script `build-launcher.ps1 -Test` vérifie ce cas avec des processus de test.
