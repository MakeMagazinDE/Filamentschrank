#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <U8g2lib.h>
#include <FastLED.h>

// =====================
// WiFi and MQTT Settings
// =====================
const char* ssid = "SSID";
const char* password = "password";

// MQTT settings – adjust these to match your MQTT broker
const char* mqtt_server = "mqtt-server-ip";
const int mqtt_port = 1883;
const char* mqtt_clientName = "FilamentStorage"; 
const char* mqtt_user     = "mqtt-user";
const char* mqtt_password = "password";  

WiFiClient espClient;
PubSubClient client(espClient);

// Global flags
bool wifiInitialized = false;
bool mqttConnected   = false;

// =====================
// Display Settings (OLED)
// =====================
// For the ESP32, default I2C pins are SDA=21 and SCL=22.
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// =====================
// Sensor Settings (DHT)
// =====================
// Using GPIO33 for the DHT sensor data pin (placed near the 3V3 pin)
#define DHTPIN 33    
#define DHTTYPE DHT22
DHT dht1(DHTPIN, DHTTYPE);

// Relay and door sensor
// Relay on GPIO23 and door sensor on GPIO15 (with INPUT_PULLUP)
#define RELAY_PIN 23
const int doorPin = 15;  
unsigned long doorCloseTime = 0; // marks when door closed

// =====================
// LED Settings
// =====================
#define DATA_PIN 13      // LED data pin for WS2812B on GPIO13
#define NUM_LEDS 300
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
#define BRIGHTNESS 130
CRGB leds[NUM_LEDS];

// Instead of using a fixed palette and a hold/transition,
// we'll continuously rotate the hue. We'll define a cycle time:
const unsigned long hueCycleTime = 60000;  // 60 seconds for one full hue rotation
const unsigned long colorCycleDuration = 10 * 60 * 1000; // 10 minutes total for colorful mode

// =====================
// Sensor Update Settings
// =====================
const unsigned long sensorUpdateInterval = 10000; // update every 10 sec
unsigned long lastSensorTime = 0;

// =====================
// State Machine Modes
// =====================
enum Mode { MODE_SENSOR, MODE_DOOR_OPEN, MODE_COLORFUL };
Mode currentMode = MODE_SENSOR;

// =====================
// WiFi & MQTT Reconnection Functions
// =====================
void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost. Reconnecting...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    unsigned long startTime = millis();
    while(WiFi.status() != WL_CONNECTED && (millis() - startTime < 10000)) {
      delay(500);
    }
    if(WiFi.status() == WL_CONNECTED) {
      Serial.println("WiFi reconnected.");
    } else {
      Serial.println("WiFi reconnection failed.");
    }
  }
}

void connectMQTTOnce() {
  if (client.connected()) {
    mqttConnected = true;
    return;
  }
  mqttConnected = false;
  client.setServer(mqtt_server, mqtt_port);
  Serial.print("Connecting to MQTT...");
  unsigned long startTime = millis();
  const unsigned long mqttTimeout = 30000; // 30-second timeout
  
  while (!client.connected() && (millis() - startTime < mqttTimeout)) {
    String clientId = String(mqtt_clientName) + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
      Serial.println(" MQTT connected.");
      mqttConnected = true;
      break;
    } else {
      Serial.print(" MQTT connection failed, rc=");
      Serial.print(client.state());
      Serial.println(" - retrying...");
      delay(2000);
    }
  }
  if (!client.connected()) {
    Serial.println("MQTT connection not established after timeout.");
  }
}

void checkMQTTConnection() {
  if (!client.connected()) {
    Serial.println("MQTT connection lost. Reconnecting...");
    connectMQTTOnce();
  }
}

// =====================
// WiFi Connection Function (Only Once)
// =====================
void connectWiFiOnce() {
  if (wifiInitialized) return;
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  unsigned long wifiStartTime = millis();
  const unsigned long wifiTimeout = 30000;
  
  while (WiFi.status() != WL_CONNECTED && (millis() - wifiStartTime < wifiTimeout)) {
    Serial.print(".");
    delay(500);
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWiFi connection failed. Restarting...");
    ESP.restart();
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  // Display WiFi info on OLED
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0, 24, "WiFi Connected!");
  u8g2.setCursor(0, 48);
  u8g2.print("IP: ");
  u8g2.print(WiFi.localIP());
  u8g2.sendBuffer();
  delay(2000);
  
  wifiInitialized = true;
}

