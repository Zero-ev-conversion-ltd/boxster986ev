/*
  CANBUS Library, created by Seb Smith
*/

#ifndef CANBUS_h
#define CANBUS_h

#include "arduino.h"

class CANBUS
{
  public:
    double decode(unsigned char msg[], int meslength, int startBit, int bitLength, String byteOrder, String dataType, double Scale, double bias);
  private:
  	String reverseString(String inputString);
  	String toBinary(int input1, int length1);
};

#endif

