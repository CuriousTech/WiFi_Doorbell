#include "LedRing.h"
#include <TimeLib.h>

void LedRing::init(int pin)
{
  strip = Adafruit_NeoPixel(LED_CNT, pin, NEO_GRB + NEO_KHZ800);
  strip.begin();
  strip.setBrightness(40);
  strip.show(); // Initialize all pixels to 'off'
  clear();
  m_Color[0] = strip.Color( 0, 0, 127);
  m_Color[1] = strip.Color(30,  0, 0);
  m_Color[2] = strip.Color(40, 40, 0);
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
  m_Color[n] = strip.Color(r, g, b);
}

// Set all pixel to given color
void LedRing::setColor(uint8_t r, uint8_t g, uint8_t b)
{
  strip.fill(strip.Color(r, g, b), 0, LED_CNT);
  m_Effect = ef_Hold;
}

void LedRing::alert(bool bOn)
{
  if(bOn)
  {
    m_oldEffect = m_Effect;
    m_Effect = ef_alert;
  }
  else
  {
    m_Effect = m_oldEffect;    
  }
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
  }

  clear();
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
    case ef_theaterChase:
      theaterChase(m_Color[0], 40, neoCnt);
      break;
    case ef_theaterChaseRainbow:
      theaterChaseRainbow(30, neoCnt);
      break;
    case ef_alert:
      if(neoCnt & 1)
        colorWipe(m_Color[1], 1);
      else
        colorWipe(strip.Color(0, 0, 0), 1);
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
  show();
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
  j <<= 2;
  j &= 0xFF;
  if(j > 127) j = 255-j;
  uint32_t m = j | (j<<8) | (j<<16);
  for(int i = 0; i < LED_CNT; i++)
    m_leds[i] = strip.gamma32(m_Color[0] & m);
}

// cnout + spinner
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
  static _rgb last[TRAIL_CNT];
  static uint8_t trail[TRAIL_CNT];
  static uint8_t lastPos;

  uint8_t realpt = pt % LED_CNT;
  setPixel(realpt, m_Color[0]);
  for(int i = 0; i < TRAIL_CNT; i++)
    setPixel(trail[i], last[i].c);
  if(lastPos != realpt)
  {
    lastPos = realpt;
    for(int i = TRAIL_CNT-1; i > 0; i--)
    {
      trail[i] = trail[i-1];
      last[i] = last[i-1];
      fade(last[i], 70);
    }
    trail[0] = realpt;
    last[0].c = m_Color[0];
    fade(last[0], 70);
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

// reduce brightness by percent
void LedRing::fade(_rgb &c, uint8_t perc)
{
  c.r = c.r * perc / 100;
  c.g = c.g * perc / 100;
  c.b = c.b * perc / 100;
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

// White light at level (0 = off)
void LedRing::light(uint8_t level)
{
  static uint8_t oldMode;
  if(level)
  {
    oldMode = m_Effect;
    m_Effect = ef_Hold;
    colorWipe(strip.Color(level, level, level), 10);
  }
  else
  {
    m_Effect = oldMode;
  }
}

void LedRing::clock(uint32_t c1, uint32_t c2, uint16_t n)
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
      setPixel(i, strip.Color(cnt, cnt, cnt) );
      uint32_t c3 = strip.Color(127-cnt, 127-cnt, 127-cnt);
      setPixel((i+LED_CNT-1) % LED_CNT, strip.gamma32(c3) );
    }
    if(i == h)
      setPixel(i, c1);
    if(i == m)
      setPixel(i, c2);
  }
  if(cnt < 125)
    cnt++;
}

// fading trail
void LedRing::spiral(uint16_t n)
{
  int8_t e = n % LED_CNT;
  _rgb c;
  c.c = m_Color[0];

  for(int i = 0; i < LED_CNT; i++)
  {
    setPixel(e, c.c);
    if( --e < 0) e += LED_CNT;
    fade(c, 75);
  }
}

// reverse trailing opposites
void LedRing::doubleSpiral(uint16_t n)
{
  int8_t e = LED_CNT - (n>>1) % LED_CNT;
  _rgb c[2];

  c[0].c = m_Color[0];
  c[1].c = m_Color[1];

  for(int i = 0; i < LED_CNT; i++)
  {
    setPixel(e+i, c[0].c);
    setPixel(e+i + (LED_CNT/2), c[1].c);
    fade(c[0], 70);
    fade(c[1], 70);
  }
}

void LedRing::wings(uint16_t n)
{
  static bool bDir = true;
  static int16_t fader = 0;
  static uint8_t pt = 0;

  _rgb c;
  c.c = m_Color[1];

  int i;
  for(i = 0; i < pt; i++)
  {
    setPixel((LED_CNT / 2) + i, c.c);
    setPixel((LED_CNT / 2) - i, c.c);
  }
  if(bDir)
  {
    fader += 10;
    if(fader > 90)
    {
      if(pt < LED_CNT/2+1)
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
      if(pt > 0)
      {
        pt--;
        fader = 90;
      }
      else
        bDir = true;
    }
  }
  fade(c, (uint8_t)fader);
  setPixel((LED_CNT / 2) + i, strip.gamma32(c.c) );
  setPixel((LED_CNT / 2) - i, strip.gamma32(c.c) );
}

void LedRing::clear()
{
  memset(m_leds, 0, sizeof(m_leds));
}

void LedRing::setPixel(uint8_t i, uint32_t c)
{
  m_leds[i % LED_CNT] |= c;
}

void LedRing::show()
{
  for(int i = 0; i < LED_CNT; i++)
    strip.setPixelColor(i, m_leds[i]);
  strip.show();
}

// Fill the dots one after the other with a color
void LedRing::colorWipe(uint32_t c, uint8_t wait)
{
  for(uint16_t i=0; i<LED_CNT; i++) {
    setPixel(i, c);
    strip.show();
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

//Theatre-style crawling lights.
void LedRing::theaterChase(uint32_t c, uint8_t wait, uint16_t j)
{
  for (int q=0; q < 3; q++) {
    for (uint16_t i=0; i < LED_CNT; i=i+3)
    {
      m_leds[i+q] = 0;
      setPixel(i+q, c);    //turn every third pixel on
    }
    show();

    delay(wait);

    for (uint16_t i=0; i < LED_CNT; i=i+3)
      m_leds[i+q] = 0;        //turn every third pixel off
  }
}

//Theatre-style crawling lights with rainbow effect
void LedRing::theaterChaseRainbow(uint8_t wait, uint16_t j)
{
  for (int q=0; q < 3; q++) {
    for (uint16_t i=0; i < LED_CNT; i=i+3)
    {
      m_leds[i+q] = 0;
      setPixel(i+q, Wheel( (i+j) % 255));    //turn every third pixel on
    }
    show();

    delay(wait);

    for (uint16_t i=0; i < LED_CNT; i=i+3)
      m_leds[i+q] = 0;        //turn every third pixel off
  }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t LedRing::Wheel(byte WheelPos)
{
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}
