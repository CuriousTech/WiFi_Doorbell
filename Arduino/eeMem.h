#ifndef EEMEM_H
#define EEMEM_H

#include <Arduino.h>

#define EESIZE (offsetof(eeMem, end) - offsetof(eeMem, size) )

class eeMem
{
public:
  eeMem();
  void update(void);
private:
  uint16_t Fletcher16( uint8_t* data, int count);
public:
  uint16_t size = EESIZE;          // if size changes, use defauls
  uint16_t sum = 0xAAAA;           // if sum is diiferent from memory struct, write
  char    szSSID[32] = "";
  char    szSSIDPassword[64] = "";
  int8_t  tz = -5;
  char    location[32] =  "4291945";   // location for weathermap
  bool    bEnablePB[2] = {false, false};   // enable pushbullet for doorbell, motion
  bool    bEnableOLED = true;
  bool    bMotion = false;        // motion activated clear
  char    pbToken[40] = "pushbullet token";    // 34
  char    owKey[36] = "openweather key";      // 32
  char    szNotifIP[16] = "192.168.31.158"; // Another device that can make sounds or text
  char    szNotifPath[44] = "/s?key=esp8266ct&music=0";
  uint16_t NotifPort = 80;
  uint8_t  hostIP[4] = {192,168,31,100};
  uint16_t hostPort = 80;
  uint8_t  melody = 0;
  uint8_t  effect = 0;
  uint8_t  bright[24] = {40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40};
  uint8_t  res[14] = {0};
  uint8_t end;
};

extern eeMem ee;

#endif // EEMEM_H
