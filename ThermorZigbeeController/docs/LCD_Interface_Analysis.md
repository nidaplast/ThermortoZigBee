# Analyse de l'interface LCD Thermor Emotion 4

## Architecture typique LCD 7-segments

Les radiateurs Thermor utilisent généralement un afficheur LCD 7-segments multiplexé avec:

### Driver LCD: HT1621 ou compatible
- Communication série 3 fils (CS, WR, DATA)
- Tension de fonctionnement: 2.4-5.2V
- 32 segments × 4 commons (ou 128 segments × 8 commons)
- Générateur de bias intégré

### Configuration typique Thermor
```
Affichage:
- 3-4 digits pour température (XX.X°C)
- Icônes spéciales:
  * Mode confort (maison)
  * Mode éco (lune)
  * Mode hors-gel (flocon)
  * Programmation (horloge)
  * Verrouillage (cadenas)
  * Détection présence (personne)
  * Fenêtre ouverte (fenêtre)
```

### Mappage segments (typique)
```
Digit 1: SEG0-SEG7   (dizaines)
Digit 2: SEG8-SEG15  (unités)
Digit 3: SEG16-SEG23 (décimales)
Icônes:  SEG24-SEG31
```

### Signaux d'interface
- VDD: 3.3V ou 5V (vérifier sur carte)
- VSS: GND
- CS: Chip Select (actif bas)
- WR: Write clock
- DATA: Données série
- LED+/LED-: Rétroéclairage (si présent)

## Matrice de boutons

Configuration typique 2×3 ou 3×3:
- Bouton Mode/Menu
- Bouton + (augmenter)
- Bouton - (diminuer)
- Bouton Prog (programmation)
- Bouton OK/Validation
- Bouton Verrouillage (optionnel)

### Connexion matrice
```
     COL1  COL2  COL3
ROW1  [M]   [+]   [-]
ROW2  [P]   [OK]  [L]
```

Les lignes sont scannées séquentiellement avec lecture des colonnes.

## Tensions et niveaux logiques

- LCD driver: Généralement 3.3V ou 5V
- Pull-ups boutons: 10kΩ typique
- Débounce: 20-50ms recommandé
- Fréquence scan matrice: 50-100Hz