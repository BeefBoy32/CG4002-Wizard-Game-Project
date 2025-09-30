//LED
#include <LEDControl.h>

Colour spell_colour = CYAN;
LEDControl ledControl(D2, D3, D4, 5000, 8);


void setup() {
  ledControl.initializeLED();
}

void loop() {
  // ledControl.on_initialize_light();
  /*
  // put your main code here, to run repeatedly:
  for(int dutyCycle = 0; dutyCycle < 256; dutyCycle++){   
    // changing the LED brightness with PWM
    ledcWrite(RED_PIN, dutyCycle);
    delay(15);
  }
  ledcWrite(RED_PIN, 0);
  delay(15);
  
  for(int dutyCycle = 0; dutyCycle < 256; dutyCycle++){   
    // changing the LED brightness with PWM
    ledcWrite(GREEN_PIN, dutyCycle);
    delay(15);
  }
  ledcWrite(GREEN_PIN, 0);
  delay(15);
  
  for(int dutyCycle = 0; dutyCycle < 256; dutyCycle++){   
    // changing the LED brightness with PWM
    ledcWrite(BLUE_PIN, dutyCycle);
    delay(15);
  }
  ledcWrite(BLUE_PIN, 0);
  delay(15);
  
  //Ratio for purple is 1:2, R:B
  for(int dutyCycle = 0; dutyCycle < 128; dutyCycle++){   
    // changing the LED brightness with PWM
    ledcWrite(BLUE_PIN, dutyCycle * 2);
    ledcWrite(RED_PIN, dutyCycle);
    delay(20);
  }
  ledcWrite(BLUE_PIN, 0);
  ledcWrite(RED_PIN, 0);
  delay(20);
  //Ratio for yellow is 4:3, G:R
  for(int dutyCycle = 0; dutyCycle < 128; dutyCycle++){   
    // changing the LED brightness with PWM
    ledcWrite(GREEN_PIN, dutyCycle * 2);
    ledcWrite(RED_PIN, (int) (dutyCycle * 1.5));
    delay(20);
  }
  ledcWrite(GREEN_PIN, 0);
  ledcWrite(RED_PIN, 0);
  delay(20);
  */
  // Ratio for cyan
  spell_colour = RED;
  for(int dutyCycle = 0; dutyCycle <= 5; dutyCycle++){   
    ledControl.on_spell_light(spell_colour, dutyCycle);
    delay(1000);
  }
  
  spell_colour = GREEN;
  for(int dutyCycle = 0; dutyCycle <= 5; dutyCycle++){   
    ledControl.on_spell_light(spell_colour, dutyCycle);
    delay(1000);
  }

  spell_colour = BLUE;
  for(int dutyCycle = 0; dutyCycle <= 5; dutyCycle++){   
    ledControl.on_spell_light(spell_colour, dutyCycle);
    delay(1000);
  }

  spell_colour = YELLOW;
  for(int dutyCycle = 0; dutyCycle <= 5; dutyCycle++){   
    ledControl.on_spell_light(spell_colour, dutyCycle);
    delay(1000);
  }

  spell_colour = PURPLE;
  for(int dutyCycle = 0; dutyCycle <= 5; dutyCycle++){   
    ledControl.on_spell_light(spell_colour, dutyCycle);
    delay(1000);
  }

  spell_colour = CYAN;
  for(int dutyCycle = 0; dutyCycle <= 5; dutyCycle++){   
    ledControl.on_spell_light(spell_colour, dutyCycle);
    delay(1000);
  }
}
