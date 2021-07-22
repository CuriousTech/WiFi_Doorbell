#ifndef LEDRING_H
#define LEDRING_H

#include <FastLED.h> // https://github.com/FastLED/FastLED

#define LED_CNT 24
#define LED_PIN 5

enum neoEffect
{
  ef_none,          // only indicator
  ef_Hold,          // no change
  ef_Off,           // turn off
  ef_rainbowCycle,  // inifinite
  ef_rainbow,       // inifinite
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
  void init(void);
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
  void clock(CRGB c1, CRGB c2, uint16_t n);
  void spiral(uint16_t n);
  void doubleSpiral(uint16_t n);
  void wings(uint16_t n);

  // AdaFruit examples
  void colorWipe(CRGB c, uint8_t wait);
  void rainbow(uint16_t j);
  void rainbowCycle();
  CRGB Wheel(byte WheelPos);

  void setPixel(uint8_t i, CRGB c); // composite set color

  uint8_t m_Effect;
  uint8_t m_IndCnt;
  uint8_t m_IndType;
  uint8_t m_oldEffect = ef_Off;
  CRGB m_Color[4];
  CRGB m_leds[LED_CNT];

  uint32_t m_effectTimer;
};

#define min(a,b) ((a) < (b) ? (a) : (b))
#define max(a,b) ((a) > (b) ? (a) : (b))

#endif // LEDRING_H
