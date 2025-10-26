#define CLK 6
#define DT 7
#define SW 8

int counter = 0;
int maxPos = 20;
int currentState;
int lastState;

void setup() {
  pinMode(CLK, INPUT);
  pinMode(DT, INPUT);
  pinMode(SW, INPUT_PULLUP);
  
  Serial.begin(9600);
  attachInterrupt(digitalPinToInterrupt(CLK), updateEncoder, CHANGE);
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
  return;
  }
}

// Read CLK and DT state to update counter using Interrupt
void updateEncoder(){
  int lastState = 0;
  int currentState= (digitalRead(CLK) << 1) | digitalRead(DT);      // Left shift the current state of CLK to combine current state of DT 
  int transition = (lastState << 2) | currentState;                // Combine the Previous and Current state values
    switch (transition) {                                         // Switch cases with possible clockwise and counter-clockwise states
    case 0b0001:
    case 0b0111:
    case 0b1110:
    case 0b1000:
      counter++;
      break;
    case 0b0010:
    case 0b0100:
    case 0b1101:
    case 0b1011:
      counter--;
      break;
  }
  lastState = currentState;
}
