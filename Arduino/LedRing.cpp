#include "LedRing.h"
#include <TimeLib.h>
#include "eemem.h"

extern void WsPrint(String s);

void LedRing::init()
{
  LEDS.addLeds<WS2812B, LED_PIN, GRB>(m_leds, LED_CNT);
//  LEDS.setMaxPowerInVoltsAndMilliamps(uint8_t volts, uint32_t milliamps);
  LEDS.clear();
  m_Color[0].setRGB( 0, 0, 127);
  m_Color[1].setRGB(30,  0, 0);
  m_Color[2].setRGB(40, 40, 0);
}

void LedRing::setEffect(uint8_t ef)
{
  m_Effect = ef;
}

void LedRing::setIndicatorCount(uint8_t n)
{
  m_IndCnt = min(n, LED_CNT);
}

// 0 = off, 1 = basic blink, 2 = rotating counter
void LedRing::setIndicatorType(int n)
{
  m_IndType = n;
}

// Set colors used for effects (0-3, r, g, b)
void LedRing::Color(uint8_t n, uint8_t r, uint8_t g, uint8_t b)
{
  n &= 3;
  m_Color[n].setRGB(r, g, b);
}

// Set all pixel to given color
void LedRing::setColor(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness)
{
  m_brightness = brightness;
  LEDS.setBrightness(brightness);
  LEDS.showColor(CRGB(r, g, b));
  m_Effect = ef_Hold;
}

void LedRing::alert(uint16_t sec)
{
  LEDS.setBrightness(50);
  m_brightness = 50;
  if(sec)
  {
    m_oldEffect = m_Effect;
    m_Effect = ef_alert;
    m_alertTimer = sec;
  }
  else
  {
    m_Effect = m_oldEffect;    
  }
}

// White light at level (0 = off)
void LedRing::light(uint8_t level)
{
  if(level)
  {
    if(m_Effect != ef_Hold)
      m_lightRevert = m_Effect;
    LEDS.setBrightness(level);
    m_brightness = level;
    m_Effect = ef_Hold;
    colorWipe(CRGB(127, 127, 127), 10);
  }
  else
  {
    m_Effect = m_lightRevert;
  }
}

void LedRing::lightTimer(uint16_t sec)
{
  m_lightTimer = sec;
}

void LedRing::setBrightness(uint8_t n)
{
  LEDS.setBrightness(n);
  m_brightness = n;
  ee.bright[m_Effect] = n;
}

void LedRing::service()
{
  static uint32_t last;

  if(millis() - last < 18) // service no less than every 18ms
    return;
  last = millis();
  
  static uint16_t neoCnt = 0;
  static uint8_t lastMode = 0;

  neoCnt++;

  if(m_Effect != lastMode) // reset to 0 if effect changes
  {
    neoCnt = 0;
    lastMode = m_Effect;
    m_brightness = ee.bright[m_Effect];
    LEDS.setBrightness(m_brightness);
  }

  LEDS.clearData();

  if(m_Effect != ef_chaser && m_IndCnt) // composite effect and count blinker
    switch(m_IndType)
    {
      case 1:
        countblink(neoCnt, m_IndCnt);
        break;
      case 2:
        countspin(neoCnt, m_IndCnt);
        break;
    }

  static uint8_t s;
  if(s != second())
  {
    s = second();
    if(m_lightTimer)
      if(--m_lightTimer == 0)
        m_Effect = m_lightRevert;

    if(m_alertTimer)
      if(--m_alertTimer == 0)
        m_Effect = m_oldEffect;
  }

  switch(m_Effect)
  {
    case ef_Hold: // no update
      return;
    case ef_Off: // clear and stop updates
      m_Effect = ef_Hold;
      break;
    case ef_rainbowCycle:
      rainbowCycle();
      break;
    case ef_rainbow:
      rainbow(neoCnt);
      break;
    case ef_alert:
      if(neoCnt & 2)
        colorWipe(m_Color[1], 3);
      else
        colorWipe(CRGB(0, 0, 0), 3);
      break;
    case ef_glow:
      glow(neoCnt);
      break;
    case ef_chaser:
      chaser(neoCnt, m_IndCnt);
      if(neoCnt >= LED_CNT )
        neoCnt = 0;
      break;
    case ef_pendulum:
      pendulum();
      break;
    case ef_orbit:
      orbit(neoCnt);
      break;
    case ef_clock:
      pendulum();
      clock(m_Color[1], m_Color[2], neoCnt);
      break;
    case ef_spiral:
      spiral(neoCnt);
      break;
    case ef_doubleSpiral:
      doubleSpiral(neoCnt);
      break;
    case ef_wings:
      wings(neoCnt);
      break;
  }
  FastLED.show();
  if(m_effectTimer) // automatically revert to old effect
  {
    if(neoCnt > m_effectTimer)
    {
      m_effectTimer = 0;
      m_Effect = m_oldEffect;
    }
  }
}

