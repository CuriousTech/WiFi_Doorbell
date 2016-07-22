/**The MIT License (MIT)

Copyright (c) 2016 by Greg Cunningham, CuriousTech

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
*/

// Build with Arduino IDE 1.6.9, esp8266 SDK 2.2.0 or 2.3.0

#include <Wire.h>
#include "icons.h" // if errors for icons, remove the one in the library (or rename)
#include <DHT.h> // http://www.github.com/markruys/arduino-DHT
#include <TimeLib.h> // http://www.pjrc.com/teensy/td_libs_Time.html
#include <ssd1306_i2c.h>  // https://github.com/CuriousTech/WiFi_Doorbell/Libraries/ssd1306_i2c

#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <EEPROM.h>
#include <ESP8266mDNS.h>
#include "WiFiManager.h"
#include <ESP8266WebServer.h>
#include <Event.h>  // https://github.com/CuriousTech/ESP8266-HVAC/tree/master/Libraries/Event
#include <JsonClient.h> // https://github.com/CuriousTech/ESP8266-HVAC/tree/master/Libraries/JsonClient

const char controlPassword[] = "password"; // device password for modifying any settings
int serverPort = 80; // port to access this device

#define ESP_LED    2  // low turns on ESP blue LED
#define DOORBELL   4 // Note: GPIO4 and GPIO5 are reversed on every pinout on the net
#define PIR        14
#define SCL        13
#define SDA        12

IPAddress lastIP;
int nWrongPass;

const char *pIcon = icon12;
const char *pAlertIcon = pubAnn;
char szWeatherCond[32] = "NA"; // short description
char szWind[32] = "NA";        // Wind direction (SSW, NWN...)
char szAlertDescription[64];   // Severe Thunderstorm Warning, Dense Fog, etc.
char szAlertMessage[4096];    // these are huge
unsigned long alert_expire;   // epoch of alert sell by date
float TempF;
int rh;
int8_t TZ;
int8_t DST;  // TZ includes DST

struct timeStamp{ // weekday + 12hr time
  uint8_t x,wd,h,m,s,a;
};

#define DB_CNT 16
timeStamp doorbellTimes[DB_CNT];
int doorbellTimeIdx = 0;
bool bAutoClear;
unsigned long dbTime;
timeStamp pirStamp;
unsigned long pirTime;

SSD1306 display(0x3c, SDA, SCL); // Initialize the oled display for address 0x3c, sda=12, sdc=13
int displayOnTimer;             // temporary OLED turn-on
String sMessage;

WiFiManager wifi(0);  // AP page:  192.168.4.1
ESP8266WebServer server( serverPort );

int httpPort = 80; // may be modified by open AP scan

struct eeSet // EEPROM backed data
{
  uint16_t size;          // if size changes, use defauls
  uint16_t sum;           // if sum is diiferent from memory struct, write
  char    location[32];   // location for wunderground
  bool    bEnablePB[2];   // enable pushbullet for doorbell, motion
  bool    bEnableOLED;
  bool    bMotion;        // motion activated clear
  char    pbToken[40];    // 34
  char    wuKey[20];      // 16
  char    reserved[64];
};
eeSet ee = { sizeof(eeSet), 0xAAAA,
  "41042", // "KKYFLORE10"
  {false, false},  // Enable pushbullet
  true,   // Enable OLED
  false,
  "pushbullet token here",
  "wunderground key goes here",
  ""
};

String dataJson() // timed/instant pushed data
{
    String s = "{\"weather\": \"";
    s += szWeatherCond;
    s += "\",\"pir\": \"";
    s += timeToTxt(pirStamp);
    s += "\",\"bellCount\": ";
    s += doorbellTimeIdx;
    for(int i = 0; i < DB_CNT; i++)
    {
      s += ",\"t";
      s += i;
      s += "\":\"";
      if(doorbellTimeIdx > i)  // just make them "" if not valid
        s += timeToTxt(doorbellTimes[i]);
      s += "\"";
    }
    s += "}";
    return s;
}

eventHandler event(dataJson);

void wuCondCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue);
JsonClient wuClient1(wuCondCallback);
void wuConditions(void);
void wuAlertsCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue);
JsonClient wuClient2(wuAlertsCallback);
void wuAlerts(void);

void pushBullet(const char *pTitle, String sBody);

const char days[7][4] = {"Sun","Mon","Tue","Wed","Thr","Fri","Sat"};
const char months[12][4] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

