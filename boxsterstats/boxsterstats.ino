#include <FlexCAN.h>
#include <evilOLED.h>
#include <CANBUS.h>      //https://github.com/seb43654/canbus-interpreter
CANBUS canbus;           //Initiate class for canbus-interpreter

int sda = 18;
int scl = 19;
evilOLED disp(sda, scl);

long unsigned int rxId;
unsigned char len = 0;
byte rxBuf[8];
char msgString[128];       // Array to store serial string
int canreceivedebug = 0;
int cansentdebug = 0;

int batterytemp = 0;
int motortemp = 0;
int soc = 0;
int lowcell = 0;
int highcell = 0;
int dcl = 0;
int ccl = 0;
int current = 0;

CAN_message_t msg;
CAN_message_t inMsg;

void setup() {

  pinMode(sda, OUTPUT);
  pinMode(scl, OUTPUT);
  evilOLED disp(sda, scl);
  delay(100);
  disp.cls(0x00);
  disp.setCursor(0, 0); // sets text cursor (x,y)
  disp.putString("INIT..");
  delay(100);
  
  Can0.begin(500000);
  CAN_filter_t allPassFilter;
  allPassFilter.id = 0;
  allPassFilter.ext = 1;
  allPassFilter.rtr = 0;

  //Enables extended addresses
  for (int filterNum = 4; filterNum < 16; filterNum++) {Can0.setFilter(allPassFilter, filterNum);}

  Serial.begin(115200);
  Serial.println("Starting up!");

}

void loop() {
  while (Can0.available()){ canread();}
  updateDisplay();
}

void canread(){
  Can0.read(inMsg);

  switch (inMsg.id){

    //Batterystatus
    case 0x356:
        batterytemp = canbus.decode(inMsg, 32, 16, "LSB", "SIGNED", 0.1, 0);
        current = canbus.decode(inMsg, 16, 16, "LSB", "SIGNED", 0.1, 0);  
      break;

    //Tesla drive unit
    case 0x126:
       motortemp = inMsg.buf[0];
       
      break; 

    //BmsSOC
    case 0x355:
       soc = canbus.decode(inMsg, 0, 16, "LSB", "UNSIGNED", 1, 0);
      break;
    
    //BMSLimits
    case 0x351:     
      dcl = canbus.decode(inMsg, 32, 16, "LSB", "UNSIGNED", 0.1, 0);
      ccl = canbus.decode(inMsg, 16, 16, "LSB", "UNSIGNED", 0.1, 0);
      break;

    //Cell voltages
    case 0x35C:
        String tempbyte02 = toBinary(inMsg.buf[0], 8);
        String tempbyte12 = toBinary(inMsg.buf[1], 8);
        String tempbyte22 = toBinary(inMsg.buf[2], 8);
        String tempbyte32 = toBinary(inMsg.buf[3], 8);
          
        String tempbyte01highcell = tempbyte02 + tempbyte12;
        String tempbyte23lowcell = tempbyte22 + tempbyte32;
        
        char tempbyte01highcellarray[17];
        char tempbyte23lowcellarray[17];
    
        tempbyte01highcell.toCharArray(tempbyte01highcellarray, 17);
        tempbyte23lowcell.toCharArray(tempbyte23lowcellarray, 17);
        
        lowcell = strtol(tempbyte01highcellarray, NULL, 2);
        highcell = strtol(tempbyte23lowcellarray, NULL, 2);
      break;
    
    default:
      break;
  }

  if (canreceivedebug == 1) {
    if ((inMsg.id & 0x10000000) == 0x10000000) {    // Determine if ID is standard (11 bits) or extended (29 bits)
      sprintf(msgString, "Extended ID: 0x%.3lX, DLC: %1d", inMsg.id, inMsg.len);
    }
    else{
      sprintf(msgString, "Standard ID: 0x%.3lX, DLC: %1d", inMsg.id, inMsg.len);
    }
    Serial.print(msgString);
      if ((inMsg.id & 0x40000000) == 0x40000000) {  // Determine if message is a remote request frame.
        sprintf(msgString, " REMOTE REQUEST FRAME");
        Serial.print(msgString);
      } else {
        for (byte i = 0; i < inMsg.len; i++) {
          sprintf(msgString, ", 0x%.2X", inMsg.buf[i]);
          Serial.print(msgString);
        }
      }
    Serial.println();
  }
}

