// Duncan Wilson Oct 2025 - v1 - MQTT messager to vespera

// works with MKR1010

#include <SPI.h>
#include <WiFiNINA.h>
#include <PubSubClient.h>
#include "arduino_secrets.h" 
#include <utility/wifi_drv.h>   // library to drive to RGB LED on the MKR1010
   


/*
**** please enter your sensitive data in the Secret tab/arduino_secrets.h
**** using format below
#define SECRET_SSID "ssid name"
#define SECRET_PASS "ssid password"
#define SECRET_MQTTUSER "user name - eg student"
#define SECRET_MQTTPASS "password";
 */
const char* ssid          = SECRET_SSID;
const char* password      = SECRET_PASS;
// const char* ssid1         = SECRET_SSID1;
// const char* password1     = SECRET_PASS1;
const char* mqtt_username = SECRET_MQTTUSER;
const char* mqtt_password = SECRET_MQTTPASS;
const char* mqtt_server   = "mqtt.cetools.org";
const int mqtt_port       = 1884;

// create wifi object and mqtt object
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// Make sure to update your lightid value below with the one you have been allocated
String lightId = "15"; // the topic id number or user number being used.

// Here we define the MQTT topic we will be publishing data to
String mqtt_topic = "student/CASA0014/luminaire/" + lightId;            
String clientId = ""; // will set once i have mac address so that it is unique

// NeoPixel Configuration - we need to know this to know how to send messages 
// to vespera 
const int num_leds = 72;
const int payload_size = num_leds * 3; // x3 for RGB

// Create the byte array to send in MQTT payload this stores all the colours 
// in memory so that they can be accessed in for example the rainbow function
byte RGBpayload[payload_size];

// define pins
int CLK = 6;
int DT = 7;
int SW = 8;

int LED = 13;

volatile int counter = 0;  // Made volatile for ISR access
int maxPos = 20;
bool LEDstate = true;
// int num_leds = 72;
const uint8_t colorTable[4][3] = {
  {255, 255, 255},  // Face 0 - MIX / WHITE
  {255, 0, 0},      // Face 1 - RED
  {0, 255, 0},      // Face 2 - GREEN
  {0, 0, 255}       // Face 3 - BLUE
};
static const char* colorNames[4] = {"MIX", "RED", "GREEN", "BLUE"};


void setup() {
  Serial.begin(115200);
  //while (!Serial); // Wait for serial port to connect (useful for debugging)
  Serial.println("Vespera");

  pinMode(CLK, INPUT);
  pinMode(DT, INPUT);
  pinMode(SW, INPUT_PULLUP);

  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);  // Turn LED ON at startup (faceIndex starts at 0)
  
  determineColor(0);        // Set initial MIX colors at startup

  attachInterrupt(digitalPinToInterrupt(CLK), updateEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(DT), updateEncoder, CHANGE);


  // print your MAC address:
  byte mac[6];
  WiFi.macAddress(mac);
  Serial.print("MAC address: ");
  printMacAddress(mac);

  Serial.print("This device is Vespera ");
  Serial.println(lightId);

  // Connect to WiFi
  startWifi();

  // Connect to MQTT broker
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setBufferSize(2000);
  mqttClient.setCallback(callback);
  
  Serial.println("Set-up complete");
}
 
void loop() {
  // Reconnect if necessary
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  
  if (WiFi.status() != WL_CONNECTED){
    startWifi();
  }
  // keep mqtt alive
  mqttClient.loop();

  static int lastCounterVal = 0;
  static int lastFaceIndex = 0;  // Initialize to 0 since we start there
  static unsigned long lastMixUpdate = 0;

  // Handle negative counter values
  int normalizedCounter = counter;
  if (counter < 0) {
    // Map negative values to corresponding positive counter values
    normalizedCounter = (counter % maxPos + maxPos) % maxPos;
  }
  
  int faceIndex = (normalizedCounter / 5) % 4; // 0–3 

  // Periodic MIX color refresh when at face 0
  if (faceIndex == 0 && (millis() - lastMixUpdate >= 200)) {
    determineColor(0); // refresh random colors periodically
    lastMixUpdate = millis();
    Serial.println(lastMixUpdate);
  }

  if(counter != lastCounterVal){
    // LED control based on faceIndex
    if (LEDstate) {
      if (faceIndex == 0) {
        digitalWrite(LED, HIGH);   // LED ON when at face 0
      } else {
        digitalWrite(LED, LOW);    // LED OFF for other faces
      }
    }
    
    if (abs(counter) >= maxPos) {  // Use >= for safety
      counter = 0;
      Serial.println("Loop Complete");
    }
   
    // Color update only when face changes and at face boundaries
    if (faceIndex != lastFaceIndex && normalizedCounter % 5 == 0) {
      determineColor(faceIndex);
      lastFaceIndex = faceIndex;
    }
    lastCounterVal = counter;
  }
}