void parseParams()
{
  char temp[100];
  char password[64] = "";
  int val;
  bool bRemote = false;
  bool ipSet = false;

//  Serial.println("parseArgs");

  if(server.args() == 0)
    return;

  // get password first
  for ( uint8_t i = 0; i < server.args(); i++ ) {
    server.arg(i).toCharArray(temp, 100);
    String s = wifi.urldecode(temp);
    switch( server.argName(i).charAt(0)  )
    {
      case 'k': // key
        s.toCharArray(password, sizeof(password));
        break;
    }
  }

  IPAddress ip = server.client().remoteIP();

  if(strcmp(controlPassword, password))
  {
    if(nWrongPass == 0) // it takes at least 10 seconds to recognize a wrong password
      nWrongPass = 10;
    else if((nWrongPass & 0xFFFFF000) == 0 ) // time doubles for every high speed wrong password attempt.  Max 1 hour
      nWrongPass <<= 1;
    if(ip != lastIP)  // if different IP drop it down
       nWrongPass = 10;
    String data = "{\"ip\":\"";
    data += ip.toString();
    data += "\",\"pass\":\"";
    data += password; // a String object here adds a NULL.  Possible bug in SDK
    data += "\"}";
    event.push("hack", data); // log attempts
    lastIP = ip;
    return;
  }

  lastIP = ip;

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    server.arg(i).toCharArray(temp, 100);
    String s = wifi.urldecode(temp);
    bool bValue = (s == "true" || s == "1") ? true:false;
//    Serial.println( i + " " + server.argName ( i ) + ": " + s);
 
    switch( server.argName(i).charAt(0)  )
    {
      case 'O': // OLED
          ee.bEnableOLED = bValue;
          break;
      case 'P': // PushBullet
          {
            int which = (tolower(server.argName(i).charAt(1) ) == '1') ? 1:0;
            ee.bEnablePB[which] = bValue;
          }
          break;
      case 'M': // Motion
          ee.bMotion = bValue;
          break;
      case 'R': // reset
          doorbellTimeIdx = 0;
          break;
      case 'L': // location
          s.toCharArray(ee.location, sizeof(ee.location));
          break;
      case 'm':  // message
          alert_expire = 0; // also clears the alert
          sMessage = server.arg(i);
          if(ee.bEnableOLED == false && sMessage.length())
          {
            displayOnTimer = 60;
          }
          break;
      case 'w': // wunderground key
          s.toCharArray(ee.wuKey, sizeof(ee.wuKey));
          break;
      case 'b': // pushbullet token
          s.toCharArray(ee.pbToken, sizeof(ee.pbToken));
          break;
      case 't': // test
          doorBell();
          break;
    }
  }
}

