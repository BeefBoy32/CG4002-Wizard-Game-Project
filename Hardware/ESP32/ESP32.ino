#include <Button.h>

// MPU6050
#define MPU_INT_PIN 36   // using GPIO36 (VP)
volatile bool mpuInterrupt = false;


void IRAM_ATTR dmpDataReady() {
    mpuInterrupt = true;
}

// Wand Button
Button myButton(D7, 50);
bool isButtonHeld;
bool isButtonReleased;


void setup() {
  Serial.begin(115200);
  myButton.InitializeButton();
}

void loop() {
  isButtonHeld = myButton.CheckHold();
  
  if (myButton.IsInitialHold()){
    Serial.println("Holding Button");

    // Enable DMP Interrupt to constantly get DMP readings
    do {
      isButtonHeld = myButton.CheckHold();
      isButtonReleased = myButton.CheckRelease();
      // TODO Get DMP readings
      
    } while (!myButton.IsInitialRelease());

    /* TODO 
     *  Calculate coordinates and send data through BLE
     *  Wait for Response of type of spell
     *  Flash LED light
     */
    Serial.println("Releasing Button");
  }

  /* TODO
   *  Keep on receiving data from MPU and send data through BLE to Cemputer
   *  Detect for spin/thrust
   *  When spin, LED gets brighter
   *  When thrust, LED disappears (Spell casted)
   *  Restart Loop
   */
}
