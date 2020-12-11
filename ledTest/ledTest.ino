#include <FastLED.h>
#define DATA_PIN    2
#define LED_TYPE    WS2811
#define COLOR_ORDER GRB
#define NUM_LEDS    24
CRGB leds[NUM_LEDS];
#define BRIGHTNESS         200
#define FRAMES_PER_SECOND  240
int SOC = 0;

void setup() {
  delay(500);
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  Serial.begin(115200);
}

//Rotating "base color"
uint8_t gHue = 0; 
  
void loop() {
  //sinelon();
  //FastLED.show();  
  FastLED.delay(1000/FRAMES_PER_SECOND); 
  //EVERY_N_MILLISECONDS( 20 ) { gHue++; } // slowly cycle the "base color" through the rainbow

  ledSOC(SOC);

  if (Serial.available() > 0){
    int inputval = Serial.parseInt();
    if (inputval > 0){SOC = inputval;}
  }
  
}

void ledSOC(int socinput){

  int amountgreen = map(socinput, 0, 100, 0, NUM_LEDS);
  int amountred = NUM_LEDS - amountgreen;
  /*
  Serial.print("Amount green:");
  Serial.println(amountgreen);
  Serial.print("Amount red:");
  Serial.println(amountred);
  */

  for(int i=0;i<amountgreen;i++){
      leds[i].setRGB(0,255,0);
      leds[i].fadeLightBy(BRIGHTNESS);
  }
  for(int j=0;j<amountred;j++){
      leds[amountgreen+j].setRGB(255,0,0);
      leds[amountgreen+j].fadeLightBy(BRIGHTNESS);
  }
  FastLED.show();
  
}

void ledBrakelight(){
  for(int i=0;i<NUM_LEDS;i++){
      leds[i].setRGB(255,0,0);
      leds[i].fadeLightBy(BRIGHTNESS);
  }
  FastLED.show();
}

void ledReverse(){
  for(int i=0;i<NUM_LEDS;i++){
      leds[i].setRGB(255,255,255);
      leds[i].fadeLightBy(BRIGHTNESS);
  }
  FastLED.show();
}


void rainbow() {
  //FastLED's built-in rainbow generator
  fill_rainbow( leds, NUM_LEDS, gHue, 7);
}

void sinelon() {
  //A colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, NUM_LEDS, 20);
  int pos = beatsin16( 13, 0, NUM_LEDS-1 );
  leds[pos] += CHSV( gHue, 255, 192);
}
