# Guide de Reverse Engineering - Thermor Emotion 4

## 🎯 Objectif

Ce guide détaille la procédure pour analyser et comprendre l'électronique des radiateurs Thermor Emotion 4 afin de remplacer le contrôleur IO-homecontrol par notre solution Zigbee.

## 🔍 Vue d'ensemble de l'architecture

### Architecture typique Thermor
```
┌─────────────────────┐
│   Carte Puissance   │
│  ┌─────────────┐    │
│  │   Triacs    │◄───┼──── 230V AC
│  │  BTA16-600  │    │
│  └──────┬──────┘    │
│         │Gate       │
└─────────┼───────────┘
          │
    Optocoupleurs
          │
┌─────────┼───────────┐
│   Carte Contrôle    │
│  ┌──────▼──────┐    │
│  │     MCU     │    │
│  │ IO-homecontrol│  │
│  └─────────────┘    │
│                     │
│  Capteurs & IHM     │
└─────────────────────┘
```

## 📋 Matériel nécessaire

- Multimètre avec fonction continuité
- Oscilloscope (min 20MHz)
- Appareil photo/smartphone
- Loupe ou microscope USB
- Fer à souder avec pointe fine
- Fils de test fins (AWG 30)
- Carnet de notes et stylos couleur

## 🔧 Procédure d'analyse

### 1. Documentation photographique

#### 1.1 Photos d'ensemble
```bash
# Prendre des photos haute résolution :
- Face avant de chaque PCB
- Face arrière de chaque PCB
- Vue d'ensemble avec connecteurs
- Détails de chaque connecteur
- Marquages des composants
```

#### 1.2 Création d'un plan
Dessiner un schéma de l'implantation :
- Position de chaque connecteur
- Orientation des nappes
- Emplacement des composants principaux

### 2. Identification des composants

#### 2.1 Composants actifs
| Référence | Type | Fonction probable |
|-----------|------|-------------------|
| U1 | MCU | Contrôleur principal |
| U2 | RF Module | Communication IO-homecontrol |
| U3 | EEPROM | Stockage paramètres |
| U4 | RTC | Horloge temps réel |
| Q1-Q2 | MOSFETs | Commutation signaux |

#### 2.2 Capteurs
```c
// Identifier et noter :
- Type de capteur température (NTC, DS18B20, etc.)
- Présence capteur PIR
- Type d'afficheur (7-seg, LCD, OLED)
- Nombre et type de boutons
```

### 3. Analyse des connecteurs

#### 3.1 Mapping des broches
Pour chaque connecteur, créer un tableau :

**Connecteur CN1 (Alimentation)**
| Pin | Signal | Tension | Direction | Notes |
|-----|--------|---------|-----------|-------|
| 1 | L (Phase) | 230V AC | IN | Fusible 10A |
| 2 | N (Neutre) | 0V | IN | Référence |
| 3 | PE (Terre) | - | - | Protection |
| 4 | NC | - | - | Non connecté |

#### 3.2 Test de continuité
```bash
# Procédure sécurisée (hors tension) :
1. Débrancher tous les connecteurs
2. Mode continuité sur multimètre
3. Tester chaque pin vers :
   - Masse/GND
   - Alimentation 3.3V/5V
   - Broches du MCU
   - Autres connecteurs
```

### 4. Analyse des signaux

#### 4.1 Alimentation
```c
// Points de test à vérifier :
- Sortie alimentation : 3.3V ± 0.1V
- Ondulation : < 50mV pp
- Consommation : ~ 50-200mA
```

#### 4.2 Communication triac
Mesurer à l'oscilloscope :
```c
// Signal typique de commande triac
- Niveau bas : 0V
- Niveau haut : 3.3V ou 5V
- Durée impulsion : 10-100µs
- Synchronisé sur zero-cross
```

#### 4.3 Capteur température
Identifier le type :
```c
// NTC (thermistance)
if (resistance_varies_with_temp) {
    // Mesurer R à 25°C (typ. 10kΩ)
    // Noter coefficient β
}

// Numérique (DS18B20, DHT22)
else if (digital_signal) {
    // Identifier protocole (1-Wire, I2C)
    // Noter timing et format
}
```

