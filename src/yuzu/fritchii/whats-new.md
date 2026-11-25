## Cervin v1.0.4 — version optimisée pour les processeurs plus modestes

### Performances

- **Cervin est maintenant compilé avec clang et optimisé par profil (PGO)**, comme Fritchii,
  au lieu de MSVC : tout l'émulateur va plus vite, surtout sur les processeurs à 6 cœurs.
- **Mario Kart World :** le minuteur interne attend sans occuper un cœur en permanence, ce qui
  libère environ un tiers de cœur pour le jeu (utile en ligne). Ocarina of Time n'est pas touché.

### Problèmes connus

- **Mario Kart World en ligne, mode Survie :** la fluidité peut encore baisser avec beaucoup de
  joueurs, surtout sur les processeurs à 6 cœurs.

### Un problème ?

**Aide > Exporter le journal**, puis ouvre une [issue](https://github.com/basthaly/Cervin/issues/new/choose) avec le zip.

Logiciel libre sous licence GPL-3.0. Shader precompile system by Frizti (Fritchii).
