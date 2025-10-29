#include <SPI.h>
#include <WiFiNINA.h>
#include <PubSubClient.h>
#include "arduino_secrets.h" 
#include <utility/wifi_drv.h>

const char* ssid          = SECRET_SSID;
const char* password      = SECRET_PASS;
const char* mqtt_username = SECRET_MQTTUSER;
const char* mqtt_password = SECRET_MQTTPASS;
const char* mqtt_server   = "mqtt.cetools.org";
const int mqtt_port       = 1884;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

String lightId = "15";
String mqtt_topic = "student/CASA0014/luminaire/" + lightId;            
String clientId = "";

// Vespera LED Matrix configuration
const int num_leds = 72;
const int COLS = 6;
const int ROWS = 12;
const int payload_size = num_leds * 3;

byte RGBpayload[payload_size];

// Define pins
int CLK = 6;    
int DT = 7;
int SW = 8;

int LED = 13;

volatile int counter = 0;     // Made volatile for ISR access
int maxPos = 20;      // Using encoder with 20 steps to complete a revelotion
bool LEDstate = true;

const uint8_t colorTable[4][3] = {
  {255, 255, 255},  // Face 0 - MIX / (Air)
  {255, 0, 0},      // Face 1 - RED (Fire)
  {0, 255, 0},      // Face 2 - GREEN (Tree)
  {0, 0, 255}       // Face 3 - BLUE (Water)
};
static const char* colorNames[4] = {"MIX", "RED", "GREEN", "BLUE"};

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Vespera");

  pinMode(CLK, INPUT);
  pinMode(DT, INPUT);
  pinMode(SW, INPUT_PULLUP);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);

  byte mac[6];
  WiFi.macAddress(mac);
  Serial.print("MAC address: ");
  printMacAddress(mac);
  
  // Create unique client ID from MAC address
  clientId = "vespera-" + String(mac[4], HEX) + String(mac[5], HEX);
  
  Serial.print("This device is Vespera ");
  Serial.println(lightId);
  Serial.print("Client ID: ");
  Serial.println(clientId);

  startWifi();
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setBufferSize(2048); // Increased buffer size
  mqttClient.setCallback(callback);

  attachInterrupt(digitalPinToInterrupt(CLK), updateEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(DT), updateEncoder, CHANGE);

  // Initialize with face 0
  animateMix();
  publishPayload();
  
  Serial.println("Set-up complete");
  LedGreen();
}

void loop() {
  // Non-blocking reconnect check
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  
  if (WiFi.status() != WL_CONNECTED){
    startWifi();
  }
  
  mqttClient.loop();

  static int lastCounterVal = 0;
  static int lastFaceIndex = 0;
  static unsigned long lastAnimUpdate = 0;
  static unsigned long lastPublish = 0;
  static bool needsPublish = false;

  // Handle negative counter values
  int normalizedCounter = counter;
  if (counter < 0) {
    normalizedCounter = (counter % maxPos + maxPos) % maxPos;
  }
  
  int faceIndex = (normalizedCounter / 5) % 4;

  // Handle counter changes - immediate response
  if(counter != lastCounterVal){
    if (LEDstate) {
      digitalWrite(LED, faceIndex == 0 ? HIGH : LOW);
    }
    
    if (abs(counter) >= maxPos) {
      counter = 0;
      Serial.println("Loop Complete");
    }
   
    // Immediate update when face changes
    if (faceIndex != lastFaceIndex || normalizedCounter % 5 == 0) {
      animateFace(faceIndex);
      needsPublish = true; // Flag for publish
      lastFaceIndex = faceIndex;
      Serial.print("Face: ");
      Serial.println(colorNames[faceIndex]);
    }
    
    lastCounterVal = counter;
  }

  // Continuous animation updates
  unsigned long currentMillis = millis();
  unsigned long animDelay;
  
  switch(faceIndex) {
    case 0: animDelay = 150; break; // MIX
    case 1: animDelay = 70;  break; // Fire - fastest
    case 2: animDelay = 50;  break; // Tree
    case 3: animDelay = 60;  break; // Water
    default: animDelay = 100; break;
  }
  
  if (currentMillis - lastAnimUpdate >= animDelay) {
    animateFace(faceIndex);
    needsPublish = true;
    lastAnimUpdate = currentMillis;
  }

  // Throttled MQTT publish - max 20Hz to prevent broker overflow
  if (needsPublish && (currentMillis - lastPublish >= 50)) {
    if (mqttClient.connected()) {
      publishPayload();
      needsPublish = false;
      lastPublish = currentMillis;
    }
  }
}


