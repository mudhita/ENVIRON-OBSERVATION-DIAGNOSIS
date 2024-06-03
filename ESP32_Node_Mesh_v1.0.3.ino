#include <ArduinoJson.h>
#include <painlessMesh.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <EEPROM.h>

// MESH Details (Static)
#define MESH_PREFIX   "INDOBOT_FS_ID"
#define MESH_PASSWORD "somethingSneaky"
#define MESH_PORT     5555

// Variables
String nodeName; 
String readings;

Scheduler userScheduler;
painlessMesh mesh;

float prevHum = 50.0, prevTemp = 25.0, prevPPM = 1000.0, prevLux = 500.0;
float calibHum = 1.0, calibTemp = 1.0, calibPPM = 1.0, calibLux = 1.0;

const float SMOOTHING_FACTOR = 0.1;
const size_t EEPROM_SIZE = sizeof(float) * 4;

AsyncWebServer server(80);

// Function Prototypes
void sendMessage();
String getReadings();
void saveCalibrationValues();
void loadCalibrationValues();
float generateSmoothRandom(float previousValue, float minValue, float maxValue);

void setup() {
    Serial.begin(115200);
    randomSeed(analogRead(0));
    uint64_t chipid = ESP.getEfuseMac();
    nodeName = "ESP32-" + String((uint16_t)(chipid >> 32), HEX) + String((uint32_t)chipid, HEX);

    mesh.setDebugMsgTypes(ERROR | STARTUP | CONNECTION);
    mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
    mesh.onReceive([](const uint32_t &from, const String &msg) {
        Serial.printf("Received from %u msg=%s\n", from, msg.c_str());
    });
    mesh.onNewConnection([](uint32_t nodeId) {
        Serial.printf("New Connection, nodeId = %u\n", nodeId);
    });
    mesh.onChangedConnections([]() {
        Serial.printf("Changed connections\n");
    });
    mesh.onNodeTimeAdjusted([](int32_t offset) {
        Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(), offset);
    });

    userScheduler.addTask(Task(TASK_SECOND * 1, TASK_FOREVER, &sendMessage));
    loadCalibrationValues();

    // HTTP Server Configuration
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        String html = R"rawliteral(
            <!DOCTYPE HTML><html>
            <head><title>Kalibrasi Node</title></head>
            <body>
                <h2>Kalibrasi Node )rawliteral" + nodeName + R"rawliteral(</h2>
                <form action="/calibrate" method="get">
                    <label for="hum">Humidity Calibration Factor:</label>
                    <input type="number" step="0.01" id="hum" name="hum" value=")rawliteral" + String(calibHum) + R"rawliteral("><br><br>
                    <label for="temp">Temperature Calibration Factor:</label>
                    <input type="number" step="0.01" id="temp" name="temp" value=")rawliteral" + String(calibTemp) + R"rawliteral("><br><br>
                    <label for="ppm">Gas Concentration Calibration Factor:</label>
                    <input type="number" step="0.01" id="ppm" name="ppm" value=")rawliteral" + String(calibPPM) + R"rawliteral("><br><br>
                    <label for="lux">Luminosity Calibration Factor:</label>
                    <input type="number" step="0.01" id="lux" name="lux" value=")rawliteral" + String(calibLux) + R"rawliteral("><br><br>
                    <input type="submit" value="Submit">
                </form>
            </body>
            </html>)rawliteral";
        request->send(200, "text/html", html);
    });

    server.on("/calibrate", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (request->hasParam("hum")) {
            calibHum = request->getParam("hum")->value().toFloat();
            calibTemp = request->getParam("temp")->value().toFloat();
            calibPPM = request->getParam("ppm")->value().toFloat();
            calibLux = request->getParam("lux")->value().toFloat();
            saveCalibrationValues();
            request->send(200, "text/plain", "Calibration values updated.");
        } else {
            request->send(400, "text/plain", "Invalid request");
        }
    });

    server.begin();
}

void loop() {
    mesh.update();
}

void saveCalibrationValues() {
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.put(0, calibHum);
    EEPROM.put(sizeof(float), calibTemp);
    EEPROM.put(sizeof(float) * 2, calibPPM);
    EEPROM.put(sizeof(float) * 3, calibLux);
    EEPROM.commit();
}

void loadCalibrationValues() {
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.get(0, calibHum);
    EEPROM.get(sizeof(float), calibTemp);
    EEPROM.get(sizeof(float) * 2, calibPPM);
    EEPROM.get(sizeof(float) * 3, calibLux);
}

float generateSmoothRandom(float previousValue, float minValue, float maxValue) {
    float newValue = previousValue + (random(minValue * 10, maxValue * 10) / 10.0 - previousValue) * SMOOTHING_FACTOR;
    return constrain(newValue, minValue, maxValue);
}

String getReadings() {
    DynamicJsonDocument jsonReadings(256);
    jsonReadings["id"] = nodeName;
    prevHum = generateSmoothRandom(prevHum, 20, 100) * calibHum;
    prevTemp = generateSmoothRandom(prevTemp, 15, 35) * calibTemp;
    prevPPM = generateSmoothRandom(prevPPM, 400, 2000) * calibPPM;
    prevLux = generateSmoothRandom(prevLux, 100, 1000) * calibLux;

    jsonReadings["hum"] = prevHum;
    jsonReadings["temp"] = prevTemp;
    jsonReadings["ppm"] = prevPPM;
    jsonReadings["lux"] = prevLux;

    String readings;
    serializeJson(jsonReadings, readings);
    return readings;
}

void sendMessage() {
    String msg = getReadings();
    mesh.sendBroadcast(msg);
}
