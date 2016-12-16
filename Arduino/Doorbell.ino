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
#include "pages.h"

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

#define DB_CNT 16
int doorbellTimeIdx = 0;
unsigned long doorbellTimes[DB_CNT];
bool bAutoClear;
unsigned long dbTime;
unsigned long pirTime;

SSD1306 display(0x3c, SDA, SCL); // Initialize the oled display for address 0x3c, sda=12, sdc=13
int displayOnTimer;             // temporary OLED turn-on
String sMessage;

WiFiManager wifi;  // AP page:  192.168.4.1
AsyncWebServer server( serverPort );
AsyncEventSource events("/events"); // event source (Server-Sent events)
AsyncWebSocket ws("/ws"); // access at ws://[esp ip]/ws

PushBullet pb;

eeMem eemem;

String dataJson() // timed/instant pushed data
{
  String s = "{";
  s += "\"t\":";  s += ( now() - (TZ * 3600) );
  s += ",\"weather\":\""; s += szWeatherCond;
  s += "\",\"pir\":";  s += pirTime;
  s += ",\"bellCount\":"; s += doorbellTimeIdx;
  s += ",\"o\":"; s += ee.bEnableOLED;
  s += ",\"pbdb\":"; s += ee.bEnablePB[0];
  s += ",\"pbm\":"; s += ee.bEnablePB[1];
  s += ",\"loc\":";  s += ee.location;
  s += ",\"alert\":\""; s += szAlertDescription; s += "\"";
  s += ",\"temp\":\""; s += String(TempF,1); s += "\"";
  s += ",\"rh\":\""; s += rh; s += "\"";
  s += ",\"wind\":\""; s += szWind; s += "\"";
  s += ",\"db\":[";
  for(int i = 0; i < DB_CNT; i++)
  {
    if(i) s += ",";
    s += doorbellTimes[i];
  }
  s += "]}";
  return s;
}

void wuCondCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue);
JsonClient wuClient(wuCondCallback);
void wuConditions(bool bAlerts);
void jsonCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue);
JsonClient jsonParse(jsonCallback);

void parseParams(AsyncWebServerRequest *request)
{
  char temp[100];
  char password[64] = "";
  int val;
  bool bRemote = false;
  bool ipSet = false;

//  Serial.println("parseParams");

  if(request->params() == 0)
    return;

  // get password first
  for ( uint8_t i = 0; i < request->params(); i++ ) {
    AsyncWebParameter* p = request->getParam(i);

    p->value().toCharArray(temp, 100);
    String s = wifi.urldecode(temp);

    switch( p->name().charAt(0)  )
    {
      case 'k': // key
        s.toCharArray(password, sizeof(password));
        break;
    }
  }

  uint32_t ip = request->client()->remoteIP();

  if(strcmp(controlPassword, password))
  {
    if(nWrongPass == 0) // it takes at least 10 seconds to recognize a wrong password
      nWrongPass = 10;
    else if((nWrongPass & 0xFFFFF000) == 0 ) // time doubles for every high speed wrong password attempt.  Max 1 hour
      nWrongPass <<= 1;
    if(ip != lastIP)  // if different IP drop it down
       nWrongPass = 10;
    String data = "{\"ip\":\"";
    data += request->client()->remoteIP().toString();
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
      case 's': // ssid
          s.toCharArray(ee.szSSID, sizeof(ee.szSSID));
          break;
      case 'p': // pass
          wifi.setPass(s.c_str());
          break;
    }
  }
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

String timeToTxt(unsigned long &t)  // GMT to string "Sun 12:00:00 AM"
{
  tmElements_t tm;
  breakTime(t + (3600 * TZ), tm);
 
  String s = dayShortStr(tm.Wday);
  s += " ";
  int h = tm.Hour;
  if(h == 0) h = 12;
  else if(h > 12) h -= 12;
  s += h;
  s += ":";
  if(tm.Minute < 10)  s += "0";
  s += tm.Minute;
  s += ":";
  if(tm.Second < 10)  s += "0";
  s += tm.Second;
  s += " ";
  s += tm.Hour > 11 ? "PM" : "AM";  
  return s;
}

