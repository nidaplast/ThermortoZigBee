# Guide de Sécurité - ThermortoZigBee

## ⚠️ AVERTISSEMENT CRITIQUE

**CE PROJET IMPLIQUE DES TENSIONS MORTELLES (230V AC)**

La modification d'appareils électriques présente des risques graves :
- ⚡ **Électrocution** pouvant entraîner la mort
- 🔥 **Incendie** en cas de défaut d'isolation
- 💥 **Explosion** de composants sous tension
- 🏠 **Annulation** de l'assurance habitation
- ⚖️ **Responsabilité légale** en cas d'accident

## 🛡️ Qualifications requises

### Minimum requis
- Formation en électricité (CAP/BEP minimum)
- Habilitation électrique BR (recommandée)
- Expérience en électronique de puissance
- Connaissance des normes NF C 15-100

### Compétences essentielles
- Lecture de schémas électriques
- Utilisation d'instruments de mesure
- Soudure de composants CMS
- Compréhension des risques électriques

## 📋 Règles de sécurité fondamentales

### 1. Les 5 règles d'or

1. **Séparer** : Couper l'alimentation au disjoncteur
2. **Condamner** : Verrouiller le disjoncteur avec cadenas
3. **Vérifier** : Tester l'absence de tension avec un VAT
4. **Mettre à la terre** : Court-circuiter et relier à la terre
5. **Baliser** : Délimiter la zone de travail

### 2. Équipements de Protection Individuelle (EPI)

#### Obligatoires
- 🧤 Gants isolants classe 00 (500V)
- 👓 Lunettes de protection
- 👟 Chaussures de sécurité isolantes
- 🔧 Outils isolés 1000V

#### Recommandés
- Tapis isolant
- Écran facial
- Vêtements non conducteurs
- Détecteur de tension sans contact

### 3. Environnement de travail

```
Zone de travail sécurisée :
┌─────────────────────────────────┐
│  ⚡ ZONE DANGEREUSE 230V ⚡      │
│  ┌─────────────────────────┐    │
│  │   Tapis isolant 1000V   │    │
│  │  ┌─────────────────┐    │    │
│  │  │  Plan de travail │    │    │
│  │  │    (isolé)       │    │    │
│  │  └─────────────────┘    │    │
│  └─────────────────────────┘    │
│  Extincteur CO2 à proximité     │
└─────────────────────────────────┘
```

## 🔌 Procédures de sécurité spécifiques

### Avant intervention

1. **Planification**
   ```bash
   □ Lire intégralement la documentation
   □ Préparer tous les outils nécessaires
   □ Vérifier les EPI
   □ Informer un tiers de l'intervention
   ```

2. **Mise hors tension**
   ```bash
   □ Identifier le bon disjoncteur
   □ Couper et verrouiller
   □ Afficher panneau "Ne pas réenclencher"
   □ Vérifier avec VAT sur tous les conducteurs
   ```

3. **Décharge des condensateurs**
   ```bash
   # Attendre 5 minutes minimum
   # Puis court-circuiter avec résistance 1kΩ
   □ Condensateurs d'alimentation
   □ Condensateurs de filtrage
   □ Vérifier tension résiduelle < 50V
   ```

### Pendant l'intervention

#### Manipulation des PCB
```c
// JAMAIS :
- Toucher les pistes 230V à mains nues
- Utiliser des outils non isolés
- Travailler seul
- Court-circuiter accidentellement

// TOUJOURS :
- Utiliser une main (l'autre dans la poche)
- Vérifier deux fois avant de toucher
- Maintenir l'isolation 230V/TBTS
- Respecter les distances d'isolation
```

#### Soudure et modifications
- Ventilation adéquate (fumées)
- Support PCB isolé
- Fer à souder avec terre
- Attention aux projections

### Tests et remise sous tension

