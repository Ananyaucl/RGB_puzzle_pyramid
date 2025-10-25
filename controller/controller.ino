#define outputA 6
#define outputB 7
#define btn 8

int counter = 0;
int aState;
int aLastState;


void setup() {
  // put your setup code here, to run once:
  pinMode(outputA, INPUT);
  pinMode(outputB, INPUT);
  pinMode(btn, INPUT_PULLUP);
  
  Serial.begin(9600);
  aLastState = digitalRead(outputA);

}

void loop() {
  // put your main code here, to run repeatedly:
  aState = digitalRead(outputA);
  if(aState != aLastState){
    if(digitalRead(outputB) != aState){
      counter ++;
    }else{
      counter --;
    }
    Serial.print("Position: ");
    Serial.println(counter);
  }
  if (digitalRead(btn) == LOW) {
  counter = 0;
  return;
  }
  if (abs(counter) == 40) {
  counter = 0;
  }
  aLastState = aState;

}
