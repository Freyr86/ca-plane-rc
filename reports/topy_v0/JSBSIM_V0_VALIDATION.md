# Validation logicielle JSBSim — Topy V0

## Statut

**Fonctionnement logiciel vérifié, physique non validée.** Le modèle charge, reste numériquement stable, descend en plané et répond aux deux commandes du V-tail. Ces résultats ne prouvent pas encore qu'il reproduit fidèlement le Topy réel.

## Plané neutre

| Calcul | 120 Hz | 240 Hz |
|---|---:|---:|
| Vitesse moyenne sur les 10 dernières secondes | 7.012 m/s | 7.012 m/s |
| Taux de chute moyen | 0.601 m/s | 0.601 m/s |
| Alpha moyen | 3.500° | 3.500° |
| Perte d'altitude en 20 s | 12.014 m | 12.014 m |
| Marge par rapport au temps réel | ×754 | ×357 |

L'écart 120/240 Hz vaut 0.000 % sur la vitesse, 0.000 % sur le taux de chute et 0.000 % sur la perte d'altitude.

## V-tail

- Profondeur +25 % pendant 2 s : panneaux 0.250 / 0.250, tangage final -10.66°.
- Direction +25 % pendant 2 s : panneaux 0.250 / -0.250, cap final 341.59°, roulis final -13.38°.

## Limites connues

- Profil S4083(A) candidat : le profil réel et la modification du bord de fuite restent à mesurer.
- Centrage, inerties, débattements et dérivées de stabilité sont provisoires.
- La polaire utilise les mesures UIUC dans leur domaine et une approximation en dehors.
- Vent, thermiques, sol et contact d'atterrissage ne font pas partie de cette validation.
- La prochaine validation utile consiste à mesurer le Topy réel puis à comparer vitesse, taux de chute et réponses à des échelons de commande.
