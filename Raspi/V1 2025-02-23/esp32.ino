#include <WiFi.h>
#include <WebSocketsServer.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <ESPAsyncWebServer.h>

// WiFi Credentials untuk Access Point
const char* ssid = "haisayakaka";     
const char* password = "12345678";  

// WebSocket Server pada port 81
WebSocketsServer webSocket = WebSocketsServer(81);

// HTTP Server pada port 80
AsyncWebServer server(80);

// ADS1115
Adafruit_ADS1115 ads;

// Pin Configuration
const int PH_PIN = 36;         // GPIO36 untuk pH
const int RELAY_PIN = 13;      // GPIO13 untuk relay pompa
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
float calibrationFactor = 8;
float flowRate = 0.0;
float totalLitres = 0.0;
unsigned long lastFlowTime = 0;

// Variables for readings
float avgVoltagePH = 0.0;
float phValue = 0.0;
String phStatus = "";
float tdsValue = 0.0;
float tdsVoltage = 0.0;
bool relayStatus = false;

// HTML dan JavaScript untuk klien
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>ESP32 pH, TDS & Flow Monitor</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial; text-align: center; margin: 0px auto; padding: 15px; }
    .reading { font-size: 2.8rem; }
    .card { background-color: white; box-shadow: 0px 0px 10px 1px rgba(0,0,0,0.1);
            border-radius: 10px; padding: 15px; margin: 20px; }
    .status { font-size: 1.2rem; margin: 10px; }
    .button { background-color: #4CAF50; border: none; color: white; padding: 15px 32px;
              text-align: center; text-decoration: none; display: inline-block; 
              font-size: 16px; margin: 4px 2px; cursor: pointer; border-radius: 4px; }
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
    <button class="button" onclick="sendResetFlow()">Reset Water Flow</button>
  </div>

  <div class="card">
    <h2>Kontrol Pompa Air</h2>
    <button class="button" id="relayButton" onclick="sendToggleRelay()">NYALAKAN POMPA</button>
  </div>

  <script>
    var websocket;
    var gateway = `ws://${window.location.hostname}:81/`;

    function initWebSocket() {
      websocket = new WebSocket(gateway);
      websocket.onopen = function() {
        console.log("WebSocket Connected");
      };
      websocket.onclose = function() {
        console.log("WebSocket Disconnected");
        setTimeout(initWebSocket, 2000);
      };
      websocket.onerror = function(error) {
        console.error("WebSocket Error: ", error);
      };
      websocket.onmessage = function(event) {
        try {
          var data = JSON.parse(event.data);
          document.getElementById("ph").innerHTML = data.ph;
          document.getElementById("status").innerHTML = data.phStatus;
          document.getElementById("voltage").innerHTML = data.phVoltage;
          document.getElementById("tds").innerHTML = data.tds;
          document.getElementById("tdsVoltage").innerHTML = data.tdsVoltage;
          document.getElementById("flowrate").innerHTML = data.flowRate;
          document.getElementById("totallitres").innerHTML = data.totalLitres;
          updateRelayButton(data.relay === "ON");
        } catch (e) {
          console.error("Error parsing message: ", e);
        }
      };
    }

    function sendToggleRelay() {
      websocket.send("toggleRelay");
    }

    function sendResetFlow() {
      websocket.send("resetFlow");
    }

    function updateRelayButton(isOn) {
      var button = document.getElementById("relayButton");
      if(isOn) {
        button.innerHTML = "MATIKAN POMPA";
        button.classList.add("button-off");
      } else {
        button.innerHTML = "NYALAKAN POMPA";
        button.classList.remove("button-off");
      }
    }

    window.onload = initWebSocket;
  </script>
</body>
</html>
)rawliteral";

// Water Flow Interrupt Handler
void IRAM_ATTR pulseCounter() {
  pulseCount++;
}

// TDS Functions
int getMedianNum(int16_t bArray[], int iFilterLen) {
  int16_t bTab[iFilterLen];
  for (byte i = 0; i < iFilterLen; i++)
    bTab[i] = bArray[i];
  int i, j;
  int16_t bTemp;
  for (j = 0; j < iFilterLen - 1; j++) {
    for (i = 0; i < iFilterLen - j - 1; i++) {
      if (bTab[i] > bTab[i + 1]) {
        bTemp = bTab[i];
        bTab[i] = bTab[i + 1];
        bTab[i + 1] = bTemp;
      }
    }
  }
  if ((iFilterLen & 1) > 0)
    bTemp = bTab[(iFilterLen - 1) / 2];
  else
    bTemp = (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
  return bTemp;
}

void readTDS() {
  static unsigned long analogSampleTimepoint = millis();
  
  if (millis() - analogSampleTimepoint > 40U) {
    analogSampleTimepoint = millis();
    int16_t reading = ads.readADC_SingleEnded(0);
    if (reading >= 0) {
      analogBuffer[analogBufferIndex] = reading;
      analogBufferIndex++;
      if (analogBufferIndex >= SCOUNT) 
        analogBufferIndex = 0;
    } else {
      Serial.println("Error: Failed to read TDS from ADS1115");
    }
  }
  
  static unsigned long printTimepoint = millis();
  if (millis() - printTimepoint > 800U) {
    printTimepoint = millis();
    for (int copyIndex = 0; copyIndex < SCOUNT; copyIndex++)
      analogBufferTemp[copyIndex] = analogBuffer[copyIndex];
    
    int medianValue = getMedianNum(analogBufferTemp, SCOUNT);
    tdsVoltage = ads.computeVolts(medianValue);
    
    if (tdsVoltage >= 0) {
      float compensationCoefficient = 1.0 + 0.02 * (temperature - 25.0);
      float compensationVoltage = tdsVoltage / compensationCoefficient;
      tdsValue = (133.42 * pow(compensationVoltage, 3) - 255.86 * pow(compensationVoltage, 2) + 857.39 * compensationVoltage) * 0.5;
    } else {
      tdsValue = -1;
      Serial.println("Error: Invalid TDS voltage");
    }
  }
}

// pH Functions
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
  if (voltage < 0) return -1;
  return 7.0 + ((PH7_VOLTAGE - voltage) / VOLTAGE_PER_PH);
}