void handleRoot() // Main webpage interface
{
  Serial.println("handleRoot");

  parseParams();

  String page = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>"
      "<title>WiFi Doorbell</title>"
      "<style type=\"text/css\">"
      "div,table,input{"
      "border-radius: 5px;"
      "margin-bottom: 5px;"
      "box-shadow: 2px 2px 12px #000000;"
      "background-image: -moz-linear-gradient(top, #ffffff, #50a0ff);"
      "background-image: -ms-linear-gradient(top, #ffffff, #50a0ff);"
      "background-image: -o-linear-gradient(top, #ffffff, #50a0ff);"
      "background-image: -webkit-linear-gradient(top, #efffff, #50a0ff);"
      "background-image: linear-gradient(top, #ffffff, #50a0ff);"
      "background-clip: padding-box;"
      "}"
      "body{width:240px;font-family: Arial, Helvetica, sans-serif;}"
      "</style>"

      "<script src=\"http://ajax.googleapis.com/ajax/libs/jquery/1.3.2/jquery.min.js\" type=\"text/javascript\" charset=\"utf-8\"></script>"
      "<script type=\"text/javascript\">"
      "oledon=";
  page += ee.bEnableOLED;
  page += ";pb0=";
  page += ee.bEnablePB[0];
  page += ";pb1=";
  page += ee.bEnablePB[1];
  page += ";bmot=";
  page += ee.bMotion;
  page += ";function startEvents()"
      "{"
         "eventSource = new EventSource(\"events?i=60&p=1\");"
         "eventSource.addEventListener('open', function(e){},false);"
         "eventSource.addEventListener('error', function(e){},false);"
         "eventSource.addEventListener('alert', function(e){alert(e.data)},false);"
         "eventSource.addEventListener('state',function(e){"
           "d=JSON.parse(e.data);"
           "document.all.mot.innerHTML=d.pir;"
           "for(i=0;i<";
           page += DB_CNT;
           page += ";i++){"
            "item=eval('document.all.t'+i);tm1=eval('d.t'+i);"
            "item.innerHTML=tm1;"
            "item=eval('document.all.r'+i);"
            "item.setAttribute('style',tm1.length?'':'display:none')"
           "}"
         "},false);"
      "}"
      "function reset(){"
        "$.post(\"s\", { R: 0, key: document.all.myKey.value });"
        "location.reload();"
      "}"
      "function oled(){"
        "oledon=!oledon;"
        "$.post(\"s\", { O: oledon, key: document.all.myKey.value });"
        "document.all.OLED.value=oledon?'OFF':'ON '"
      "}"
      "function pbToggle0(){"
        "pb0=!pb0;"
        "$.post(\"s\", { P0: pb0, key: document.all.myKey.value });"
        "document.all.PB0.value=pb0?'OFF':'ON '"
      "}"
      "function pbToggle1(){"
        "pb1=!pb1;"
        "$.post(\"s\", { P1: pb1, key: document.all.myKey.value });"
        "document.all.PB1.value=pb1?'OFF':'ON '"
      "}"
      "function motTog(){"
        "bmot=!bmot;"
        "$.post(\"s\", { M: bmot, key: document.all.myKey.value });"
        "document.all.MOT.value=bmot?'OFF':'ON '"
      "}"
      "setInterval(timer,1000);"
      "t=";
  page += now() - (3600 * TZ); // set to GMT
  page +="000;function timer(){" // add 000 for ms
          "t+=1000;d=new Date(t);"
          "document.all.time.innerHTML=d.toLocaleTimeString()}";

    if(szAlertMessage) // onload alert
    {
      page += "alert(\"";
      page += szAlertMessage;
      page += "\");";
    }
 page += "</script>"
      "<body onload=\"{"
      "key=localStorage.getItem('key');if(key!=null) document.getElementById('myKey').value=key;"
      "for(i=0;i<document.forms.length;i++) document.forms[i].elements['key'].value=key;"
      "startEvents();}\">"

      "<div><h3 align=\"center\">WiFi Doorbell</h3>"
      "<table align=\"center\">"
      "<tr><td><p id=\"time\">";
  page += timeFmt();
  page += "</p></td><td></td></tr>"
           "<tr><td colspan=2>Doorbell Rings: <input type=\"button\" value=\"Clear\" id=\"resetBtn\" onClick=\"{reset()}\">"
           "</td></tr>";
  for(int i = 0; i < DB_CNT; i++)
  {
    page += "<tr id=r";
    page += i;
    if(i >= doorbellTimeIdx)
      page += " style=\"display:none\""; // unused=invisible
    page += "><td></td><td><div id=\"t";
    page += i;
    page += "\"";
    page += ">";
    page += timeToTxt(doorbellTimes[i]);
    page += "</div></td></tr>";
  }
  page += "<tr><td>Motion:</td><td><div id=\"mot\">";
  page += timeToTxt(pirStamp);
  page += "</div></td></tr>";

  page += "<tr><td>Display:</td><td>"
          "<input type=\"button\" value=\"";
  page += ee.bEnableOLED ? "OFF":"ON ";
  page += "\" id=\"OLED\" onClick=\"{oled()}\">"
          " Mot <input type=\"button\" value=\"";
  page += ee.bMotion ? "OFF":"ON ";
  page += "\" id=\"MOT\" onClick=\"{motTog()}\">"
          "</td></tr>"
          "<tr><td>PushBullet:</td><td>"
          "<input type=\"button\" value=\"";
  page += ee.bEnablePB[0] ? "OFF":"ON ";
  page += "\" id=\"PB0\" onClick=\"{pbToggle0()}\">"
          " Mot <input type=\"button\" value=\"";
  page += ee.bEnablePB[1] ? "OFF":"ON ";
  page += "\" id=\"PB1\" onClick=\"{pbToggle1()}\">"
          "</td></tr>"
          "<tr><td>Message:</td><td>";
  page += valButton("M", "" );
  page += "</td></tr>"
          "<tr><td>Location:</td>";
  page += "<td>";  page += valButton("L", ee.location );
  page += "</td></tr>";
  if(alert_expire)
  {
    page += "<tr><td>";
    page += szAlertDescription;
    page += "</td></tr>";
  }
  page += "</table>"

          "<input id=\"myKey\" name=\"key\" type=text size=50 placeholder=\"password\" style=\"width: 150px\">"
          "<input type=\"button\" value=\"Save\" onClick=\"{localStorage.setItem('key', key = document.all.myKey.value)}\">"
          "<br><small>Logged IP: ";
  page += server.client().remoteIP().toString();
  page += "</small></div></body></html>";

  server.send ( 200, "text/html", page );
}