// Convert column and row to LED index
int getPixelIndex(int col, int row) {
  // Standard zigzag pattern - adjust if your wiring is different
  if (col % 2 == 0) {
    return col * ROWS + row;
  } else {
    return col * ROWS + (ROWS - 1 - row);
  }
}

void animateFace(int faceIndex) {
  switch(faceIndex) {
    case 0:
      animateMix();
      break;
    case 1:
      animateFire();
      break;
    case 2:
      animateTree();
      break;
    case 3:
      animateWater();
      break;
  }
}

void animateMix(){
  for(int n = 0; n < num_leds; n++){
    setPixelColor(random(50, 256), random(50, 256), random(50, 256), n);
  }
}

void animateFire(){
  static uint16_t fireOffset = 0;
  
  for(int col = 0; col < COLS; col++){
    for(int row = 0; row < ROWS; row++){
      int pixelIndex = getPixelIndex(col, row);
      
      // FLAME SHAPE: Wider at BOTTOM (row 5), narrower at TOP (row 0)
      float distanceFromCenter = abs(col - COLS/2.0);
      float maxWidth = map(row, 0, ROWS-1, 2.0, COLS/2.0 + 1.0); // Slightly wider to cover more columns
      
      bool inFlame = (distanceFromCenter <= maxWidth);
      
      if(inFlame) {
        // Base intensity with more dramatic range
        int baseIntensity = map(row, 0, ROWS-1, 180, 255); // Higher base brightness
        
        // Dramatic flicker at TOP (flame tips)
        int flickerAmount = map(row, 0, ROWS-1, 80, 100);
        int flicker = random(-flickerAmount, flickerAmount);
        
        // HORIZONTAL wave motion
        float horizWave = sin((col * 0.5 + fireOffset * 0.2)) * 40;
        
        // UPWARD dancing flames
        float upwardWave = sin((row * 0.8 - fireOffset * 0.15)) * 35;
        
        int intensity = constrain(baseIntensity + flicker + horizWave + upwardWave, 120, 255);
        
        // MAXIMUM SATURATION: RED -> ORANGE -> YELLOW
        uint8_t r, g, b;
        
        if(row >= 4) {
          // Bottom rows 4-5: PURE DEEP RED (max saturation)
          r = 255;
          g = map(intensity, 120, 255, 0, 100); // Limited green for pure red
          b = 0;
        } 
        else if(row >= 2) {
          // Middle rows 2-3: PURE ORANGE (max saturation)
          r = 255;
          g = map(intensity, 120, 255, 100, 200); // Orange range
          b = 0;
        } 
        else {
          // Top rows 0-1: PURE YELLOW (max saturation, bright tips)
          r = 255;
          g = map(intensity, 120, 255, 200, 255); // Full yellow
          b = 0; // No blue for pure yellow
        }
        
        // Hot white sparks at tips
        if(row < 2 && random(120) < 5){
          r = 255;
          g = 191;
          b = 0; // Bright white-yellow spark
        }
        
        setPixelColor(r, g, b, pixelIndex);
      } else {
        // Outside flame - very dark red glow
        int glowRow = map(row, 0, ROWS-1, 5, 0);
        setPixelColor(glowRow, 0, 0, pixelIndex);
      }
    }
  }
  fireOffset++;
}


