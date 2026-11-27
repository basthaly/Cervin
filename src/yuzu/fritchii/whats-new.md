## Cervin v1.0.5 — Paramètres dans la fenêtre, shaders d'OoT préparés en jouant

### Interface

- **Les Paramètres et les Paramètres du jeu s'ouvrent dans la fenêtre principale**, à la place de
  la Bibliothèque, au lieu d'une fenêtre à part : les boutons OK et Annuler sont toujours visibles.
  Une icône de la barre de gauche referme la page (comme Annuler).
- Les textes sur la préparation des shaders précisent qu'il faut mettre **Cervin** en pause
  (Émulation > Pause), pas le menu pause du jeu.

### Ocarina of Time

- **Moins de saccades au passage de Link adulte à Link enfant** (et aux autres changements de
  modèles) : la préparation des shaders avance aussi pendant que tu joues, sur 2 threads en
  priorité basse, sur les processeurs à 16 threads ou plus. Mettre Cervin en pause l'accélère.
- La préparation reprend après une fermeture brutale de Cervin (elle restait bloquée pour toujours).

### Mario Kart World

- **Mode 30 FPS et FPS dynamique :** une seule valeur, celle des Paramètres de Cervin. Un réglage
  propre au jeu passait avant sans le dire. Relance MKW après les avoir changés.
- Les copies des résultats que le jeu relit sur le GPU partent plus tôt, pour moins d'attente
  en course sur les processeurs à 6 cœurs.

### Un problème ?

**Aide > Exporter le journal**, puis ouvre une [issue](https://github.com/basthaly/Cervin/issues/new/choose) avec le zip.

Logiciel libre sous licence GPL-3.0. Shader precompile system by Frizti (Fritchii).
