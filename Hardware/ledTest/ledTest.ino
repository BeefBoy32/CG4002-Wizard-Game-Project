//LED
#include <LEDControl.h>

Colour spell_colour = CYAN;
LEDControl ledControl(D2, D3, D4, 5000, 8);


void setup() {
  ledControl.initializeLED();
}

void loop() {
  ledControl.on_initialize_light();
  delay(5000);
  ledControl.on_spell_light(WHITE, 0);
  delay(5000);
  /*
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
  */
}
