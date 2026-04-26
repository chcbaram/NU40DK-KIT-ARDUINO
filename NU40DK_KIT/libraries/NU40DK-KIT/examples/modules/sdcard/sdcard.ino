#include <NU40DK.h>



NU40DK nu40;



void setup() {

  Serial1.begin(115200);

  nu40.begin(115200);  
  nu40.cliBegin(Serial1);
}


void loop() {
  nu40.ledToggle();
  delay(500);  
}
