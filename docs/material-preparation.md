# Guide de Préparation du Matériel

## Matériel Nécessaire pour le Projet

### 1. Composants Électroniques Essentiels

#### Module Principal
- **ESP32-C6-DevKitC-1** ou **ESP32-C6-WROOM-1**
  - Support Zigbee 3.0 natif
  - WiFi pour debug/configuration
  - Prix: ~10-15€

#### Composants de Protection et Interface
| Composant | Quantité | Référence | Fournisseur | Prix Unit. |
|-----------|----------|-----------|-------------|------------|
| Optocoupleur | 3 | PC817C | Mouser/DigiKey | 0.30€ |
| Régulateur 3.3V | 1 | AMS1117-3.3 | AliExpress | 0.20€ |
| Résistances 10kΩ | 5 | 0805 SMD | Kit général | 0.01€ |
| Résistances 330Ω | 2 | 0805 SMD | Kit général | 0.01€ |
| Résistances 1kΩ | 2 | 0805 SMD | Kit général | 0.01€ |
| Condensateurs 100nF | 3 | 0805 X7R | Kit général | 0.02€ |
| Condensateurs 10µF | 2 | 0805/1206 | Kit général | 0.05€ |
| LED 3mm | 2 | Rouge/Verte | Kit général | 0.10€ |

#### Connecteurs
- JST-XH 2.54mm 6 positions (x1)
- JST-XH 2.54mm 4 positions (x1)
- Headers mâles/femelles 2.54mm
- Fils dupont pour prototypage

### 2. Matériel de Prototypage

#### Option A : Breadboard (Recommandé pour débuter)
- Breadboard 830 points
- Fils de prototypage souples
- Alimentation breadboard 3.3V/5V

#### Option B : Perfboard
- Plaque à pastilles 7x9cm
- Soudure sans plomb 0.8mm
- Flux liquide no-clean

### 3. Outils Indispensables

#### Mesure et Test
- **Multimètre** avec mesure True RMS
  - Mesure tension AC/DC
  - Test continuité
  - Mesure résistance
  
- **Alimentation de laboratoire** (optionnel mais recommandé)
  - 0-30V, 0-3A
  - Protection court-circuit

#### Sécurité Électrique
- **Transformateur d'isolation** 230V/230V 100VA
- **Gants isolants** classe 00 (500V)
- **Lunettes de protection**
- **Tapis isolant** ou plan de travail non conducteur

#### Soudure (si PCB)
- Fer à souder 60W température réglable
- Éponge et support
- Pompe à dessouder
- Loupe ou microscope USB

### 4. Matériel de Test Sécurisé

#### Simulateur de Charge
```
Construction d'un simulateur safe :
┌─────────────────────────────┐
│  Ampoule 40W E27           │
│  ┌───┐                      │
│  │ ○ │ Charge résistive     │
│  └─┬─┘                      │
│    │                        │
│  Domino ──── Sortie triac   │
│    │                        │
│  Terre ───── Protection     │
└─────────────────────────────┘
```

### 5. Liste d'Achat Optimisée

#### Kit de Base (~35€)
1. ESP32-C6-DevKitC-1 : 12€
2. Kit optocoupleurs PC817 (x10) : 3€
3. Kit résistances SMD 0805 : 8€
4. Kit condensateurs SMD : 8€
5. Breadboard + fils : 4€

#### Kit Complet (~80€)
- Kit de base + 
- Alimentation labo : 30€
- Multimètre basique : 15€

#### Kit Pro (~150€)
- Kit complet +
- Transformateur isolation : 50€
- Oscilloscope USB : 20€

### 6. Préparation de l'Espace de Travail

#### Zone de Travail Sécurisée
1. **Table non conductrice** (bois, plastique)
2. **Éclairage** LED blanc 5000K minimum
3. **Prises** avec terre et disjoncteur différentiel
4. **Extincteur** CO2 classe C à proximité

#### Organisation
```
┌─────────────────────────────────────┐
│  Zone 230V    │   Zone Basse        │
│  (Isolation)  │   Tension           │
│               │                     │
│  ┌─────┐      │  ┌──────┐ ┌──────┐ │
│  │Trans│      │  │ESP32 │ │Proto │ │
│  │ fo  │      │  └──────┘ └──────┘ │
│  └─────┘      │                     │
│               │  ┌──────────────┐  │
│  Multimètre   │  │  Oscillo     │  │
│               │  └──────────────┘  │
└─────────────────────────────────────┘
```

### 7. Commandes Recommandées

#### AliExpress (Délai 2-3 semaines)
- ESP32-C6 boards
- Composants passifs en quantité
- Outils basiques

#### Mouser/DigiKey (Délai 2-3 jours)
- Composants critiques (optocoupleurs)
- Qualité garantie
- Datasheet disponibles

#### Amazon
- Outils (multimètre, fer à souder)
- Matériel de sécurité
- Livraison rapide

### 8. Checklist Avant de Commencer

- [ ] ESP32-C6 et câble USB-C
- [ ] Breadboard ou perfboard
- [ ] Composants de base (opto, résistances)
- [ ] Multimètre fonctionnel
- [ ] Espace de travail sécurisé
- [ ] Connaissance des risques 230V
- [ ] Plan de câblage imprimé
- [ ] Firmware de test prêt

### 9. Alternatives et Économies

#### Récupération
- Alimentations 5V/3.3V de chargeurs
- Résistances de vieux appareils
- Fils de câbles ethernet

#### Fabrication Maison
- PCB gravure chimique
- Boîtier impression 3D
- Dissipateurs thermiques DIY

### 10. Ressources Supplémentaires

- [ESP32-C6 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-c6_datasheet_en.pdf)
- [PC817 Datasheet](https://www.farnell.com/datasheets/73758.pdf)
- Forums : esp32.com, eevblog.com
- Discord/Telegram groupes Zigbee

---

**⚠️ RAPPEL SÉCURITÉ**
- Ne JAMAIS travailler sous tension 230V
- TOUJOURS utiliser un transformateur d'isolation
- VÉRIFIER les connexions avant mise sous tension
- En cas de doute, demander de l'aide