#include <Arduino.h>
#include <painlessMesh.h>
#include <PubSubClient.h>
#include <WiFiClient.h>

#define MESH_PREFIX   "INDOBOT FSID"
#define MESH_PASSWORD "somethingSneaky"
#define MESH_PORT 5555

#define STATION_SSID "vivo V29"
#define STATION_PASSWORD "Nabila041185"

#define HOSTNAME "MQTT_Bridge2"

#define SERIAL_BAUD_RATE 115200
#define SERIAL_RX_PIN 16 // Pin untuk menerima data serial dari ESP32 #6
#define SERIAL_TX_PIN 17 // Pin untuk mengirim data serial ke ESP32 #6

// Prototypes
void receivedCallback(const uint32_t &from, const String &msg);
void mqttCallback(char* topic, byte* payload, unsigned int length);
IPAddress getLocalIP();
void sendSerialData(const String &msg);

IPAddress myIP(0, 0, 0, 0);
const char* mqttBroker = "broker.emqx.io";
const int mqttPort = 1883;
const char* mqttUser = "emqx";
const char* mqttPassword = "public";

painlessMesh mesh;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
HardwareSerial SerialESP32_6(2); // Using UART2 which is usually available as Serial2

void setup() {
  Serial.begin(115200);
  SerialESP32_6.begin(SERIAL_BAUD_RATE, SERIAL_8N1, SERIAL_RX_PIN, SERIAL_TX_PIN);

  mesh.setDebugMsgTypes(ERROR | STARTUP | CONNECTION); // Set before init() to see startup messages

  // Initialize the mesh network
  mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT, WIFI_AP_STA);
  mesh.onReceive(&receivedCallback);

  // Connect to the specified Wi-Fi network
  mesh.stationManual(STATION_SSID, STATION_PASSWORD);
  mesh.setHostname(HOSTNAME);
  mesh.setRoot(true);       // Set this node as root
  mesh.setContainsRoot(true);

  mqttClient.setServer(mqttBroker, mqttPort);
  mqttClient.setCallback(mqttCallback);
}

void loop() {
  mesh.update();
  mqttClient.loop();

  // Check and update the IP address
  if (myIP != getLocalIP()) {
    myIP = getLocalIP();
    Serial.println("My IP is " + myIP.toString());

    if (mqttClient.connect("painlessMeshClient", mqttUser, mqttPassword)) {
      mqttClient.publish("painlessMesh/from/gateway", "Ready!");
      mqttClient.subscribe("painlessMesh/to/#");
    } 
  }

  // Check for incoming serial data and send it to the mesh network
  if (SerialESP32_6.available()) {
    String serialMsg = SerialESP32_6.readStringUntil('\n');
    Serial.println("Received from SerialESP32_6: " + serialMsg);
    mesh.sendBroadcast(serialMsg);
    mqttClient.publish("painlessMesh/from/gateway", serialMsg.c_str());
  }
}

// Callback for messages received from the mesh network
void receivedCallback(const uint32_t &from, const String &msg) {
  Serial.printf("bridge: Received from %u msg=%s\n", from, msg.c_str());
  String topic = "all_node3";
  mqttClient.publish(topic.c_str(), msg.c_str());
  sendSerialData(msg); // Send received mesh message to serial
}

// Callback for messages received from the MQTT broker
void mqttCallback(char* topic, uint8_t* payload, unsigned int length) {
  char* cleanPayload = (char*)malloc(length + 1);
  memcpy(cleanPayload, payload, length);
  cleanPayload[length] = '\0';
  String msg = String(cleanPayload);
  free(cleanPayload);

  String targetStr = String(topic).substring(16); // Extract target ID from topic

  if (targetStr == "gateway") {
    if (msg == "getNodes") {
      auto nodes = mesh.getNodeList(true);
      String str;
      for (auto &&id : nodes)
        str += String(id) + String(" ");
      mqttClient.publish("painlessMesh/from/gateway", str.c_str());
    }
  } else if (targetStr == "broadcast") {
    mesh.sendBroadcast(msg);
  } else {
    uint32_t target = strtoul(targetStr.c_str(), NULL, 10);
    if (mesh.isConnected(target)) {
      mesh.sendSingle(target, msg);
    } else {
      mqttClient.publish("painlessMesh/from/gateway", "Client not connected!");
    }
  }
}

// Function to get the local IP address of the node
IPAddress getLocalIP() {
  return IPAddress(mesh.getStationIP());
}

// Function to send data through SerialESP32_6
void sendSerialData(const String &msg) {
  SerialESP32_6.println(msg);
}
