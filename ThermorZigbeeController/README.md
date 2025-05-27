# Thermor Zigbee Controller - Remplacement complet

Projet de remplacement du contrôleur Thermor Emotion 4 par ESP32-C6 avec réutilisation LCD/boutons.

## Structure du projet

- `hardware/` - Schémas KiCad et fichiers de fabrication PCB
- `firmware/` - Code ESP32 complet avec drivers LCD/boutons
- `docs/` - Documentation technique et guides
- `tools/` - Scripts et utilitaires

## Caractéristiques

- ESP32-C6 avec Zigbee natif
- Réutilisation écran LCD 7-segments existant
- Interface boutons originale conservée
- Contrôle triacs isolé galvaniquement
- Algorithme PID optimisé
- Interface utilisateur complète