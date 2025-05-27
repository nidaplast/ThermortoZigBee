# ThermortoZigBee - Conversion des radiateurs Thermor IO-homecontrol vers Zigbee

[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.2-blue)](https://github.com/espressif/esp-idf)
[![Zigbee](https://img.shields.io/badge/Zigbee-3.0-green)](https://csa-iot.org/all-solutions/zigbee/)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

## 🎯 Objectif

Ce projet permet de remplacer complètement l'électronique de contrôle des radiateurs Thermor Emotion 4 (IO-homecontrol) par un ESP32-C6 avec support Zigbee natif, offrant :

- ✅ Intégration native Zigbee 3.0 (Home Assistant, Zigbee2MQTT)
- ✅ Contrôle précis de la puissance (0-100% par modulation de phase)
- ✅ Algorithmes avancés (PID, détection fenêtre, apprentissage)
- ✅ Capteurs intégrés (température, présence, consommation)
- ✅ Interface utilisateur conservée (écran, boutons)
- ✅ Sécurité renforcée (isolation galvanique, watchdog)

## 📋 Prérequis

### Matériel
- ESP32-C6-DevKitC-1 ou module ESP32-C6-WROOM-1
- MOC3063 (optotriac) + BTA16-600B (triac)
- Alimentation AC/DC 3.3V isolée (HLK-PM03)
- Capteur température DS18B20 ou NTC
- Composants passifs (voir BOM)

### Logiciel
- ESP-IDF v5.2+ avec support Zigbee
- KiCad 7.0+ pour les PCB
- Home Assistant avec ZHA ou Zigbee2MQTT

## 🚀 Installation rapide

```bash
# Clone du projet
git clone https://github.com/yourusername/ThermortoZigBee.git
cd ThermortoZigBee

# Configuration ESP-IDF
idf.py set-target esp32c6
idf.py menuconfig

# Compilation et flash
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## 📁 Structure du projet

```
ThermortoZigBee/
├── firmware/           # Code ESP32-C6
│   ├── main/          # Application principale
│   └── components/    # Modules (zigbee, sensors, etc.)
├── hardware/          # Conception PCB
│   ├── kicad/        # Fichiers KiCad
│   └── gerbers/      # Fichiers de production
├── docs/             # Documentation détaillée
├── home-assistant/   # Configurations YAML
└── tools/           # Scripts utilitaires
```

## ⚡ Fonctionnalités principales

### Contrôle de puissance
- Modulation de phase 0-100% (résolution 1%)
- Protection contre les surintensités
- Détection passage par zéro

### Capteurs
- Température ambiante (±0.5°C)
- Détection de présence PIR
- Mesure consommation instantanée
- Détection fenêtre ouverte

### Modes de fonctionnement
- **Confort** : Température de consigne normale
- **Éco** : Réduction de -3.5°C
- **Hors-gel** : Maintien à 7°C
- **Boost** : Chauffe rapide temporaire
- **Programmation** : Planning hebdomadaire

### Intégration Zigbee
- Device Type : Thermostat (0x0301)
- Clusters supportés :
  - Basic (0x0000)
  - Power Configuration (0x0001)
  - Thermostat (0x0201)
  - Occupancy Sensing (0x0406)
  - Electrical Measurement (0x0B04)

## 🔧 Configuration

### 1. Appairage Zigbee
```yaml
# Maintenez le bouton BOOT pendant 5 secondes
# La LED clignote rapidement = mode appairage
```

### 2. Home Assistant
```yaml
climate:
  - platform: mqtt
    name: "Radiateur Salon"
    modes: ["heat", "off"]
    preset_modes: ["comfort", "eco", "away", "boost"]
```

### 3. Paramètres avancés
Via l'interface web locale ou Zigbee :
- Coefficients PID
- Seuils de détection
- Courbe de chauffe

## 📊 Performances

| Paramètre | Valeur |
|-----------|---------|
| Consommation veille | < 0.5W |
| Temps de réponse | < 100ms |
| Précision température | ±0.5°C |
| Résolution puissance | 1% |
| Portée Zigbee | 10-30m |

## 🛡️ Sécurité

- ⚠️ **DANGER** : Intervention sur du 230V
- Isolation galvanique obligatoire
- Protection thermique intégrée
- Watchdog hardware
- Mode failsafe automatique

## 🤝 Contribution

Les contributions sont les bienvenues ! Voir [CONTRIBUTING.md](docs/CONTRIBUTING.md)

## 📝 Licence

Ce projet est sous licence MIT - voir [LICENSE](LICENSE)

## 🙏 Remerciements

- Communauté ESP32 et Espressif
- Projet Zigbee2MQTT
- Forum Home Assistant France

---

**⚠️ AVERTISSEMENT** : Ce projet implique des modifications sur des appareils 230V. L'installation doit être réalisée par une personne qualifiée. L'auteur décline toute responsabilité en cas de dommage.