### 5. Protocoles de communication

#### 5.1 Entre cartes
Analyser les signaux inter-cartes :
```c
// UART
- Baudrate : 9600/115200
- Format : 8N1
- Niveaux : 0-3.3V

// I2C
- Fréquence : 100/400 kHz
- Adresses devices
- Pull-ups présents

// SPI
- Fréquence horloge
- Mode (CPOL/CPHA)
- Chip select actif
```

#### 5.2 Interface utilisateur
```c
// Afficheur 7 segments
- Multiplexage (si présent)
- Driver (MAX7219, TM1637)
- Nombre de digits

// Boutons
- Pull-up/pull-down
- Debounce hardware
- Matrix scanning
```

### 6. Schéma électrique

#### 6.1 Reconstruction du schéma
Utiliser les informations collectées pour :
1. Dessiner le schéma bloc
2. Détailler chaque sous-circuit
3. Identifier les signaux critiques

#### 6.2 Points d'attention
```c
// Isolation galvanique
- Localiser optocoupleurs
- Vérifier distances d'isolation
- Noter barrières de sécurité

// Protection
- Fusibles et leur valeur
- Varistances (MOV)
- Diodes TVS
```

### 7. Documentation des découvertes

#### 7.1 Fiche technique custom
Créer un document avec :
```markdown
## Thermor Emotion 4 - Fiche Technique Reverse

### Microcontrôleur
- Modèle : STM32F103C8T6
- Fréquence : 72MHz
- Flash : 64KB
- Broches utilisées : voir tableau

### Connecteur Principal (CN1)
| Pin | Fonction | Type | Notes |
|-----|----------|------|-------|
| 1 | VCC_3V3 | Power | Régulé |
| 2 | GND | Power | Commun |
| 3 | TRIAC_GATE | Output | Via opto |
| ... | ... | ... | ... |
```

#### 7.2 Code de test
Créer des snippets pour tester :
```c
// Test LED status
void test_status_led() {
    gpio_set_level(GPIO_NUM_18, 1);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    gpio_set_level(GPIO_NUM_18, 0);
}

// Test lecture température
float test_read_temperature() {
    // Code spécifique au capteur identifié
    return temperature;
}
```

## 🛡️ Sécurité

### Points critiques
1. **Ne jamais** mesurer sous 230V sans équipement approprié
2. **Toujours** vérifier l'isolation avant reconnexion
3. **Utiliser** des sondes isolées pour oscilloscope
4. **Porter** des EPI adaptés

### Check-list sécurité
- [ ] Alimentation coupée et consignée
- [ ] Absence de tension vérifiée
- [ ] Condensateurs déchargés
- [ ] Zone de travail dégagée
- [ ] Outils isolés disponibles

## 📊 Exemple de documentation

### Carte contrôle Thermor V2.1
```
MCU : STM32F103C8T6
├── UART1 : Communication carte puissance
├── I2C1 : EEPROM (24C32) + RTC (DS1307)
├── SPI1 : Module RF IO-homecontrol
├── ADC1_CH0 : Capteur température (NTC 10k)
├── TIM2_CH1 : Buzzer
└── GPIO : Boutons (x3) + LED (x2)

Alimentation : HLK-PM03 (230V → 3.3V)
Protection : Fusible 1A + MOV 275V
```

## 🔄 Processus itératif

1. **Observer** : Noter tout ce qui semble important
2. **Hypothèse** : Faire des suppositions sur les fonctions
3. **Tester** : Vérifier les hypothèses par mesure
4. **Documenter** : Consigner toutes les découvertes
5. **Répéter** : Affiner la compréhension

## 📚 Ressources utiles

- [Datasheet STM32F103](https://www.st.com/resource/en/datasheet/stm32f103c8.pdf)
- [IO-homecontrol Protocol](https://github.com/Velocet/io-homecontrol)
- [Triac Control Application Notes](https://www.st.com/resource/en/application_note/an3008.pdf)
- [Forums domotique France](https://forum.home-assistant-france.fr)

---

**Note** : Ce processus peut prendre plusieurs heures. La patience et la méthode sont essentielles pour un reverse engineering réussi.