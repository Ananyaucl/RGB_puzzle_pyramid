#define CLK 6
#define DT 7
#define SW 8

#define LED 13

volatile int counter = 0;  // Made volatile for ISR access
int maxPos = 20;
bool LEDstate = true;
int num_leds = 72;
const uint8_t colorTable[4][3] = {
  {255, 255, 255},  // Face 0 - MIX / WHITE
  {255, 0, 0},      // Face 1 - RED
  {0, 255, 0},      // Face 2 - GREEN
  {0, 0, 255}       // Face 3 - BLUE
};
static const char* colorNames[4] = {"MIX", "RED", "GREEN", "BLUE"};


void setup() {
  pinMode(CLK, INPUT);
  pinMode(DT, INPUT);
  pinMode(SW, INPUT_PULLUP);

  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);  // Turn LED ON at startup (faceIndex starts at 0)
  
  Serial.begin(9600);
  
  determineColor(0);        // Set initial MIX colors at startup

  attachInterrupt(digitalPinToInterrupt(CLK), updateEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(DT), updateEncoder, CHANGE);
}

void loop() {
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
  }
  else{
    uint8_t r = colorTable[faceIndex][0];
    uint8_t g = colorTable[faceIndex][1];
    uint8_t b = colorTable[faceIndex][2];
      
    for (int n = 0; n < num_leds; n++) {
      send_RGB_to_pixel(r, g, b, n);
    }
  }
}


void send_RGB_to_pixel(int r,int g,int b,int pixel){
  // Serial.println(r);
  // Serial.println(g);
  // Serial.println(b); 
  // Serial.println(pixel); 
  return;
}

// Read CLK and DT state to update counter using Interrupt
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