#### Contrôles avant mise sous tension
```bash
1. Inspection visuelle
   □ Pas de composants manquants
   □ Soudures propres (pas de courts)
   □ Isolation correcte
   □ Connecteurs bien enfichés

2. Tests hors tension
   □ Continuité de la terre
   □ Isolation phase/terre > 2MΩ
   □ Isolation neutre/terre > 2MΩ
   □ Pas de court-circuit phase/neutre

3. Premier démarrage
   □ Utiliser un transformateur d'isolement
   □ Limiter le courant (ampoule série)
   □ Personne à proximité du disjoncteur
   □ Observer 5 minutes sans toucher
```

## ⚡ Spécificités haute tension

### Distances d'isolation minimales

| Tension | Air | PCB | Traversée |
|---------|-----|-----|-----------|
| 230V AC | 3mm | 2mm | 6mm |
| 400V DC | 5mm | 3mm | 8mm |

### Points critiques du design

1. **Isolation galvanique**
   ```
   230V AC ─┬─[Transfo]─┬─ 3.3V DC
            │           │
            └─[Opto]────┘
   
   Minimum 3kV isolation
   ```

2. **Creepage et clearance**
   - Respecter IEC 60601-1
   - Slots dans PCB si nécessaire
   - Vernis d'isolation sur pistes 230V

3. **Protection contre les surtensions**
   - Fusible rapide en entrée
   - MOV (varistance) 275V
   - TVS sur ligne de signal

## 🚨 Que faire en cas d'accident

### Électrisation/Électrocution

1. **Ne pas toucher la victime** directement
2. **Couper immédiatement** l'alimentation
3. **Appeler les secours** : 15 (SAMU) ou 112
4. **Si formé** : pratiquer les gestes de premier secours

### Début d'incendie

1. **Couper l'alimentation** générale
2. **Utiliser extincteur CO2** (jamais d'eau)
3. **Évacuer** si non maîtrisable
4. **Appeler** les pompiers : 18 ou 112

### Composant qui explose

1. **S'éloigner** immédiatement
2. **Ventiler** la pièce
3. **Couper** l'alimentation
4. **Ne pas toucher** les débris à mains nues

## 📜 Aspects légaux et normes

### Normes applicables
- **NF C 15-100** : Installations électriques
- **EN 60335** : Sécurité appareils électrodomestiques
- **EN 62368** : Équipements TIC
- **Directive 2014/53/UE** : Équipements radio

### Responsabilités
```
⚖️ En modifiant un appareil :
- Vous devenez le "fabricant"
- Vous assumez la conformité CE
- Votre assurance peut refuser de couvrir
- Vous êtes responsable des dommages
```

### Documentation obligatoire
Conserver pendant 10 ans :
- Photos avant/après modification
- Schémas électriques
- Rapports de test
- Procédures suivies

## ✅ Check-list de sécurité

### Avant chaque session
- [ ] EPI complets et en bon état
- [ ] Zone de travail dégagée et sèche
- [ ] Outils isolés disponibles
- [ ] Extincteur CO2 accessible
- [ ] Téléphone portable chargé
- [ ] Personne informée à proximité

### Avant mise sous tension
- [ ] Double vérification des connexions
- [ ] Test d'isolation effectué
- [ ] Capot de protection en place
- [ ] Zone dégagée de tout objet
- [ ] Position de sécurité adoptée

### Après intervention
- [ ] Test fonctionnel complet
- [ ] Documentation mise à jour
- [ ] Rangement sécurisé des outils
- [ ] Étiquetage de l'appareil modifié

## 📞 Numéros d'urgence

| Service | Numéro | Usage |
|---------|--------|-------|
| SAMU | 15 | Urgence médicale |
| Pompiers | 18 | Incendie |
| Urgences | 112 | Tout type |
| Centre anti-poison | 01 40 05 48 48 | Intoxication |

## 🎓 Formation recommandée

Pour travailler en sécurité :
1. Stage habilitation électrique
2. Formation premiers secours (PSC1)
3. Cours sur les normes électriques
4. Pratique encadrée par un professionnel

---

**RAPPEL FINAL** : Si vous avez le moindre doute sur votre capacité à réaliser ces modifications en toute sécurité, **NE LE FAITES PAS**. Faites appel à un professionnel qualifié.

La sécurité n'est pas négociable. Aucun projet ne vaut de risquer sa vie.