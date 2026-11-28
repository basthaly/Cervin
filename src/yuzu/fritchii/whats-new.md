## Cervin v1.0.6 — Paramètres réparés, mises à jour automatiques de retour

### Corrections

- **Ouvrir les Paramètres ne bloque plus Cervin** (v1.0.5 : plus rien ne répondait, il fallait le
  fermer de force). Pareil pour les Paramètres du jeu.
- **Les mises à jour automatiques fonctionnent de nouveau** (désactivées par erreur dans la
  v1.0.4 et la v1.0.5).
- Pendant que les Paramètres sont ouverts, leur icône s'allume dans la barre de gauche.

### Conseil pour Ocarina of Time (cartes NVIDIA)

Si des saccades reviennent alors que les shaders sont déjà préparés : dans l'application NVIDIA
(ou le Panneau de configuration NVIDIA > Gérer les paramètres 3D), règle **Taille du cache de
shaders** sur **Illimitée**, puis relance une fois **Émulation > Préparer les shaders maintenant**.
Le cache du pilote est souvent limité à 1 Go, trop petit pour les ~120 000 shaders d'OoT.

### Un problème ?

**Aide > Exporter le journal**, puis ouvre une [issue](https://github.com/basthaly/Cervin/issues/new/choose) avec le zip.

Logiciel libre sous licence GPL-3.0. Shader precompile system by Frizti (Fritchii).
