#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include <ArduinoJson.h>
#include <otadrive_esp.h>

// WiFi and InfluxDB credentials
char ssid[32];
char password[32];

const char* INFLUXDB_URL =      "https://us-east-1-1.aws.cloud2.influxdata.com";
const char* INFLUXDB_TOKEN =    "bY5Y8WsUTkW4aPPhvbrx6XWXi-s_-Da8JA3sN14ti2_QdzyCKBkoht93Ms_aoiYlZNuwGa2OmGwSuAPiwVJoYQ==";
const char* INFLUXDB_ORG =      "023982a3a96b797a";
const char* INFLUXDB_BUCKET =   "INDOBOT_FSID";
const char* TZ_INFO =           "UTC7";

// MQTT broker dan OTA Drive details
const char* mqtt_server =       "broker.hivemq.com";
const int mqtt_port = 1883;
const char* mqtt_topic =        "indobot/data";
const char* OTA_TOKEN =         "3892d52b-5199-45a3-bde5-37fdc31b9986";

// UART communication settings
const int SERIAL_BAUD_RATE = 115200;
const int SERIAL_RX_PIN = 16;
const int SERIAL_TX_PIN = 17;

// WiFiMulti instance
WiFiMulti wifiMulti;

// HardwareSerial instance for UART2
HardwareSerial SerialESP32_7(2);

// WiFi and MQTT clients
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// InfluxDB client instance
InfluxDBClient influxClient(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

// Data point for InfluxDB
Point sensor("sensor_data");

String FW_VERSION = "1.0.4";

unsigned long previousSyncMillis = 0;
const long syncInterval = 60000; // Sync every 60 seconds

void onUpdateProgress(int progress, int totalt) {
    static int last = 0;
    int progressPercent = (100 * progress) / totalt;
    Serial.print("*");
    if (last != progressPercent && progressPercent % 10 == 0) {
        // print every 10%
        Serial.printf("%d", progressPercent);
    }
    last = progressPercent;
}

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    Serial.println("Starting setup...");

    WiFiManager wm;
    bool res = wm.autoConnect("ENVIRO OBSERVER DIAGNOSIS", "password");

    if (!res) {
        Serial.println("Failed to connect to WiFi");
        // ESP.restart();
    } else {
        Serial.println("Connected to WiFi");
        // strcpy(ssid, wm.getWiFiSSID().c_str());           // Mengisi nilai ssid dari WiFiManager
        // strcpy(password, wm.getAPPassword().c_str());     // Mengisi nilai password dari WiFiManager
        Serial.println("WiFi credentials obtained");

        SerialESP32_7.begin(SERIAL_BAUD_RATE, SERIAL_8N1, SERIAL_RX_PIN, SERIAL_TX_PIN);

        OTADRIVE.setInfo(OTA_TOKEN, "v@" + FW_VERSION);
        OTADRIVE.onUpdateFirmwareProgress(onUpdateProgress);

        timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

        if (influxClient.validateConnection()) {
            Serial.print("Connected to InfluxDB: ");
            Serial.println(influxClient.getServerUrl());
        } else {
            Serial.print("InfluxDB connection failed: ");
            Serial.println(influxClient.getLastErrorMessage());
        }

        mqttClient.setServer(mqtt_server, mqtt_port);
        mqttClient.setCallback(callback);
    }

    Serial.println("Setup complete.");
}

void callback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (unsigned int i = 0; i < length; i++) {
        Serial.print((char)payload[i]);
    }
    Serial.println();
}

void reconnect_mqtt() {
    while (!mqttClient.connected()) {
        Serial.print("Attempting MQTT connection...");
        if (mqttClient.connect("ESP32Client")) {
            Serial.println("connected");
            mqttClient.publish(mqtt_topic, "ESP32 connected");
            mqttClient.subscribe(mqtt_topic);
        } else {
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" try again in 5 seconds");
            delay(5000); // Wait 5 seconds before retrying
        }
    }
}

void sync_task() {
    unsigned long currentMillis = millis();
    if (currentMillis - previousSyncMillis >= syncInterval) {
        previousSyncMillis = currentMillis;

        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi not connected! Attempting to reconnect...");
            WiFi.reconnect();
            return;
        }

        Serial.println("Firmware version: " + FW_VERSION);  
        OTADRIVE.updateFirmware();                          
    }
}

void loop() {
    if (!mqttClient.connected()) {
        reconnect_mqtt();
    }
    mqttClient.loop();

    if (SerialESP32_7.available()) {
        String serialMsg = SerialESP32_7.readStringUntil('\n');
        Serial.println("Received from SerialESP32_6: " + serialMsg);

        // Validate JSON string before parsing
        if (serialMsg[0] != '{' || serialMsg[serialMsg.length() - 1] != '}') {
            Serial.println("Invalid JSON received, ignoring...");
            return;
        }

        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, serialMsg);

        if (error) {
            Serial.print("deserializeJson() failed: ");
            Serial.println(error.c_str());
            return;
        }

        const char* id = doc["id"];  // Change to const char* to read string
        if (!id) {
            Serial.println("Invalid JSON: Missing 'id'");
            return;
        }

        float hum = doc["hum"];
        float temp = doc["temp"];
        float ppm = doc["ppm"];
        float lux = doc["lux"];

        mqttClient.publish(mqtt_topic, serialMsg.c_str());

        sensor.clearFields();
        sensor.clearTags();  // Clear previous tags to avoid accumulation
        sensor.addTag("id", id);  // Add id as tag
        sensor.addField("hum", hum);
        sensor.addField("temp", temp);
        sensor.addField("ppm", ppm);
        sensor.addField("lux", lux);

        if (!influxClient.writePoint(sensor)) {
            Serial.print("InfluxDB.write failed: ");
            Serial.println(influxClient.getLastErrorMessage());
        }
    }
    sync_task();
}