String valButton(String id, String val)
{
  String s = "<form method='post'><input name='";
  s += id;
  s += "' type=text size=8 value='";
  s += val;
  s += "'><input type=\"hidden\" name=\"key\"><input value=\"Set\" type=submit></form>";
  return s;
}

// Current time in hh:mm:ss AM/PM
String timeFmt()
{
  String r = "";
  if(hourFormat12() < 10) r = " ";
  r += hourFormat12();
  r += ":";
  if(minute() < 10) r += "0";
  r += minute();
  r += ":";
  if(second() < 10) r += "0";
  r += second();
  r += " ";
  r += isPM() ? "PM":"AM";
  return r;
}

String timeToTxt(timeStamp &t)  // string "Sun 12:00:00 AM"
{
  String s = days[t.wd];
  s += " ";
  s += t.h;
  s += ":";
  if(t.m < 10)  s += "0";
  s += t.m;
  s += ":";
  if(t.s < 10)  s += "0";
  s += t.s;
  s += " ";
  s += t.a ? "PM" : "AM";  
  return s;
}

void handleS()
{
  parseParams();

  String page = "{\"ip\": \"";
  page += WiFi.localIP().toString();
  page += ":";
  page += serverPort;
  page += "\"}";
  server.send ( 200, "text/json", page );
}

// JSON format for initial or full data read
void handleJson()
{
  String s = "{\"weather\": \"";
  s += szWeatherCond;
  s += "\",\"location\": \"";
  s += ee.location;
  s += "\",\"bellCount\": ";
  s += doorbellTimeIdx;
  s += ",\"display\": ";
  s += ee.bEnableOLED;
  s += ",\"temp\": \"";
  s += String(TempF,1);
  s += "\",\"rh\": ";
  s += rh;
  s += ",\"time\": ";
  s += now();
  s += ",\"pir\": ";
  s += pirTime;
  for(int i = 0; i < DB_CNT; i++)
  {
    s += ",\"t";
    s += i;
    s += "\":\"";
    if(doorbellTimeIdx > i)
      s += timeToTxt(doorbellTimes[i]);
    s += "\"";
  }
  s += "}";
  server.send( 200, "text/json", s );
}

// event streamer (assume keep-alive) (esp8266 2.1.0 can't handle this)
void handleEvents()
{
  char temp[100];
  Serial.println("handleEvents");
  uint16_t interval = 60; // default interval
  uint8_t nType = 0;

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    server.arg(i).toCharArray(temp, 100);
    String s = wifi.urldecode(temp);
//    Serial.println( i + " " + server.argName ( i ) + ": " + s);
    int val = s.toInt();
 
    switch( server.argName(i).charAt(0)  )
    {
      case 'i': // interval
        interval = val;
        break;
      case 'p': // push
        nType = 1;
        break;
      case 'c': // critical
        nType = 2;
        break;
    }
  }

  String content = "HTTP/1.1 200 OK\r\n"
      "Connection: keep-alive\r\n"
      "Access-Control-Allow-Origin: *\r\n"
      "Content-Type: text/event-stream\r\n\r\n";
  server.sendContent(content);
  event.set(server.client(), interval, nType);
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
  }

  server.send ( 404, "text/plain", message );
}

// called when doorbell rings (or test)
void doorBell()
{
  if(dbTime == 0) // date isn't set yet
    return;

  unsigned long newtime = now() - (3600 * TZ);

  if( newtime - dbTime < 10) // ignore bounces and double taps
    return;

  doorbellTimes[doorbellTimeIdx].wd = weekday()-1;
  doorbellTimes[doorbellTimeIdx].h = hourFormat12();
  doorbellTimes[doorbellTimeIdx].m = minute();
  doorbellTimes[doorbellTimeIdx].s = second();
  doorbellTimes[doorbellTimeIdx].a = isPM();

  event.alert("Doorbell "  + timeToTxt( doorbellTimes[doorbellTimeIdx]) );
  event.push(); // instant update on the web page

  // make sure it's more than 5 mins between triggers to send a PB
  if( newtime - dbTime > 5 * 60)
  {
    if(ee.bEnablePB[0])
      pushBullet("Doorbell", "The doorbell rang at " + timeToTxt( doorbellTimes[doorbellTimeIdx]) );
  }

  if(doorbellTimeIdx < DB_CNT)
    doorbellTimeIdx++;

  dbTime = newtime; // latest trigger
}

