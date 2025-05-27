# Hardware Design - Thermor Zigbee Controller

## Vue d'ensemble

Ce dossier contient tous les fichiers nécessaires pour la fabrication du PCB de remplacement pour les radiateurs Thermor Emotion 4.

## Structure des fichiers

```
hardware/
├── kicad/
│   ├── thermor_zigbee_controller.kicad_pro    # Projet KiCad
│   ├── thermor_zigbee_controller.kicad_sch    # Schéma électronique
│   ├── thermor_zigbee_controller.kicad_pcb    # Circuit imprimé
│   ├── thermor_zigbee_controller.kicad_prl    # Préférences locales
│   ├── thermor_zigbee_controller.net          # Netlist
│   ├── thermor_zigbee_controller-cache.lib    # Cache des symboles
│   ├── fp-lib-table                           # Table des empreintes
│   ├── sym-lib-table                          # Table des symboles
│   └── Thermor_Custom.lib                     # Bibliothèque custom
├── fabrication/
│   ├── gerbers/                               # Fichiers Gerber pour fab
│   ├── drill/                                 # Fichiers de perçage
│   └── assembly/                              # Fichiers d'assemblage
├── 3d/
│   └── thermor_controller_3d.png              # Vue 3D du PCB
├── BOM.csv                                    # Liste des composants
└── README.md                                  # Ce fichier
```

## Spécifications du PCB

- **Dimensions** : 80 x 60 mm
- **Nombre de couches** : 2
- **Épaisseur** : 1.6 mm
- **Finition** : HASL sans plomb
- **Couleur** : Vert (masque), Blanc (sérigraphie)
- **Via minimum** : 0.3 mm
- **Piste minimum** : 0.2 mm

## Composants principaux

| Référence | Composant | Description |
|-----------|-----------|-------------|
| U1 | ESP32-C6-WROOM-1 | Microcontrôleur principal avec Zigbee |
| U2 | HT1621 | Contrôleur LCD 7 segments |
| U3-U5 | PC817 | Optocoupleurs isolation triacs |
| U6 | AMS1117-3.3 | Régulateur 3.3V |
| J1 | JST-XH 3P | Connecteur capteur PIR |
| J2 | JST-XH 6P | Connecteur carte puissance |
| J3 | FPC 20P | Connecteur LCD |
| J4 | JST-XH 8P | Connecteur boutons |

## Instructions de fabrication

### 1. Génération des Gerbers (KiCad 7.0+)

1. Ouvrir `thermor_zigbee_controller.kicad_pcb`
2. Fichier → Tracer → Format Gerber
3. Cocher les couches :
   - F.Cu (Cuivre face avant)
   - B.Cu (Cuivre face arrière)
   - F.Paste (Pâte à braser avant)
   - B.Paste (Pâte à braser arrière)
   - F.SilkS (Sérigraphie avant)
   - B.SilkS (Sérigraphie arrière)
   - F.Mask (Masque soudure avant)
   - B.Mask (Masque soudure arrière)
   - Edge.Cuts (Contour PCB)
4. Générer aussi les fichiers de perçage

### 2. Commande chez JLCPCB/PCBWay

1. Zipper tous les Gerbers
2. Uploader sur le site du fabricant
3. Spécifications recommandées :
   - 2 couches
   - 1.6mm épaisseur
   - HASL sans plomb
   - Vert/Blanc
   - Quantité : 5-10 pièces

### 3. Assemblage

#### Ordre de soudure recommandé :
1. **Composants CMS côté TOP** :
   - ESP32-C6 (utiliser flux et air chaud)
   - Résistances/condensateurs 0805
   - Régulateur AMS1117
   - HT1621 si CMS

2. **Composants traversants** :
   - Optocoupleurs PC817
   - Connecteurs JST
   - Condensateurs électrolytiques

3. **Test avant connexion** :
   - Vérifier courts-circuits
   - Tester alimentation 3.3V
   - Programmer ESP32 avec firmware de test

## Points d'attention

### Isolation galvanique
- Les optocoupleurs PC817 assurent l'isolation entre 230V et basse tension
- Respecter un espacement minimum de 3mm entre les zones
- Ne pas router de pistes sous les optocoupleurs

### Thermique
- L'ESP32-C6 peut chauffer légèrement
- Prévoir des vias thermiques sous le module
- Espacer du régulateur de tension

### CEM (Compatibilité Électromagnétique)
- Plan de masse continu côté BOTTOM
- Condensateurs de découplage proches des IC
- Pistes courtes pour signaux rapides

## Modifications possibles

1. **Version mini** (60x40mm) :
   - Retirer certains connecteurs
   - Utiliser ESP32-C6 bare chip

2. **Version pro** avec :
   - Mesure de puissance intégrée
   - RTC avec supercap
   - Antenne externe

## Tests de validation

Liste de vérification avant utilisation :

- [ ] Continuité masse
- [ ] Isolation 230V/3.3V > 1MΩ
- [ ] Tension 3.3V stable
- [ ] Communication LCD fonctionnelle
- [ ] Lecture boutons OK
- [ ] Détection PIR active
- [ ] Commande triacs (avec simulateur)
- [ ] Communication Zigbee

## Support

Pour toute question sur le hardware :
- Vérifier les datasheets des composants
- Consulter le schéma annoté
- Tester avec le firmware de test basique

## Licence

Ce design hardware est publié sous licence Open Hardware.
Vous êtes libre de le modifier et l'utiliser.