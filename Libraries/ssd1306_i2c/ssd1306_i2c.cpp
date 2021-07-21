/**The MIT License (MIT)

Copyright (c) 2015 by Daniel Eichhorn

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

See more at http://blog.squix.ch

Credits for parts of this code go to Mike Rankin. Thank you so much for sharing!
*/

#include "ssd1306_i2c.h"
#include <Wire.h>
#include "font.h"

SSD1306::SSD1306(int i2cAddress, int sda, int sdc)
{
  myI2cAddress = i2cAddress;
  mySda = sda;
  mySdc = sdc;
}

void SSD1306::init() {
  Wire.begin(mySda, mySdc);
  Wire.setClock(400000);
  sendInitCommands();
  resetDisplay();
}

void SSD1306::resetDisplay(void)
{
  displayOff();
  clear();
  display();
  displayOn();
}

void SSD1306::reconnect() {
  Wire.begin(mySda, mySdc);  
}

void SSD1306::displayOn(void)
{
    sendCommand(0xaf);        //display on
}

void SSD1306::displayOff(void)
{
  sendCommand(0xae);		//display off
}

void SSD1306::setContrast(char contrast) {
  sendCommand(0x81);
  sendCommand(contrast);
}

void SSD1306::flipScreenVertically() {
  sendCommand(0xA0 | 0x1);      //SEGREMAP   //Rotate screen 180 deg
  sendCommand(0xC8);            //COMSCANDEC  Rotate screen 180 Deg
}

#define B_SIZE (128 * 64 / 8)

void SSD1306::clear(void) {
    memset(buffer, 0, B_SIZE);
}

void SSD1306::updateChunk(void) {
    static uint16_t o = 0;

    if(o >= B_SIZE)
        o = 0;
    for (uint16_t i=0; i < B_SIZE / 8; i++) {
      Wire.beginTransmission(myI2cAddress);
      Wire.write(0x40);
      for (uint8_t x=0; x<16; x++) {
        Wire.write(buffer[o++]);
        i++;
      }
      Wire.endTransmission();
    }
}

void SSD1306::display(void) {

    for (uint16_t i=0; i< B_SIZE; i++) {
      // send a bunch of data in one xmission
      //Wire.begin(mySda, mySdc);
      Wire.beginTransmission(myI2cAddress);
      Wire.write(0x40);
      for (uint8_t x=0; x<16; x++) {
        Wire.write(buffer[i]);
        i++;
      }
      i--;
      yield();
      Wire.endTransmission();
    }
}

void SSD1306::scroll8up()
{
    for(int i=0;i<896;i++)
      buffer[i] = buffer[i+128];
    for(int i=896;i<1024;i++)
      buffer[i] = 0;
    display();
}

void SSD1306::print(String text){
  scroll8up();
  drawString(0, 56, text);
  display();
}

void SSD1306::setPixel(int x, int y) {
  if (x >= 0 && x < 128 && y >= 0 && y < 64) {
     
     switch (myColor) {
      case WHITE:   buffer[x+ (y/8)*128] |=  (1 << (y&7)); break;
      case BLACK:   buffer[x+ (y/8)*128] &= ~(1 << (y&7)); break; 
      case INVERSE: buffer[x+ (y/8)*128] ^=  (1 << (y&7)); break; 
    }
  }
}

void SSD1306::setChar(int x, int y, unsigned char data) {
  for (int i = 0; i < 8; i++) {
    if (bitRead(data, i)) {
     setPixel(x,y + i);
    }
  }   
}

void SSD1306::drawString(int x, int y, String text) {
  for (int j=0; j < text.length(); j++) {
    for(int i=0;i<8;i++) {
      unsigned char charColumn = pgm_read_byte(myFont[text.charAt(j)-0x20]+i);
      for (int pixel = 0; pixel < 8; pixel++) {
        if (bitRead(charColumn, pixel)) {
          if (myIsFontScaling2x2) {
             setPixel(x + 2*(j* 7 + i),y + 2*pixel);
             setPixel(x + 2*(j* 7 + i)+1,y + 2*pixel +1);
             setPixel(x + 2*(j* 7 + i)+1,y + 2*pixel);
             setPixel(x + 2*(j* 7 + i),y + 2*pixel +1);
          } else {
             setPixel(x + j* 7 + i,y + pixel);  
          }
        }   
      }
    }
  }
}

#define PropFont crystalclear_14ptFontInfo
int SSD1306::drawPropString(int x, int y, String text)
{
  for (int j=0; j < text.length(); j++)
  {
    switch(text.charAt(j)){
      case ' ':  x += PropFont.space_width + 1; continue; // skip spaces
      case '1':  x += 4; break;
    }
    unsigned char ch = text.charAt(j) - PropFont.start_ch;

    uint16_t w = PropFont.pInfo[ch].w;
    uint16_t offset = PropFont.pInfo[ch].offset;
    const unsigned char *p = PropFont.pBitmaps + offset;

    if(x >= 128) // offscreen
      break;

    int bytes = (w+7) >> 3;
    for(int i = 0; i < PropFont.height; i++)
    {
      int x2 = x;
      for(int b = 0; b < bytes; b++)
      {
        unsigned char charColumn = pgm_read_byte(p++);
        for (int pixel = 0; pixel < 8; pixel++)
        {
          if (bitRead(charColumn, 7-pixel))
          {
             setPixel(x2 + pixel, y + i);
          }
        }
        x2 += 8;
      }
    }

    x += w + 1;
  }
  return x; // return width used
}


