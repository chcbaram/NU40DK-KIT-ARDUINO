#include <NU40DK.h>



NU40DK nu40;



void setup() {
  nu40.begin(115200);  
  nu40.cliBegin(Serial);
}


void loop() {
  nu40.ledToggle();
  delay(500);  
}
