# Guide d'installation ThermortoZigBee

## ⚠️ Avertissement de sécurité

**DANGER : Ce projet implique des modifications sur des appareils 230V AC. Une mauvaise manipulation peut entraîner des électrocutions, incendies ou dommages matériels. L'installation doit être réalisée uniquement par des personnes qualifiées avec les protections appropriées.**

## 📋 Prérequis

### Compétences requises
- Expérience en électronique et soudure CMS
- Connaissance des normes électriques
- Compréhension des risques liés au 230V
- Capacité à utiliser un multimètre et oscilloscope

### Outils nécessaires
- Station de soudure avec pointe fine
- Flux et tresse à dessouder
- Multimètre
- Oscilloscope (recommandé)
- Tournevis isolés
- EPI (gants isolants, lunettes)
- Programmateur USB-UART (CP2102 ou similaire)

### Composants
Voir le fichier [BOM.csv](../hardware/BOM.csv) pour la liste complète.

## 🔧 Procédure d'installation

### 1. Préparation du radiateur

#### 1.1 Mise hors tension
```
⚡ CRITIQUE : Couper le disjoncteur du circuit
⚡ Vérifier l'absence de tension avec un VAT
⚡ Condamner le disjoncteur
```

#### 1.2 Démontage
1. Retirer le radiateur du mur
2. Dévisser le panneau arrière (6-8 vis)
3. Localiser la carte de contrôle (généralement en haut)
4. Photographier tous les connecteurs avant débranchement
5. Débrancher délicatement tous les connecteurs
6. Retirer la carte de contrôle

### 2. Analyse de la carte originale

#### 2.1 Identification des connexions
Mapper chaque connecteur :
- **CN1** : Alimentation (L, N, PE)
- **CN2** : Contrôle triac (gate, MT1)
- **CN3** : Capteur température
- **CN4** : Interface utilisateur (écran, boutons)
- **CN5** : Capteur PIR (si présent)

#### 2.2 Mesures de sécurité
```bash
# Vérifier les tensions avec la carte originale
# ATTENTION : Manipuler avec précaution
- Alimentation logique : 3.3V ou 5V
- Signal triac : impulsions 5V
- Capteurs : signaux < 3.3V
```

### 3. Préparation du PCB de remplacement

#### 3.1 Assemblage des composants
1. Appliquer la pâte à souder sur les pads
2. Placer les composants CMS (commencer par l'ESP32-C6)
3. Souder au fer ou au pistolet à air chaud
4. Vérifier chaque soudure à la loupe
5. Nettoyer les résidus de flux

#### 3.2 Test initial
```bash
# Test de continuité (hors tension)
- Vérifier absence de court-circuit
- Tester isolation 230V / basse tension
- Mesurer résistances pull-up/down
```

### 4. Programmation du firmware

#### 4.1 Configuration initiale
```bash
# Cloner le projet
git clone https://github.com/yourusername/ThermortoZigBee.git
cd ThermortoZigBee/firmware

# Configurer l'environnement
idf.py set-target esp32c6
idf.py menuconfig
```

#### 4.2 Personnalisation
Éditer `main/thermor_config.h` :
```c
// Adapter selon votre modèle
#define HEATER_MAX_POWER    1500  // Puissance en Watts
#define TEMP_SENSOR_TYPE    DS18B20
#define HAS_PIR_SENSOR      true
```

#### 4.3 Flash du firmware
```bash
# Connecter le programmateur (TX->RX, RX->TX, GND->GND)
# Maintenir BOOT appuyé au démarrage

idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

### 5. Installation dans le radiateur

#### 5.1 Connexions
1. **CRITIQUE** : Vérifier à nouveau l'absence de tension
2. Connecter chaque câble selon le mapping établi
3. Utiliser des connecteurs appropriés (pas de fils volants)
4. Sécuriser tous les câbles avec des serre-câbles

#### 5.2 Isolation
- Vérifier la distance minimale 230V/TBTS : 6mm
- Appliquer du vernis isolant si nécessaire
- S'assurer qu'aucun composant 230V n'est accessible

#### 5.3 Test de sécurité final
```bash
# Avec un mégohmmètre
- Tester isolation phase/terre : > 2 MΩ
- Tester isolation neutre/terre : > 2 MΩ
- Tester continuité PE
```

### 6. Mise en service

#### 6.1 Premier démarrage
1. Remonter partiellement le radiateur (sans fermer)
2. Rétablir l'alimentation au disjoncteur
3. Observer les LEDs de statut
4. Vérifier absence d'échauffement anormal

#### 6.2 Appairage Zigbee
```bash
# Sur Home Assistant / Zigbee2MQTT
1. Activer le mode appairage
2. Maintenir le bouton BOOT 5 secondes
3. LED clignote rapidement = mode appairage
4. Attendre la détection (30-60 secondes)
```

#### 6.3 Configuration initiale
Via l'interface Zigbee :
- Régler la température de consigne
- Configurer les modes (Confort/Eco)
- Ajuster les paramètres PID si nécessaire

### 7. Tests de validation

#### 7.1 Test fonctionnel
- [ ] Changement de température consigne
- [ ] Passage entre les modes
- [ ] Détection de présence
- [ ] Coupure fenêtre ouverte
- [ ] Affichage correct

#### 7.2 Test de sécurité
- [ ] Protection thermique
- [ ] Watchdog actif
- [ ] Mode failsafe
- [ ] Coupure d'urgence

#### 7.3 Test de performance
```yaml
# Mesurer sur 24h
- Stabilité température : ±0.5°C
- Temps de réponse : < 5 min
- Consommation veille : < 0.5W
- Portée Zigbee : > 10m
```

## 🐛 Dépannage

### LED de statut éteinte
1. Vérifier alimentation 3.3V
2. Tester le régulateur HLK-PM03
3. Vérifier les fusibles

### Pas de détection Zigbee
1. Vérifier l'antenne
2. Effacer la flash : `idf.py erase_flash`
3. Reflasher le firmware

### Chauffage ne s'active pas
1. Vérifier le signal triac à l'oscilloscope
2. Tester l'optotriac MOC3063
3. Vérifier le triac de puissance

### Température incorrecte
1. Calibrer le capteur
2. Vérifier les connexions
3. Ajouter condensateur de filtrage

## 📞 Support

- Forum : [home-assistant-france.fr](https://forum.home-assistant-france.fr)
- Issues : [GitHub Issues](https://github.com/yourusername/ThermortoZigBee/issues)
- Wiki : [Documentation complète](https://github.com/yourusername/ThermortoZigBee/wiki)

---

**Rappel** : Ne jamais travailler sous tension. En cas de doute, faire appel à un électricien qualifié.