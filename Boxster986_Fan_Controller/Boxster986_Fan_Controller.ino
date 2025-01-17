#include <EEPROM.h>
#include "config.h"
#include <mcp_can.h>
#include <SPI.h>
#include <avr/wdt.h>
#include <Arduino.h>
#include <PWM.h>
#include <FastLED.h>
#include "CANBUS.h"
CANBUS canbus;           //Initiate class for canbus-interpreter

EEPROMSettings settings;

//Led ring pin
#define ledRing    4
#define LED_TYPE    WS2811
#define COLOR_ORDER GRB
#define NUM_LEDS    24
CRGB leds[NUM_LEDS];
#define BRIGHTNESS          100
#define FRAMES_PER_SECOND  120

unsigned long looptime, canlooptime, gaugeresettime = 0;

long unsigned int rxId;
unsigned char len = 0;
unsigned char rxBuf[8];
char msgString[128];                        // Array to store serial string

#define CAN0_INT 2                              // Set INT to pin 2
MCP_CAN CAN0(9);                               // Set CS to pin 9 de[ending on shield used

char mes[8] = {0, 0, 0, 0, 0, 0, 0, 0};

int16_t InverterTemp = 0;
int16_t BatteryTemp = 0;
int16_t MotorSpeed = 0;
int packCurrent = 0;
int tempguageval = 0;

//---------Fuel Guage------------//
//High for anti-clockwise
int fuel_direction_pin = 6;
int fuel_fuel_step_pin = 7;

int full_sweep_steps = 385;

int desiredposition = 0;
int actualposition = 0;
int SOC = 0;

int highestTemp = 0;
bool usersetsoc = false;

int DCL = 0;
int DCL32 = 0;
int throtmax = 0;
int throtmax32 = 0;

bool errorBMS = false;

int gear = 0;


//---------PWM Pump Control -------//
const int motorPWMpin = 10;
const int batteryPWMpin = 5;
int motorDutyCycle = 10;
int batteryDutyCycle = 50;
///////////////////////////////////

bool canactive = 0;
int32_t cantime = 0;

///////////Menu variables///////////////////
int menuload = 0;
int debug = 1;
int candebug = 0;
//////////////////////////////////////////

void loadSettings(){
  settings.Temp1 = 35;
  settings.Temp2 = 55;
  settings.TempHys = 5;
}

void setup(){
  Serial.begin(115200);
  Serial.setTimeout(20);

  FastLED.addLeds<LED_TYPE,ledRing,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  // set master brightness control
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.show(); 

  // Initialize MCP2515 running at 16MHz with a baudrate of 500kb/s and the masks and filters disabled.
  if (CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK)
    Serial.println("MCP2515 Initialized Successfully!");
  else
    Serial.println("Error Initializing MCP2515...");

  CAN0.setMode(MCP_NORMAL);                     // Set operation mode to normal so the MCP2515 sends acks to received data.

  pinMode(CAN0_INT, INPUT);                            // Configuring pin for /INT input

  Serial.println("Time Stamp,ID,Extended,LEN,D1,D2,D3,D4,D5,D6,D7,D8");

  ///////////////////CONFIGURE PINS///////////////////////////////////
  pinMode(Fan1, OUTPUT);
  pinMode(Fan2, OUTPUT);
  //pinMode(motorPWMpin, OUTPUT);
  //pinMode(batteryPWMpin, OUTPUT);
  pinMode(fuel_direction_pin, OUTPUT);
  pinMode(fuel_fuel_step_pin, OUTPUT);
  //////////////////////////////////////////////////////////////////////////////////
  EEPROM.get(0, settings);

  //Set pumps to off
  //analogWrite(motorPWMpin, (255-map(motorDutyCycle, 0, 100, 0, 255)));
  InitTimersSafe();
  SetPinFrequencySafe(motorPWMpin, 100);
  //SetPinFrequencySafe(batteryPWMpin, 100);
  pwmWriteHR(motorPWMpin, map(motorDutyCycle, 0, 100, 65535, 0));
  //pwmWriteHR(batteryPWMpin, map(batteryDutyCycle, 0, 100, 65535, 0));
 
  //Wind dial anticlockwise at startup to calibrate
  digitalWrite(fuel_direction_pin, HIGH);
  for (int i = 0; i <= full_sweep_steps*3; i++) {
    digitalWrite(fuel_fuel_step_pin, HIGH);
    delay(1);
    digitalWrite(fuel_fuel_step_pin, LOW);
  }
  
  //Dial sweep
  tempguageval = 139;
  MotorSpeed = 8000;
  cansend();
  desiredposition = full_sweep_steps;
  movefuelguageneedle();

  for (int i = 0; i <= 5; i++) {
      //Keep canbus happy by keep sending message
      cansend();
      delay(180);
  }

  tempguageval = 0;
  MotorSpeed = 0;
  cansend();
  desiredposition = 0;
  movefuelguageneedle();

  delay(1200);
  
  wdt_enable(WDTO_2S);
}