// glowy thing
void LedRing::glow(uint16_t j)
{
  int n = (j<<2) & 0xFF;

  if( n > 127) n = 255 - n;

  for(int i = 0; i < LED_CNT; i++)
  {
    m_leds[i] = m_Color[0];
    m_leds[i].nscale8(n);
  }
}

// count + spinner
void LedRing::chaser(uint16_t n, uint8_t cnt)
{
  n %= LED_CNT;

  static uint8_t b;
  static uint8_t d;

  if(cnt)
  {
    for(int i = 0; i < cnt; i++)
    {
      uint8_t x = (b+LED_CNT-i) % LED_CNT;
      setPixel(x, m_Color[1]);
    }
  }
  setPixel(n, m_Color[0]);
  if(++d > 1)
  {
    d = 0;
    b++;
    b %= LED_CNT;
  }
}

// basic flashing counter
void LedRing::countblink(uint16_t n, uint8_t cnt)
{
  static uint8_t d;

  for(int i = 0; i < LED_CNT; i++)
  {
    if(i < cnt && (n & 16))
      setPixel(i, m_Color[1]);
  }
}

// show count while spinning
void LedRing::countspin(uint16_t n, uint8_t cnt)
{
  n >>= 2;

  if(cnt)
  {
    for(int i = 0; i < cnt; i++)
    {
      uint8_t x = (n+LED_CNT-i) % LED_CNT;
      setPixel(x, m_Color[1]);
    }
  }
}

void LedRing::pendulum()
{
  static int8_t dir = 1;
  static int8_t mdir = 1;
  static uint8_t dly = 1;
  static int8_t m = 2;
  static uint8_t pt = LED_CNT;
#define TRAIL_CNT 5
  static CRGB last[TRAIL_CNT];
  static uint8_t trail[TRAIL_CNT];
  static uint8_t lastPos;

  uint8_t realpt = pt % LED_CNT;
  setPixel(realpt, m_Color[0]);
  for(int i = 0; i < TRAIL_CNT; i++)
    setPixel(trail[i], last[i]);
  if(lastPos != realpt)
  {
    lastPos = realpt;
    for(int i = TRAIL_CNT-1; i > 0; i--)
    {
      trail[i] = trail[i-1];
      last[i] = last[i-1];
      last[i].nscale8(180);
    }
    trail[0] = realpt;
    last[0] = m_Color[0];
    last[0].nscale8(180);
  }

  if(--dly == 0)
  {
    pt += dir;
    dly = (m >> 1) | 1;
  }

  m += mdir;
  if( m < 1 || m > 18)
  {
    if( m > 18) // change direction at slow point
      dir = -dir;
    mdir = -mdir; // reverse momentum at fastest or slowest
  }
}

// 2 different speeds and directions, rainbow
void LedRing::orbit(uint16_t n)
{
  uint8_t e = (n / 2) % LED_CNT;
  uint8_t d = (LED_CNT - (n % LED_CNT) );

  for(int i = 0; i < LED_CNT; i++)
  {
    if(i == e)
      setPixel(i, Wheel((byte)n) );
    else if(i == d )
      setPixel(i,  Wheel((byte)n/2) );
  }
}