void determineColor(int faceIndex){
  faceIndex = faceIndex % 4;
  
  Serial.print("Face: ");
  Serial.println(colorNames[faceIndex]);

  if(faceIndex == 0){
    // MIX mode - send random RGB values to each LED
    for(int n=0; n < num_leds; n++){
      uint8_t r = random(0,256);
      uint8_t g = random(0,256);
      uint8_t b = random(0,256);
      send_RGB_to_pixel(r,g,b,n);
    }
    delay(100);
  }
  else{
    uint8_t r = colorTable[faceIndex][0];
    uint8_t g = colorTable[faceIndex][1];
    uint8_t b = colorTable[faceIndex][2];
      
    for (int n = 0; n < num_leds; n++) {
      send_RGB_to_pixel(r, g, b, n);
    }
    delay(100);
  }
}


// Function to update the R, G, B values of a single LED pixel
// RGB can a value between 0-254, pixel is 0-71 for a 72 neopixel strip
void send_RGB_to_pixel(int r, int g, int b, int pixel) {
  // Check if the mqttClient is connected before publishing
  if (mqttClient.connected()) {
    // Update the byte array with the specified RGB color pattern
    RGBpayload[pixel * 3 + 0] = (byte)r; // Red
    RGBpayload[pixel * 3 + 1] = (byte)g; // Green
    RGBpayload[pixel * 3 + 2] = (byte)b; // Blue

    // Publish the byte array
    mqttClient.publish(mqtt_topic.c_str(), RGBpayload, payload_size);
    
    Serial.println("Published whole byte array after updating a single pixel.");
  } else {
    Serial.println("MQTT mqttClient not connected, cannot publish from *send_RGB_to_pixel*.");
  }
}

void send_all_off() {
  // Check if the mqttClient is connected before publishing
  if (mqttClient.connected()) {
    // Fill the byte array with the specified RGB color pattern
    for(int pixel=0; pixel < num_leds; pixel++){
      RGBpayload[pixel * 3 + 0] = (byte)0; // Red
      RGBpayload[pixel * 3 + 1] = (byte)0; // Green
      RGBpayload[pixel * 3 + 2] = (byte)0; // Blue
    }
    // Publish the byte array
    mqttClient.publish(mqtt_topic.c_str(), RGBpayload, payload_size);
    
    Serial.println("Published an all zero (off) byte array.");
  } else {
    Serial.println("MQTT mqttClient not connected, cannot publish from *send_all_off*.");
  }
}

void send_all_random() {
  // Check if the mqttClient is connected before publishing
  if (mqttClient.connected()) {
    // Fill the byte array with the specified RGB color pattern
    for(int pixel=0; pixel < num_leds; pixel++){
      RGBpayload[pixel * 3 + 0] = (byte)random(50,256); // Red - 256 is exclusive, so it goes up to 255
      RGBpayload[pixel * 3 + 1] = (byte)random(50,256); // Green
      RGBpayload[pixel * 3 + 2] = (byte)random(50,256); // Blue
    }
    // Publish the byte array
    mqttClient.publish(mqtt_topic.c_str(), RGBpayload, payload_size);
    
    Serial.println("Published an all random byte array.");
  } else {
    Serial.println("MQTT mqttClient not connected, cannot publish from *send_all_random*.");
  }
}

void printMacAddress(byte mac[]) {
  for (int i = 5; i >= 0; i--) {
    if (mac[i] < 16) {
      Serial.print("0");
    }
    Serial.print(mac[i], HEX);
    if (i > 0) {
      Serial.print(":");
    }
  }
  Serial.println();
}

void updateEncoder(){

  static uint8_t prev_ABAB = 3;        //ABAB
  static int8_t ABABval= 0;
  const int enc_states[]  = {0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0}; // Lookup table

  prev_ABAB <<= 2;        // Make 2 bit space for adding next AB state details

  if (digitalRead(CLK)){        // Add current state of CLK pin
    prev_ABAB |= 0x02;
  }

  if (digitalRead(DT)){         // Add current state of DT pin
    prev_ABAB |= 0x01;
  }
  
  ABABval += enc_states[(prev_ABAB & 0x0f )];       // Look up aggregated value of the previous and current CLK and DT pins

  // Update counter value only if all 4 states of the pins are complete
  if (ABABval >= 4) {               // 4 states completed in clockwise direction 
    counter++;
    ABABval -= 4;
  } else if (ABABval <= -4) {     // 4 states completed in counterclockwise direction
    counter--;
    ABABval += 4;
  }
}