// called when motion sensed
void motion()
{
  if(pirTime == 0) // date isn't set yet (the PIR triggers at start anyway)
    return;

//  Serial.println("Motion");
  unsigned long newtime = now() - (3600 * TZ);

  if( newtime - pirTime < 30) // limit triggers
    return;

  pirStamp.wd = weekday()-1;
  pirStamp.h = hourFormat12();
  pirStamp.m = minute();
  pirStamp.s = second();
  pirStamp.a = isPM();

  event.alert("Motion " + timeToTxt( pirStamp ) );
  event.push();
  // make sure it's more than 5 mins between triggers to send a PB
  if( newtime - pirTime > 5 * 60)
  {
    if(ee.bEnablePB[1])
      pushBullet("Doorbell", "Motion at " + timeToTxt( pirStamp) );
  }
 
  pirTime = newtime; // latest trigger
}

volatile bool bDoorBellTriggered = false;

void doorbellISR()
{
  bDoorBellTriggered = true;
}

volatile bool bPirTriggered = false;

void pirISR()
{
  bPirTriggered = true;
}

void setup()
{
  pinMode(ESP_LED, OUTPUT);
  pinMode(DOORBELL, INPUT_PULLUP);
  attachInterrupt(DOORBELL, doorbellISR, FALLING);
  pinMode(PIR, INPUT);
  attachInterrupt(PIR, pirISR, FALLING);

  // initialize dispaly
  display.init();

  display.clear();
  display.display();

  Serial.begin(115200);
//  delay(3000);
  Serial.println();

  WiFi.hostname("doorbell");
  wifi.autoConnect("Doorbell");
  eeRead(); // don't access EE before WiFi init

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  if ( MDNS.begin ( "doorbell", WiFi.localIP() ) ) {
    Serial.println ( "MDNS responder started" );
  }

  server.on ( "/", handleRoot );
  server.on ( "/s", handleS );
  server.on ( "/json", handleJson );
  server.on ( "/events", handleEvents );
//  server.on ( "/inline", []() {
//    server.send ( 200, "text/plain", "this works as well" );
//  } );
  server.onNotFound ( handleNotFound );
  server.begin();
  MDNS.addService("http", "tcp", serverPort);

  wuConditions();
}

void loop()
{
  static uint8_t hour_save, sec_save, min_save;
  static uint8_t wuMins = 11;
  static uint8_t wuCall = 0;
  static bool bPulseLED = false;

  MDNS.update();
  server.handleClient();
  wuClient1.service();
  wuClient2.service();

  if(bPirTriggered)
  {
    bPirTriggered = false;
    if(ee.bEnableOLED == false && displayOnTimer == 0 && doorbellTimeIdx) // motion activated indicator
    {
      displayOnTimer = 30;
      bAutoClear = true;
    }
    motion();
    bPulseLED = true; // blinks the blue LED
  }

  if(bDoorBellTriggered)
  {
    bDoorBellTriggered = false;
    doorBell();
    bPulseLED = true;
  }

  if(sec_save != second()) // only do stuff once per second (loop is maybe 20-30 Hz)
  {
    sec_save = second();

    if(min_save != minute())
    {
      min_save = minute();

      static int8_t condCnt = 6;
      bool bGetCond = false;

      if(ee.location[0] && --wuMins == 0) // call wunderground API
      {
        switch(++wuCall) // put a list of wu callers here
        {
          case 0:
            if(ee.bEnableOLED == false)
            {
              if(--condCnt == 0) // get conditions and time every hour when display is off
              {
                bGetCond = true;
                condCnt = 6;
              }
            }
            else
            {
              bGetCond = true;
            }

            if(bGetCond)
            {
              wuConditions();
              break;
            } // fall through if not getting conditions (checks alerts every 10 mins)
          case 1:
            wuAlerts();
            break;
          default:
            wuCall = 0;
            break;
        }
        wuMins = 10; // free account has something like a max of 10 per hour
      }

      if (hour_save != hour()) // hourly stuff
      {
        hour_save = hour();
        eeWrite(); // update EEPROM if needed
      }
    }

    if(displayOnTimer) // if alerts or messages, turn the display on
      if(--displayOnTimer == 0)
      {
        if(bAutoClear) // motion activated doorbell rings display
        {
          if(ee.bMotion) doorbellTimeIdx = 0;
          bAutoClear = false;
        }
      }

    if(nWrongPass)
      nWrongPass--;

    if(alert_expire && alert_expire < now()) // if active alert
      alert_expire = 0;

    if(bPulseLED)
    {
      bPulseLED = false;
      digitalWrite(ESP_LED, LOW); // turn blue LED on for a second
    }
    else
    {
      digitalWrite(ESP_LED, HIGH);
    }

    event.heartbeat();
  }

  DrawScreen();
}