void animateTree() {
  static uint16_t treeOffset = 0;

  for (int col = 0; col < COLS; col++) {
    for (int row = 0; row < ROWS; row++) {
      int pixelIndex = getPixelIndex(col, row);

      // 🌬 Faster and stronger wind shimmer
      float horizWind = sin((col * 0.55 + treeOffset * 0.25)) * 60;    // more motion
      float upwardShimmer = sin((row * 1.2 - treeOffset * 0.18)) * 35; // quicker vertical flicker
      float diagWave = sin((col * 0.4 + row * 0.5 + treeOffset * 0.2)) * 30; // stronger diagonal motion

      uint8_t r, g, b;

      // 🌳 Enhanced GRADIENT: rich dark green → bright yellow-green top
      if (row >= 5) {
        // Deep, visible dark green
        int greenVal = 60 + horizWind / 3 + upwardShimmer / 3;
        greenVal = constrain(greenVal, 60, 100);
        r = 10;
        g = greenVal;
        b = 10;
      }
      else if (row >= 4) {
        // Forest green
        int greenVal = 110 + horizWind / 2 + upwardShimmer / 2 + diagWave / 3;
        greenVal = constrain(greenVal, 90, 150);
        r = constrain(25 + diagWave / 4, 15, 45);
        g = greenVal;
        b = constrain(15 + diagWave / 5, 10, 25);
      }
      else if (row >= 3) {
        // Medium green — vivid
        int greenVal = 160 + horizWind + upwardShimmer + diagWave / 2;
        greenVal = constrain(greenVal, 140, 200);
        r = constrain(40 + diagWave / 3, 25, 65);
        g = greenVal;
        b = constrain(20 + diagWave / 4, 10, 35);
      }
      else if (row >= 2) {
        // Bright green
        int greenVal = 200 + horizWind + upwardShimmer + diagWave;
        greenVal = constrain(greenVal, 180, 245);
        r = constrain(70 + diagWave / 2, 50, 110);
        g = greenVal;
        b = constrain(15 + diagWave / 3, 8, 25);
      }
      else if (row >= 1) {
        // Light yellow-green
        int greenVal = 225 + horizWind + upwardShimmer + diagWave;
        greenVal = constrain(greenVal, 210, 255);
        int yellowTint = constrain(150 + diagWave, 130, 200);
        r = yellowTint;
        g = greenVal;
        b = constrain(10 + diagWave / 3, 5, 20);
      }
      else {
        // Very top: bright yellow-green highlights (sunlit tips)
        int greenVal = 245 + horizWind + upwardShimmer + diagWave;
        greenVal = constrain(greenVal, 230, 255);
        int yellowTint = constrain(200 + diagWave, 180, 255);
        r = yellowTint;
        g = greenVal;
        b = constrain(10 + diagWave / 4, 5, 20);
      }

      // 🌑 Shadows with higher contrast but never black
      float shadowFactor = sin((col * 0.5 + treeOffset * 0.1));
      if (shadowFactor < -0.5) {
        r = max((int)(r * 0.55), 15);
        g = max((int)(g * 0.55), 40);
        b = max((int)(b * 0.55), 10);
      }

      // ☀️ Sun flicker on top leaves
      if (row <= 1 && sin((col * 0.7 + treeOffset * 0.15)) > 0.7) {
        r = min(255, r + 80);
        g = 255;
      }

      // Safety clamp (ensures visibility)
      r = constrain(r, 10, 255);
      g = constrain(g, 40, 255);
      b = constrain(b, 10, 100);

      setPixelColor(r, g, b, pixelIndex);
    }
  }

  // Faster motion for more lively shimmer
  treeOffset += 2;
}


