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

// Build with Arduino IDE 1.6.11, esp8266 SDK >2.3.0 (git repo 9/20/2016+)

#include <Wire.h>
#include "icons.h"
#include <DHT.h> // http://www.github.com/markruys/arduino-DHT
#include <TimeLib.h> // http://www.pjrc.com/teensy/td_libs_Time.html
#include <ssd1306_i2c.h>  // https://github.com/CuriousTech/WiFi_Doorbell/Libraries/ssd1306_i2c

#include <EEPROM.h>
#include <ESP8266mDNS.h>
#include "WiFiManager.h"
#include <ESPAsyncWebServer.h> // https://github.com/me-no-dev/ESPAsyncWebServer

#include <JsonClient.h> // https://github.com/CuriousTech/ESP8266-HVAC/tree/master/Libraries/JsonClient
#include "PushBullet.h"
#include "eeMem.h"

const char controlPassword[] = "password"; // device password for modifying any settings
int serverPort = 80; // port to access this device

#define ESP_LED    2  // low turns on ESP blue LED
#define DOORBELL   4 // Note: GPIO4 and GPIO5 are reversed on every pinout on the net
#define PIR        14
#define SCL        13
#define SDA        12

IPAddress lastIP;
int nWrongPass;

const char *pIcon = icon11;
const char *pAlertIcon = pubAnn;
char szWeatherCond[32] = "NA"; // short description
char szWind[32] = "NA";        // Wind direction (SSW, NWN...)
char szAlertDescription[64];   // Severe Thunderstorm Warning, Dense Fog, etc.
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

WiFiManager wifi;  // AP page:  192.168.4.1
AsyncWebServer server( serverPort );
AsyncEventSource events("/events"); // event source (Server-Sent events)

PushBullet pb;

int httpPort = 80; // may be modified by open AP scan

eeMem eemem;

String dataJson() // timed/instant pushed data
{
  String s = "{";
  s += "\"t\": ";
  s += now() - (TZ * 3600);
  s +=",\"weather\": \"";
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

void wuCondCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue);
JsonClient wuClient1(wuCondCallback);
void wuConditions(void);
void wuAlertsCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue);
JsonClient wuClient2(wuAlertsCallback);
void wuAlerts(void);

const char days[7][4] = {"Sun","Mon","Tue","Wed","Thr","Fri","Sat"};
const char months[12][4] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

