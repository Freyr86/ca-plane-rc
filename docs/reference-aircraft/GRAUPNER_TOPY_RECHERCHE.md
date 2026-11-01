# Graupner Topy 4522 — dossier de recherche

**Statut :** candidat pour le planeur physique de référence  
**Dernière recherche :** 13 octobre 2026  
**Identification confirmée par le propriétaire :** **Graupner Topy**, référence documentaire **4522**.

## 1. Pourquoi ce modèle convient bien

Le Topy est assez simple pour isoler les problèmes du moteur physique :

- planeur pur, sans propulsion ;
- deux axes seulement ;
- empennage en V commandant profondeur et direction ;
- faible masse et faible charge alaire ;
- comportement de vol documenté par un essai d'époque ;
- exemplaire réel disponible pour les mesures et les essais comparatifs.

Il est aussi exigeant sur les bons sujets : faible nombre de Reynolds, lacet induit, virage sans ailerons, empennage en V et sensibilité au vent. Il sera donc plus instructif que le Generic 250 actuel.

## 2. Identification

| Élément | Valeur retrouvée | Confiance |
|---|---:|---|
| Marque | Graupner | élevée |
| Nom | Topy | élevée |
| Référence catalogue | 4522 | moyenne à élevée |
| Type | planeur thermique et vent faible, lancer main/sandow | élevée |
| Commandes | profondeur + direction par mixage en V | élevée |
| Époque | commercialisé autour de 2000 | élevée |

La référence 4522 apparaît dans un inventaire d'anciens produits Graupner. L'essai FMT 3/2000 désigne explicitement Graupner comme fabricant. Aucun document fiable trouvé pendant cette recherche n'attribue ce Topy à Robbe.

Il ne faut pas le confondre avec le **Graupner Topsy**, petit avion motorisé d'environ 820 mm d'envergure.

## 3. Données publiées

Les valeurs ci-dessous proviennent de la fiche d'essai FMT, sauf mention contraire.

| Paramètre | Valeur publiée | Valeur SI à employer |
|---|---:|---:|
| Envergure | 1 190 mm | 1,190 m |
| Longueur hors tout | 860 mm | 0,860 m |
| Corde à l'emplanture | 186 mm | 0,186 m |
| Corde au saumon | 135 mm | 0,135 m |
| Envergure de l'empennage en V | 380 mm | 0,380 m |
| Surface alaire | 20,78 dm² | 0,2078 m² |
| Masse annoncée par le fabricant | 360 g | 0,360 kg |
| Masse en vol du modèle essayé | 320 g | 0,320 kg |
| Charge alaire du modèle essayé | 15,4 g/dm² | 1,54 kg/m² |
| Profil à l'emplanture | OCR FMT : « SD 7934 mod. » ; autre source : S4083 | hypothèse S4083 mod., à confirmer |
| Profil d'empennage | plaque plane | à mesurer |

### Construction décrite dans l'essai

- fuselage en fibre de verre ;
- verrière noire en fibre de verre teintée ;
- aile monobloc, entoilée et comportant plusieurs ruptures de dièdre ;
- empennage en V ;
- bord de fuite de l'aile abaissé d'environ 1,5 mm, volontairement, comme un petit volet fixe.

Ce dernier détail doit être reproduit dans la géométrie aérodynamique : il modifie la cambrure et explique probablement le suffixe « mod. » du profil.

## 4. Grandeurs dérivées

Ces valeurs sont calculées à partir des dimensions publiées, pas mesurées sur l'exemplaire réel.

| Grandeur | Résultat |
|---|---:|
| Allongement géométrique `b²/S` | 6,815 |
| Corde moyenne de référence `S/b` | 174,6 mm |
| Charge alaire à 320 g | 15,40 g/dm² |
| Charge alaire à 360 g | 17,32 g/dm² |
| Reynolds approximatif à 5 m/s | 58 000 |
| Reynolds approximatif à 7 m/s | 81 000 |
| Reynolds approximatif à 12 m/s | 140 000 |

Le Reynolds utilise la corde moyenne et une viscosité cinématique arrondie à `1,5 × 10⁻⁵ m²/s`. La plage montre qu'une polaire calculée uniquement à Reynolds élevé ne conviendra pas.

### Incohérence utile à résoudre