int8_t msgCnt = 0;

void DrawScreen()
{
  static int16_t ind;
  static bool blnk = false;
  static long last_blnk;
  static long last_min;
  static int16_t iconX = 0;
  static int16_t infoX = 64;

  if(minute() != last_min) // alternate static images
  {
    last_min = minute();
    if(iconX)
    {
      iconX = 0;
      infoX = 64;
    }
    else
    {
      iconX = 64;
      infoX = 0;
    }
  }
  
  // draw the screen here
  display.clear();

  if( (millis() - last_blnk) > 400) // 400ms toggle for blinky things
  {
    last_blnk = millis();
    blnk = !blnk;
  }

  if( (ee.bEnableOLED || displayOnTimer || alert_expire) && (bAutoClear == false)) // skip for motion enabled display
  {
    String s;

    if(alert_expire) // if there's an alert message
    {
      sMessage = szAlertDescription; // set the scroller message
      if(blnk) display.drawXbm(iconX+10, 20, 44, 42, pAlertIcon);
    }
    else
    {
      display.drawXbm(iconX+10, 20, 44, 42, pIcon);
    }
  
    if(sMessage.length()) // message sent over wifi or alert
    {
      s = sMessage;
      s += timeFmt(); // display current time too
      s += "  ";

      if(msgCnt == 0) // starting
        msgCnt = 3; // times to repeat message
    }
    else
    {
      s = timeFmt();
      s += "  ";
      s += days[weekday()-1];
      s += " ";
      s += String(day());
      s += " ";
      s += months[month()-1];
      s += "  ";
      s += szWeatherCond;
      s += "  ";
    }
    Scroller(s);

    display.drawPropString(infoX, 23, String(TempF, 1) + "]" );
    display.drawString(infoX + 20, 43, String(rh) + "%");
    if(blnk && doorbellTimeIdx) // blink small gauge if doorbell logged
    { // up to 64 pixels (128x64)
      display.fillRect(infoX, 61, doorbellTimeIdx << 2, 3);
    }
  }
  else if(doorbellTimeIdx) // blink a bell if doorbell logged and display is off
  {
    display.drawPropString(0, 0, timeToTxt(doorbellTimes[0]) ); // the first time
    if(blnk) display.drawXbm(iconX+10, 20, 44, 42, bell);
    display.drawPropString(infoX, 23, String(doorbellTimeIdx) ); // count
  }
  display.display();
}

// Text scroller optimized for very long lines
void Scroller(String s)
{
  static int16_t ind = 0;
  static char last = 0;
  static int16_t x = 0;

  if(last != s.charAt(0)) // reset if content changed
  {
    x = 0;
    ind = 0;
  }
  last = s.charAt(0);
  int len = s.length(); // get length before overlap added
  s += s.substring(0, 18); // add ~screen width overlap
  int w = display.propCharWidth(s.charAt(ind)); // first char for measure
  String sPart = s.substring(ind, ind + 18);
  display.drawPropString(x, 0, sPart );

  if( --x <= -(w))
  {
    x = 0;
    if(++ind >= len) // reset at last char
    {
      ind = 0;
      if(msgCnt && alert_expire == 0) // end of custom message display
      {
        if(--msgCnt == 0) // decrement times to repeat it
        {
          sMessage = "";
        }
      }
    }
  }
}