void parseParams(AsyncWebServerRequest *request)
{
  char temp[100];
  char password[64] = "";
  int val;
  bool bRemote = false;
  bool ipSet = false;

//  Serial.println("parseArgs");

  if(request->params() == 0)
    return;

  // get password first
  for ( uint8_t i = 0; i < request->params(); i++ ) {
    AsyncWebParameter* p = request->getParam(i);

    p->value().toCharArray(temp, 100);
    String s = wifi.urldecode(temp);
/* // monitor this for entertainment if it's on port 80 :)
  Serial.print(" par=");
  Serial.println(p->name());
  Serial.print(" > ");
  Serial.println(s);
*/
    switch( p->name().charAt(0)  )
    {
      case 'k': // key
        s.toCharArray(password, sizeof(password));
        break;
    }
  }

  uint32_t ip = request->client()->remoteIP(); // Bug: need remote IP

  if(strcmp(controlPassword, password))
  {
    if(nWrongPass == 0) // it takes at least 10 seconds to recognize a wrong password
      nWrongPass = 10;
    else if((nWrongPass & 0xFFFFF000) == 0 ) // time doubles for every high speed wrong password attempt.  Max 1 hour
      nWrongPass <<= 1;
    if(ip != lastIP)  // if different IP drop it down
       nWrongPass = 10;
    String data = "{\"ip\":\"";
    data += request->client()->remoteIP().toString(); // Bug: need remote IP
    data += "\",\"pass\":\"";
    data += password; // a String object here adds a NULL.  Possible bug in SDK
    data += "\"}";
    events.send(data.c_str(), "hack" ); // log attempts
    lastIP = ip;
    return;
  }

  lastIP = ip;

  for ( uint8_t i = 0; i < request->params(); i++ ) {
    AsyncWebParameter* p = request->getParam(i);
    p->value().toCharArray(temp, 100);
    String s = wifi.urldecode(temp);
    bool bValue = (s == "true" || s == "1") ? true:false;
//    Serial.println( i + " " + server.argName ( i ) + ": " + s);
 
    switch( p->name().charAt(0) )
    {
      case 'O': // OLED
          ee.bEnableOLED = bValue;
          break;
      case 'P': // PushBullet
          {
            int which = (tolower(p->name().charAt(1) ) == '1') ? 1:0;
            ee.bEnablePB[which] = bValue;
          }
          break;
      case 'M': // Motion
          ee.bMotion = bValue;
          break;
      case 'R': // reset
          if(doorbellTimeIdx)
            doorbellTimeIdx = 0;
          else
            alert_expire = 0; // clear the alert
          break;
      case 'L': // location
          s.toCharArray(ee.location, sizeof(ee.location));
          break;
      case 'm':  // message
          alert_expire = 0; // also clears the alert
          sMessage = p->value();
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

void onBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  //Handle body
}

void onUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
  //Handle upload
}

const char part1[] PROGMEM = "<!DOCTYPE html>\n"
      "<html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>\n"
      "<title>WiFi Doorbell</title>\n"
      "<style type=\"text/css\">\n"
      "div,table,input{\n"
      "border-radius: 5px;\n"
      "margin-bottom: 5px;\n"
      "box-shadow: 2px 2px 12px #000000;\n"
      "background-image: -moz-linear-gradient(top, #ffffff, #50a0ff);\n"
      "background-image: -ms-linear-gradient(top, #ffffff, #50a0ff);\n"
      "background-image: -o-linear-gradient(top, #ffffff, #50a0ff);\n"
      "background-image: -webkit-linear-gradient(top, #efffff, #50a0ff);"
      "background-image: linear-gradient(top, #ffffff, #50a0ff);\n"
      "background-clip: padding-box;\n"
      "}\n"
      "body{width:240px;display:block;margin-left:auto;margin-right:auto;text-align:right;font-family: Arial, Helvetica, sans-serif;}}\n"
      "</style>\n"

      "<script src=\"http://ajax.googleapis.com/ajax/libs/jquery/1.3.2/jquery.min.js\" type=\"text/javascript\" charset=\"utf-8\"></script>\n"
      "<script type=\"text/javascript\">\n"
      "a=document.all\n"
      "oledon=";

const char part2[] PROGMEM = "\nfunction startEvents(){\n"
         "eventSource = new EventSource(\"events\")\n"
         "eventSource.addEventListener('open', function(e){},false)\n"
         "eventSource.addEventListener('error', function(e){},false)\n"
         "eventSource.addEventListener('alert', function(e){alert(e.data)},false)\n"
         "eventSource.addEventListener('state',function(e){\n"
           "d=JSON.parse(e.data)\n"
           "a.mot.innerHTML=d.pir\n"
           "dt=new Date(d.t*1000)\n"
           "a.time.innerHTML=dt.toLocaleTimeString()\n"
           "for(i=0;i<";

const char part3[] PROGMEM = ";i++){\n"
            "item=eval('document.all.t'+i);tm1=eval('d.t'+i)\n"
            "item.innerHTML=tm1\n"
            "item=eval('document.all.r'+i)\n"
            "item.setAttribute('style',tm1.length?'':'display:none')\n"
           "}\n"
         "},false)\n"
      "}\n"
      "function reset(){\n"
        "$.post(\"s\", { R: 0, key: a.myKey.value })\n"
        "location.reload()\n"
      "}\n"
      "function oled(){\n"
        "oledon=!oledon\n"
        "$.post(\"s\", { O: oledon, key: a.myKey.value })\n"
        "a.OLED.value=oledon?'OFF':'ON '\n"
      "}\n"
      "function pbToggle0(){\n"
        "pb0=!pb0\n"
        "$.post(\"s\", { P0: pb0, key: a.myKey.value })\n"
        "a.PB0.value=pb0?'OFF':'ON '\n"
      "}\n"
      "function pbToggle1(){\n"
        "pb1=!pb1\n"
        "$.post(\"s\", { P1: pb1, key: a.myKey.value })\n"
        "a.PB1.value=pb1?'OFF':'ON '\n"
      "}"
      "function motTog(){\n"
        "bmot=!bmot\n"
        "$.post(\"s\", { M: bmot, key: a.myKey.value })\n"
        "a.MOT.value=bmot?'OFF':'ON '\n"
      "}\n"
      "</script>\n"
      "<body onload=\"{\n"
      "key=localStorage.getItem('key')\nif(key!=null) document.getElementById('myKey').value=key\n"
      "for(i=0;i<document.forms.length;i++) document.forms[i].elements['key'].value=key\n"
      "startEvents()\n}\">\n"

      "<div><h3 align=\"center\">WiFi Doorbell</h3>\n"
      "<table align=\"center\">\n"
      "<tr><td><p id=\"time\">"
      "</p></td><td></td></tr>\n"
      "<tr><td colspan=2>Doorbell Rings: <input type=\"button\" value=\"Clear\" id=\"resetBtn\" onClick=\"{reset()}\">"
      "</td></tr>\n";

void handleRoot(AsyncWebServerRequest *request) // Main webpage interface
{
//  Serial.println("handleRoot");
//  Serial.print("handleRoot for ");
//  Serial.println(request->client()->remoteIP().toString());

  parseParams(request);

  AsyncResponseStream *response = request->beginResponseStream("text/html");

  response->print(String(part1));
  String page = "";
  page += ee.bEnableOLED;
  page += "\npb0=";
  page += ee.bEnablePB[0];
  page += "\npb1=";
  page += ee.bEnablePB[1];
  page += "\nbmot=";
  page += ee.bMotion;
  response->print(page);
  response->print(String(part2));
  response->print(String(DB_CNT));
  response->print(String(part3));
  page = "";
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
    page += "</div></td></tr>\n";
  }
  page += "<tr><td>Motion:</td><td><div id=\"mot\">";
  page += timeToTxt(pirStamp);
  page += "</div></td></tr>\n";

  page += "<tr><td>Display:</td><td>"
          "<input type=\"button\" value=\"";
  page += ee.bEnableOLED ? "OFF":"ON ";
  page += "\" id=\"OLED\" onClick=\"{oled()}\">"
          " Mot <input type=\"button\" value=\"";
  page += ee.bMotion ? "OFF":"ON ";
  page += "\" id=\"MOT\" onClick=\"{motTog()}\">"
          "</td></tr>\n"
          "<tr><td>PushBullet:</td><td>"
          "<input type=\"button\" value=\"";
  page += ee.bEnablePB[0] ? "OFF":"ON ";
  page += "\" id=\"PB0\" onClick=\"{pbToggle0()}\">"
          " Mot <input type=\"button\" value=\"";
  page += ee.bEnablePB[1] ? "OFF":"ON ";
  page += "\" id=\"PB1\" onClick=\"{pbToggle1()}\">"
          "</td></tr>\n"
          "<tr><td>Message:</td><td>";
  page += valButton("M", "" );
  page += "</td></tr>\n"
          "<tr><td>Location:</td>";
  page += "<td>";  page += valButton("L", ee.location );
  page += "</td></tr>\n</table>\n";

  response->print(page);
  if(alert_expire)
  {
    response->print(szAlertDescription);
  }

  page = "<input id=\"myKey\" name=\"key\" type=text size=50 placeholder=\"password\" style=\"width: 150px\">"
          "<input type=\"button\" value=\"Save\" onClick=\"{localStorage.setItem('key', key = document.all.myKey.value)}\">\n"
          "<br><small>Logged IP: ";
  page += request->client()->remoteIP().toString();
  page += "</div>\n"
          "Copyright &copy 2016 CuriousTech.net</small>\n"
          "</body>\n</html>";
  response->print(page);

  request->send ( response );
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

void handleS(AsyncWebServerRequest *request)
{
  parseParams(request);

  String page = "{\"ip\": \"";
  page += WiFi.localIP().toString();
  page += ":";
  page += serverPort;
  page += "\"}";
  request->send( 200, "text/json", page );
}

// JSON format for initial or full data read
void handleJson(AsyncWebServerRequest *request)
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
  request->send( 200, "text/json", s );
}

