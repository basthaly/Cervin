## Cervin v0.3 — des réglages lisibles, et MKW démarre à nouveau

Les pages de réglages sont refaites en cartes, comme sur la maquette, et le lancement de
Mario Kart World, qui plantait avec la v0.2, est réparé.

### Réglages en cartes

- **Chaque réglage a sa carte** (onglets Vidéo et Avancée) : ce qu'il fait, son effet « En jeu »,
  des pastilles d'impact (**+ FPS**, **− image**, **info**) et la valeur recommandée.
- **« Explications visibles / Liste compacte »** en haut de page : la liste compacte ne garde que
  les noms et les choix. Ton choix est retenu.
- **Les Paramètres s'ouvrent en grand**, à la taille de la fenêtre principale.

### Image et DLSS

- **Agrandissement, taille de sortie, priorité, Résolution max. et modèle DLSS se choisissent
  en cliquant sur des cartes**, chacune avec son explication.
- Chaque carte de priorité indique la base qu'elle donne (par exemple « Base 720p »). Les
  priorités qui donneraient la même base ne sont pas proposées deux fois.
- **Une carte « Résultat »** dessine la base dans l'image de sortie et indique combien de pixels
  la carte graphique s'épargne.
- Les modèles DLSS indiquent leur netteté et leur stabilité.
- Sous le filtre d'écran, une seule note au lieu de trois.

### Français

- **Tout en français** : la bibliothèque, les contrôles en jeu et les panneaux de
  téléchargement DLSS et FSR 4 restaient en partie en anglais.
- **Cervin te tutoie partout** (il mélangeait « tu » et « vous »).

### Corrections

- **Mario Kart World plantait au lancement avec la v0.2.** Une partie du moteur graphique
  avait été compilée avec une ancienne version des réglages et les lisait au mauvais endroit.

### Problèmes connus

- **Mario Kart World tourne encore très lentement** (1 à 2 images par seconde après l'écran
  titre) : le jeu fait recharger en continu ses grandes textures d'éclairage. C'est le prochain
  chantier.

### À savoir

- Les données sont partagées avec Fritchii (`%APPDATA%\fritchii`) : jeux, clés et sauvegardes sont retrouvés.
- La mise à jour automatique est désactivée : reviens sur cette page pour les nouvelles versions.

### Un problème ?

**Aide > Exporter le journal**, puis ouvre une [issue](https://github.com/basthaly/Cervin/issues/new/choose) avec le zip.

Logiciel libre sous licence GPL-3.0. Shader precompile system by Frizti (Fritchii).