void iconFromName(char *pName)
{
  struct cond2icon
  {
    const char *pName;
    const char *pIcon;
  };
  cond2icon data[] = { // row column from image at http://www.alessioatzeni.com/meteocons/
    {"chanceflurries", icon72},// 0
    {"chancerain", icon64},
    {"chancesleet", icon44},
    {"chancesnow", icon43},
    {"chancetstorms", icon34},
    {"clear",  icon54},// 5
    {"cloudy", icon75},
    {"flurries", icon73},
    {"fog", icon31},
    {"hazy", icon26},
    {"mostlycloudy", icon62},// 10
    {"mostlysunny", icon11},
    {"partlycloudy", icon22},
    {"partlysunny", icon56},
    {"rain", icon65},
    {"sleet", icon73},
    {"snow", icon74},
    {"sunny", icon54},
    {"tstorms", icon53},

    {"nt_chanceflurries", icon45},
    {"nt_chancerain", icon35},
    {"nt_chancesleet", icon42},
    {"nt_chancesnow", icon44},
    {"nt_chancetstorms", icon33},
    {"nt_clear", icon13},
    {"nt_cloudy", icon32},
    {"nt_flurries", icon45},
    {"nt_fog", icon26},
    {"nt_hazy", icon25},
    {"nt_mostlycloudy", icon32},
    {"nt_mostlysunny", icon14},
    {"nt_partlycloudy", icon23},
    {"nt_partlysunny", icon23},
    {"nt_rain", icon22},
    {"nt_sleet", icon36},
    {"nt_snow", icon46},
    {"nt_sunny", icon56},
    {"nt_tstorms", icon34},
    {NULL, NULL}
  };
  for(int i = 0; data[i].pName; i++)
    if(!strcmp(pName, data[i].pName))
    {
      pIcon = data[i].pIcon;
      break;
    }
}

// things we want from conditions request
const char *jsonList1[] = { "",
  "weather",           // 0
  "temp_f",
  "relative_humidity",
  "wind_string",       //       Calm
  "wind_dir",          //       East, ENE, SW
  "wind_degrees",      // 5     0,45,90
  "wind_mph",          //       n
  "pressure_in",       //       nn.nn
  "pressure_trend",    //       +/-
  "dewpoint_f",        // 
  "heat_index_string", // 10     NA
  "windchill_string",  //        NA
  "feelslike_f",       //        n.n
  "local_epoch",       //        local no DST
  "local_tz_short",
  "local_tz_offset",  // 15     -0400
  "icon",             // 16     name list
  "error",
  NULL
};

void wuCondCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue)
{
  static unsigned long epoch;

  if(iEvent == -1) // connection events
    return;

  switch(iName)
  {
    case 0:
      strcpy(szWeatherCond, psValue);
      break;
    case 1:
      TempF = atof(psValue);
      break;
    case 2:
      rh = iValue;
      break;
    case 3:
      strcpy(szWind, psValue);
      break;
    case 4: // wind dir
      break;
    case 5: // wind deg
      break;
    case 6: // wind mph
      break;
    case 7: // pressure in
      break;
    case 8: // pressure trend
      break;
    case 9: // dewpoint F
      break;
    case 10: // heat index str
      break;
    case 11: // windchill str
      break;
    case 12: // feelslike F
      break;
    case 13: // local epoch
      epoch = iValue;
      break;
    case 14: // local TZ short (DST)
      DST = (psValue[1] == 'D') ? 1:0;
      break;
    case 15: // local TZ offset
      TZ = (iValue / 100);
      epoch += (3600 * TZ );
      setTime(epoch);
      if(dbTime == 0)
        dbTime = pirTime = epoch; // powerup setting
      break;
    case 16:
      iconFromName(psValue);
      break;
    case 17: // error (like a bad key or url)
      event.alert(psValue);
      break;
  }
}

// Call wunderground for conditions
void wuConditions()
{
  if(ee.location[0] == 0)
    return;
  String path = "/api/";
  path += ee.wuKey;
  path += "/conditions/q/";
  path += ee.location;
  path += ".json";

  char sz[64];
  path.toCharArray(sz, sizeof(sz));
  wuClient1.begin("api.wunderground.com", sz, 80, false);
  wuClient1.addList(jsonList1);
}

// things we want from alerts request
const char *jsonList2[] = { "",
  "type",
  "description",
  "expires_epoch",
  "message",
  "phenomena", // 5  //: "HT",
  "significance", //: "Y",
  "error",
  NULL
};

void wuAlertsCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue)
{
  if(iEvent == -1) // connection events
  {
    if(iName == JC_CONNECTED)
      alert_expire = 0; // clear last.  Alerts can cancel before expire time
    return;
  }

  switch(iName)
  {
    case 0: // type (3 letter)
      alertIcon(psValue);
      break;
    case 1: // description
      strncpy(szAlertDescription, psValue, sizeof(szAlertDescription) );
      strcat(szAlertDescription, "  "); // separate end and start on scroller
//      Serial.print("alert_desc ");
//      Serial.println(szAlertDescription);
      break;
    case 2: // expires
      alert_expire = iValue;
      alert_expire += (3600 * TZ );
      break;
    case 3: // message (too long to watch on the scroller)
    {
        int i;
        for(i = 0; i < sizeof(szAlertMessage)-2; i++)
        {
          if(*psValue == '\\')
          {
            psValue += 2; // assume \u000A (unsigned 16 bit)
            psValue += 4;
            szAlertMessage[i] = ' '; // convert to a space
          }
          else
          {
            szAlertMessage[i] = *psValue++;
          }
        }
        szAlertMessage[i++] = ' '; // space
        szAlertMessage[i] = 0; //null term
      }
      event.alert(szAlertMessage);

//    Serial.print("alert_message ");
//    Serial.println(szAlertMessage);

      break;
    case 6: // error
      event.alert(psValue);
      break;
  }
}