void onRequest(AsyncWebServerRequest *request){
  //Handle Unknown Request
  request->send(404);
}

void onEvents(AsyncEventSourceClient *client)
{
//  client->send(":ok", NULL, millis(), 1000);
  events.send(dataJson().c_str(), "state");
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

  String s = "Doorbell "  + timeToTxt( doorbellTimes[doorbellTimeIdx]);
  events.send(s.c_str(), "alert" );
  events.send(dataJson().c_str(), "state"); // instant update on the web page

  // make sure it's more than 5 mins between triggers to send a PB
  if( newtime - dbTime > 5 * 60)
  {
    if(ee.bEnablePB[0])
    {
      if(!pb.send("Doorbell", "The doorbell rang at " + timeToTxt( doorbellTimes[doorbellTimeIdx]), ee.pbToken ))
      {
        events.send("PushBullet connection failed", "print");
        Serial.println("PB error DB");
      }
    }
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
  String s = "Motion " + timeToTxt( pirStamp );
  events.send(s.c_str(), "alert" );
  events.send(dataJson().c_str(), "state"); // instant update on the web page
  // make sure it's more than 5 mins between triggers to send a PB
  if( newtime - pirTime > 5 * 60)
  {
    if(ee.bEnablePB[1])
       if(!pb.send("Doorbell", "Motion at " + timeToTxt( pirStamp), ee.pbToken ))
       {
        events.send("PushBullet connection failed", "print");
        Serial.println("PB error MOT");
       }
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
  Serial.println();
  WiFi.hostname("doorbell");
  wifi.autoConnect("Doorbell");

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  if ( !MDNS.begin ( "doorbell" ) ) {
    Serial.println ( "MDNS responder error" );
  }

  // attach AsyncEventSource
  events.onConnect(onEvents);
  server.addHandler(&events);

  server.on ( "/", HTTP_GET | HTTP_POST, handleRoot );
  server.on ( "/s", HTTP_GET | HTTP_POST, handleS );
  server.on ( "/json", HTTP_GET, handleJson );

  server.onNotFound(onRequest);
  server.onFileUpload(onUpload);
  server.onRequestBody(onBody);

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

    static uint8_t cnt = 20;
    if(--cnt == 0) // heartbeat I guess
    {
      cnt = 10;
      events.send(dataJson().c_str(), "state");
    }

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
        wuMins = 10; // free account has a max of 10 per minute, 500 per day (every 3 minutes)
      }

      if (hour_save != hour()) // hourly stuff
      {
        hour_save = hour();
        eemem.update(); // update EEPROM if needed
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
      events.send(psValue, "print");
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
      events.send(psValue, "alert");
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
      events.send(psValue, "alert");
      break;
    case 6: // error
      events.send(psValue, "alert");
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
