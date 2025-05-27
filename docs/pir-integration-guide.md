# Guide d'Intégration du Capteur de Présence PIR

## Vue d'ensemble

Le projet intègre un capteur PIR HC-SR501 pour la détection de présence, permettant une gestion intelligente du chauffage basée sur l'occupation de la pièce.

## Caractéristiques du Capteur PIR

### HC-SR501 Specifications
- **Tension** : 3.3V (compatible ESP32 directement)
- **Portée** : 3-7 mètres (ajustable)
- **Angle détection** : 120°
- **Temps de maintien** : 5s-300s (ajustable)
- **Sensibilité** : Ajustable via potentiomètre
- **Sortie** : Digital HIGH (3.3V) lors de détection

### Connexion Hardware

```
HC-SR501          ESP32-C6
┌─────┐          ┌────────┐
│ VCC ├──────────┤ 3.3V   │
│ OUT ├──────────┤ GPIO7  │
│ GND ├──────────┤ GND    │
└─────┘          └────────┘
```

### Schéma d'intégration dans le radiateur

```
                    Radiateur Thermor
┌─────────────────────────────────────────────┐
│                                             │
│  ┌─────────┐        ┌──────────────┐       │
│  │   PIR   │        │  ESP32-C6    │       │
│  │ HC-SR501│        │              │       │
│  │         ├────────┤ GPIO7        │       │
│  └─────────┘        │              │       │
│                     │              │       │
│  Position idéale:   └──────────────┘       │
│  - Hauteur: 1-1.5m                         │
│  - Angle: vers le centre de la pièce       │
│  - Éviter: sources de chaleur directes     │
└─────────────────────────────────────────────┘
```

## Configuration Logicielle

### 1. Initialisation du Capteur

```c
// Dans thermor_zigbee.h
#define GPIO_PIR_SENSOR     7
#define PIR_DEBOUNCE_MS     500
#define PIR_TIMEOUT_MIN     30  // Minutes avant passage en éco

// Initialisation
void init_presence_sensor(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_PIR_SENSOR),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_POSEDGE
    };
    gpio_config(&io_conf);
}
```

### 2. Logique de Détection

```c
// Gestion de la présence avec hystérésis
typedef struct {
    bool presence_active;
    uint32_t last_detection;
    uint32_t absence_count;
} presence_state_t;

void update_presence_state(presence_state_t *state) {
    if (gpio_get_level(GPIO_PIR_SENSOR)) {
        state->presence_active = true;
        state->last_detection = esp_timer_get_time() / 1000;
        state->absence_count = 0;
    } else {
        uint32_t elapsed = (esp_timer_get_time() / 1000) - state->last_detection;
        if (elapsed > PIR_TIMEOUT_MIN * 60 * 1000) {
            state->presence_active = false;
        }
    }
}
```

### 3. Intégration avec le Contrôle de Température

```c
// Ajustement automatique selon présence
float calculate_target_temp(thermostat_mode_t mode, bool presence) {
    switch(mode) {
        case MODE_COMFORT:
            return presence ? 21.0f : 19.0f;  // -2°C si absent
        case MODE_ECO:
            return presence ? 19.0f : 17.0f;
        case MODE_FROST_PROTECT:
            return 7.0f;  // Toujours 7°C
        default:
            return 19.0f;
    }
}
```

## Modes de Fonctionnement

### 1. Mode Présence Active
- Température de confort maintenue
- Réactivité maximale
- LED état allumée

### 2. Mode Absence Temporaire (< 30 min)
- Maintien température réduite (-1°C)
- Retour rapide au confort si présence

### 3. Mode Absence Prolongée (> 30 min)
- Passage automatique en éco (-3°C)
- Économies d'énergie significatives

### 4. Mode Programmation avec Présence
- La détection de présence peut outrepasser la programmation
- Option configurable via menu

## Configuration Zigbee

### Cluster Occupancy Sensing (0x0406)

