// Small EEPROM wrapper

#include "eeMem.h"
#include <EEPROM.h>

eeSet ee = { sizeof(eeSet), 0xAAAA,
  "",  // saved SSID (set these to bypass the SoftAP config)
  "", // router password
  "41042", // "KKYFLORE10"
  {false, false},  // Enable pushbullet
  true,   // Enable OLED
  false,
  "pushbullet token",
  "wunderground key",
  ""
};

eeMem::eeMem()
{
  EEPROM.begin(512);
  delay(10);

  uint8_t data[sizeof(eeSet)];
  eeSet *pEE = (eeSet *)data;

  int addr = 0;
  for(int i = 0; i < sizeof(eeSet); i++, addr++)
  {
    data[i] = EEPROM.read( addr );
  }

  if(pEE->size != sizeof(eeSet)) return; // revert to defaults if struct size changes
  uint16_t sum = pEE->sum;
  pEE->sum = 0;
  pEE->sum = Fletcher16(data, sizeof(eeSet) );
  if(pEE->sum != sum) return; // revert to defaults if sum fails
  memcpy(&ee, data, sizeof(eeSet) );
}

void eeMem::update() // write the settings if changed
{
  uint16_t old_sum = ee.sum;
  ee.sum = 0;
  ee.sum = Fletcher16((uint8_t*)&ee, sizeof(eeSet));

  if(old_sum == ee.sum)
  {
    return; // Nothing has changed?
  }

  uint16_t addr = 0;
  uint8_t *pData = (uint8_t *)&ee;
  for(int i = 0; i < sizeof(eeSet); i++, addr++)
  {
    EEPROM.write(addr, pData[i] );
  }
  EEPROM.commit();
}

uint16_t eeMem::Fletcher16( uint8_t* data, int count)
{
   uint16_t sum1 = 0;
   uint16_t sum2 = 0;

   for( int index = 0; index < count; ++index )
   {
      sum1 = (sum1 + data[index]) % 255;
      sum2 = (sum2 + sum1) % 255;
   }

   return (sum2 << 8) | sum1;
}
