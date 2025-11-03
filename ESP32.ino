/* ESP32 – Full Integration
   DHT22 (in/out), LDR, MH-Z14A CO2, Relays, MQTT, Telegram, Serial2 -> Mega
   Author: tailored for user's mushroom-farm project
*/

// ---------------- Libraries ----------------
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <HardwareSerial.h>
#include <UniversalTelegramBot.h>
#include <HTTPClient.h>

// ---------------- CONFIG ----------------
// WiFi
const char* WIFI_SSID = "";
const char* WIFI_PASS = "";

const char* NODE_RED_URL = "http://172.20.10.9:1880/mushroom";

// MQTT (public broker example)
const char* MQTT_SERVER = "broker.emqx.io";
const uint16_t MQTT_PORT = 1883;
const char* MQTT_TOPIC_STATUS  = "farmingmushroom/status";
const char* MQTT_TOPIC_CONTROL = "farmingmushroom/control";

// Telegram (ใส่ค่าแทนที่นี้)
#define BOT_TOKEN ""   // <-- แทนด้วย token ของคุณ
#define CHAT_ID   ""     // <-- แทนด้วย chat id ของคุณ

// ---------------- Sensors / Pins ----------------
#define DHTPIN_IN   21
#define DHTPIN_OUT  22
#define DHTTYPE     DHT22
DHT dht_in(DHTPIN_IN, DHTTYPE);
DHT dht_out(DHTPIN_OUT, DHTTYPE);

#define LDR_PIN 35 // ADC pin

// Relays (active HIGH assumed)
#define SOL_PIN 26
#define EVAP_PIN 27
#define LED_PIN 14
#define FAN_PIN 12

#define SOL2_PIN 33

// MH-Z14A CO2 on UART2
HardwareSerial CO2Serial(2);
#define MHZ_BAUD 9600
uint8_t cmd_getppm[9] = {0xFF,0x01,0x86,0,0,0,0,0,0x79};

// ---------------- Thresholds & timing ----------------
#define TEMP_HIGH 32
#define TEMP_LOW  25
#define HUM_LOW   70
#define HUM_HIGH  85
#define CO2_HIGH  1500
#define CO2_LOW   1400

unsigned long lastAlert = 0;
#define ALERT_INTERVAL 300000UL // 5 min between Telegram alerts

// fallback last valid values
float last_temp_in = 0, last_hum_in = 0, last_temp_out = 0, last_hum_out = 0;

// WiFi/MQTT objects
WiFiClient espClient;
PubSubClient mqttClient(espClient);

WiFiClientSecure botClient;
UniversalTelegramBot bot(BOT_TOKEN, botClient);

// MQTT reconnect timing
unsigned long lastMQTTReconnectAttempt = 0;
const unsigned long MQTT_RETRY_DELAY = 5000UL;

// loop timing
unsigned long lastSensorMillis = 0;
const unsigned long SENSOR_INTERVAL = 10000UL; // 10s

void sendHTTPStatus(String jsonPayload) {
  if ((WiFi.status() == WL_CONNECTED)) {
    HTTPClient http;
    http.begin(NODE_RED_URL);
    http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.POST(jsonPayload);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.print("HTTP Response: "); Serial.println(response);
    } else {
      Serial.print("Error in HTTP request: "); Serial.println(httpResponseCode);
    }

    http.end();
  } else {
    Serial.println("WiFi not connected for HTTP");
  }
}

// ---------------- Setup ----------------
void setup() {
  Serial.begin(115200);       // debug
  Serial2.begin(115200);      // to Mega/TFT (TX2->RX1 Mega)

  // start CO2 UART (RX=16, TX=17) — adjust if wiring different
  CO2Serial.begin(MHZ_BAUD, SERIAL_8N1, 16, 17);

  // init relays
  pinMode(SOL_PIN, OUTPUT);
  pinMode(EVAP_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(SOL2_PIN, OUTPUT);
  digitalWrite(SOL_PIN, LOW);
  digitalWrite(EVAP_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(FAN_PIN, LOW);
  digitalWrite(SOL2_PIN, LOW);

  // init DHT sensors
  dht_in.begin();
  dht_out.begin();

  // Wi-Fi connect (with timeout)
  Serial.println();
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000UL) {
    Serial.print(".");
    delay(500);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("WiFi connected!");
    Serial.print("IP: "); Serial.println(WiFi.localIP());
  } else {
    Serial.println();
    Serial.println("WiFi connect failed (timeout). Will retry from loop.");
  }

  // MQTT setup
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);

  // Telegram client – skip certificate verification for simplicity on ESP32
  botClient.setInsecure();
}

