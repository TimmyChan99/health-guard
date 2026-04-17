# SPRM-Patient - Moniteur de Signes Vitaux

## Description

Ce projet est un système de monitoring des signes vitaux pour patients basé sur ESP32. Il surveille la température corporelle, la pression artérielle, et détecte les chutes.

## Matériel Requis

- ESP32 (uPesy Wroom)
- Capteur BME280 (température, pression, humidité)
- Écran TM1637 (7 segments)
- MPU6050 (accéléromètre pour détection de chute)
- Bouton SOS
- Buzzer
- LED d'alerte

## Broches GPIO

| GPIO | Composant |
|------|---------|
| 5    | TM1637 DIO |
| 18   | TM1637 CLK |
| 21   | I2C SDA (BME280, MPU6050) |
| 22   | I2C SCL (BME280, MPU6050) |
| 26   | Buzzer |
| 33   | LED Alerte |
| 34   | Bouton SOS |

---

## Comment utiliser

### 1. Premier démarrage

1. Brancher l'ESP32 par USB
2. Le point d'accès WiFi "Hospital_Device2" apparaît
3. Connecter votre téléphone/ordinateur à ce WiFi
4. Mot de passe: **12345678**

### 2. Configuration WiFi

1. Ouvrir navigateur → **http://192.168.4.1**
2. WiFiManager s'affiche
3. Sélectionner votre réseau WiFi
4. Entrer le mot de passe WiFi
5. Cliquer "Save"
6. L'ESP32 redémarre et se connecte au WiFi

### 3. Interface Health Monitor

Après connexion WiFi:
- **http://192.168.4.1:8080** - Interface principale
- **patient-monitor.local** - Sur Windows uniquement

L'interface permet de:
- Configurer les seuils de température (min/max)
- Configurer la pression artérielle
- Configurer l'intervalle d'envoi MQTT
- Déclencher une alarme d'urgence

### 4. Bouton SOS

En cas d'urgence, appuyez sur le bouton GPIO 34.
- La LED clignote
- Le buzzer sonne
- Une alerte MQTT est envoyée

### 5. Détection de chute

Le MPU6050 détecte automatiquement:
- Chute libre (perte soudaine d'accélération)
- Impact (coup violent)
- Posture (si la personne est par terre)

---

## MQTT - Réception des données

### Broker HiveMQ

- Host: `e16a2ae3d3034163bb42dceb064fe375.s1.eu.hivemq.cloud`
- Port: **8883** (TLS)
- Username: `FUSION_AI`
- Password: `Aa12345678`

### Topics

| Topic | Description |
|-------|------------|
| `patient/vitals` | Données des signes vitaux |
| `patient/alerts` | Alertes et urgences |

### Exemple de message vital

```json
{
  "deviceId": "MakerBoard_01",
  "temperature": 25.5,
  "pressure": 1014.12,
  "humidity": 54.4,
  "alert": "NORMAL"
}
```

### Types d'alertes

| Alert | Signification |
|-------|-------------|
| NORMAL | Tout va bien |
| HIGH_TEMP | Température > seuil maximum |
| LOW_TEMP | Température < seuil minimum |
| HYPERTENSION | Pression > seuil sys |
| HYPOTENSION | Pression < seuil dia |
| FALL | Chute détectée |
| SOS | Bouton d'urgence |

---

## Détection de chute

Le système utilise 3 étapes:

1. **Chute libre**: Accélération < 0.4g (le corps tombe)
2. **Impact**: Accélération > 1.5g (contact avec le sol)
3. **Posture**: Axe Z > 0.7g (personne横向 au sol)

Si les 3 conditions sont détectées → Alerte de chute envoyée.

---

## Comment lancer le projet

### Avec PlatformIO (Recommandé)

#### 1. Ouvrir le projet
- Ouvrir VS Code
- File → Open Folder
- Sélectionner le dossier `SPRM-patient`

#### 2. Compiler (vérifier le code)
```bash
pio run
```
ou cliquer sur ✓ dans la barre VS Code

#### 3. Uploader vers l'ESP32
```bash
pio run --target upload --upload-port COM7
```
- Remplacer `COM7` par le port utilisé (voir dans le gestionnaire de périphériques)
- L'ESP32 doit être en mode Boot (appuyer sur BOOT + RST puis relâser RST)

#### 4. Voir les messages (monitor série)
```bash
pio device monitor --port COM7 --baud 115200
```
- Appuyer sur Ctrl+C pour quitter

### Avec VS Code

1. Installer l'extension "PlatformIO" dans VS Code
2. Cliquer sur ✓ pour compiler
3. Cliquer sur → pour uploader
4. Cliquer sur prises série pour voir les logs

### Trouver le bon port COM

#### Windows
- Gestionnaire de périphériques → Ports (COM et LPT)
- ou taper dans CMD: `mode`

#### Linux
```bash
ls /dev/tty*
```

#### Mac
```bash
ls /dev/cu.*

---

## Dépannage

### L'AP n'apparaît pas
- Vérifier l'alimentation USB
- Redémarrer l'ESP32
- Vérifier la broche GPIO 21/22 (I2C)

### WiFi ne connecte pas
- Aller à http://192.168.4.1
- Reconfigurer les identifiants WiFi
- Le WiFi doit être 2.4GHz (pas 5GHz)

### MQTT ne connecte pas
- Vérifier la connexion internet
- Vérifier les identifiants MQTT dans platformio.ini

### Interface lente
- Utiliser http://192.168.4.1:8080 directement

### patient-monitor.local ne fonctionne pas
- Fonctionne uniquement sur Windows avec Bonjour
- Sur mobile, utiliser http://192.168.4.1:8080

---

## Fichiers du projet

| Fichier | Rôle |
|---------|------|
| `src/main.cpp` | Programme principal, boucle |
| `src/NetworkManager.h` | Gestion WiFi, MQTT, AP |
| `src/WebServerManager.h` | Interface web HTML |
| `src/AlertService.h` | Envoi des alertes MQTT |
| `src/Config.h` | Topics MQTT |
| `platformio.ini` | Configuration Build |

---

## Licence

Usage hospitalier - Projet médical