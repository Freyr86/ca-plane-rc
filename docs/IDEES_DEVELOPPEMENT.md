# Idées de développement possibles

Carnet de brainstorming de Ça plane ! Les idées sont classées par thème. Leur ordre ne fixe aucune priorité de développement.

| Thème | Idée | Maturité | Question à explorer |
|---|---|---|---|
| HUD et télémétrie | Écran radio virtuel alimenté par le simulateur. Le pilote place et redimensionne un ou plusieurs instruments, puis sauvegarde et verrouille sa disposition. | Faisabilité étudiée | Un altimètre et une courbe de vario sur 30 secondes restent-ils lisibles depuis la caméra sol ? |
| Personnalisation | Widgets Lua partageables. Chaque script reçoit une zone de dessin et des mesures en lecture seule. | Faisabilité étudiée | Tester deux widgets à 10 Hz, mesurer le coût sur le PC de référence et vérifier qu’un script bloqué ne perturbe pas le pilotage. |
| Compatibilité radio | Réutilisation de widgets EdgeTX de la TX16S dans le HUD. | À explorer | Choisir un widget réel et relever les capteurs, fonctions de dessin et événements nécessaires. |
| Physique et essais en vol | Caler le K3600 sur des vols réels avec GPS, vario et sorties radio horodatées. | Faisabilité à vérifier sur un premier journal de vol | Estimer la polaire sur des planés stabilisés à plusieurs vitesses en air calme, puis rejouer les commandes et comparer sur un vol indépendant. Relever masse, centrage, mixages, cadence et décalage des capteurs. |
| Audio | Musique du menu, ambiance locale, sons du modèle et télémétrie sonore. | Plan de référence disponible | Comparer à l’oreille un passage lent du Topy et un passage rapide du K3600. [Plan audio](PLAN_AUDIO.md) |

Le vario graphique et le futur vario sonore pourraient partager la même mesure de vitesse verticale du modèle, positive en montée. Le retour de télémétrie vers la radio physique reste une piste distincte, mise de côté.


### Physique — boîte noire du K3600

Piste de développement. Un interrupteur de la TX16S démarre ou termine une séance en vol. Un bouton marque chaque manœuvre dans le journal continu. Un bip confirme le marqueur sans quitter le planeur des yeux.

- **Radio** — journaliser les commandes après mixage, le mode de vol, les trims disponibles, le GPS et le vario. Vérifier sur la radio les champs effectivement enregistrés.
- **À bord** — si une centrale inertielle est ajoutée, conserver les trois vitesses de rotation, trois accélérations et trois composantes du champ magnétique avec leur heure d’acquisition. Enregistrer aussi les voies reçues ou un canal marqueur pour synchroniser les horloges. Tester cadence réelle, pertes, orientation et calibration au sol.
- **Analyse sur PC** — découper les passages par marqueur. Tracer commande et réponse sur le même axe de temps : ailerons/roulis, direction/lacet et roulis induit, profondeur/tangage et vario. Pour les planés stabilisés, tracer taux de chute en fonction de la vitesse et préciser si elle est mesurée dans l’air ou au sol.
- **Validation** — superposer réel et simulation, comparer délai, pic, couplage et amortissement. Garder un vol indépendant pour vérifier les paramètres ajustés.

Premier jalon : un enregistrement sur table avec un mouvement connu par axe et plusieurs marqueurs, avant de choisir entre journal radio seul et enregistreur embarqué.

**Deux variantes à comparer.** Essayer d’abord un vario de parapente GPS si un exemplaire est disponible. Inspecter un export réel pour connaître ses colonnes, sa cadence et ses délais. Avec deux journaux, rechercher un événement observable dans chacun au début et à la fin pour estimer décalage et dérive. Le délai entre commande et réponse du planeur fait partie de la mesure.

**Boîtier transférable.** Lire passivement les voies reçues sur S.BUS, horodater chaque trame et chaque acquisition IMU, pression et GPS sur une horloge commune, puis écrire sur microSD. Déclencher par une voie auxiliaire et marquer les passages pour analyse ultérieure. Par planeur, conserver l’affectation et le sens des voies, les mixages, l’orientation de montage, la masse en vol et le centrage avec le boîtier installé. Vérifier au banc la cadence et les délais réels des capteurs.
