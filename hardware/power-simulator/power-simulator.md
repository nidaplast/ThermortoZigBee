# Simulateur de Carte Puissance pour Tests Sécurisés

## Vue d'ensemble

Ce simulateur remplace temporairement la carte puissance du radiateur pour permettre des tests en toute sécurité sans manipuler le 230V directement.

## Schéma du Simulateur

```
                    SIMULATEUR DE CARTE PUISSANCE
┌─────────────────────────────────────────────────────────────────┐
│                                                                 │
│  ALIMENTATION                    SIMULATION TRIAC               │
│  ┌─────────┐                    ┌─────────────┐                │
│  │ USB 5V  │                    │   LED + R    │                │
│  │ ┌─────┐ │      ┌──────┐      │  ┌───┐470Ω  │                │
│  │ │     │ ├──────┤ 3.3V ├──────┤  │LED│──R───┤                │
│  │ └─────┘ │      │ LDO  │      │  └───┘      │                │
│  └─────────┘      └──────┘      └─────────────┘                │
│                                                                 │
│  ZERO CROSS GEN                 CONNECTEUR VERS ESP32           │
│  ┌─────────────┐                ┌─────────────────┐            │
│  │ 555 @ 100Hz │                │ JST-XH 6 broches│            │
│  │  ┌─────┐    ├────────────────┤ 1: GND          │            │
│  │  │ 555 │    │                │ 2: 3V3          │            │
│  │  └─────┘    │                │ 3: ZERO_CROSS   │            │
│  └─────────────┘                │ 4: TRIAC1_CMD   │            │
│                                 │ 5: TRIAC2_CMD   │            │
│  SIMULATION NTC                 │ 6: NC           │            │
│  ┌─────────────┐                └─────────────────┘            │
│  │ POT 10kΩ    │                                               │
│  │  ┌───┐      ├───────────────► Vers ADC ESP32                │
│  │  │POT│      │                                               │
│  │  └───┘      │                                               │
│  └─────────────┘                                               │
└─────────────────────────────────────────────────────────────────┘
```

## Liste des Composants

| Composant | Valeur | Description | Quantité |
|-----------|---------|-------------|----------|
| NE555 | - | Timer pour générer 100Hz | 1 |
| R1 | 10kΩ | Résistance timing 555 | 1 |
| R2 | 68kΩ | Résistance timing 555 | 1 |
| C1 | 100nF | Condensateur timing | 1 |
| C2 | 10µF | Découplage alimentation | 1 |
| LED1, LED2 | Rouge 5mm | Simulation triacs | 2 |
| R3, R4 | 470Ω | Limitation courant LED | 2 |
| POT1 | 10kΩ | Potentiomètre linéaire | 1 |
| REG1 | AMS1117-3.3 | Régulateur 3.3V | 1 |
| SW1 | - | Interrupteur présence | 1 |
| CN1 | JST-XH 6P | Connecteur vers ESP32 | 1 |

## Construction sur Breadboard

```
    A B C D E F G H I J
1   ┌─────────────────┐
2   │     NE555       │
3   │  1 ● ─ ─ ─ ● 8  │  VCC
4   │  2 ●       ● 7  │  DISCHARGE  
5   │  3 ●       ● 6  │  THRESHOLD
6   │  4 ●       ● 5  │  CONTROL
7   └─────────────────┘
8   GND   OUT   TRIG
9   
10  [R1 10k]  [R2 68k]
11  
12  === C1 100nF ===
13  
14  ──────────────────  GND
15  ──────────────────  3V3
16  
17  LED1 ►│ [R3 470Ω]
18  LED2 ►│ [R4 470Ω]
19  
20  POT 10k
21  ┌─┴─┐
22  │   │ → ADC
23  └─┬─┘
24    GND
```

## Code de Test pour le Simulateur