// ---------------- Loop ----------------
void loop() {
  // ensure WiFi
  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long lastWiFiTry = 0;
    if (millis() - lastWiFiTry > 5000UL) {
      lastWiFiTry = millis();
      Serial.println("WiFi reconnect attempt...");
      WiFi.begin(WIFI_SSID, WIFI_PASS);
    }
    delay(200);
    return;
  }

  // MQTT reconnect handling (non-blocking)
  if (!mqttClient.connected()) {
    if (millis() - lastMQTTReconnectAttempt > MQTT_RETRY_DELAY) {
      lastMQTTReconnectAttempt = millis();
      reconnectMQTT();
    }
  } else {
    mqttClient.loop();
  }

  // sensor cycle
  if (millis() - lastSensorMillis >= SENSOR_INTERVAL) {
    lastSensorMillis = millis();
    readAndPublishSensors();
  }

  // other (no blocking)
}

// ---------------- Read sensors, handle alert, publish ----------------
void readAndPublishSensors() {
  // read DHTs
  float temp_in = dht_in.readTemperature();
  float hum_in  = dht_in.readHumidity();
  float temp_out = dht_out.readTemperature();
  float hum_out  = dht_out.readHumidity();

  // LDR (approx lux)
int ldrRaw = analogRead(LDR_PIN); // 0–4095
// map raw → lux (สมมุติ max 1000 lux, ปรับตามสภาพจริง)
float ldrLux = map(ldrRaw, 0, 4095, 0, 1000);

  // CO2
  int co2 = readCO2();

  // fallback to last valid if NaN
  if (!isnan(temp_in)) last_temp_in = temp_in; else temp_in = last_temp_in;
  if (!isnan(hum_in))  last_hum_in  = hum_in;  else hum_in  = last_hum_in;
  if (!isnan(temp_out)) last_temp_out = temp_out; else temp_out = last_temp_out;
  if (!isnan(hum_out))  last_hum_out  = hum_out;  else hum_out  = last_hum_out;

  unsigned long now = millis();

  // Build alert message based on thresholds
  String alertMsg = "";
  if (temp_in > TEMP_HIGH) alertMsg += "⚠ Temp IN HIGH: " + String(temp_in,1) + " C\n";
  if (temp_in < TEMP_LOW)  alertMsg += "⚠ Temp IN LOW: "  + String(temp_in,1) + " C\n";
  if (hum_in > HUM_HIGH)   alertMsg += "⚠ Hum IN HIGH: "   + String(hum_in,1)  + " %\n";
  if (hum_in < HUM_LOW)    alertMsg += "⚠ Hum IN LOW: "    + String(hum_in,1)  + " %\n";
  if (co2 > CO2_HIGH)      alertMsg += "⚠ CO2 HIGH: "      + String(co2)    + " ppm\n";
  if (co2 < CO2_LOW && co2 >= 0) alertMsg += "⚠ CO2 LOW: " + String(co2) + " ppm\n";

  // Send Telegram alert if conditions and not sent recently
  if (alertMsg.length() > 0 && (now - lastAlert > ALERT_INTERVAL)) {
    Serial.println("Sending Telegram alert...");
    bool sent = bot.sendMessage(CHAT_ID, alertMsg, "");
    if (sent) {
      Serial.println("Telegram alert sent.");
      lastAlert = now;
    } else {
      Serial.println("Telegram send failed.");
    }
  }

  // Compose status string (labels compatible with Mega parser)
  String status = "";
  status += "TEMP_IN:" + String(temp_in,1) + ",";
  status += "HUM_IN:"  + String(hum_in,1)  + ",";
  status += "TEMP_OUT:"+ String(temp_out,1)+ ",";
  status += "HUM_OUT:" + String(hum_out,1) + ",";
  status += "LDR:"     + String(ldrLux,1)       + ",";
  status += "CO2:"     + String(co2)       + ",";
  status += "SOL:"     + String(digitalRead(SOL_PIN)) + ",";
  status += "EVAP:"    + String(digitalRead(EVAP_PIN)) + ",";
  status += "FAN:"     + String(digitalRead(FAN_PIN)) + ",";
  status += "LED:"     + String(digitalRead(LED_PIN));
  status += "SOL2:"    + String(digitalRead(SOL2_PIN)) + ",";

  // Publish to MQTT (if connected)
  if (mqttClient.connected()) {
    mqttClient.publish(MQTT_TOPIC_STATUS, status.c_str());
  }

  // ---------------- Send HTTP fallback ----------------
  // สร้าง JSON payload
  String jsonPayload = "{";
  jsonPayload += "\"temp_in\":" + String(temp_in,1) + ",";
  jsonPayload += "\"hum_in\":" + String(hum_in,1) + ",";
  jsonPayload += "\"temp_out\":" + String(temp_out,1) + ",";
  jsonPayload += "\"hum_out\":" + String(hum_out,1) + ",";
  jsonPayload += "\"ldr\":" + String(ldrLux,1) + ",";
  jsonPayload += "\"co2\":" + String(co2) + ",";
  jsonPayload += "\"sol\":" + String(digitalRead(SOL_PIN)) + ",";
  jsonPayload += "\"evap\":" + String(digitalRead(EVAP_PIN)) + ",";
  jsonPayload += "\"fan\":" + String(digitalRead(FAN_PIN)) + ",";
  jsonPayload += "\"led\":" + String(digitalRead(LED_PIN)) + ",";
jsonPayload += "\"sol2\":" + String(digitalRead(SOL2_PIN));

  jsonPayload += "}";

  sendHTTPStatus(jsonPayload);


  // Send to Mega via Serial2
  Serial2.println(status);

  // Debug print local Serial
  Serial.println(status);
}

