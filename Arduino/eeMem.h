#ifndef EEMEM_H
#define EEMEM_H

#include <Arduino.h>

struct eeSet // EEPROM backed data
{
  uint16_t size;          // if size changes, use defauls
  uint16_t sum;           // if sum is diiferent from memory struct, write
  char    szSSID[32];
  char    szSSIDPassword[64];
  int8_t  tz;
  char    location[32];   // location for weathermap
  bool    bEnablePB[2];   // enable pushbullet for doorbell, motion
  bool    bEnableOLED;
  bool    bMotion;        // motion activated clear
  char    pbToken[40];    // 34
  char    owKey[36];      // 32
  char    szNotifIP[16];
  char    szNotifPath[44];
  uint16_t NotifPort;
};

class eeMem
{
public:
  eeMem();
  void update(void);
private:
  uint16_t Fletcher16( uint8_t* data, int count);
};

extern eeSet ee;
extern eeMem eemem;

#endif // EEMEM_H
