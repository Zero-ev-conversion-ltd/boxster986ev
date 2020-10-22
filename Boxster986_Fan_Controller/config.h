#pragma once

#include <Arduino.h>

///Pinouts///
#define Fan1     2
#define Fan2     3

#define EEPROM_PAGE         0

typedef struct {
  uint8_t version;
  uint8_t Temp1;       //temperature fan stage 1 comes on
  uint8_t Temp2;      //temperature fan stage 2 comes on
  uint8_t TempHys;      //temperature hysterisis
}EEPROMSettings;