void reportReq(AsyncWebServerRequest *request) // report full request to PC
{
  String s = "{\"remote\":\"";
  s += request->client()->remoteIP().toString();
  s += "\",\"method\":\"";
  switch(request->method())
  {
    case HTTP_GET: s += "GET"; break;
    case HTTP_POST: s += "POST"; break;
    case HTTP_DELETE: s += "DELETE"; break;
    case HTTP_PUT: s += "PUT"; break;
    case HTTP_PATCH: s += "PATCH"; break;
    case HTTP_HEAD: s += "HEAD"; break;
    case HTTP_OPTIONS: s += "OPTIONS"; break;
    default: s += "<unknown>"; break;
  }
  s += "\",\"host\":\"";
  s += request->host();
  s += "\",\"url\":\"";
  s += request->url();
  s += "\"";
  if(request->contentLength()){
    s +=",\"contentType\":\"";
    s += request->contentType().c_str();
    s += "\",\"contentLen\":";
    s += request->contentLength();
  }

  int headers = request->headers();
  int i;
  if(headers)
  {
    s += ",\"header\":[";
    for(i = 0; i < headers; i++){
      AsyncWebHeader* h = request->getHeader(i);
      if(i) s += ",";
      s +="\"";
      s += h->name().c_str();
      s += "=";
      s += h->value().c_str();
      s += "\"";
    }
    s += "]";
  }
  int params = request->params();
  if(params)
  {
    s += ",\"params\":[";
    for(i = 0; i < params; i++)
    {
      AsyncWebParameter* p = request->getParam(i);
      if(i) s += ",";
      s += "\"";
      if(p->isFile()){
        s += "FILE";
      } else if(p->isPost()){
        s += "POST";
      } else {
        s += "GET";
      }
      s += ",";
      s += p->name().c_str();
      s += "=";
      s += p->value().c_str();
      s += "\"";
    }
    s += "]";
  }
  s +="}";
  events.send(s.c_str(), "request");
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
  reportReq(request);
}

// JSON format for initial or full data read
void handleJson(AsyncWebServerRequest *request)
{
  String s = "{\"";
  s += "weather\": \""; s += szWeatherCond;
  s += "\",\"location\": \""; s += ee.location;
  s += "\",\"bellCount\": "; s += doorbellTimeIdx;
  s += ",\"display\": "; s += ee.bEnableOLED;
  s += ",\"temp\": \""; s += String(TempF,1);
  s += "\",\"rh\": "; s += rh;
  s += ",\"time\": "; s += ( now() - (TZ * 3600) );
  s += ",\"pir\": "; s += pirTime;
  s += ",\"t\":[";
  for(int i = 0; i < DB_CNT; i++)
  {
    s += doorbellTimes[i];
    s += ",";
  }
  s += "]}";
  request->send( 200, "text/json", s );
}

void onEvents(AsyncEventSourceClient *client)
{
  static bool rebooted = true;
  if(rebooted)
  {
    rebooted = false;
    events.send("Restarted", "alert");
  }
  sendState();
}

const char *jsonListCmd[] = { "cmd", // WebSocket command list
  "key",
  "reset",
  "pbdb", // pushbullet doorbell option
  "pbm", // pushbullet motion
  "mot", // motion
  "oled",
  "msg", // message
  "loc", // location
  NULL
};

bool bKeyGood;

void jsonCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue)
{
  if(iName && !bKeyGood)
    return;
  switch(iEvent)
  {
    case 0: // cmd
      switch(iName)
      {
        case 0: // key
          if(!strcmp(psValue, controlPassword)) // first item must be key
            bKeyGood = true;
          break;
        case 1: // reset
          if(doorbellTimeIdx)
          {
            doorbellTimeIdx = 0;
            memset(doorbellTimes, 0, sizeof(doorbellTimes));
            sendState();
          }
          else
            alert_expire = 0; // clear the alert
          break;
        case 2: // pbdb
          ee.bEnablePB[0] = iValue;
          break;
        case 3: // pbm
          ee.bEnablePB[1] = iValue;
          break;
        case 4: // motion
          ee.bMotion =iValue;
          break;
        case 5: // OLED
          ee.bEnableOLED = iValue ? true:false;
          break;
        case 6: // msg
          alert_expire = 0; // also clears the alert
          sMessage = psValue;
          if(ee.bEnableOLED == false && sMessage.length())
          {
            displayOnTimer = 60;
          }
          break;
        case 7: // loc
          strncpy(ee.location, psValue, sizeof(ee.location));
          break;
      }
      break;
  }
}

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len)
{  //Handle WebSocket event
  static bool bReset = true;

  switch(type)
  {
    case WS_EVT_CONNECT:      //client connected
      if(bReset)
      {
        client->printf("alert;restarted");
        bReset = false;
      }
      client->printf("state;%s", dataJson().c_str());
      client->ping();
      break;
    case WS_EVT_DISCONNECT:    //client disconnected
      break;
    case WS_EVT_ERROR:    //error was received from the other end
      break;
    case WS_EVT_PONG:    //pong message was received (in response to a ping request maybe)
      break;
    case WS_EVT_DATA:  //data packet
      AwsFrameInfo * info = (AwsFrameInfo*)arg;
      if(info->final && info->index == 0 && info->len == len){
        //the whole message is in a single frame and we got all of it's data
        if(info->opcode == WS_TEXT){
          data[len] = 0;

          char *pCmd = strtok((char *)data, ";"); // assume format is "name;{json:x}"
          char *pData = strtok(NULL, "");

          if(pCmd == NULL || pData == NULL) break;
          bKeyGood = false; // for callback (all commands need a key)
          jsonParse.process(pCmd, pData);
        }
      }
      break;
  }
}

static int ssCnt = 10;
void sendState()
{
  events.send(dataJson().c_str(), "state"); // instant update on the web page
  ws.printfAll("state;%s", dataJson().c_str());
  ssCnt = 10;
}

// called when doorbell rings (or test)
void doorBell()
{
  if(dbTime == 0) // date isn't set yet
    return;

  unsigned long newtime = now() - (3600 * TZ);

  if( newtime - dbTime < 10) // ignore bounces and double taps
    return;

  doorbellTimes[doorbellTimeIdx] = newtime;

  String s = "Doorbell "  + timeToTxt( doorbellTimes[doorbellTimeIdx]);
  events.send(s.c_str(), "alert" );
  ws.printfAll("alert;%s", s.c_str());
  sendState();
  // make sure it's more than 5 mins between triggers to send a PB
  if( newtime - dbTime > 3 * 60)
  {
    if(ee.bEnablePB[0])
    {
      if(strlen(ee.pbToken) < 30)
        Serial.println("PB token is missing");
      else if(!pb.send("Doorbell", "The doorbell rang at " + timeToTxt( doorbellTimes[doorbellTimeIdx]), ee.pbToken ))
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

  String s = "Motion " + timeToTxt( newtime );
  events.send(s.c_str(), "alert" );
  sendState();
  // make sure it's more than 3 mins between triggers to send a PB
  if( newtime - pirTime > 3 * 60)
  {
    if(ee.bEnablePB[1])
       if(!pb.send("Doorbell", "Motion at " + timeToTxt(newtime), ee.pbToken ))
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

  if(wifi.isCfg())
  {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  
    if ( !MDNS.begin ( "doorbell" ) )
      Serial.println ( "MDNS responder error" );
  }

  // attach AsyncEventSource
  events.onConnect(onEvents);
  server.addHandler(&events);
  // attach AsyncWebSocket
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on ( "/", HTTP_GET | HTTP_POST, [](AsyncWebServerRequest *request){ // main page (avoids call from root)
    parseParams(request);
    if(wifi.isCfg())
      request->send( 200, "text/html", wifi.page() );
    else
      request->send_P( 200, "text/html", page1 );
    reportReq(request);
  });

  server.on ( "/s", HTTP_GET | HTTP_POST, handleS );
  server.on ( "/json", HTTP_GET, handleJson );

  server.onNotFound([](AsyncWebServerRequest *request){ // root and exploits will be called here with *no response* sent back
    //Handle Unknown Request
    reportReq(request);
//    request->send(404);
  });

  server.on("/scan", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "[";
    int n = WiFi.scanComplete();
    if(n == -2){
      WiFi.scanNetworks(true);
    } else if(n){
      for (int i = 0; i < n; ++i){
        if(i) json += ",";
        json += "{";
        json += "\"rssi\":"+String(WiFi.RSSI(i));
        json += ",\"ssid\":\""+WiFi.SSID(i)+"\"";
        json += ",\"bssid\":\""+WiFi.BSSIDstr(i)+"\"";
        json += ",\"channel\":"+String(WiFi.channel(i));
        json += ",\"secure\":"+String(WiFi.encryptionType(i));
        json += ",\"hidden\":"+String(WiFi.isHidden(i)?"true":"false");
        json += "}";
      }
      WiFi.scanDelete();
      if(WiFi.scanComplete() == -2){
        WiFi.scanNetworks(true);
      }
    }
    json += "]";
    request->send(200, "text/json", json);
    json = String();
  });
  server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });
  server.onFileUpload([](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
  });
  server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  });

  server.begin();
  jsonParse.addList(jsonListCmd);

  MDNS.addService("http", "tcp", serverPort);

  if(!wifi.isCfg())
    wuConditions(false);
}

