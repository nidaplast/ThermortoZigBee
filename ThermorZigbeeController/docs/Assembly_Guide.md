# Guide d'assemblage - Contrôleur Zigbee Thermor

## Vue d'ensemble

Ce guide détaille l'assemblage du contrôleur de remplacement pour radiateurs Thermor Emotion 4, permettant de convertir votre radiateur IO-homecontrol en radiateur Zigbee tout en conservant l'écran LCD et les boutons d'origine.

## ⚠️ AVERTISSEMENT DE SÉCURITÉ

**ATTENTION : Ce projet implique de travailler avec du 230V AC !**
- **Coupez TOUJOURS le disjoncteur** avant toute intervention
- Vérifiez l'absence de tension avec un multimètre
- Si vous n'êtes pas qualifié, faites appel à un électricien
- L'isolation galvanique via optocoupleurs est OBLIGATOIRE

## Matériel nécessaire

### Outils
- Fer à souder avec panne fine
- Soudure sans plomb 0.8mm
- Flux de soudure
- Multimètre
- Oscilloscope (recommandé pour debug)
- Tournevis isolés
- Pince à dénuder
- Pistolet à colle chaude

### Composants
Voir le fichier BOM.csv pour la liste complète. Composants critiques :
- PCB fabriqué (fichiers Gerber fournis)
- ESP32-C6-WROOM-1
- Optocoupleurs PC817 et MOC3021
- Module alimentation HLK-PM03
- Connecteurs JST-XH

## Étape 1 : Préparation du radiateur

1. **Coupez le disjoncteur** du circuit radiateur
2. Démontez le capot avant du radiateur (4 vis généralement)
3. Photographiez tous les connecteurs avant de les débrancher
4. Repérez les connexions :
   - Alimentation secteur (L, N, PE)
   - Triacs de puissance (3 fils vers résistances)
   - Connecteur LCD (6-8 fils)
   - Connecteur boutons (6-10 fils)
   - Capteur température (2 fils)

## Étape 2 : Assemblage du PCB

### Ordre de soudure recommandé

1. **Composants CMS** (si non assemblés par JLCPCB)
   - Résistances et condensateurs 0805
   - MOSFETs BSS138
   - ESP32-C6 (attention à l'orientation)

2. **Composants traversants bas**
   - Résistances
   - Diodes (attention à la polarité)
   - Support CI si utilisé

3. **Connecteurs**
   - JST-XH (LCD, boutons, capteurs)
   - Borniers à vis (puissance)
   - USB-C pour programmation

4. **Composants hauts**
   - Optocoupleurs
   - Module HLK-PM03
   - Condensateurs électrolytiques

### Points d'attention

- Vérifiez la polarité des diodes et condensateurs
- L'ESP32-C6 : le point indique la broche 1
- Espacez bien les pistes haute tension

## Étape 3 : Tests avant installation

### Test alimentation (SANS 230V)
1. Alimentez via USB-C
2. Vérifiez 3.3V sur les points de test
3. LED power doit s'allumer

### Test programmation
1. Connectez USB-C au PC
2. Flashez le bootloader de test
3. Vérifiez la détection de l'ESP32

### Test isolation
1. Multimètre en mode continuité
2. Vérifiez l'isolation entre :
   - Partie 230V et partie basse tension
   - Chaque triac et la masse

## Étape 4 : Connexion des périphériques

### LCD (Connecteur J2)
```
Pin 1: VDD (3.3V ou 5V selon LCD)
Pin 2: VSS (GND)
Pin 3: CS (Chip Select)
Pin 4: WR (Write Clock)
Pin 5: DATA
Pin 6: LED+ (rétroéclairage si présent)
```

### Matrice boutons (Connecteur J3)
```
Pin 1-2: Lignes (ROW1, ROW2)
Pin 3-5: Colonnes (COL1, COL2, COL3)
Pin 6-8: Non utilisés
```

### Capteur température (Connecteur J6)
```
Pin 1: VCC (3.3V)
Pin 2: Signal (vers ADC)
Pin 3: GND
Pin 4: Shield (si blindé)
```

## Étape 5 : Installation finale

1. **Vérifications finales**
   - Tous les connecteurs bien enfichés
   - Pas de court-circuit visible
   - Isolation correcte

2. **Montage mécanique**
   - Fixez le PCB avec entretoises isolantes
   - Éloignez les câbles 230V des câbles basse tension
   - Utilisez de la colle chaude pour sécuriser les connecteurs

3. **Connexion puissance**
   - L (Phase) sur bornier J4-1
   - N (Neutre) sur bornier J4-2
   - PE (Terre) sur bornier J4-3
   - Sorties triacs sur J5 (vers résistances)

## Étape 6 : Premier démarrage

1. **Avant de remettre le courant**
   - Revérifiez toutes les connexions
   - Assurez-vous que le capot est ouvert pour accès

2. **Mise sous tension**
   - Remettez le disjoncteur
   - La LED power doit s'allumer
   - L'écran LCD doit afficher le test pattern

3. **Configuration Zigbee**
   - Appuyez longuement sur MODE pour entrer en appairage
   - LED clignote = mode appairage
   - Associez avec votre coordinateur Zigbee

## Dépannage

### L'ESP32 ne démarre pas
- Vérifiez l'alimentation 3.3V
- Contrôlez le quartz 32.768kHz
- Reflashz le firmware

### LCD n'affiche rien
- Vérifiez les niveaux de tension (3.3V ou 5V)
- Testez avec l'oscilloscope les signaux CS, WR, DATA
- Ajustez le contraste si potentiomètre présent

### Boutons ne répondent pas
- Vérifiez les pull-ups
- Testez la matrice avec multimètre
- Contrôlez le débounce logiciel

### Pas de chauffe
- Vérifiez la détection zero-cross
- Testez les optocoupleurs
- Contrôlez les triacs avec multimètre

## Optimisations possibles

1. **Amélioration thermique**
   - Ajoutez dissipateur sur régulateur
   - Ventilez le boîtier si nécessaire

2. **Amélioration CEM**
   - Ajoutez ferrites sur câbles
   - Blindez les parties sensibles

3. **Fonctionnalités supplémentaires**
   - Ajoutez capteur d'humidité
   - Intégrez mesure consommation
   - Ajoutez buzzer pour feedback

## Support

Pour toute question ou problème :
- Consultez les logs via USB (115200 bauds)
- Activez le mode debug dans le firmware
- Rejoignez le forum de discussion du projet

Bonne installation et profitez de votre radiateur Zigbee !