void SendCan(CAN_message_t msg){
  if (cansentdebug == 1){
    sprintf(msgString, "Sent ID: 0x%.3lX, Data: 0x%.3lX, 0x%.3lX, 0x%.3lX, 0x%.3lX, 0x%.3lX, 0x%.3lX, 0x%.3lX, 0x%.3lX", msg.id, msg.buf[0],msg.buf[1],msg.buf[2],msg.buf[3],msg.buf[4],msg.buf[5],msg.buf[6],msg.buf[7]);
    Serial.println(msgString);
  }
  Can0.write(msg);
}

void intepretCurrentLimits(CAN_message_t inMsg){
        String tempbyte21 = toBinary(inMsg.buf[2], 8);
        String tempbyte31 = toBinary(inMsg.buf[3], 8);
        String tempbyte41 = toBinary(inMsg.buf[4], 8);
        String tempbyte51 = toBinary(inMsg.buf[5], 8);
          
        String tempbyte23chargeCurrentLimit = tempbyte21 + tempbyte31;
        String tempbyte45dischargeCurrentLimit = tempbyte41 + tempbyte51;
        
        char tempbyte23chargeCurrentLimitarray[17];
        char tempbyte45dischargeCurrentLimitarray[17];
    
        tempbyte23chargeCurrentLimit.toCharArray(tempbyte23chargeCurrentLimitarray, 17);
        tempbyte45dischargeCurrentLimit.toCharArray(tempbyte45dischargeCurrentLimitarray, 17);
        
        ccl = strtol(tempbyte23chargeCurrentLimitarray, NULL, 2);
        dcl = strtol(tempbyte45dischargeCurrentLimitarray, NULL, 2);
}

void interpretCurrent(CAN_message_t inMsg){
        String tempbyte2 = toBinary(inMsg.buf[2], 8);
        String tempbyte3 = toBinary(inMsg.buf[3], 8);
        String tempbyte23packCurrent = tempbyte2 + tempbyte3;
        char tempbyte23packCurrentarray[17];
        tempbyte23packCurrent.toCharArray(tempbyte23packCurrentarray,17);
        current = strtol(tempbyte23packCurrentarray, NULL, 2);
        if(current > 32767){current = current - 65535;};
        current = current/10;
}

void updateDisplay(){

  String highcell12 = String(highcell);
  String lowcell12 = String(lowcell);
  
  
  //Serial.println(highcell12);
  String high1 = highcell12.substring(0,1);
  String high2 = highcell12.substring(1,5);

  String low1 = lowcell12.substring(0,1);
  String low2 = lowcell12.substring(1,5);
  
  //Serial.println(high1);
  //Serial.println(high2);

  int high1int = high1.toInt();
  int high2int = high2.toInt();
  int low1int = low1.toInt();
  int low2int = low2.toInt();
  
  disp.setCursor(0, 0);
  disp.putString("BAT TEMP:");
  disp.setCursor(9, 0);
  disp.putString(batterytemp);
  disp.setCursor(0, 1);
  disp.putString("MTR TEMP:");
  disp.setCursor(9, 1);
  disp.putString(motortemp);
  disp.setCursor(0, 2);
  disp.putString("LOW CELL:");
  disp.setCursor(9, 2);
  disp.putString(low1int);
  disp.setCursor(10, 2);
  disp.putString(">");
  disp.setCursor(11, 2);
  disp.putString("    ");
  disp.setCursor(11, 2);
  disp.putString(low2int);
  disp.setCursor(0, 3);
  disp.putString(" HI CELL:");
  disp.setCursor(9, 3);
  disp.putString(high1int);
  disp.setCursor(10, 3);
  disp.putString(">");
  disp.setCursor(11, 3);
  disp.putString("    ");
  disp.setCursor(11, 3);
  disp.putString(high2int);
  disp.setCursor(0, 4);
  disp.putString("     SOC:");
  disp.setCursor(9, 4);
  disp.putString("     ");
  disp.setCursor(9, 4);
  disp.putString(soc);
  disp.setCursor(0, 5);
  disp.putString("     CCL:");
  disp.setCursor(9, 5);
  disp.putString("     ");
  disp.setCursor(9, 5);
  disp.putString(ccl);
  disp.setCursor(0, 6);
  disp.putString("     DCL:");
  disp.setCursor(9, 6);
  disp.putString("     ");
  disp.setCursor(9, 6);
  disp.putString(dcl);
  disp.setCursor(0, 7);
  disp.putString(" CURRENT:");
  disp.setCursor(9, 7);
  disp.putString("     ");
  disp.setCursor(9, 7);
  disp.putString(current);
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
