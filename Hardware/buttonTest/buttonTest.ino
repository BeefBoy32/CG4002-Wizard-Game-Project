#include <Button.h>
// Wand Button
Button myButton(D7, 50);
bool isButtonHeld;
bool isButtonReleased;

void setup() {
  Serial.begin(115200);
  // put your setup code here, to run once:
  myButton.InitializeButton();
}

void loop() {
  // put your main code here, to run repeatedly:
  isButtonHeld = myButton.CheckHold();
  if (myButton.IsInitialHold()) {
    Serial.println("Holding Button");
    do {
      isButtonHeld = myButton.CheckHold();
      isButtonReleased = myButton.CheckRelease();
    }while (!myButton.IsInitialRelease());
    Serial.println("Releasing Button");
  }
}
