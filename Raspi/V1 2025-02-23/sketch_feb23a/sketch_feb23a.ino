#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

// WiFi Credentials untuk Access Point
const char* ssid = "haisayakaka";
const char* password = "12345678";

// HTTP Server pada port 80
AsyncWebServer server(80);

// ADS1115 untuk sensor TDS
Adafruit_ADS1X15 ads;

// Pin Configuration
const int PH_PIN = 36;         // GPIO36 untuk pH
const int SENSOR = 34;         // GPIO34 untuk water flow sensor

// Sampling Configuration
const int SAMPLES = 10;
const float VOLTAGE_REF = 3.3 - 0.1;

// pH Calibration
const float PH7_VOLTAGE = 2.5;
const float PH4_VOLTAGE = 3.0;
const float VOLTAGE_PER_PH = (PH4_VOLTAGE - PH7_VOLTAGE) / 3;

// TDS Configuration
const int SCOUNT = 30;
int16_t analogBuffer[SCOUNT];
int16_t analogBufferTemp[SCOUNT];
int analogBufferIndex = 0;
float temperature = 25;

// Water Flow Configuration
volatile byte pulseCount;
float calibrationFactor = 450;  // Nilai sementara, akan kita kalibrasi
float flowRate = 0.0;
float totalLitres = 0.0;
unsigned long lastFlowTime = 0;
unsigned long checkInterval = 100;  // Cek setiap 100 ms
float targetVolumeMl = 0.0;
bool isFillingActive = false;

// Variables for readings
float avgVoltagePH = 0.0;
float phValue = 0.0;
String phStatus = "";
float tdsValue = 0.0;
float tdsVoltage = 0.0;

