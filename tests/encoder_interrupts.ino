#define CLK 6
#define DT 7
#define SW 8

int counter = 0;
int maxPos = 20;
int currentStateCLK;
int lastStateCLK;

void setup() {
  pinMode(CLK, INPUT);
  pinMode(DT, INPUT);
  pinMode(SW, INPUT_PULLUP);
  
  Serial.begin(9600);
  lastStateCLK = digitalRead(CLK);
  attachInterrupt(digitalPinToInterrupt(CLK), updateEncoder, CHANGE);

}

void loop() {
  static int lastCounterVal = 0;
  if(counter != lastCounterVal){
    // Check for maximum number of rotatory steps per cycle to restart cycle
    if (abs(counter) == maxPos) {
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

// Read CLK state to update counter using Interrupt
void updateEncoder(){
  currentStateCLK = digitalRead(CLK);
  if(currentStateCLK != lastStateCLK and currentStateCLK == 1){
    // Encoder in anticlockwise direction
    if(digitalRead(DT) != currentStateCLK){
      counter --;
    }
    else{
      // Encoder in clockwise direction
      counter ++;
    }
  }
  lastStateCLK = currentStateCLK;
}