int8_t msgCnt;

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
    if(ee.bEnableOLED == false) // motion activated display on
    {
      displayOnTimer = 30;
      if(doorbellTimeIdx) // keep flashing bell for this time, and auto reset it in 30 secs
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

    if(--ssCnt == 0) // heartbeat I guess
    {
      sendState();
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
              wuConditions(false);
              break;
            } // fall through if not getting conditions (checks alerts every 10 mins)
          case 1:
            wuConditions(true);
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

  if(wifi.isCfg())
    return;

// screen draw from here on (fixed a stack trace dump)
  static int16_t ind;
  static bool blnk = false;
  static long last_blnk;
  static long last_min;
  static int16_t iconX = 0;
  static int16_t infoX = 64;
  String s;

  if(minute() != last_min) // alternate the display to prevent burn
  {
    last_min = minute();
    if(iconX)
    {
      iconX = 0;      infoX = 64;
    }
    else
    {
      infoX = 0;      iconX = 64;
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
      s += dayShortStr(weekday());
      s += " ";
      s += String(day());
      s += " ";
      s += monthShortStr(month());
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
    display.drawPropString(infoX + 5, 23, String(doorbellTimeIdx) ); // count
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

struct cond2icon
{
  const char *pName;
  const char *pIcon;
};
const cond2icon cdata[] = { // row column from image at http://www.alessioatzeni.com/meteocons/
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

void iconFromName(char *pName)
{
  for(int i = 0; cdata[i].pName; i++)
    if(!strcmp(pName, cdata[i].pName))
    {
      pIcon = cdata[i].pIcon;
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
  // alert names
  "type",             // 17
  "description",
  "expires_epoch",
  "message",
  "phenomena", // 5  //: "HT",
  "significance", //: "Y",
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
        dbTime = pirTime = epoch - (3*60); // powerup setting
      break;
    case 16:
      iconFromName(psValue);
      break;
// Alerts
    case 17: // type (3 letter)
      alertIcon(psValue);
      break;
    case 18: // description
      strncpy(szAlertDescription, psValue, sizeof(szAlertDescription) );
      strcat(szAlertDescription, "  "); // separate end and start on scroller
//      Serial.print("alert_desc ");
//      Serial.println(szAlertDescription);
      break;
    case 19: // expires
      alert_expire = iValue;
      alert_expire += (3600 * TZ );
      break;
    case 20: // message (too long to watch on the scroller)
      events.send(psValue, "alert");
      break;
    case 21: // error (like a bad key or url)
      events.send(psValue, "alert");
      break;
  }
}

// Call wunderground for conditions or alerts
void wuConditions(bool bAlerts)
{
  if(ee.location[0] == 0)
    return;
  String path = "/api/";
  path += ee.wuKey;
  if(bAlerts)
    path += "/alerts/q/";
  else
    path += "/conditions/q/";
  path += ee.location;
  path += ".json";

  char sz[64];
  path.toCharArray(sz, sizeof(sz));
  wuClient.begin("api.wunderground.com", sz, 80, false);
  wuClient.addList(jsonList1);
}

struct alert2icon
{
  const char name[4];
  const char *pIcon;
};
const alert2icon data[] = {
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

void alertIcon(char *p)
{

  for(int i = 0; data[i].name[0]; i++)
  {
    if(!strcmp(p, data[i].name))
    {
      pAlertIcon = data[i].pIcon;
      break;
    }
  }
}