// ---------------- MQTT reconnect ----------------
void reconnectMQTT() {
  Serial.print("Connecting MQTT...");
  // build clientId: device-unique + random to avoid collision
  String clientId = "ESP32-" + String((uint32_t)ESP.getEfuseMac(), HEX) + "-" + String(random(0xffff), HEX);

  // connect without auth (public broker). If your broker requires user/pass, use client.connect(id,user,pass)
  if (mqttClient.connect(clientId.c_str())) {
    Serial.println("connected");
    mqttClient.subscribe(MQTT_TOPIC_CONTROL);
    // send initial status
    mqttClient.publish(MQTT_TOPIC_STATUS, "ESP32 Online");
  } else {
    Serial.print("failed, rc=");
    Serial.println(mqttClient.state());
  }
}

// ---------------- MQTT callback (control messages) ----------------
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  Serial.print("MQTT received: ");
  Serial.println(msg);

  // Commands accepted: SOL_ON / SOL_OFF, EVAP_ON / EVAP_OFF, FAN_ON / FAN_OFF, LED_ON / LED_OFF
  if (msg.indexOf("SOL_ON") >= 0)  digitalWrite(SOL_PIN, HIGH);
  if (msg.indexOf("SOL_OFF") >= 0) digitalWrite(SOL_PIN, LOW);

  if (msg.indexOf("EVAP_ON") >= 0)  digitalWrite(EVAP_PIN, HIGH);
  if (msg.indexOf("EVAP_OFF") >= 0) digitalWrite(EVAP_PIN, LOW);

  if (msg.indexOf("FAN_ON") >= 0)  digitalWrite(FAN_PIN, HIGH);
  if (msg.indexOf("FAN_OFF") >= 0) digitalWrite(FAN_PIN, LOW);

  if (msg.indexOf("LED_ON") >= 0)  digitalWrite(LED_PIN, HIGH);
  if (msg.indexOf("LED_OFF") >= 0) digitalWrite(LED_PIN, LOW);

  if (msg.indexOf("SOL2_ON") >= 0)  digitalWrite(SOL2_PIN, HIGH);
  if (msg.indexOf("SOL2_OFF") >= 0) digitalWrite(SOL2_PIN, LOW);


  // Publish ack (current relay states)
  String ack = String("ACK:SOL:") + digitalRead(SOL_PIN) +
               ",EVAP:" + digitalRead(EVAP_PIN) +
               ",FAN:" + digitalRead(FAN_PIN) +
               ",LED:" + digitalRead(LED_PIN) +
               ",SOL2:" + digitalRead(SOL2_PIN);
  if (mqttClient.connected()) mqttClient.publish(MQTT_TOPIC_STATUS, ack.c_str());

  // Also send to Mega so TFT updates quickly
  Serial2.println(ack);
}

// ---------------- Read MH-Z14A CO2 via UART ----------------
int readCO2() {
  uint8_t response[9];
  CO2Serial.write(cmd_getppm, 9);
  delay(50);

  int idx = 0;
  while (CO2Serial.available() && idx < 9) {
    uint8_t b = CO2Serial.read();
    if (idx == 0 && b != 0xFF) {
      // ข้าม noise จนกว่าจะเจอ 0xFF
      continue;
    }
    response[idx++] = b;
  }

  if (idx == 9 && response[0] == 0xFF && response[1] == 0x86) {
    int ppm = (response[2] << 8) | response[3];
    return ppm;
  }
  return -1; // error
}