```c
// Attributs exposés via Zigbee
typedef struct {
    uint8_t occupancy;           // 0x0000: 0=absent, 1=présent
    uint8_t occupancy_sensor_type; // 0x0001: 0=PIR
    uint16_t pir_u_to_o_delay;   // 0x0011: Délai présent->absent
    uint16_t pir_o_to_u_delay;   // 0x0010: Délai absent->présent
} occupancy_cluster_t;
```

### Intégration Home Assistant

```yaml
binary_sensor:
  - platform: zigbee2mqtt
    name: "Radiateur Salon Présence"
    state_topic: "zigbee2mqtt/radiateur_salon"
    value_template: "{{ value_json.occupancy }}"
    device_class: occupancy

automation:
  - alias: "Chauffage intelligent avec présence"
    trigger:
      - platform: state
        entity_id: binary_sensor.radiateur_salon_presence
    action:
      - choose:
          - conditions:
              - condition: state
                entity_id: binary_sensor.radiateur_salon_presence
                state: "on"
            sequence:
              - service: climate.set_preset_mode
                target:
                  entity_id: climate.radiateur_salon
                data:
                  preset_mode: "comfort"
          - conditions:
              - condition: state
                entity_id: binary_sensor.radiateur_salon_presence
                state: "off"
                for: "00:30:00"
            sequence:
              - service: climate.set_preset_mode
                target:
                  entity_id: climate.radiateur_salon
                data:
                  preset_mode: "eco"
```

## Optimisation et Réglages

### 1. Ajustement Sensibilité PIR

Le HC-SR501 dispose de deux potentiomètres :
- **Sensibilité** : Tourner dans le sens horaire pour augmenter
- **Temps** : Durée du signal HIGH après détection

### 2. Positionnement Optimal

```
     Vue du dessus de la pièce
    ┌─────────────────────────┐
    │                         │
    │         ╱───╲          │
    │        ╱ PIR ╲         │ Zone de
    │       ╱  120° ╲        │ détection
    │      ╱─────────╲       │
    │     ╱───────────╲      │
    │    ╱─────────────╲     │
    └────────────────────────┘
```

### 3. Éviter les Faux Positifs

- Ne pas placer face à une fenêtre
- Éviter proximité sources de chaleur
- Éloigner des ventilateurs/climatiseurs
- Masquer partiellement pour réduire la zone

## Diagnostic et Dépannage

### LED de Statut
- **Allumée fixe** : Présence détectée
- **Éteinte** : Aucune présence
- **Clignotante** : Mode test/calibration

### Commandes de Test

```bash
# Via console série ESP32
> pir status
PIR Status: ACTIVE
Last detection: 45s ago
Presence mode: COMFORT
Detection count: 127

> pir test
Starting PIR test mode...
Move to trigger detection
Detection 1: OK (signal: HIGH)
Detection 2: OK (signal: HIGH)
Test completed: PASS
```

### Problèmes Courants

| Problème | Cause | Solution |
|----------|-------|----------|
| Détections intempestives | Sensibilité trop élevée | Réduire via potentiomètre |
| Pas de détection | Câblage ou alimentation | Vérifier connexions |
| Détection retardée | Temps de stabilisation | Attendre 60s au démarrage |
| Zone trop large | Angle trop ouvert | Masquer partiellement |

## Statistiques et Métriques

Le système enregistre :
- Nombre de détections par jour
- Temps de présence total
- Économies réalisées
- Patterns d'occupation

Ces données sont accessibles via :
- Interface LCD (menu stats)
- Zigbee (attributs custom)
- Logs série pour debug

## Économies d'Énergie

Exemple de gains avec détection de présence :

| Scenario | Sans PIR | Avec PIR | Économie |
|----------|----------|----------|----------|
| Bureau 8h/jour | 100% | 35% | 65% |
| Salon soirée | 100% | 60% | 40% |
| Chambre nuit | 100% | 80% | 20% |

Économie moyenne : **30-40%** sur la consommation