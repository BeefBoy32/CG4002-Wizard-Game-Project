//LED
#define RED_PIN D2
#define GREEN_PIN D3
#define BLUE_PIN D4
#define RED_CHANNEL 0
#define GREEN_CHANNEL 1
#define BLUE_CHANNEL 2
#define FREQ 5000
#define RESOLUTION 8

void setup() {
  ledcAttachChannel(RED_PIN, FREQ, RESOLUTION, RED_CHANNEL);
  ledcAttachChannel(GREEN_PIN, FREQ, RESOLUTION, GREEN_CHANNEL);
  ledcAttachChannel(BLUE_PIN, FREQ, RESOLUTION, BLUE_CHANNEL);
}

void loop() {
  /*
  // put your main code here, to run repeatedly:
  for(int dutyCycle = 0; dutyCycle < 8; dutyCycle++){   
    // changing the LED brightness with PWM
    ledcWrite(RED_PIN, dutyCycle);
    delay(500);
  }
  ledcWrite(RED_PIN, 0);
  delay(500);
  
  for(int dutyCycle = 0; dutyCycle < 8; dutyCycle++){   
    // changing the LED brightness with PWM
    ledcWrite(GREEN_PIN, dutyCycle);
    delay(500);
  }
  ledcWrite(GREEN_PIN, 0);
  delay(500);
  
  for(int dutyCycle = 0; dutyCycle < 8; dutyCycle++){   
    // changing the LED brightness with PWM
    ledcWrite(BLUE_PIN, dutyCycle);
    delay(500);
  }
  ledcWrite(BLUE_PIN, 0);
  delay(500);
  */

  //Ratio for purple is 1:2
  for(int dutyCycle = 0; dutyCycle < 128; dutyCycle++){   
    // changing the LED brightness with PWM
    ledcWrite(BLUE_PIN, dutyCycle * 2);
    ledcWrite(RED_PIN, dutyCycle);
    delay(20);
  }
  ledcWrite(BLUE_PIN, 0);
  ledcWrite(RED_PIN, 0);
  delay(20);
  //Ratio for yellow is 1:2
  for(int dutyCycle = 0; dutyCycle < 128; dutyCycle++){   
    // changing the LED brightness with PWM
    ledcWrite(GREEN_PIN, dutyCycle * 2);
    ledcWrite(RED_PIN, (int) (dutyCycle * 1.5));
    delay(20);
  }
  ledcWrite(GREEN_PIN, 0);
  ledcWrite(RED_PIN, 0);
  delay(20);
}