void LedRing::clock(CRGB c1, CRGB c2, uint16_t n)
{
  static uint8_t lastsec;
  static uint8_t cnt;

  uint8_t h = ((hour()<<1)+12) % LED_CNT;
  uint8_t m = ((minute()+30) * LED_CNT /60) % LED_CNT;
  uint8_t s = ((second()+30) * LED_CNT / 60) % LED_CNT;

  if(lastsec != s)
  {
    lastsec = s;
    cnt = 0;
  }

  for(int i = 0; i < LED_CNT; i++)
  {
    if(i == s)
    { // Cheap transition
      setPixel(i, CRGB(cnt, cnt, cnt) );
      CRGB c3 = CRGB(127-cnt, 127-cnt, 127-cnt);
      setPixel((i+LED_CNT-1) % LED_CNT, c3 );
    }
    if(i == h)
      setPixel(i, c1);
    if(i == m)
      setPixel(i, c2);
  }
  if(cnt < 125)
    cnt++;
}

// From FastLED
void LedRing::addGlitter( fract8 chanceOfGlitter) 
{
  if( random8() < chanceOfGlitter) {
    m_leds[ random16(LED_CNT) ] += CRGB::White;
  }
}

// fading trail
void LedRing::spiral(uint16_t n)
{
  int8_t e = n % LED_CNT;
  CRGB c = m_Color[0];

  for(int i = 0; i < LED_CNT; i++)
  {
    setPixel(e, c);
    if( --e < 0) e += LED_CNT;
    c.nscale8(200);
  }
}

// reverse trailing opposites
void LedRing::doubleSpiral(uint16_t n)
{
  int8_t e = LED_CNT - (n>>1) % LED_CNT;
  CRGB c[2];

  c[0] = m_Color[0];
  c[1] = m_Color[1];

  for(int i = 0; i < LED_CNT; i++)
  {
    setPixel(e+i, c[0]);
    setPixel(e+i + (LED_CNT/2), c[1]);
    c[0].nscale8(200);
    c[1].nscale8(200);
  }
}

void LedRing::wings(uint16_t n)
{
  static bool bDir;
  static int16_t fader = 0;
  static int8_t pt = 0;

  CRGB c = m_Color[1];

  int i;
  for(i = 0; i < pt; i++)
  {
    m_leds[((LED_CNT / 2) + i) % LED_CNT] = c;
    m_leds[((LED_CNT / 2) - i) % LED_CNT] = c;
  }
  if(bDir)
  {
    fader += 10;
    if(fader > 90)
    {
      if(pt < LED_CNT/2)
      {
        pt++;
        fader = 0;
      }
      else
      {
        bDir = false;
      }
    }
  }
  else
  {
    fader -= 10;
    if(fader <= 10)
    {
      if(pt > 1)
      {
        pt--;
        fader = 90;
      }
      else
        bDir = true;
    }
  }
  c.nscale8(fader);
  m_leds[((LED_CNT / 2) + i) % LED_CNT] = c;
  m_leds[((LED_CNT / 2) - i) % LED_CNT] = c;
}

void LedRing::setPixel(uint8_t i, CRGB c)
{
  m_leds[i % LED_CNT] |= c;
}

// Fill the dots one after the other with a color
void LedRing::colorWipe(CRGB c, uint8_t wait)
{
  for(uint16_t i=0; i<LED_CNT; i++) {
    setPixel(i, c);
    FastLED.show();
    delay(wait);
  }
}

void LedRing::rainbow(uint16_t j)
{
  for(int i = 0; i < LED_CNT; i++)
    setPixel(i, Wheel((i+j) & 255));
}

// Slightly different, this makes the rainbow equally distributed throughout
void LedRing::rainbowCycle()
{
  static uint8_t j = 0;
  uint16_t i;

  for(i=0; i< LED_CNT; i++) {
    setPixel(i, Wheel(((i * 256 / LED_CNT) + j) & 255));
  }
  j++;
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
CRGB LedRing::Wheel(byte WheelPos)
{
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return CRGB(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return CRGB(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return CRGB(WheelPos * 3, 255 - WheelPos * 3, 0);
}