void loop(){
  //Check for incoming messages
  if (CAN0.checkReceive() == 3) {candecode();}

  //Check serial and go to menu
  if (Serial.available() > 0){
    //menu();
    int inputval = Serial.parseInt();
    if (inputval > 0){
      SOC = inputval;
    }
    usersetsoc = true;
  }

  if (canactive == 1){cantime = millis();}
  else {if (millis() - cantime > 2000){canactive = 0;}}

  //Send CAN every 80ms
  if (millis() - canlooptime > 80){
    cansend();
    canlooptime = millis();
  }

  //Re-calibrate gauge every 3 minutes
  if (millis() - gaugeresettime > 180000){
    gaugeReset();
    gaugeresettime = millis();
  }

  //Fan control every 500ms
  if (millis() - looptime > 500){
    //cansend();
    looptime = millis();
    if (debug == 1)
    {
      Serial.println();
      if (digitalRead(Fan1)){Serial.print("Fan 1 : ON");}
      else{Serial.print("Fan 1 : OFF");}
      Serial.println();
      if (digitalRead(Fan2)){Serial.print("Fan 2 : ON");}
      else{Serial.print("Fan 2 : OFF");}
      Serial.println();
      Serial.print("Inverter Temp : ");
      Serial.print(InverterTemp);
      Serial.println("˚C");
      Serial.print("Battery Temp : ");
      Serial.print(BatteryTemp);
      Serial.println("˚C");
      Serial.print("Motor Speed : ");
      Serial.print(MotorSpeed);
      Serial.println("rpm");
      Serial.print("Pack Current: ");
      Serial.print(packCurrent);
      Serial.println("A");
      Serial.print("Battery SOC : ");
      Serial.print(SOC);
      Serial.println("%");
      Serial.print("Can Active : ");
      Serial.print(canactive);
      Serial.println();
      Serial.print("DCL : ");
      Serial.print(DCL);
      Serial.println();
      Serial.print("Calc Throtmax : ");
      Serial.print(throtmax);
      Serial.println();
      Serial.print("Gear : ");
      Serial.print(gear);
      Serial.println();
      Serial.print("BMS Errors : ");
      if(errorBMS){Serial.println("Yes");}
      else{Serial.println("No");}
      Serial.print("idcmax sent to Drive unit: ");
      Serial.println(DCL32/32);
      Serial.println();
      Serial.println();
    }
    
    if (canactive == 1){
      if (highestTemp > settings.Temp1){
        digitalWrite(Fan1, HIGH);
      }
      if (highestTemp < (settings.Temp1 - settings.TempHys)){
        digitalWrite(Fan1, LOW);
      }
      if (highestTemp > settings.Temp2){
        digitalWrite(Fan2, HIGH);
        digitalWrite(Fan1, LOW);
      }
      if (highestTemp < (settings.Temp2 - settings.TempHys) && highestTemp > settings.Temp1){
        digitalWrite(Fan2, LOW);
        digitalWrite(Fan1, HIGH);
      }

    }
    else{digitalWrite(Fan1, HIGH);}
    canactive = 0;

    //Neutral
    if(gear == 0){
      ledSOC(SOC);
    }
    //Drive
    if(gear == 1){
      ledBrakelight();
    }
    //Reverse
    if(gear == 15){
      ledReverse();
    }
    
  }
  
  movefuelguageneedle();
  wdt_reset(); 
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

void gaugeReset(){

  int amountofstepstorewind = map(SOC, 0, 100, full_sweep_steps, 0);
  amountofstepstorewind = amountofstepstorewind + 50;
  digitalWrite(fuel_direction_pin, HIGH);
  for (int i = 0; i <= amountofstepstorewind; i++) {
    digitalWrite(fuel_fuel_step_pin, HIGH);
    delay(1);
    digitalWrite(fuel_fuel_step_pin, LOW);
  }

  //let movefuelguageneedle know it has been reset
  actualposition = 0;
  Serial.println(amountofstepstorewind);
}

void menu(){
  byte incomingByte = Serial.read(); // read the incoming byte:
  delay(10);
  if (menuload == 3)
  {
    switch (incomingByte){
      case 'q': //q for quit
        menuload = 0;
        incomingByte = 's';
        break;
      case '1':
        menuload = 3;
        candebug = !candebug;
        incomingByte = '1';
        break;
    }
  }
  if (menuload == 1){
    switch (incomingByte)
    {
      case '1':
        if (Serial.available() > 0){
          settings.Temp1 = (Serial.parseInt());
          menuload = 0;
          incomingByte = 's';
        }
        break;

      case '2':
        if (Serial.available() > 0){
          settings.Temp2 = (Serial.parseInt());
          menuload = 0;
          incomingByte = 's';
        }
        break;

      case '3':
        if (Serial.available() > 0){
          settings.TempHys = (Serial.parseInt());
          menuload = 0;
          incomingByte = 's';
        }
        break;
      case '4':
        if (Serial.available() > 0){
          desiredposition = map(Serial.parseInt(), 0, 100, full_sweep_steps, 0);
          menuload = 0;
          incomingByte = 's';
        }
        break;
      case 'q': //q for quit
        EEPROM.put(0, settings);
        debug = 1;
        menuload = 0;
        break;
      case 'd':
        debug = 0;
        menuload = 1;
        Serial.println();
        Serial.println();
        Serial.println();
        Serial.println("/////////// Fan Controller Settings ///////////");
        Serial.println("/////////// Debug Settings/////////////////////////");
        Serial.println("Zero-EV Conversions LTD");
        Serial.println("");
        Serial.print("1 - Can Debug : ");
        Serial.println(candebug);
        Serial.println();
        menuload = 3;
        break;
      case 'r': //
        Serial.println();
        Serial.print("Settings Reset");
        loadSettings();
        EEPROM.put(0, settings);
        menuload = 0;
        incomingByte = 's';
        break;
      default:
        // if nothing else matches, do the default
        // default is optional
        break;
    }
  }
  if (menuload == 0){
    switch (incomingByte){
      case 's': //Setup
        debug = 0;
        menuload = 1;
        Serial.println();
        Serial.println();
        Serial.println();
        Serial.println("/////////// Fan Controller Settings ///////////");
        Serial.println("/////////// Main Menu ////////////////////////////");
        Serial.println("Zero-EV Conversions LTD");
        Serial.println("");
        Serial.print("1 - Fan stage 1 On Temperature : ");
        Serial.print(settings.Temp1);
        Serial.println(" C");
        Serial.print("2 - Fan stage 2 On Temperature : ");
        Serial.print(settings.Temp2);
        Serial.println(" C");
        Serial.print("3 - Temperature Hystersis : ");
        Serial.print(settings.TempHys);
        Serial.println(" C");
        Serial.print("4 - Fuel Guage Test Position : ");
        Serial.print(desiredposition);
        Serial.println(" steps");
        Serial.println("d - Debug Settings");
        Serial.println("R - Reset to Factory Settings");
        Serial.println("Q - Exit Menu");
        Serial.println();
        break;
      default:
        // if nothing else matches
        break;
    }
  }
}

void movefuelguageneedle(){
  //Need to move clockwise
  if(desiredposition > actualposition){
    digitalWrite(fuel_direction_pin, LOW);
    int num_steps_calculated = desiredposition - actualposition;
    //Serial.print(num_steps_calculated);
    //Serial.println(" steps moved clockwise");
    for (int i = 0; i <= num_steps_calculated; i++) {
      digitalWrite(fuel_fuel_step_pin, HIGH);
      delay(1);
      digitalWrite(fuel_fuel_step_pin, LOW);
    }
    actualposition = desiredposition; //Move has been completed!
  }

  //Need to move anti-clockwise
  if(desiredposition < actualposition){
    digitalWrite(fuel_direction_pin, HIGH);
    int num_steps_calculated = actualposition - desiredposition;
    //Serial.print(num_steps_calculated);
    //Serial.println(" steps moved anti-clockwise");
    for (int i = 0; i <= num_steps_calculated; i++) {
      digitalWrite(fuel_fuel_step_pin, HIGH);
      delay(1);
      digitalWrite(fuel_fuel_step_pin, LOW);
    }
    actualposition = desiredposition; //Move has been completed!
  }
}

void candecode(){

  CAN0.readMsgBuf(&rxId, &len, rxBuf);      // Read data: len = data length, buf = data byte(s)

  //Battery Temp
  if (rxId == 0x356){
    //BatteryTemp = rxBuf[5];
    BatteryTemp = canbus.decode(rxBuf, len, 32, 16, "LSB", "SIGNED", 0.1, 0);
    if(BatteryTemp > 200){
      BatteryTemp = 0;
    }
    canactive = 1;
  }

  //DU1 Feedback
  if (rxId == 0x125){
    canactive = 1;
  }

  //DU1 Status
  if (rxId == 0x126){
    gear = canbus.decode(rxBuf, len, 8, 4, "LSB", "UNSIGNED", 1, 0);
    InverterTemp = canbus.decode(rxBuf, len, 32, 16, "LSB", "SIGNED", 1, 0);
    canactive = 1;
  }

  //BMS Errors
  if (rxId == 0x35A){
    int dtc1_0 = rxBuf[0];
    int dtc1_1 = rxBuf[1];
    int dtc2_0 = rxBuf[2];
    int dtc2_1 = rxBuf[3];
    if (dtc1_0 > 0 || dtc1_1 > 0 || dtc2_0 > 0 || dtc2_1 > 0){errorBMS = 1;} else{errorBMS = 0;}
    canactive = 1;
  }

  //SOC
  if (rxId == 0x355){
    if (!usersetsoc){
      //SOC = rxBuf[1]/2;
      SOC = canbus.decode(rxBuf, len, 0, 16, "LSB", "UNSIGNED", 1, 0);
    }
    desiredposition = map(SOC, 0, 100, full_sweep_steps, 0);
    canactive = 1;
  }

  //BMS Limits
  if (rxId == 0x351){
    DCL = canbus.decode(rxBuf, len, 32, 16, "LSB", "UNSIGNED", 0.1, 0);
    throtmax = map(DCL, 0, 400, 30, 100);
    throtmax32 = throtmax * 32;
    DCL32 = (DCL) * 32;     //DEVIDE BY TWO TO CORRECT INNACURACY OF DU DC BUS MEASURE
  }

  //Batterystatus
  if (rxId == 0x356){    
    packCurrent = canbus.decode(rxBuf, len, 16, 16, "LSB", "SIGNED", 0.1, 0);
    //Devide by 10 and make number positive
    packCurrent = abs(packCurrent);
    //Map to have 0 amps at 4k rpm
    MotorSpeed = map(packCurrent, 0, 400, 0, 4000); 
    canactive = 1;
  }

  //Get highest of two temperatures
  if(InverterTemp > BatteryTemp){highestTemp = InverterTemp;}
  else {highestTemp = BatteryTemp;}
  
  //Convert to farenheit
  //highestTemp = (highestTemp*1.8) + 32;
  //Map to guage value
  //Serial.println(tempguageval);
  tempguageval = map(highestTemp, -5, 50, 0, 55);  
  
  if (debug == 1){
    if (candebug == 1){
      Serial.print(millis());
      if ((rxId & 0x80000000) == 0x80000000)    // Determine if ID is standard (11 bits) or extended (29 bits)
        sprintf(msgString, ",0x%.8lX,true,1, %1d", (rxId & 0x1FFFFFFF), len);
      else
        sprintf(msgString, ",0x%.3lX,false,1,%1d", rxId, len);

      Serial.print(msgString);

      if ((rxId & 0x40000000) == 0x40000000) {  // Determine if message is a remote request frame.
        sprintf(msgString, " REMOTE REQUEST FRAME");
        Serial.print(msgString);
      } else {
        for (byte i = 0; i < len; i++) {
          sprintf(msgString, ", %.2X", rxBuf[i]);
          Serial.print(msgString);
        }
      }
      Serial.println();
    }
  }
}

String toBinary(int input1, int length1){
    
    String tempString = String(input1, BIN);

    if(tempString.length() == length1){
      return tempString;
    } else {
      int paddingToAdd = length1-tempString.length();
      String zeros = "";
      for (int i = 1; i <= paddingToAdd; i++) {
        zeros = zeros + "0";
      }
      tempString = zeros + tempString;
    }
  
}

void cansend(){  
  uint32_t id = 0x289;
  mes[0] = 0x00;
  mes[1] = (2 * 1.655 * tempguageval) + 48;
  mes[2] = 0x00;
  mes[3] = 0x00;
  mes[4] = 0x00;
  mes[5] = 0x00;
  mes[6] = 0x00;
  mes[7] = 0x00;
  CAN0.sendMsgBuf(id, 0, 8, mes);
  delay(4);
  
  //if(!errorBMS){
    //Engine MIL Light
    id = 0x4E0;
    mes[0] = 0x00;
    mes[1] = 0x00;
    mes[2] = 0x00;
    mes[3] = 0x00;
    mes[4] = 0x00;
    mes[5] = 0x00;
    mes[6] = 0x00;
    mes[7] = 0x00;
    CAN0.sendMsgBuf(id, 0, 8, mes);
    delay(4);
  //}
  
  id = 0x280;
  mes[0] = 0x00;
  mes[1] = 0x00;
  mes[2] = lowByte(MotorSpeed *4); //engine rpm x4 lsb
  mes[3] = highByte(MotorSpeed *4); //engine rpm x4 msb
  mes[4] = 0x00;
  mes[5] = 0x00;
  mes[6] = 0x00;
  mes[7] = 0x00;
  CAN0.sendMsgBuf(id, 0, 8, mes);
}
