## Cervin v1.0.1 — Mario Kart World reste fluide après un autre jeu

### Correction

- **Mario Kart World retombait à 1-2 images par seconde quand on le lançait après un autre jeu**
  (par exemple Ocarina of Time) sans fermer Cervin. Des informations laissées par la partie
  précédente lui faisaient recharger sans fin son tableau d'éclairage. Elles sont maintenant
  effacées au début de chaque partie. Vérifié : OoT puis MKW dans le même Cervin, MKW à 40-55
  images par seconde.

Avec la v1.0, fermer et rouvrir Cervin avant de lancer MKW suffisait à éviter le problème.

### Le reste

- Toutes les nouveautés de la [v1.0](https://github.com/basthaly/Cervin/releases/tag/cervin-v1.0).

### Un problème ?

**Aide > Exporter le journal**, puis ouvre une [issue](https://github.com/basthaly/Cervin/issues/new/choose) avec le zip.

Logiciel libre sous licence GPL-3.0. Shader precompile system by Frizti (Fritchii).
