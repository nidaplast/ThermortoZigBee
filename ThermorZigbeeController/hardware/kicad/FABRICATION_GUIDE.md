# Guide de Fabrication PCB - Thermor Zigbee Controller

## Prérequis

- KiCad 7.0 ou supérieur installé
- Tous les fichiers du projet dans le dossier `hardware/kicad/`

## Étape 1 : Vérification du Projet

1. Ouvrir KiCad
2. Ouvrir le fichier `thermor_zigbee_controller.kicad_pro`
3. Vérifier que le schéma s'ouvre sans erreur
4. Vérifier que le PCB s'ouvre correctement

## Étape 2 : Génération des Gerbers

### Dans KiCad PCB Editor :

1. **Fichier → Tracer**
2. **Format de sortie** : Gerber
3. **Répertoire de sortie** : `../fabrication/gerbers/`

### Couches à sélectionner :

- ✓ F.Cu (Front Copper)
- ✓ B.Cu (Back Copper)  
- ✓ F.Paste (Front Solder Paste)
- ✓ B.Paste (Back Solder Paste)
- ✓ F.SilkS (Front Silkscreen)
- ✓ B.SilkS (Back Silkscreen)
- ✓ F.Mask (Front Solder Mask)
- ✓ B.Mask (Back Solder Mask)
- ✓ Edge.Cuts (Board Outline)

### Options :

- ✓ Tracer les valeurs des composants
- ✓ Tracer les références des composants
- Format coordonnées : 4.6
- ✓ Utiliser les extensions Protel

4. Cliquer **Tracer**

## Étape 3 : Génération des Fichiers de Perçage

1. Dans la même fenêtre, cliquer **Générer fichiers de perçage**
2. Format : Excellon
3. Unités : Millimètres
4. Format : 3.3
5. **Répertoire** : `../fabrication/drill/`
6. Cliquer **Générer**

## Étape 4 : Préparation pour JLCPCB

### Renommer les fichiers :

```bash
cd fabrication/gerbers/
mv thermor_zigbee_controller-F_Cu.gbr thermor_zigbee_controller.GTL
mv thermor_zigbee_controller-B_Cu.gbr thermor_zigbee_controller.GBL
mv thermor_zigbee_controller-F_Mask.gbr thermor_zigbee_controller.GTS
mv thermor_zigbee_controller-B_Mask.gbr thermor_zigbee_controller.GBS
mv thermor_zigbee_controller-F_SilkS.gbr thermor_zigbee_controller.GTO
mv thermor_zigbee_controller-B_SilkS.gbr thermor_zigbee_controller.GBO
mv thermor_zigbee_controller-Edge_Cuts.gbr thermor_zigbee_controller.GKO
mv thermor_zigbee_controller-F_Paste.gbr thermor_zigbee_controller.GTP
mv thermor_zigbee_controller-B_Paste.gbr thermor_zigbee_controller.GBP

cd ../drill/
mv thermor_zigbee_controller.drl thermor_zigbee_controller.TXT
```

### Créer le ZIP :

```bash
cd ..
zip -r thermor_zigbee_pcb.zip gerbers/*.G* gerbers/*.TXT drill/*.TXT
```

## Étape 5 : Commander chez JLCPCB

1. Aller sur [jlcpcb.com](https://jlcpcb.com)
2. Cliquer **Quote Now**
3. Uploader `thermor_zigbee_pcb.zip`
4. Paramètres recommandés :
   - **Layers** : 2
   - **Dimensions** : 80 x 60 mm (auto-détecté)
   - **PCB Thickness** : 1.6mm
   - **PCB Color** : Green
   - **Silkscreen** : White
   - **Surface Finish** : HASL Lead Free
   - **Outer Copper Weight** : 1oz
   - **Via Covering** : Tented
   - **Remove Order Number** : Yes (payant)

5. **Quantité** : 5 ou 10 pièces
6. Vérifier l'aperçu Gerber
7. Passer commande

## Étape 6 : Alternative PCBWay

Si vous préférez PCBWay :

1. [pcbway.com](https://www.pcbway.com)
2. Instant Quote → PCB
3. Upload du même ZIP
4. Mêmes paramètres
5. Options supplémentaires disponibles :
   - Via-in-pad
   - Finition ENIG (meilleure qualité)

## Vérification Gerbers

Avant de commander, vérifier avec :

### Option 1 : Viewer en ligne
- [Tracespace View](https://tracespace.io/view/)
- [Gerber Viewer](https://gerber-viewer.ucamco.com/)

### Option 2 : KiCad
- Outils → Visualiseur Gerber
- Ouvrir tous les fichiers .G*
- Vérifier chaque couche

## Checklist Finale

- [ ] Dimensions PCB correctes (80x60mm)
- [ ] Tous les trous sont présents
- [ ] Sérigraphie lisible
- [ ] Pas de court-circuit visible
- [ ] Références composants visibles
- [ ] Contour PCB fermé
- [ ] Fichiers de perçage inclus

## Coûts Estimés

| Fabricant | 5 PCB | 10 PCB | Délai |
|-----------|-------|--------|-------|
| JLCPCB | ~2$ | ~5$ | 7-10j |
| PCBWay | ~5$ | ~10$ | 7-10j |
| OSHPark | ~30$ | ~60$ | 14-21j |

*Prix hors livraison

## Support

En cas de problème :
1. Vérifier que KiCad est à jour
2. Regénérer les Gerbers
3. Utiliser le viewer en ligne pour validation
4. Contacter le support du fabricant avec captures d'écran

## Assemblage

Une fois les PCB reçus, suivre le guide `assembly-guide.md` pour le montage des composants.