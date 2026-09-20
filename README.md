# ER-IoT-Infrastructure

![C](https://img.shields.io/badge/C-00599C?style=for-the-badge&logo=c&logoColor=white)
![Contiki-NG](https://img.shields.io/badge/Contiki--NG-3A6EA5?style=for-the-badge&logoColor=white)
![Java](https://img.shields.io/badge/Java-ED8B00?style=for-the-badge&logo=java&logoColor=white)
![MQTT](https://img.shields.io/badge/MQTT-660066?style=for-the-badge&logo=mqtt&logoColor=white)
![CoAP](https://img.shields.io/badge/CoAP-4B8BBE?style=for-the-badge&logoColor=white)
![InfluxDB](https://img.shields.io/badge/InfluxDB-22ADF6?style=for-the-badge&logo=influxdb&logoColor=white)
![MySQL](https://img.shields.io/badge/MySQL-4479A1?style=for-the-badge&logo=mysql&logoColor=white)
![Grafana](https://img.shields.io/badge/Grafana-F46800?style=for-the-badge&logo=grafana&logoColor=white)
[![AI](https://img.shields.io/badge/AI-Artificial%20Intelligence-brightgreen)](https://img.shields.io/badge/AI-Artificial%20Intelligence-brightgreen)
![Academic Project](https://img.shields.io/badge/Academic-Project-blue?style=for-the-badge)

---

## 🇮🇹 Italiano

### 📖 Descrizione

**ER-IoT-Infrastructure** è un progetto universitario realizzato per il corso di *Internet of Things* (Anno Accademico 2025/2026) del corso di Laurea Magistrale in Computer Engineering presso l'**Università di Pisa (@unipi)**.

Il progetto simula l'infrastruttura di monitoraggio di un **pronto soccorso**. Ad ogni paziente, dopo il triage, viene assegnato un dispositivo indossabile che ne campiona periodicamente i parametri vitali — frequenza cardiaca, saturazione, temperatura, pressione sistolica e diastolica, frequenza respiratoria — e li trasmette all'applicazione cloud. Il personale sanitario dispone a sua volta di dispositivi dedicati, così da poter essere allertato in caso di necessità.

I reparti sono modellati sui sei *percorsi* del protocollo di triage della Regione Toscana, ciascuno servito da un proprio border router; i border router possono segnalarsi reciprocamente una richiesta di comunicazione.

Il sistema integra inoltre:

* un **modello di machine learning a bordo del dispositivo** che classifica autonomamente il codice di triage del paziente a partire da una finestra scorrevole di letture, segnalando al cloud ogni variazione;
* due **meccanismi adattivi** che reagiscono alle condizioni di stress della rete — soppressione selettiva della telemetria di routine dei codici a bassa priorità in caso di congestione, e ritenzione locale delle letture con successiva ritrasmissione in caso di guasto del gateway;
* una **dashboard Grafana** per la visualizzazione dei parametri vitali e delle metriche raccolte durante gli stress test.

L'interazione del personale medico avviene tramite un'applicazione a riga di comando che si interfaccia con il cloud attraverso un'API RESTful.

### 👥 Autori

* [Francesco Della Maggiora](https://github.com/FrancescoDellaMaggiora/FrancescoDellaMaggiora)
* [Alessandro Xavier Battisti](https://github.com/alebattisti)

### 🛠️ Tecnologie Utilizzate

* **Firmware:** C, Contiki-NG, nRF52840 (Nordic Semiconductor)
* **Protocolli:** MQTT (Eclipse Paho / Mosquitto), CoAP (Eclipse Californium), 6LoWPAN, RPL
* **Formato dati:** SenML, JSON
* **Cloud Application:** Java 21, Javalin, Jackson
* **Databases:** MySQL (dati anagrafici e relazionali), InfluxDB (serie temporali dei parametri vitali)
* **Visualizzazione:** Grafana
* **Machine Learning:** Python, Keras/TensorFlow per l'addestramento, [emlearn](https://github.com/emlearn/emlearn) per il deployment su microcontrollore, dataset [VitalDB](https://vitaldb.net/)
* **Simulazione:** Cooja
* **Documentazione:** disponibile nella cartella `/doc`

### ⚙️ Installazione

#### 1. Contiki-NG

L'intero firmware è costruito su Contiki-NG, che va installato per primo seguendo la [documentazione ufficiale](https://docs.contiki-ng.org/en/develop/doc/getting-started/index.html). Il progetto va collocato all'interno dell'albero dei sorgenti:

```bash
git clone https://github.com/contiki-ng/contiki-ng.git --recursive
cd contiki-ng/examples
git clone <questo-repository> FinalProject
```

Per il deployment su hardware reale è necessaria la toolchain ARM (`gcc-arm-none-eabi`) e `nrfutil` per il caricamento via DFU.

#### 2. Servizi esterni

```bash
# Broker MQTT
sudo apt install mosquitto mosquitto-clients

# Database relazionale
sudo apt install mysql-server

# Serie temporali e visualizzazione: seguire le rispettive
# documentazioni ufficiali per InfluxDB 2.x e Grafana
```

Mosquitto deve essere configurato per accettare connessioni su tutte le interfacce IPv6, altrimenti i nodi non riusciranno a raggiungerlo:

```
# /etc/mosquitto/conf.d/er-project.conf
listener 1883 ::
allow_anonymous true
```

#### 3. Database

```bash
mysql -u root -p < doc/ER_IOT.sql
influx bucket create --name vitals --org <organizzazione>
```

#### 4. Cloud Application

I file di configurazione si trovano nella cartella `config/` e vanno adattati al proprio ambiente: credenziali del database, token InfluxDB, indirizzo del broker e indirizzi IPv6 dei dispositivi.

```bash
cd CloudApplication
mvn clean compile
mvn exec:java
```

#### 5. Firmware

Il firmware è parametrizzato a tempo di compilazione. Per la simulazione:

```bash
cd src/patient
make TARGET=cooja patient.cooja DEVICE_ID=1 ADAPTIVE=1 BUFFERING=1
```

Per il dispositivo reale, con il dongle in modalità DFU:

```bash
make TARGET=nrf52840 BOARD=dongle patient.dfu-upload PORT=/dev/ttyACM0 DEVICE_ID=1
```

I flag `ADAPTIVE` e `BUFFERING` abilitano i due meccanismi adattivi in modo indipendente: disattivandoli si ottengono le build di riferimento usate per il confronto sperimentale.

#### 6. Avvio della rete

Il border router va collegato all'host tramite tunnel SLIP, dopo aver avviato l'applicazione cloud:

```bash
# hardware reale
sudo ~/contiki-ng/tools/serial-io/tunslip6 -s /dev/ttyACM0 fd00:1::1/64

# simulazione (Cooja, con il Serial Socket attivo sulla porta 60001)
sudo ~/contiki-ng/tools/serial-io/tunslip6 -a 127.0.0.1 -p 60001 fd00:1::1/64
```

Con più reparti, ciascun border router richiede una propria porta e un proprio prefisso, e l'host deve instradare fra le interfacce:

```bash
sudo sysctl -w net.ipv6.conf.all.forwarding=1
```

#### 7. Applicazione medico

```bash
cd DoctorApp
mvn clean compile
mvn exec:java
```

---

## 🇬🇧 English

### 📖 Description

**ER-IoT-Infrastructure** is an academic project developed for the *Internet of Things* course (Academic Year 2025/2026) of the Master's Degree in Computer Engineering at the **University of Pisa (@unipi)**.

The project simulates the monitoring infrastructure of an **emergency room**. After triage, each patient is given a wearable device that periodically samples their vital signs — heart rate, oxygen saturation, temperature, systolic and diastolic pressure, respiratory rate — and transmits them to the cloud application. Medical staff carry dedicated devices of their own, so that they can be alerted whenever assistance is needed.

Departments are modelled on the six *pathways* of the Tuscany Region triage protocol, each served by its own border router; border routers can signal a request for communication to one another.

The system also includes:

* an **on-device machine learning model** that autonomously classifies the patient's triage code from a sliding window of readings, reporting every change to the cloud;
* two **adaptive mechanisms** reacting to network stress conditions — selective suppression of routine telemetry for low-priority codes under congestion, and local retention of readings with later replay upon gateway failure;
* a **Grafana dashboard** displaying both the vital signs and the metrics collected during the stress tests.

Medical staff interact with the system through a command-line application that talks to the cloud over a RESTful API.

### 👥 Authors

* [Francesco Della Maggiora](https://github.com/FrancescoDellaMaggiora/FrancescoDellaMaggiora)
* [Alessandro Xavier Battisti](https://github.com/alebattisti)

### 🛠️ Technologies Used

* **Firmware:** C, Contiki-NG, nRF52840 (Nordic Semiconductor)
* **Protocols:** MQTT (Eclipse Paho / Mosquitto), CoAP (Eclipse Californium), 6LoWPAN, RPL
* **Data format:** SenML, JSON
* **Cloud Application:** Java 21, Javalin, Jackson
* **Databases:** MySQL (relational and demographic data), InfluxDB (vital-sign time series)
* **Visualization:** Grafana
* **Machine Learning:** Python, Keras/TensorFlow for training, [emlearn](https://github.com/emlearn/emlearn) for microcontroller deployment, [VitalDB](https://vitaldb.net/) dataset
* **Simulation:** Cooja
* **Documentation:** available in the `/doc` folder

### ⚙️ Installation

#### 1. Contiki-NG

The whole firmware is built on Contiki-NG, which must be installed first by following the [official documentation](https://docs.contiki-ng.org/en/develop/doc/getting-started/index.html). The project belongs inside the source tree:

```bash
git clone https://github.com/contiki-ng/contiki-ng.git --recursive
cd contiki-ng/examples
git clone <this-repository> FinalProject
```

Deploying on real hardware additionally requires the ARM toolchain (`gcc-arm-none-eabi`) and `nrfutil` for DFU upload.

#### 2. External services

```bash
# MQTT broker
sudo apt install mosquitto mosquitto-clients

# Relational database
sudo apt install mysql-server

# Time series and visualization: follow the respective official
# documentation for InfluxDB 2.x and Grafana
```

Mosquitto must be configured to accept connections on every IPv6 interface, otherwise the nodes will not be able to reach it:

```
# /etc/mosquitto/conf.d/er-project.conf
listener 1883 ::
allow_anonymous true
```

#### 3. Databases

```bash
mysql -u root -p < doc/ER_IOT.sql
influx bucket create --name vitals --org <organization>
```

#### 4. Cloud Application

Configuration files live in the `config/` folder and must be adapted to the local environment: database credentials, InfluxDB token, broker address and the devices' IPv6 addresses.

```bash
cd CloudApplication
mvn clean compile
mvn exec:java
```

#### 5. Firmware

The firmware is parameterized at build time. For simulation:

```bash
cd src/patient
make TARGET=cooja patient.cooja DEVICE_ID=1 ADAPTIVE=1 BUFFERING=1
```

For real hardware, with the dongle in DFU mode:

```bash
make TARGET=nrf52840 BOARD=dongle patient.dfu-upload PORT=/dev/ttyACM0 DEVICE_ID=1
```

The `ADAPTIVE` and `BUFFERING` flags enable the two adaptive mechanisms independently: turning them off produces the baseline builds used for the experimental comparison.

#### 6. Bringing up the network

The border router is attached to the host through a SLIP tunnel, after the cloud application has been started:

```bash
# real hardware
sudo ~/contiki-ng/tools/serial-io/tunslip6 -s /dev/ttyACM0 fd00:1::1/64

# simulation (Cooja, with the Serial Socket listening on port 60001)
sudo ~/contiki-ng/tools/serial-io/tunslip6 -a 127.0.0.1 -p 60001 fd00:1::1/64
```

With multiple departments, each border router needs its own port and prefix, and the host must route between the interfaces:

```bash
sudo sysctl -w net.ipv6.conf.all.forwarding=1
```

#### 7. Doctor application

```bash
cd DoctorApp
mvn clean compile
mvn exec:java
```