uint16_t SSD1306::propCharWidth(char c)
{
  unsigned char ch = c - PropFont.start_ch;
  uint16_t w = PropFont.pInfo[ch].w;

  switch(c){
    case ' ':  return PropFont.space_width + 1;
    case '1':  w += 4; break;
  }

  return w + 1; // return width plus space
}

void SSD1306::setFontScale2x2(bool isFontScaling2x2) {
  myIsFontScaling2x2 = isFontScaling2x2;
}

void SSD1306::drawBitmap(int x, int y, int width, int height, const char *bitmap) {
  for (int i = 0; i < width * height / 8; i++ ){
    unsigned char charColumn = 255 - pgm_read_byte(bitmap + i);
    for (int j = 0; j < 8; j++) { 
      int targetX = i % width + x;
      int targetY = (i / (width)) * 8 + j + y;
      if (bitRead(charColumn, j)) {
        setPixel(targetX, targetY);  
      }
    }
  }
}

void SSD1306::setColor(int color) {
  myColor = color;  
}

void SSD1306::drawRect(int x, int y, int width, int height) {
  for (int i = x; i < x + width; i++) {
    setPixel(i, y);
    setPixel(i, y + height);    
  }
  for (int i = y; i < y + height; i++) {
    setPixel(x, i);
    setPixel(x + width, i);  
  }
}

void SSD1306::fillRect(int x, int y, int width, int height) {
  for (int i = x; i < x + width; i++) {
    for (int j = y; j < y + height; j++) {
      setPixel(i, j);
    }
  }
}

void SSD1306::drawXbm(int x, int y, int width, int height, const char *xbm) {
  if (width % 8 != 0) {
    width =  ((width / 8) + 1) * 8;
  }
  for (int i = 0; i < width * height / 8; i++ ){
    unsigned char charColumn = pgm_read_byte(xbm + i);
    for (int j = 0; j < 8; j++) { 
      int targetX = (i * 8 + j) % width + x;
      int targetY = (8 * i / (width)) + y;
      if (bitRead(charColumn, j)) {
        setPixel(targetX, targetY);  
      }
    }
  }    
}

void SSD1306::sendCommand(unsigned char com)
{
  //Wire.begin(mySda, mySdc);
  Wire.beginTransmission(myI2cAddress);     //begin transmitting
  Wire.write(0x80);                          //command mode
  Wire.write(com);
  Wire.endTransmission();                    // stop transmitting
}

void SSD1306::sendInitCommands(void)
{
  sendCommand(0xA3);        //stop scroll
  sendCommand(0x00);        //stop scroll
  sendCommand(64);        //stop scroll
  sendCommand(0x2E);        //stop scroll

  // Adafruit Init sequence for 128x64 OLED module
  sendCommand(0xAE);        //DISPLAYOFF
  sendCommand(0xD5);        //SETDISPLAYCLOCKDIV
  sendCommand(0x80);        // the suggested ratio 0x80
  sendCommand(0xA8);        //SSD1306_SETMULTIPLEX
  sendCommand(0x3F);
  sendCommand(0xD3);        //SETDISPLAYOFFSET
  sendCommand(0x0);         //no offset
  sendCommand(0x40 | 0x0);  //SETSTARTLINE
  sendCommand(0x8D);        //CHARGEPUMP
  sendCommand(0x14);
  sendCommand(0x20);        //MEMORYMODE
  sendCommand(0x00);        //0x0 act like ks0108

  sendCommand(0x21);        //reset Column address
  sendCommand(0x00);
  sendCommand(0x7F);
  sendCommand(0x22);        //reset Page address
  sendCommand(0x00);
  sendCommand(0x7F);
  
  sendCommand(0xA0);
  
  sendCommand(0xC0);
  sendCommand(0xDA);        //0xDA
  sendCommand(0x12);        //COMSCANDEC
  sendCommand(0x81);        //SETCONTRAS
  sendCommand(0x01);        // 0xCF
  sendCommand(0xd9);        //SETPRECHARGE
  sendCommand(0xF1);
  sendCommand(0xDB);        //SETVCOMDETECT
  sendCommand(0x40);
  sendCommand(0xA4);        //DISPLAYALLON_RESUME
  sendCommand(0xA6);        //NORMALDISPLAY

  sendCommand(0xAF);        //DISPLAYALLON_RESUME
  //----------------------------REVERSE comments----------------------------//
//  sendCommand(0xc8);        // flip
//    sendCommand(0xa0);	  	//seg re-map 0->127(default)
//    sendCommand(0xa1);	  	//seg re-map 127->0

  //----------------------------REVERSE comments----------------------------//
  // sendCommand(0xa7);  //Set Inverse Display
  // sendCommand(0xae);		//display off
  // sendCommand(0x02);         // Set Memory Addressing Mode ab Page addressing mode(RESET)  
}
