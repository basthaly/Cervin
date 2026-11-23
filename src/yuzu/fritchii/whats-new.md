## Cervin v0.2 — une interface plus claire

Cette version revoit l'interface : réglages expliqués, DLSS plus simple, nouvelle bibliothèque de
jeux. Rien ne change dans l'émulation elle-même par rapport à la v0.1.

### Bibliothèque

- **Les jeux s'affichent en cartes** : jaquette, nom, bouton **Jouer** et bouton ⚙ des réglages du jeu.
  La grille s'adapte à la largeur de la fenêtre. La liste classique reste accessible avec « Tous les jeux ».
- **Réglages rapides** à côté de la grille, pour le jeu sélectionné : priorité DLSS, modèle DLSS,
  mode console (TV ou portable) et limite de vitesse. Ils sont enregistrés dans les réglages de ce
  jeu seulement. « Comme dans les Paramètres » reprend le réglage général.

### DLSS

- **Choisissez seulement la résolution de sortie** et une priorité : Image, Équilibre, FPS, FPS max
  ou Lissage seul (DLAA). Cervin calcule la résolution de base qui donne vraiment un gain, et
  indique combien de pixels la carte graphique s'épargne.
- **Les modèles DLSS ont un nom clair** (K : polyvalent, M : petite base pour les FPS, L : 4K depuis
  une très petite base, J : un peu plus net que K). Une explication s'affiche sous le choix.

### Réglages expliqués

- **Une ligne d'explication sous chaque réglage** des onglets Graphismes et Graphismes avancés :
  ce qu'il fait et son effet sur l'image ou les FPS.
- **Le filtre d'écran est rangé par usage** : Réduire, Agrandir, Pixel art, Simple, Avancé.
- **Les effets d'image** (menu « Ajouter un effet ») sont rangés par famille : netteté, couleurs,
  ambiance, rétro.

### Mario Kart World

- **Plus de question à chaque lancement.** Les réglages recommandés pour MKW sont proposés dans
  les réglages du jeu (bouton ⚙ de sa carte, ou clic droit > Configurer le jeu), avec un tableau
  « le tien / recommandé / pourquoi ». « Les utiliser » les applique à chaque lancement,
  « Les désactiver » revient à vos réglages.

### Problèmes connus

- **Mario Kart World** peut encore planter ou se figer en course sur certaines machines : les
  correctifs mémoire de la v0.1 sont toujours en cours de test. Envoyez le journal si ça arrive.

### À savoir

- Les données sont partagées avec Fritchii (`%APPDATA%\fritchii`) : jeux, clés et sauvegardes sont retrouvés.
- La mise à jour automatique est désactivée : revenez sur cette page pour les nouvelles versions.

### Un problème ?

**Aide > Exporter le journal**, puis ouvrez une [issue](https://github.com/basthaly/Cervin/issues/new/choose) avec le zip.

Logiciel libre sous licence GPL-3.0. Shader precompile system by Frizti (Fritchii).
