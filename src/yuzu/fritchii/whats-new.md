## Cervin v1.0.7 — Moins de saccades sur NVIDIA, Paramètres plus rapides et mieux rangés

### Moins de saccades

- **Cartes NVIDIA : cache de shaders du pilote à 8 Go.** Au premier lancement, Cervin te propose
  d'agrandir ce cache, pour Cervin seulement (le pilote le limite souvent à 1 Go, trop petit pour
  les ~120 000 shaders d'OoT). Plus besoin de passer par l'application NVIDIA.
- **Préparation des shaders plus maligne.** Cervin retient les shaders que tu croises en jouant :
  la préparation en arrière-plan commence par les plus fréquents, et pendant la partie il prépare
  en avance ceux qui viennent d'habitude juste après. Ça s'améliore à chaque session.

### Paramètres

- **Ils s'ouvrent quasi instantanément** (environ 25 ms au lieu d'une demi-seconde) : chaque
  onglet est préparé seulement quand tu en as besoin.
- **Vidéo et Vidéo › Avancée sont rangés en sections** (Résolution et upscaling, Netteté et
  filtres, Image, Écran, Performance ; Fréquence d'images, Shaders et saccades, Vitesse, Textures,
  Précision et compatibilité). Clique sur un titre pour replier ou déplier la section.
- Deux réglages internes qui n'avaient rien à faire là sont cachés.

### Un problème ?

**Aide > Exporter le journal**, puis ouvre une [issue](https://github.com/basthaly/Cervin/issues/new/choose) avec le zip.

Logiciel libre sous licence GPL-3.0. Shader precompile system by Frizti (Fritchii).