Une aile purement trapézoïdale de 1,190 m, avec les cordes publiées de 186 et 135 mm, aurait une surface de **0,1910 m²**, soit environ 8 % de moins que les **0,2078 m²** publiés. L'aile réelle n'est donc pas réductible à ce trapèze sans mesurer les tronçons, les ruptures de bord et la convention de surface utilisée.

## 5. Comportement rapporté

L'essai FMT décrit :

- un faible taux de chute et de bonnes capacités en thermique faible ;
- la possibilité de virer très serré ;
- un décrochage difficile à provoquer dans l'utilisation normale ;
- un comportement calme à faible vitesse, mais encore assez maniable pour des boucles ;
- des débattements recommandés par la notice jugés adaptés ;
- plusieurs trims de profondeur utiles pour le sandow, la thermique et le vol rapide.

Ces appréciations donnent des tendances à vérifier, pas encore des cibles numériques. L'essai mentionne aussi des vols proches de quatre minutes après un lancement au sandow dans de mauvaises conditions, mais cela ne constitue pas une mesure de finesse exploitable.

## 6. Données encore manquantes

Les recherches n'ont pas permis de retrouver une notice complète donnant des valeurs lisibles pour :

- position du centre de gravité ;
- débattements de profondeur et de direction ;
- angle total de l'empennage en V ;
- surface et cordes de l'empennage ;
- positions exactes des ruptures de dièdre ;
- calage de l'aile et de l'empennage ;
- vrillage de l'aile ;
- profil exact au saumon ;
- inerties et répartition des masses ;
- polaires mesurées et performances chiffrées.

La lecture OCR « SD 7934 mod. » doit rester provisoire : aucun profil usuel correspondant n'a été retrouvé. Dans une discussion technique, un ancien utilisateur indique explicitement avoir utilisé un **S4083** sur son Topy et décrit le même bord de fuite abaissé. La base UIUC confirme que le S4083 est un profil de 8 % conçu pour les planeurs RC lancés main, ce qui correspond exactement à la catégorie du Topy.

L'hypothèse de travail devient donc **S4083 modifié**, avec une confiance moyenne. Même si le nom est confirmé, la modification du bord de fuite impose de relever la section réelle ou de reconstruire précisément cette modification.

## 7. Stratégie de modélisation proposée

1. Photographier les éventuelles étiquettes et la décoration pour documenter la variante exacte.
2. Mesurer l'exemplaire prêt à voler avec la fiche `TOPY_MESURES_IRL.md`.
3. Relever l'aile par tronçons et créer la géométrie dans XFLR5.
4. Comparer le profil S4083 candidat et la section réelle, sans choisir sur le seul résultat OCR.
5. Produire des polaires sur environ `Re = 50 000 à 160 000`.
6. Construire un premier modèle 6-DOF sans ajustement empirique caché.
7. Faire des essais réels simples et filmés pour régler uniquement les données impossibles à mesurer directement.

## 8. Sources

- [Essai « Topy von Graupner », FMT 3/2000, p. 92 et suivantes](https://rcbookcase.com/data/media/47/03FMT_March_2000.pdf) — source principale pour les dimensions, la masse, la construction et le comportement.
- [Index officiel du numéro FMT 3/2000](https://www.vth.de/fmt/heftarchiv/jahrgaenge-bis-2013/2000) — confirme le numéro, le titre de l'article, l'auteur Frank Witt et la page 92.
- [Inventaire mentionnant « Graupner, Typ Topy 4522 »](https://www.rc-network.de/threads/graupner-lageraufl%C3%B6sung-rarit%C3%A4ten.406498/) — recoupement de la référence produit, source secondaire.
- [Discussion d'utilisateurs du Topy](https://www.rc-network.de/threads/topy.168722/) — retours communautaires, utilisés seulement comme indications qualitatives.
- [Discussion technique mentionnant le S4083 sur un Topy](https://www.rc-network.de/threads/zum-x-ten-mal-auslegung-thermiksegler.46568/) — indice secondaire pour identifier le profil et sa modification de bord de fuite.
- [UIUC Airfoil Coordinates Database](https://m-selig.ae.illinois.edu/ads/coord_database.html) — base de référence à consulter après confirmation du profil exact.
- [Fiche UIUC du S4083](https://m-selig.ae.illinois.edu/ads/ref/s4083_ref.html) — profil de 8 % destiné au planeur RC lancé main.

