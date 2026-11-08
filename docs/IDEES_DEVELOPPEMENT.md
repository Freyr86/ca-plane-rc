# Idées de développement restantes

Carnet de Ça plane ! · Mise à jour du 8 novembre 2026. Les idées sont regroupées par thème, sans ordre de priorité.

## Vol et multijoueur

- **Enregistrement des vols et défis personnels.** Enregistrer automatiquement chaque vol depuis le lancement, puis le sauvegarder à l’atterrissage ou au crash. Conserver la trajectoire, la durée, la distance parcourue, le gain d’altitude et les vitesses, avec le modèle, le site et les conditions météo. Afficher un bilan distinguant atterrissage et crash, tenir un historique et des records personnels pour permettre au joueur de se challenger. Prévoir une relecture de la trajectoire et la possibilité de retenter un défi dans les mêmes conditions.
- **Repère visuel en bout d’aile.** Proposer deux traînées discrètes et activables qui dessinent la trajectoire récente des saumons par rapport au sol. Les laisser s’effacer après une durée fixe. Depuis la caméra au sol, à vitesse air égale, la trace sera plus courte face au vent et plus longue vent arrière. Par vent de travers, elle rendra la dérive visible.
- **Suite du F3F.** Le parcours manuel, les portes optionnelles et le chronométrage sont implémentés (voir `F3F.md`). À poursuivre : historique et records par modèle/parcours, rejeu de la même météo, classement de plusieurs pilotes et manches, contrôle complet des appareils et aires d’atterrissage. Évaluer le VJX 2022 sur le parcours avant de décider s’il faut un autre planeur.
- **Remorquage en solo.** Confier l’avion remorqueur à l’IA et laisser au pilote du planeur la commande de largage.
- **Vol à plusieurs sur la pente.** Tester une session privée avec deux pilotes au sol, des trajectoires fluides et des ascendances partagées.
- **Remorqueur piloté en multijoueur.** Un joueur pilote l’avion tracteur, l’autre le planeur. Synchroniser la tension du câble et le largage pour les deux pilotes.

- **Modes simulateur et exploration.** Prévoir un mode de vol direct et un mode de découverte à pied des sites, avec planeur et matériel transportés dans le sac.

## Hangar et crédits

- **Atelier de livrées.** Peindre l’extrados et l’intrados, poser bandes et motifs, faire tourner le planeur pour inspecter la déco, enregistrer plusieurs livrées par appareil et restaurer celle d’origine.
- **Crédits par appareil.** Ajouter progressivement le créateur du modèle RC et l’auteur de la 3D. Vérifier les noms, les sources et les mentions de licence à afficher pour chaque appareil.

## Instruments et télémétrie

- **Écran radio virtuel.** Placer, redimensionner et verrouiller plusieurs instruments dans le HUD, puis enregistrer leur disposition. Essayer un altimètre et un graphe des 30 dernières secondes de vario.
- **Widgets Lua partageables.** Fournir aux scripts une zone de dessin et des mesures en lecture seule. Tester deux instruments à 10 Hz et vérifier qu’un script bloqué laisse le pilotage fluide.
- **Compatibilité EdgeTX.** Essayer un widget réel de la TX16S sur l’écran virtuel et identifier les capteurs, les commandes de dessin et les événements dont il a besoin.

## Audio

- **Réglages de volume.** Proposer un volume général et des réglages indépendants pour la musique, l’ambiance, le planeur et la télémétrie.
- **Alerte de batterie.** La relier à une future simulation de consommation électrique, avec un seuil et des rappels espacés. [Plan audio](PLAN_AUDIO.md)

## Idées mises de côté

- **Musique du menu et playlist locale.** Prévoir une boucle douce, puis la possibilité de lire ses propres fichiers audio.
- **Retour de télémétrie vers la TX16S physique.** Transmettre les mesures par USB pour alimenter notamment le vario de la radio.

## Essais réels du K3600

**Boîte noire transférable.** Un interrupteur de la TX16S démarre ou termine une séance en vol. Un bouton marque chaque manœuvre dans le journal continu. Un bip confirme le marqueur sans quitter le planeur des yeux.

- **Radio** — journaliser les commandes après mixage, le mode de vol, les trims disponibles, le GPS et le vario. Vérifier sur la radio les champs effectivement enregistrés.
- **À bord** — si une centrale inertielle est ajoutée, conserver les trois vitesses de rotation, trois accélérations et trois composantes du champ magnétique avec leur heure d’acquisition. Enregistrer aussi les voies reçues ou un canal marqueur pour synchroniser les horloges. Tester cadence réelle, pertes, orientation et calibration au sol.
- **Analyse sur PC** — découper les passages par marqueur. Tracer commande et réponse sur le même axe de temps : ailerons/roulis, direction/lacet et roulis induit, profondeur/tangage et vario. Pour les planés stabilisés, tracer taux de chute en fonction de la vitesse et préciser si elle est mesurée dans l’air ou au sol.
- **Calage du modèle de vol du K3600** — relever masse, centrage et mixages, estimer la polaire sur des planés stabilisés à plusieurs vitesses en air calme, puis rejouer les commandes. Superposer réel et simulation, comparer délai, pic, couplage et amortissement. Garder un vol indépendant pour vérifier les paramètres ajustés.

Premier jalon : un enregistrement sur table avec un mouvement connu par axe et plusieurs marqueurs, avant de choisir entre journal radio seul et enregistreur embarqué.

**Deux variantes à comparer.** Essayer d’abord un vario de parapente GPS si un exemplaire est disponible. Inspecter un export réel pour connaître ses colonnes, sa cadence et ses délais. Avec deux journaux, rechercher un événement observable dans chacun au début et à la fin pour estimer décalage et dérive. Le délai entre commande et réponse du planeur fait partie de la mesure.

**Boîtier transférable.** Lire passivement les voies reçues sur S.BUS, horodater chaque trame et chaque acquisition IMU, pression et GPS sur une horloge commune, puis écrire sur microSD. Déclencher par une voie auxiliaire et marquer les passages pour analyse ultérieure. Par planeur, conserver l’affectation et le sens des voies, les mixages, l’orientation de montage, la masse en vol et le centrage avec le boîtier installé. Vérifier au banc la cadence et les délais réels des capteurs.