// Call wunderground to check alerts
void wuAlerts()
{
  if(ee.location[0] == 0)
    return;

  String path = "/api/";
  path += ee.wuKey;
  path += "/alerts/q/";
  path += ee.location;
  path += ".json";

  char sz[64];
  path.toCharArray(sz, sizeof(sz));
  wuClient2.begin("api.wunderground.com", sz, 80, false);
  wuClient2.addList(jsonList2);
}

void eeWrite() // write the settings if changed
{
  uint16_t old_sum = ee.sum;
  ee.sum = 0;
  ee.sum = Fletcher16((uint8_t *)&ee, sizeof(eeSet));

  if(old_sum == ee.sum)
    return; // Nothing has changed?

  wifi.eeWriteData(64, (uint8_t*)&ee, sizeof(ee)); // WiFiManager already has an instance open, so use that at offset 64+
}

void alertIcon(char *p)
{
  struct alert2icon
  {
    const char name[4];
    const char *pIcon;
  };
  alert2icon data[] = {
    {"HUR", tornado}, //Hurricane Local Statement
    {"TOR", tornado}, //Tornado Warning
    {"TOW", tornado}, //Tornado Watch
    {"WRN", icon53},  //Severe Thunderstorm Warning
    {"SEW", icon53},  //Severe Thunderstorm Watch
    {"WIN", icon41},  //Winter Weather Advisory
    {"FLO", icon65},  //Flood Warning
    {"WAT", iconEye},  //Flood Watch / Statement
    {"WND", icon16},  //High Wind Advisory
    {"SVR", pubAnn},  //Severe Weather Statement
    {"HEA", icon54},  //Heat Advisory
    {"FOG", icon26},  //Dense Fog Advisory
    {"SPE", pubAnn},  //Special Weather Statement
    {"FIR", icon11},  //xFire Weather Advisory
    {"VOL", icon11},  //xVolcanic Activity Statement
    {"HWW", tornado}, //Hurricane Wind Warning
    {"REC", icon11},  //xRecord Set
    {"REP", pubAnn},  //Public Reports
    {"PUB", pubAnn},  //Public Information Statement
    {"", 0}
  };

  for(int i = 0; data[i].name[0]; i++)
  {
    if(!strcmp(p, data[i].name))
    {
      pAlertIcon = data[i].pIcon;
      break;
    }
  }
}

void eeRead()
{
  eeSet eeTest;

  wifi.eeReadData(64, (uint8_t*)&eeTest, sizeof(eeSet));
  if(eeTest.size != sizeof(eeSet)) return; // revert to defaults if struct size changes
  uint16_t sum = eeTest.sum;
  eeTest.sum = 0;
  eeTest.sum = Fletcher16((uint8_t *)&eeTest, sizeof(eeSet));
  if(eeTest.sum != sum) return; // revert to defaults if sum fails
  memcpy(&ee, &eeTest, sizeof(eeSet));
}

uint16_t Fletcher16( uint8_t* data, int count)
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

// Send a push notification
void pushBullet(const char *pTitle, String sBody)
{
  WiFiClientSecure client;
  const char host[] = "api.pushbullet.com";
  const char url[] = "/v2/pushes";

  if (!client.connect(host, 443))
  {
    event.print("PushBullet connection failed");
    return;
  }

  // Todo: The key should be verified here

  String data = "{\"type\": \"note\", \"title\": \"";
  data += pTitle;
  data += "\", \"body\": \"";
  data += sBody;
  data += "\"}";

  client.print(String("POST ") + url + " HTTP/1.1\r\n" +
              "Host: " + host + "\r\n" +
              "Content-Type: application/json\r\n" +
              "Access-Token: " + ee.pbToken + "\r\n" +
              "User-Agent: Arduino\r\n" +
              "Content-Length: " + data.length() + "\r\n" + 
              "Connection: close\r\n\r\n" +
              data + "\r\n\r\n");
 
  int i = 0;
  while (client.connected() && ++i < 10)
  {
    String line = client.readStringUntil('\n');
    Serial.println(line);
  }
}