// HTML dan JavaScript untuk klien (tetap sama)
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>ESP32 pH, TDS & Flow Monitor</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial; text-align: center; margin: 0px auto; padding: 15px; }
    .reading { font-size: 2.8rem; }
    .card { background-color: white; box-shadow: 0px 0px 10px 1px rgba(0,0,0,0.1); border-radius: 10px; padding: 15px; margin: 20px; }
    .status { font-size: 1.2rem; margin: 10px; }
    .button { background-color: #4CAF50; border: none; color: white; padding: 15px 32px; text-align: center; text-decoration: none; display: inline-block; font-size: 16px; margin: 4px 2px; cursor: pointer; border-radius: 4px; }
    .button-off { background-color: #f44336; }
  </style>
</head>
<body>
  <div class="card">
    <h2>pH Sensor Monitor</h2>
    <div class="reading">pH: <span id="ph">0.00</span></div>
    <div class="status">Status: <span id="status">Waiting...</span></div>
    <div class="status">Voltage: <span id="voltage">0.000</span> V</div>
  </div>
  
  <div class="card">
    <h2>TDS Sensor Monitor</h2>
    <div class="reading">TDS: <span id="tds">0</span> ppm</div>
    <div class="status">Voltage: <span id="tdsVoltage">0.000</span> V</div>
  </div>
  
  <div class="card">
    <h2>Water Flow Monitor</h2>
    <div class="reading">Flow Rate: <span id="flowrate">0.00</span> L/min</div>
    <div class="reading">Total: <span id="totallitres">0.00</span> L</div>
    <button class="button" onclick="resetFlow()">Reset Water Flow</button>
    <button class="button button-off" onclick="emergencyStop()">EMERGENCY STOP</button>
  </div>
  
  </div>

  
  
  <script>
    function updateData() {
      fetch('/data')
        .then(response => response.json())
        .then(data => {
          document.getElementById("ph").innerHTML = data.ph;
          document.getElementById("status").innerHTML = data.phStatus;
          document.getElementById("voltage").innerHTML = data.phVoltage;
          document.getElementById("tds").innerHTML = data.tds;
          document.getElementById("tdsVoltage").innerHTML = data.tdsVoltage;
          document.getElementById("flowrate").innerHTML = data.flowRate;
          document.getElementById("totallitres").innerHTML = data.totalLitres;
        });
    }

    function resetFlow() {
      fetch('/resetFlow', { method: 'POST' })
        .then(response => response.json())
        .then(data => {
          if(data.status === "ok") {
            updateData();
          }
        });
    }
    function emergencyStop() {
      fetch('/emergencyStop', { method: 'POST' })
        .then(response => response.json())
        .then(data => {
          if(data.status === "ok") {
            updateData();
          }
        });
    }

    setInterval(updateData, 2000);
    updateData();
  </script>
</body>
</html>
)rawliteral";

// Water Flow Interrupt Handler
void IRAM_ATTR pulseCounter() {
    pulseCount++;
}

// TDS Functions (tetap sama)
int getMedianNum(int16_t bArray[], int iFilterLen) {
    int16_t bTab[iFilterLen];
    for (byte i = 0; i < iFilterLen; i++) bTab[i] = bArray[i];
    for (int j = 0; j < iFilterLen - 1; j++) {
        for (int i = 0; i < iFilterLen - j - 1; i++) {
            if (bTab[i] > bTab[i + 1]) {
                int16_t bTemp = bTab[i];
                bTab[i] = bTab[i + 1];
                bTab[i + 1] = bTemp;
            }
        }
    }
    return (iFilterLen & 1) ? bTab[(iFilterLen - 1) / 2] : (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
}

void readTDS() {
    static unsigned long analogSampleTimepoint = millis();
    if (millis() - analogSampleTimepoint > 40U) {
        analogSampleTimepoint = millis();
        int16_t reading = ads.readADC_SingleEnded(0);
        if (reading < 0) {
            Serial.println("Error: TDS reading failed from ADS1115");
            tdsValue = -1;
            return;
        }
        analogBuffer[analogBufferIndex] = reading;
        analogBufferIndex = (analogBufferIndex + 1) % SCOUNT;
    }

    static unsigned long printTimepoint = millis();
    if (millis() - printTimepoint > 800U) {
        printTimepoint = millis();
        memcpy(analogBufferTemp, analogBuffer, sizeof(analogBuffer));
        int medianValue = getMedianNum(analogBufferTemp, SCOUNT);
        tdsVoltage = ads.computeVolts(medianValue);
        if (tdsVoltage < 0) {
            Serial.println("Error: Invalid TDS voltage");
            tdsValue = -1;
        } else {
            float compensationCoefficient = 1.0 + 0.02 * (temperature - 25.0);
            float compensationVoltage = tdsVoltage / compensationCoefficient;
            tdsValue = (133.42 * pow(compensationVoltage, 3) - 255.86 * pow(compensationVoltage, 2) + 857.39 * compensationVoltage) * 0.5;
        }
    }
}

// pH Functions (tetap sama)
float readPHVoltage() {
    float voltage = 0.0;
    int samples[SAMPLES];
    for (int i = 0; i < SAMPLES; i++) {
        samples[i] = analogRead(PH_PIN);
        if (samples[i] < 0) {
            Serial.println("Error: Failed to read pH sensor");
            return -1;
        }
        delay(10);
    }
    
    for (int i = 0; i < SAMPLES - 1; i++) {
        for (int j = i + 1; j < SAMPLES; j++) {
            if (samples[i] > samples[j]) {
                int temp = samples[i];
                samples[i] = samples[j];
                samples[j] = temp;
            }
        }
    }
    int validSamples = SAMPLES - 4;
    float avgValue = 0;
    for (int i = 2; i < SAMPLES - 2; i++) {
        avgValue += samples[i];
    }
    voltage = (avgValue / validSamples) * (VOLTAGE_REF / 4095.0);
    return voltage;
}

float voltageToPH(float voltage) {
    if (voltage < 0) {
        Serial.println("Error: Invalid pH voltage");
        return -1;
    }
    return 7.0 + ((PH7_VOLTAGE - voltage) / VOLTAGE_PER_PH);
}

// Fungsi Water Flow yang sudah diperbaiki
void handleWaterFlow() {
    detachInterrupt(digitalPinToInterrupt(SENSOR));
    
    unsigned long elapsedTime = millis() - lastFlowTime;
    if (elapsedTime == 0) elapsedTime = 1; // Hindari pembagian dengan nol
    
    flowRate = ((1000.0 / elapsedTime) * pulseCount) / calibrationFactor;
    
    if (flowRate >= 0) {
        float flowLitres = (flowRate * (elapsedTime / 1000.0)) / 60.0;
        totalLitres += flowLitres;
        
        Serial.printf("Pulsa: %d, Waktu: %lu ms, Laju: %.2f L/min, Volume: %.4f L, Total: %.2f L\n",
                      pulseCount, elapsedTime, flowRate, flowLitres, totalLitres);
        
        if (isFillingActive) {
            float currentVolumeMl = totalLitres * 1000;
            if (currentVolumeMl >= targetVolumeMl) {
                Serial.printf("Target tercapai: %.2f ml dari %.2f ml\n", currentVolumeMl, targetVolumeMl);
                isFillingActive = false;
                checkInterval = 1000;
            }
        }
    } else {
        Serial.println("Error: Perhitungan laju aliran salah");
        flowRate = 0;
    }
    
    pulseCount = 0;
    lastFlowTime = millis();
    attachInterrupt(digitalPinToInterrupt(SENSOR), pulseCounter, FALLING);
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    // Initialize Water Flow Sensor
    pinMode(SENSOR, INPUT_PULLUP);
    pulseCount = 0;
    flowRate = 0.0;
    totalLitres = 0.0;
    lastFlowTime = 0;
    
    if (digitalPinToInterrupt(SENSOR) == NOT_AN_INTERRUPT) {
        Serial.println("Error: Water flow sensor pin does not support interrupts");
    } else {
        attachInterrupt(digitalPinToInterrupt(SENSOR), pulseCounter, FALLING);
        Serial.println("Water flow sensor initialized");
    }

    // Initialize I2C for ADS1115
    Wire.begin(21, 22);
    if (!ads.begin()) {
        Serial.println("Error: Failed to initialize ADS1115!");
        while (1) delay(10);
    }
    ads.setGain(GAIN_ONE);
    Serial.println("ADS1115 initialized");

    // Setup ADC
    analogSetWidth(12);
    analogSetAttenuation(ADC_11db);

    // Setup WiFi AP
    WiFi.mode(WIFI_AP);
    if (!WiFi.softAP(ssid, password)) {
        Serial.println("Error: Failed to start Access Point!");
        while (1) delay(10);
    }
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);

    // HTTP Routes (tetap sama)
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", index_html);
    });

    server.on("/setCalibration", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("factor", true)) {
        String factorStr = request->getParam("factor", true)->value();
        calibrationFactor = factorStr.toFloat();
        String response = "{\"status\":\"ok\",\"calibrationFactor\":\"" + String(calibrationFactor) + "\"}";
        request->send(200, "application/json", response);
        Serial.printf("Calibration factor set to: %.2f\n", calibrationFactor);
    } else {
        request->send(400, "text/plain", "Factor parameter required");
    }
    });

    server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request) {
        StaticJsonDocument<512> doc;
        doc["ph"] = String(phValue, 2);
        doc["phStatus"] = phStatus;
        doc["phVoltage"] = String(avgVoltagePH, 3);
        doc["tds"] = String(tdsValue, 0);
        doc["tdsVoltage"] = String(tdsVoltage, 3);
        doc["flowRate"] = String(flowRate, 2);
        doc["totalLitres"] = String(totalLitres, 2);
        doc["isActive"] = isFillingActive;
        doc["completed"] = !isFillingActive && (totalLitres * 1000 >= targetVolumeMl);
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    server.on("/setTarget", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("volume", true)) {
            String volumeStr = request->getParam("volume", true)->value();
            targetVolumeMl = volumeStr.toFloat();
            isFillingActive = true;
            checkInterval = 100;
            totalLitres = 0.0;
            
            String response = "{\"status\":\"ok\",\"target\":\"" + String(targetVolumeMl) + "\"}";
            request->send(200, "application/json", response);
            Serial.printf("New target volume set: %.2f ml\n", targetVolumeMl);
        } else {
            request->send(400, "text/plain", "Volume parameter required");
        }
    });

    server.on("/resetFlow", HTTP_POST, [](AsyncWebServerRequest *request) {
        totalLitres = 0.0;
        isFillingActive = false;
        targetVolumeMl = 0.0;
        checkInterval = 1000;
        request->send(200, "application/json", "{\"status\":\"ok\"}");
        Serial.println("Flow reset requested");
    });

      server.on("/emergencyStop", HTTP_POST, [](AsyncWebServerRequest *request) {
        // Set as completed
        isFillingActive = false;
        // checkInterval = 100;
        // Send response indicating emergency stop activated
        request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Emergency stop activated\"}");
        Serial.println("Emergency stop activated");
    });

    server.begin();
    Serial.println("HTTP server started");
    Serial.println("System ready");
}

void loop() {
    static unsigned long lastCheck = millis();
    
    if (millis() - lastCheck >= checkInterval) {
        handleWaterFlow();
        lastCheck = millis();
    }

    // Read pH
    avgVoltagePH = readPHVoltage();
    if (avgVoltagePH >= 0) {
        phValue = voltageToPH(avgVoltagePH);
        if (phValue < 0 || phValue > 14) {
            phStatus = "Error: Invalid reading";
        } else if (phValue < 6.5) {
            phStatus = "Acidic";
        } else if (phValue > 7.5) {
            phStatus = "Basic";
        } else {
            phStatus = "Neutral";
        }
    } else {
        phValue = -1;
        phStatus = "Error: Sensor failed";
    }

    // Read TDS
    readTDS();

    delay(10);
}