// Fungsi untuk mengirim data ke semua klien WebSocket
void sendData() {
  String json = "{";
  json += "\"ph\":\"" + String(phValue, 2) + "\",";
  json += "\"phStatus\":\"" + phStatus + "\",";
  json += "\"phVoltage\":\"" + String(avgVoltagePH, 3) + "\",";
  json += "\"tds\":\"" + String(tdsValue, 0) + "\",";
  json += "\"tdsVoltage\":\"" + String(tdsVoltage, 3) + "\",";
  json += "\"flowRate\":\"" + String(flowRate, 2) + "\",";
  json += "\"totalLitres\":\"" + String(totalLitres, 2) + "\",";
  json += "\"relay\":\"" + String(relayStatus ? "ON" : "OFF") + "\"";
  json += "}";
  webSocket.broadcastTXT(json);
}

// Callback untuk event WebSocket
void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("Client [%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED:
      Serial.printf("Client [%u] Connected from %s\n", num, WiFi.softAPIP().toString().c_str());
      sendData();
      break;
    case WStype_TEXT: {
      // Tambahkan scope dengan kurung kurawal
      String message = String((char*)payload);
      Serial.printf("Client [%u] Sent: %s\n", num, message.c_str());
      if (message == "toggleRelay") {
        relayStatus = !relayStatus;
        digitalWrite(RELAY_PIN, relayStatus);
        sendData();
      } else if (message == "resetFlow") {
        totalLitres = 0;
        sendData();
      }
      break;
    }
    case WStype_ERROR:
      Serial.printf("Client [%u] Error!\n", num);
      break;
  }
}

void setup() {
  Serial.begin(115200);
  
  // Inisialisasi pin
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(SENSOR, INPUT_PULLUP);
  digitalWrite(RELAY_PIN, LOW);
  
  // Inisialisasi Water Flow
  pulseCount = 0;
  flowRate = 0.0;
  totalLitres = 0.0;
  lastFlowTime = 0;
  if (digitalPinToInterrupt(SENSOR) == NOT_AN_INTERRUPT) {
    Serial.println("Error: Water flow sensor pin does not support interrupts");
  } else {
    attachInterrupt(digitalPinToInterrupt(SENSOR), pulseCounter, FALLING);
  }
  
  // Inisialisasi I2C untuk ADS1115
  Wire.begin(21, 22);
  if (!ads.begin()) {
    Serial.println("Error: Failed to initialize ADS1115!");
    while (1);
  }
  ads.setGain(GAIN_ONE);
  
  analogSetWidth(12);          
  analogSetAttenuation(ADC_11db);
  
  // Mengatur ESP32 sebagai Access Point
  Serial.println("Mengatur Access Point...");
  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(ssid, password)) {
    Serial.println("Error: Failed to start Access Point!");
    while (1);
  }
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP Address: ");
  Serial.println(myIP);

  // Memulai HTTP Server untuk menyajikan HTML
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });
  server.begin();
  Serial.println("HTTP server aktif");

  // Memulai WebSocket server
  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);
  Serial.println("WebSocket server aktif");
  Serial.print("Hubungkan ke WiFi: ");
  Serial.println(ssid);
  Serial.print("Kemudian akses: http://");
  Serial.println(myIP);

  delay(2000);
}

void loop() {
  webSocket.loop();  // Tangani koneksi WebSocket
  
  // Read pH
  avgVoltagePH = readPHVoltage();
  if (avgVoltagePH >= 0) {
    phValue = voltageToPH(avgVoltagePH);
    if (phValue < 0 || phValue > 14) {
      phStatus = "Error: Pembacaan tidak valid";
    } else if (phValue < 6.5) {
      phStatus = "Asam";
    } else if (phValue > 7.5) {
      phStatus = "Basa";
    } else {
      phStatus = "Netral";
    }
  } else {
    phValue = -1;
    phStatus = "Error: Sensor pH gagal";
  }

  // Read TDS
  readTDS();
  
  // Calculate Water Flow
  if (millis() - lastFlowTime > 1000) {
    detachInterrupt(digitalPinToInterrupt(SENSOR));
    
    flowRate = ((1000.0 / (millis() - lastFlowTime)) * pulseCount) / calibrationFactor;
    if (flowRate >= 0) {
      float flowLitres = (flowRate / 60);
      totalLitres += flowLitres;
    } else {
      Serial.println("Error: Invalid flow rate calculation");
    }
    
    pulseCount = 0;
    lastFlowTime = millis();
    
    attachInterrupt(digitalPinToInterrupt(SENSOR), pulseCounter, FALLING);
  }
  
  // Kirim data ke semua klien setiap 2 detik
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 2000) {
    sendData();
    lastUpdate = millis();
  }
  
  delay(100);  // Hindari CPU overload
}