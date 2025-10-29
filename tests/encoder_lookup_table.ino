#define CLK 6
#define DT 7
#define SW 8

int counter = 0;
int maxPos = 20;


void setup() {
  pinMode(CLK, INPUT);
  pinMode(DT, INPUT);
  pinMode(SW, INPUT_PULLUP);
  
  Serial.begin(9600);
  attachInterrupt(digitalPinToInterrupt(CLK), updateEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(DT), updateEncoder, CHANGE);
}

void loop() {
  static int lastCounterVal = 0;
  if(counter != lastCounterVal){
    if (abs(counter) == maxPos) {           // Check for maximum number of rotatory steps per cycle to restart cycle
    counter = 0;
    Serial.print("Loop Complete");
    }
    Serial.println(counter);
    lastCounterVal = counter;
  }
  if (digitalRead(SW) == LOW) {
  counter = 0;
  lastCounterVal = counter;
  delay(50);
  
  }
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

  // Update counter value only if all 4 states of the pins are complere
  if (ABABval >= 4) {               // 4 states completed in clockwise direction 
    counter++;
    ABABval -= 4;
  } else if (ABABval <= -4) {     // 4 states completed in counterclockwise direction
    counter--;
    ABABval += 4;
  }
  
}
