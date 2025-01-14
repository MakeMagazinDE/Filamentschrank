#include <DHT.h>
#include <U8g2lib.h>
#include <FastLED.h>

// For the OLED display using U8g2
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);


// Example: Reduce frequency of sensor reading and OLED updates
unsigned long lastSensorUpdate = 0;
const long sensorUpdateInterval = 3000; // 3 seconds


// DHT22 sensor setup
#define DHT1P 0
#define DHT2P 2
#define DHTTYPE DHT22
DHT dht1(DHT1P,DHTTYPE);
DHT dht2(DHT2P,DHTTYPE);

#define RELAY_PIN 12        // Relay connected to GPIO12 or D6

// Switch
const int switchPin = 14; //D5 GPIO14

// LEDs
#define DATA_PIN 13 //D7 GPIO13
#define NUM_LEDS 300 // Adjust based on your LED setup
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
#define BRIGHTNESS 160
#define SATURATION 255
CRGB leds[NUM_LEDS];

static float pulseSpeed = 0.5;  // Larger value gives faster pulse.

uint8_t hueA = 5;  // Start hue at valueMin.
uint8_t satA = 150;  // Start saturation at valueMin.
float valueMin = 70.0;  // Pulse minimum value (Should be less then valueMax).

uint8_t hueB = 200;  // End hue at valueMax.
uint8_t satB = 255;  // End saturation at valueMax.
float valueMax = 255.0;  // Pulse maximum value (Should be larger then valueMin).

uint8_t hue = hueA;  // Do Not Edit
uint8_t sat = satA;  // Do Not Edit
float val = valueMin;  // Do Not Edit
uint8_t hueDelta = hueA - hueB;  // Do Not Edit
static float delta = (valueMax - valueMin) / 2.35040238;  // Do Not Edit



void setup() {
  pinMode(RELAY_PIN, OUTPUT); // Initialize the relay pin as an output 
  digitalWrite(RELAY_PIN, LOW); // Ensure relay is OFF on startup 

  pinMode(switchPin, INPUT_PULLUP); // Assuming you have an external pull-up resistor
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear();
  FastLED.show();
  u8g2.begin();
  dht1.begin();
  dht2.begin();
}

void loop() {
 
unsigned long currentMillis = millis();

if (currentMillis - lastSensorUpdate >= sensorUpdateInterval) {
  lastSensorUpdate = currentMillis;
 
// Read humidity values
  float humidity1 = 0;
  float humidity2 = 0;

humidity1 = dht1.readHumidity();
humidity2 = dht2.readHumidity();

// Control the relay based on sensor 1 humidity
  if(humidity1 > 30.0) {  // Humidity threshold is 30%
    digitalWrite(RELAY_PIN, HIGH); // Turns on the relay
  } else {
    digitalWrite(RELAY_PIN, LOW); // Turns off the relay
  }

  // Display humidity on OLED
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0, 24, "Humidity Sensor 1:");
  u8g2.setCursor(0, 36);
  u8g2.print(humidity1);
  u8g2.drawStr(0, 48, "Humidity Sensor 2:");
  u8g2.setCursor(0, 60);
  u8g2.print(humidity2);
  u8g2.sendBuffer();

}

  // Read the switch state
  bool switchState = digitalRead(switchPin);
  
  if (switchState == HIGH) {
    // Switch is OPEN, show PULSATING RAINBOW
    float dV = ((exp(sin(pulseSpeed * millis()/2000.0*PI)) -0.36787944) * delta);
  val = valueMin + dV;
  hue = map(val, valueMin, valueMax, hueA, hueB);  // Map hue based on current val
  sat = map(val, valueMin, valueMax, satA, satB);  // Map sat based on current val

    for (int i=0;i<NUM_LEDS;i++) {
    leds[i] = CHSV(hue, sat, val);
     // You can experiment with commenting out these dim8_video lines
    // to get a different sort of look.
    leds[i].r = dim8_video(leds[i].r);
    leds[i].g = dim8_video(leds[i].g);
    leds[i].b = dim8_video(leds[i].b);
  
  }
   
  } else {
     for (int i=0;i<NUM_LEDS;i++)
    {
    leds[i] = CRGB::White;
   } 

  }
  FastLED.show();

}