// =====================
// Update Sensor Readings, Display, and MQTT Publishing
// =====================
void updateSensors() {
  float humidityInside = dht1.readHumidity();
  
  // Control relay based on a humidity threshold.
  digitalWrite(RELAY_PIN, humidityInside > 30.0 ? HIGH : LOW);
  
  // Publish sensor reading via MQTT.
  char humidityStr[8];
  dtostrf(humidityInside, 1, 2, humidityStr);
  client.publish("sensor/humidity_inside", humidityStr);
  
  // Publish door status to MQTT.
  if(digitalRead(doorPin) == LOW) {
    client.publish("sensor/door", "open");
  } else {
    client.publish("sensor/door", "closed");
  }
  
  // Update OLED display.
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  
  // First line: "Humidity: Value %" with one digit precision.
  char buffer[20];
  sprintf(buffer, "Humidity: %.1f %%", humidityInside);
  u8g2.drawStr(0, 12, buffer);
  
  // Line 3: WiFi status.
  u8g2.drawStr(0, 36, "Wifi: ");
  u8g2.drawStr(50, 36, (WiFi.status() == WL_CONNECTED) ? "connected" : "(not) connected");
  
  // Line 4: MQTT status.
  u8g2.drawStr(0, 48, "Mqtt: ");
  u8g2.drawStr(50, 48, (mqttConnected) ? "connected" : "(not) connected");
  
  u8g2.sendBuffer();
}

// =====================
// Setup Function
// =====================
void setup() {
  Serial.begin(115200);
  Serial.println("=== Program Start ===");
  
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0, 24, "=== Program Start ===");
  u8g2.sendBuffer();
  delay(2000);
  
  // Connect WiFi and MQTT (only once)
  connectWiFiOnce();
  connectMQTTOnce();
  
  // Setup pins, LEDs, sensors, etc.
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  pinMode(doorPin, INPUT_PULLUP);
  
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear();
  FastLED.show();
  
  dht1.begin();
  
  doorCloseTime = millis();
  currentMode = MODE_SENSOR;
  lastSensorTime = millis();
}

// =====================
// Main Loop Function (Non-blocking)
// =====================
void loop() {
  client.loop();
  unsigned long now = millis();

  // Check and reconnect WiFi and MQTT if needed.
  checkWiFiConnection();
  checkMQTTConnection();
  
  // Use a flag to prevent LED updates during OLED/sensor update.
  static bool updatingDisplay = false;
  
  // ----- SENSOR / OLED UPDATE BLOCK -----
  if(now - lastSensorTime >= sensorUpdateInterval) {
    updatingDisplay = true;
    updateSensors();
    lastSensorTime = now;
    updatingDisplay = false;
  }
  
  // ----- LED STATE MACHINE -----
  if (!updatingDisplay) {
    // Switch to DOOR_OPEN mode if door is open.
    if (digitalRead(doorPin) == LOW) {
      if (currentMode != MODE_DOOR_OPEN) {
        currentMode = MODE_DOOR_OPEN;
      }
    }
  
    switch(currentMode) {
      case MODE_SENSOR:
        FastLED.clear();
        FastLED.show();
        break;
        
      case MODE_DOOR_OPEN:
        fill_solid(leds, NUM_LEDS, CRGB::White);
        FastLED.show();
        if (digitalRead(doorPin) != LOW) {
          doorCloseTime = now;
          currentMode = MODE_COLORFUL;
        }
        break;
        
      case MODE_COLORFUL:
        if (digitalRead(doorPin) == LOW) {
          currentMode = MODE_DOOR_OPEN;
          break;
        }
        if (now - doorCloseTime < colorCycleDuration) {
          // Continuous hue rotation: one full cycle every 5 seconds.
          unsigned long elapsed = now - doorCloseTime;
          uint8_t hue = (elapsed * 256UL / hueCycleTime) % 256;
          fill_solid(leds, NUM_LEDS, CHSV(hue, 255, 255));
          FastLED.show();
        } else {
          FastLED.clear();
          FastLED.show();
          currentMode = MODE_SENSOR;
        }
        break;
    }
  }
  
  delay(10);
}
