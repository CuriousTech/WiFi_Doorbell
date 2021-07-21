#ifndef LEDRING_H
#define LEDRING_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif

#define LED_CNT 24

typedef union _rgb{
  uint32_t c;
  struct{
    uint32_t b:8;
    uint32_t g:8;
    uint32_t r:8;
    uint32_t w:8;
  };
};

enum neoEffect
{
  ef_none,          // only indicator
  ef_Hold,          // no change
  ef_Off,           // turn off
  ef_rainbowCycle,  // inifinite
  ef_rainbow,       // inifinite
  ef_theaterChase,  // theater chase Color1 for 10 cycles, then off
  ef_theaterChaseRainbow, // 255 cycles
  ef_alert,         // fast color wipe Color2 50 cycles
  ef_glow,          // glow Color1
  ef_chaser,        // single Color1, Indicator count for Color2
  ef_pendulum,      // led swing
  ef_orbit,
  ef_clock,
  ef_spiral,
  ef_doubleSpiral,
  ef_wings,
  ef_END
};

class LedRing
{
public:
  LedRing(){};
  void init(int pin);
  void service(void);
  void setEffect(uint8_t ef); // set an effect
  void setColor(uint8_t r, uint8_t g, uint8_t b); // set color directly
  void light(uint8_t level); // set to white and return to last effect when set to 0
  void alert(bool bOn);
  void setIndicatorCount(uint8_t n); // number of LEDs for blink and chaser effects
  void setIndicatorType(int n);
  void Color(uint8_t n, uint8_t r, uint8_t g, uint8_t b);

private:
  void glow(uint16_t j);
  void chaser(uint16_t n, uint8_t cnt);
  void countblink(uint16_t n, uint8_t cnt);
  void countspin(uint16_t n, uint8_t cnt);
  void pendulum(void);
  void orbit(uint16_t n);
  void clock(uint32_t c1, uint32_t c2, uint16_t n);
  void spiral(uint16_t n);
  void doubleSpiral(uint16_t n);
  void wings(uint16_t n);
  void fade(_rgb &c, uint8_t perc);

  // AdaFruit examples
  void colorWipe(uint32_t c, uint8_t wait);
  void rainbow(uint16_t j);
  void rainbowCycle();
  void theaterChase(uint32_t c, uint8_t wait, uint16_t j);
  void theaterChaseRainbow(uint8_t wait, uint16_t j);
  uint32_t Wheel(byte WheelPos);

  void clear(void);
  void setPixel(uint8_t i, uint32_t c); // composite set color
  void show(void);

  Adafruit_NeoPixel strip;
  uint8_t m_Effect;
  uint8_t m_IndCnt;
  uint8_t m_IndType;
  uint8_t m_oldEffect = ef_Off;
  uint32_t m_Color[4];
  uint32_t m_leds[LED_CNT];
  uint32_t m_effectTimer;
};

#define min(a,b) ((a) < (b) ? (a) : (b))
#define max(a,b) ((a) > (b) ? (a) : (b))

#endif // LEDRING_H
