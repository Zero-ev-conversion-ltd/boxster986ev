#include <FlexCAN.h>
#include "canmessage.h"
CAN_message_t outMsg;
CAN_message_t inMsg;
#define LSB 0
#define MSB 1
#define UNSIGNED 0
#define SIGNED 1

//Vital statistics
int16_t InverterTemp = 0;
int16_t MotorTemp = 0;
int16_t BatteryTemp = 0;
int16_t MotorSpeed = 0;
int packCurrent = 0;
int packVoltage = 0;
int tempguageval = 0;

//Fuel Guage
unsigned long gaugeresettime = 0;
int fuel_direction_pin = 15;
int fuel_fuel_step_pin = 13;
int full_sweep_steps = 385;
int desiredposition = 0;
int actualposition = 0;
int SOC = 0;

//Chevy DCDC converter
const int dcdcTargetVoltage = 14;

//Cyclic Timing Variables
elapsedMillis ms500Timer;
elapsedMillis min3Timer;
elapsedMillis ms10Timer;
const int ms10 = 10;
const int ms500 = 500;
const int min3 = 180000;

void setup() {
  Can0.begin(500000);
  CAN_filter_t allPassFilter;
  allPassFilter.id = 0;
  allPassFilter.ext = 1;
  allPassFilter.rtr = 0;
  for (int filterNum = 4; filterNum < 16; filterNum++) {Can0.setFilter(allPassFilter, filterNum);}

  Serial.begin(115200);
  Serial.println("Starting up!");

  pinMode(fuel_direction_pin, OUTPUT);
  pinMode(fuel_fuel_step_pin, OUTPUT);

  //Ram needle against endstop to calibrate
  digitalWrite(fuel_direction_pin, HIGH);
  for (int i = 0; i <= full_sweep_steps*3; i++) {
    digitalWrite(fuel_fuel_step_pin, HIGH);
    delay(1);
    digitalWrite(fuel_fuel_step_pin, LOW);
  }
}

void loop() {
  while (Can0.available()){canread();}
  if(ms10Timer >= ms10){                  //Every 10ms
    ms10Timer = ms10Timer - ms10;
    sendRPMgauge();
    sendTempgauge();
    sendMILlight();
  }
  if(ms500Timer >= ms500){                //Every 500ms
    ms500Timer = ms500Timer - ms500;
    movefuelguageneedle();
    sendDCDCcommand();
    printStats();
  }
  if(min3Timer >= min3){                 //Every 3 minutes
    //Ram gauge against endstop every 
    //3 minutes as holding force is low
    min3Timer = min3Timer - min3;
    gaugeReset();
  }
}

void sendRPMgauge(){
  outMsg.id  = 0x280;
  outMsg.len = 8;
  outMsg.ext = 0;
  for (int i = 0; i < outMsg.len; i++) {outMsg.buf[i] = 0x00;}
  outMsg.buf[2] = lowByte(MotorSpeed *4);
  outMsg.buf[3] = highByte(MotorSpeed *4);
  Can0.write(outMsg);
}

void sendTempgauge(){
  outMsg.id  = 0x289;
  outMsg.len = 8;
  outMsg.ext = 0;
  for (int i = 0; i < outMsg.len; i++) {outMsg.buf[i] = 0x00;}
  outMsg.buf[1] = (2 * 1.655 * tempguageval) + 48;
  Can0.write(outMsg);
}

void sendMILlight(){
  //Stop sending this frame to show engine light
  outMsg.id  = 0x4E0;
  outMsg.len = 8;
  outMsg.ext = 0;
  for (int i = 0; i < outMsg.len; i++) {outMsg.buf[i] = 0x00;}
  Can0.write(outMsg);
}

void sendDCDCcommand(){
  outMsg.id  = 0x1D4;
  outMsg.len = 2;
  outMsg.ext = 0;
  for (int i = 0; i < outMsg.len; i++) {outMsg.buf[i] = 0x00;}
  outMsg.buf[0] = 0xA0;
  outMsg.buf[1] = dcdcTargetVoltage * 1.27;
  Can0.write(outMsg);
}

void canread(){
  Can0.read(inMsg);
  switch (inMsg.id){
    case 0x126:
      InverterTemp = CAN_decode(&inMsg, 32, 16, LSB, SIGNED, 1, 0);
      tempguageval = map(InverterTemp, -5, 50, 0, 55);                //Map temperature gauge
      MotorTemp = CAN_decode(&inMsg, 48, 16, LSB, SIGNED, 1, 0);      //Motor temp current not used as inverter hotter
    break;
    case 0x125:
      packVoltage = CAN_decode(&inMsg, 16, 16, LSB, SIGNED, 0.1, 0);
      packCurrent = CAN_decode(&inMsg, 0, 16, LSB, SIGNED, 0.1, 0);
      packCurrent = abs(packCurrent);                                 //Make current always positive
      packCurrent = packCurrent * 1.6;                                //Correction for tesla du DC current
      SOC = map(packVoltage, 300, 395, 0, 100);                       //Rudimentary map of SOC to voltage
      SOC = constrain(SOC, 0, 100);                                   //Prevent out of bounds
      desiredposition = map(SOC, 0, 100, full_sweep_steps, 0);        //Update needles desired stepper position
      MotorSpeed = map(packCurrent, 0, 400, 0, 4000);                 //Map current to motorspeed aka gauge revs
    break;
    default:
      break;
  }
}

void printStats(){
  Serial.println();
  Serial.print("Inverter Temp : ");
  Serial.print(InverterTemp);
  Serial.println("˚C");
  Serial.print("Motor Temp : ");
  Serial.print(MotorTemp);
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
  Serial.print("Pack Voltage: ");
  Serial.print(packVoltage);
  Serial.println("V");
  Serial.print("Battery SOC : ");
  Serial.print(SOC);
  Serial.println("%");
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