void animateWater(){
  static uint16_t waterOffset = 0;
  
  for(int col = 0; col < COLS; col++){
    for(int row = 0; row < ROWS; row++){
      int pixelIndex = getPixelIndex(col, row);
      
      // WAVES RISING from BOTTOM (row 5) to TOP (row 0)
      int rowFromBottom = ROWS - 1 - row;
      
      // Upward wave motion
      float upwardWave1 = sin((rowFromBottom * 1.2 - waterOffset * 0.2)) * 45;
      float upwardWave2 = sin((rowFromBottom * 0.8 - waterOffset * 0.15)) * 35;
      
      // HORIZONTAL ripples
      float horizRipple = sin((col * 0.5 + waterOffset * 0.18)) * 40;
      
      // Diagonal wave
      float diagWave = sin((col * 0.4 + rowFromBottom * 0.6 - waterOffset * 0.12)) * 30;
      
      // Wave peaks
      int wavePhase = (rowFromBottom + waterOffset) % 4;
      bool isWavePeak = ((col + (waterOffset/2)) % 2 == 0 && wavePhase < 2) || 
                        ((col + (waterOffset/2)) % 2 == 1 && wavePhase >= 2);
      
      // DRAMATIC brightness range: Very dark bottom, very bright top
      int baseBrightness = map(row, 0, ROWS-1, 240, 35); // Extreme contrast
      
      int waveLift = isWavePeak ? 60 : 0;
      
      int brightness = constrain(baseBrightness + upwardWave1 + upwardWave2 + 
                                 horizRipple + diagWave + waveLift, 30, 255);
      
      // DRAMATIC COLOR: VIOLET-BLUE (bottom) -> DEEP BLUE -> CYAN -> BRIGHT CYAN (top)
      uint8_t r, g, b;
      
      if(row >= 5) {
        // Bottom row 5: DARK VIOLET-BLUE (deep ocean)
        r = map(brightness, 30, 255, 20, 50); // Violet component
        g = map(brightness, 30, 255, 5, 30);  // Very little green
        b = map(brightness, 30, 255, 80, 140); // Strong blue
        
      } else if(row >= 4) {
        // Row 4: DEEP BLUE with violet tint
        r = map(brightness, 30, 255, 10, 40); // Less violet
        g = map(brightness, 30, 255, 15, 50);
        b = map(brightness, 30, 255, 100, 170);
        
      } else if(row >= 3) {
        // Row 3: DEEP BLUE
        r = 0;
        g = map(brightness, 30, 255, 30, 80);
        b = map(brightness, 30, 255, 130, 200);
        
      } else if(row >= 2) {
        // Row 2: MEDIUM BLUE-CYAN
        r = 0;
        g = map(brightness, 30, 255, 80, 150);
        b = map(brightness, 30, 255, 170, 230);
        
      } else if(row >= 1) {
        // Row 1: BRIGHT CYAN
        r = map(brightness, 30, 255, 0, 120);
        g = map(brightness, 30, 255, 140, 220);
        b = map(brightness, 30, 255, 200, 255);
        
      } else {
        // Row 0: VERY BRIGHT CYAN (surface)
        r = map(brightness, 30, 255, 50, 150);
        g = map(brightness, 30, 255, 180, 255);
        b = 255;
      }
      
      // Extra brightness on wave peaks near surface
      if(isWavePeak && row <= 2) {
        r = min(255, r + 90);
        g = min(255, g + 100);
        b = 255;
      }
      
      // Surface sparkles
      if(row == 0 && brightness > 240 && random(80) < 5){
        r = 220;
        g = 255;
        b = 255;
      }
      
      setPixelColor(r, g, b, pixelIndex);
    }
  }
  waterOffset++;
}


// CRITICAL: Only update array, DO NOT publish
void setPixelColor(int r, int g, int b, int pixel) {
  if (pixel >= 0 && pixel < num_leds) {
    RGBpayload[pixel * 3 + 0] = (byte)constrain(r, 0, 255);
    RGBpayload[pixel * 3 + 1] = (byte)constrain(g, 0, 255);
    RGBpayload[pixel * 3 + 2] = (byte)constrain(b, 0, 255);
  }
}

// Publish entire payload ONCE
void publishPayload() {
  if (mqttClient.connected()) {
    bool success = mqttClient.publish(mqtt_topic.c_str(), RGBpayload, payload_size);
    if (!success) {
      Serial.println("Publish failed - buffer full?");
    }
  }
}

// Legacy compatibility
void send_RGB_to_pixel(int r, int g, int b, int pixel) {
  setPixelColor(r, g, b, pixel);
}

void determineColor(int faceIndex){
  faceIndex = faceIndex % 4;
  animateFace(faceIndex);
}

void send_all_off() {
  for(int pixel = 0; pixel < num_leds; pixel++){
    setPixelColor(0, 0, 0, pixel);
  }
  publishPayload();
  Serial.println("All off");
}

void send_all_random() {
  for(int pixel = 0; pixel < num_leds; pixel++){
    setPixelColor(random(50, 256), random(50, 256), random(50, 256), pixel);
  }
  publishPayload();
  Serial.println("Random");
}

void printMacAddress(byte mac[]) {
  for (int i = 5; i >= 0; i--) {
    if (mac[i] < 16) Serial.print("0");
    Serial.print(mac[i], HEX);
    if (i > 0) Serial.print(":");
  }
  Serial.println();
}

void updateEncoder(){
  static uint8_t prev_ABAB = 3;
  static int8_t ABABval = 0;
  const int enc_states[] = {0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0};

  prev_ABAB <<= 2;
  if (digitalRead(CLK)) prev_ABAB |= 0x02;
  if (digitalRead(DT)) prev_ABAB |= 0x01;
  
  ABABval += enc_states[(prev_ABAB & 0x0f)];

  if (ABABval >= 4) {
    counter++;
    ABABval -= 4;
  } else if (ABABval <= -4) {
    counter--;
    ABABval += 4;
  }
}


