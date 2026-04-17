# Smart Hospital Monitor - Tableau de Bord Hospitalier Intelligent

Ce projet est un système de surveillance environnementale pour hôpital basé sur un ESP32. Il lit les données des capteurs (température, humidité, pression), les affiche sur un écran 7 segments, et les publie via MQTT.

---

## Table des Matières

1. [Description du Projet](#description-du-projet)
2. [Matériel Requis](#matériel-requis)
3. [Câblage](#câblage)
4. [Installation](#installation)
5. [Comment Lancer le Projet](#comment-lancer-le-projet)
6. [Comment Utiliser le Système](#comment-utiliser-le-système)
7. [Configuration MQTT](#configuration-mqtt)
8. [Fichiers de Documentation](#fichiers-de-documentation)
9. [Dépannage](#dépannage)

---

## Description du Projet

Ce système de surveillance hospitalière inclut :

- **Capteur BME280** : Température, humidité, pression atmosphérique
- **Écran TM1637** : Affichage 7 segments 4 chiffres
- ** Bouton SOS d'urgence** : GPIO 35
- **Bouton d'arrêt** : GPIO 34
- **LEDs indicateurs** : Verte (normal) / Rouge (alerte)
- **Buzzer** : Alerte sonore
- **WiFi** : Connexion automatique avec portail de configuration
- **MQTT** : Publication vers un broker HiveMQ Cloud

---

## Matériel Requis

- ESP32 (Upesy Wroom)
- Module d'affichage TM1637 (4 chiffres, 7 segments)
- Capteur BME280 (I2C)
- LED verte + LED rouge
- Buzzer piezo
- 2 boutons poussoirs
- Résistance 10kΩ (pull-up interne utilisé)

---

## Câblage

### Tableau des Connexions

| Composant | GPIO | Notes |
|-----------|------|-------|
| TM1637 CLK | 18 | Horloge display |
| TM1637 DIO | 5 | Données display |
| LED Verte | 32 | État normal |
| LED Rouge | 33 | État alerte |
| Buzzer | 26 | Alarme sonore |
| Bouton SOS | 35 | Urgence (pull-up) |
| Bouton Arrêt | 34 | Couper alarme |
| BME280 SDA | 21 | I2C Data |
| BME280 SCL | 22 | I2C Clock |

---

## Installation

### 1. Prérequis

- [PlatformIO Core或CLI](https://platformio.org/)
- Python 3.x

### 2. Installer les Dépendances

```bash
pio lib install
```

Les bibliothèques installées automatiquement via platformio.ini :
- PubSubClient 2.8
- NextTM1637
- Adafruit BME280 Library 2.2.2
- Adafruit Unified Sensor 1.1.14
- ArduinoJson 6.21.3
- WiFiManager 2.0.17

---

## Comment Lancer le Projet

### Compilez et téléversez

Via PlatformIO CLI :

```bash
pio run --target upload
```

Ou via l'extension VS Code PlatformIO.

### Monitor Série

Pour voir les logs :

```bash
pio device monitor
```

Vitesse : 115200 baud

---

## Comment Utiliser le Système

###1. Première Connexion WiFi

Au premier démarrage, si aucune credential n'est sauvegardée :

1. Le système crée un point d'accès WiFi `HospitalBoard-Setup`
2. Mot de passe : `setup1234`
3. Connectez-vous à ce WiFi
4. Ouvrez `192.168.4.1` dans un navigateur
5. Sélectionnez votre réseau WiFi et entrez le mot de passe
6. Cliquez "Save"

###2. Indicateurs d'État

| Message Display | Signification |
|---------------|--------------|
| `0000` | Initialisation |
| `1111` | BME280 non trouvé |
| `8888` | Système prêt |

###3. Affichage Cycle

L'écran alterne toutes les 2 secondes :
- Température (°C)
- Pression (hPa)
- Humidité (%)

###4. Bouton SOS (GPIO 35)

- Appuyé → Alerte d'urgence immediata
- LED rouge ALLUMÉE
- Buzzer sonne 500ms
- Publication MQTT immédiate

###5. Bouton d'Arrêt (GPIO 34)

- Appuyé → Coupe toutes les alarmes
- LED verte ALLUMÉE
- Retour au fonctionnement normal

###6. Alerte de Température

Seuil : 28°C (configurable)

- Au-dessus de 28°C → Alerte activée
- LED rouge ALLUMÉE
- Buzzer pulse toutes les 2 secondes

---

## Configuration MQTT

### Paramètres du Broker

| Paramètre | Valeur |
|----------|-------|
| Host | e16a2ae3d3034163bb42ddeb064fe375.s1.eu.hivemq.cloud |
| Port | 8883 (TLS) |
| Username | FUSION_AI |
| Password | Aa12345678 |
| Topic | hospital/sensor/data |

### Format JSON Publié

```json
{
  "device_id": "SmartHospitalMonitor_01",
  "temperature": 25.5,
  "humidity": 65.3,
  "pressure": 1013.2,
  "alert_status": "NORMAL",
  "emergency_status": "OK",
  "timestamp": 123456789
}
```

---

## Fichiers de Documentation

Ce projet contient les fichiers Markdown suivants :

### APPLICATION_GUIDE.md

Guide complet de référence pour toutes les fonctions, options de configuration, et instructions étape par étape. Contient :

- Architecture système
- Référence des fonctions principales
- Structures de données
- Personnalisation
- Workflow détaillé
- Utilisation avancée

### MQTT_INTEGRATION_GUIDE.md

Guide d'intégration MQTT pour consumir les données. Contient :

- Détails de connexion au broker
- Format des données et topics
- Comportement de publication
- Exemples d'intégration (Python, Node.js)
- Outils de surveillance
- Exemples de workflows
- Recommandations de sécurité

### clean_hospital_dashboard.html

Interface web de visualisation des données (optionnelle). Permet :

- Affichage des stats en temps réel
- Graphiques température/humidité/pression
- Tables de données
- Export CSV
- Gestion des alertes

---

## Personnalisation

### Seuils de Température

Dans `src/main.cpp` :

```cpp
const float TEMP_ALERT_THRESHOLD = 28.0;  // Modifier cette valeur
```

### Temporisations

```cpp
const unsigned long SENSOR_READ_INTERVAL = 3000;      // Lecture senseur (ms)
const unsigned long DISPLAY_CHANGE_INTERVAL = 2000;     // Changement affichage (ms)
const unsigned long MQTT_PUBLISH_INTERVAL = 60000;  // Publication MQTT (ms)
```

### Luminosité Display

Dans `setup()` :

```cpp
display.setBrightness(4);  // 0-7 (7 = plus lumineux)
```

---

## Dépannage

### Problèmes Courants

| Problème | Solution |
|---------|----------|
| Display montre 1111 | Vérifier câblage I2C (SDA/SCL) |
| WiFi portal n'apparaît pas | Appuyer sur bouton boot pendant 5s |
| MQTT ne connecte pas | Vérifier credentials et port 8883 |
| Boutons non réactifs | Vérifier pull-up interne |

### Logs Série

Les messages de debug apparaissent sur le monitor série à 115200 baud.

---

## Structure des Fichiers

```
hospital-board/
├── src/
│   ├── main.cpp              # Programme principal
│   └── clean_hospital_dashboard.html  # Interface web (optionnelle)
├── platformio.ini            # Configuration PlatformIO
├── APPLICATION_GUIDE.md    # Guide d'utilisation complet
├── MQTT_INTEGRATION_GUIDE.md # Guide d'intégration MQTT
├── .vscode/                # Configurations VS Code
└── .pio/                 # Build PlatformIO
```

---

## Librairies Utilisées

- **WiFiManager** (tzapu) - Gestion WiFi avec portail
- **PubSubClient** (knolleary) - Client MQTT
- **Adafruit BME280** - Capteur environnemental
- **NextTM1637** - Affichage 7 segments
- **ArduinoJson** - Sérialisation JSON

---

**Dernière mise à jour** : 17 avril 2026  
**Version du firmware** : Smart Hospital Monitor v1.0  
**Plateforme** : ESP32 Upesy WROOM