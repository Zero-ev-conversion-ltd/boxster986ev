/*
  CANBUS Library, created by Seb Smith
*/

#include "arduino.h"
#include "CANBUS.h"

double CANBUS::decode(unsigned char msg[],int meslength, int startBit, int bitLength, String byteOrder, String dataType, double Scale, double bias){

    //used to be: rxBufD
    //now: msg.buf

    //used to be: lengthD
    //now: msg.len

    //////////////////////////////////////////////////////////////////////////////
    //Step 1 - Gets all received data and converts to super long string
    //////////////////////////////////////////////////////////////////////////////
    String DataBinaryString;
    for(int x=0; x<meslength; x++){    
      String tempdata = toBinary(msg[x], 8); 
      if(byteOrder == "LSB" || byteOrder == "lsb" || byteOrder == "intel" || byteOrder == "INTEL"){
        tempdata = reverseString(tempdata);                                                               //For LSB byte order flip each byte around first
      }
      DataBinaryString = DataBinaryString + tempdata;                                                     //Merge into mega long string
    } 
    //Serial.print("Raw Binary Data input: ");
    //Serial.println(DataBinaryString);


    //////////////////////////////////////////////////////////////////////////////
    //Step 2 - Calculate section of string to select
    //////////////////////////////////////////////////////////////////////////////
    if(byteOrder == "MSB" || byteOrder == "msb" || byteOrder == "motorola" || byteOrder == "MOTOROLA"){
        int startbitcalc = 0;
        if(startBit < 8 && startBit > -1) {startbitcalc = (7 - startBit);}
        if(startBit < 16 && startBit > 7) {startbitcalc = (15 - startBit) + 8;}
        if(startBit < 24 && startBit > 15){startbitcalc = (23 - startBit) + 16;}
        if(startBit < 32 && startBit > 23){startbitcalc = (31 - startBit) + 24;}
        if(startBit < 40 && startBit > 31){startbitcalc = (39 - startBit) + 32;}
        if(startBit < 48 && startBit > 39){startbitcalc = (47 - startBit) + 40;}
        if(startBit < 56 && startBit > 47){startbitcalc = (55 - startBit) + 48;}
        if(startBit < 64 && startBit > 55){startbitcalc = (63 - startBit) + 56;}
        //Serial.print("startbitcalc: ");
        //Serial.println(startbitcalc);
        //Serial.print("endbit: ");
        //Serial.println((startbitcalc)+bitLength);     
        DataBinaryString = DataBinaryString.substring(startbitcalc, ((startbitcalc)+bitLength));
    } else {DataBinaryString = DataBinaryString.substring(startBit, (startBit+bitLength));}
    //Serial.print("Extracted data portion: ");
    //Serial.println(DataBinaryString);


    //////////////////////////////////////////////////////////////////////////////
    //Step 3 - Byte order correction
    //////////////////////////////////////////////////////////////////////////////
    if(byteOrder == "LSB" || byteOrder == "lsb" || byteOrder == "intel" || byteOrder == "INTEL"){
      DataBinaryString = reverseString(DataBinaryString);  
    }
    //Serial.print("After byte order correction: ");
    //Serial.println(DataBinaryString); 


    //////////////////////////////////////////////////////////////////////////////
    //Step 4 - Convert to integer
    //////////////////////////////////////////////////////////////////////////////
    int maxTheoraticalValue = (pow(2,(DataBinaryString.length()))-1);                                    //Minus one to account for zero
    //Serial.print("max theoretical value:");
    //Serial.println(maxTheoraticalValue);
    char tempholder[64];                                                                                 //char array with max size of data
    DataBinaryString.toCharArray(tempholder,64);                                                         //string to char array
    int dataInteger = strtol(tempholder, NULL, 2);                                                       //Convert from binary (base 2) to decimal
    //Serial.print("Raw integer: ");
    //Serial.println(dataInteger);
    
    
    //////////////////////////////////////////////////////////////////////////////
    //Step 4 - Account for signed values
    //////////////////////////////////////////////////////////////////////////////
    if(dataType == "SIGNED" || dataType == "signed"){
      if(dataInteger > (maxTheoraticalValue/2)){dataInteger = dataInteger - (maxTheoraticalValue+1);}    //Convert to signed value
    }
    //Serial.print("After being signed: ");
    //Serial.println(DataBinaryString);


    //////////////////////////////////////////////////////////////////////////////
    //Step 5 - Scale and bias
    //////////////////////////////////////////////////////////////////////////////
    //Referenced from: https://www.csselectronics.com/screen/page/can-dbc-file-database-intro/language/en
    double returnData = bias + (Scale*dataInteger);   
    
    return(returnData); 
}

String CANBUS::reverseString(String inputString){
      //Simply reverses string (used for MSB > LSB conversion)
      String returnString;
      for(int x=inputString.length(); x>0; x--){returnString = returnString + inputString.substring(x, (x-1));}
      return(returnString);  
}

String CANBUS::toBinary(int input1, int length1){ 
    //Function that returns a string of the input binary,and pads out with zeros to match the length requested
    String tempString = String(input1, BIN);
    if(tempString.length() == length1){return tempString;} else {
      int paddingToAdd = length1-tempString.length();
      String zeros = "";
      for (int i = 1; i <= paddingToAdd; i++) {zeros = zeros + "0";}
      tempString = zeros + tempString;
    }  
}