```c
// test_with_simulator.c
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_log.h"

#define TAG "SIM_TEST"

// Test avec simulateur
void test_power_simulator(void) {
    ESP_LOGI(TAG, "=== TEST AVEC SIMULATEUR ===");
    
    // 1. Test fréquence zero cross
    uint32_t last_time = 0;
    uint32_t periods[10];
    int period_count = 0;
    
    ESP_LOGI(TAG, "Mesure fréquence zero cross...");
    while(period_count < 10) {
        if(gpio_get_level(GPIO_ZERO_CROSS)) {
            uint32_t now = esp_timer_get_time();
            if(last_time > 0) {
                periods[period_count++] = now - last_time;
            }
            last_time = now;
            while(gpio_get_level(GPIO_ZERO_CROSS)); // Attendre front descendant
        }
    }
    
    // Calcul fréquence moyenne
    uint32_t avg_period = 0;
    for(int i = 0; i < 10; i++) {
        avg_period += periods[i];
    }
    avg_period /= 10;
    float frequency = 1000000.0f / avg_period;
    
    ESP_LOGI(TAG, "Fréquence détectée: %.1f Hz (attendu: 100Hz)", frequency);
    
    // 2. Test commande triacs (LEDs)
    ESP_LOGI(TAG, "Test LEDs triacs...");
    for(int power = 0; power <= 100; power += 10) {
        ESP_LOGI(TAG, "Puissance: %d%%", power);
        
        // Simulation modulation de phase
        uint32_t delay_us = (100 - power) * 50; // 5ms max pour 100Hz
        
        for(int i = 0; i < 10; i++) {
            // Attendre zero cross
            while(!gpio_get_level(GPIO_ZERO_CROSS));
            
            // Délai de phase
            esp_rom_delay_us(delay_us);
            
            // Pulse triac
            gpio_set_level(GPIO_TRIAC_1, 1);
            esp_rom_delay_us(100);
            gpio_set_level(GPIO_TRIAC_1, 0);
            
            // Attendre fin de demi-période
            while(gpio_get_level(GPIO_ZERO_CROSS));
        }
        
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    // 3. Test potentiomètre (simulation température)
    ESP_LOGI(TAG, "Test potentiomètre température...");
    ESP_LOGI(TAG, "Tournez le potentiomètre...");
    
    for(int i = 0; i < 20; i++) {
        uint32_t adc_val = adc1_get_raw(ADC_TEMP_CHANNEL);
        float voltage = (adc_val / 4095.0f) * 3.3f;
        
        // Simulation conversion température
        // 0V = 0°C, 3.3V = 33°C (linéaire pour test)
        float temp = voltage * 10.0f;
        
        ESP_LOGI(TAG, "ADC: %d, Tension: %.2fV, Temp simulée: %.1f°C", 
                 adc_val, voltage, temp);
        
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    ESP_LOGI(TAG, "✓ Tests simulateur terminés");
}
```

## Protocole de Test Progressif

### Phase 1 : Test à vide
1. Alimenter le simulateur via USB
2. Vérifier la LED power ON
3. Mesurer 3.3V sur les rails d'alimentation

### Phase 2 : Test avec ESP32
1. Connecter l'ESP32 au simulateur via JST
2. Charger le firmware de test
3. Vérifier :
   - Détection du signal 100Hz
   - Clignotement des LEDs selon commande
   - Variation ADC avec potentiomètre

### Phase 3 : Validation complète
1. Test modulation de phase sur LEDs
2. Test algorithme PID avec potentiomètre
3. Simulation profils de température

## Mesures de Sécurité

⚠️ **IMPORTANT** : Ce simulateur est conçu pour fonctionner en basse tension (5V/3.3V) uniquement.

- Ne JAMAIS connecter au 230V
- Utiliser uniquement avec alimentation USB isolée
- Vérifier les connexions avant mise sous tension
- Le simulateur ne remplace pas les tests finaux sur matériel réel

## Évolutions Possibles

1. **Ajout écran OLED** pour afficher :
   - Fréquence détectée
   - Puissance commandée
   - Température simulée

2. **Simulation charge réaliste** :
   - Remplacement LED par charge résistive
   - Mesure courant avec ACS712

3. **Interface PC** :
   - Communication série pour logs
   - Graphiques temps réel
   - Injection de scénarios

## Fichiers de Support

- `simulator_pcb.kicad_pcb` : PCB pour version définitive
- `simulator_test.py` : Script Python pour tests automatisés
- `calibration_data.csv` : Données de calibration ADC

Ce simulateur permet de valider 90% du code avant connexion au radiateur réel, minimisant les risques lors de l'intégration finale.