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

// Build with Arduino IDE 1.8.5, esp8266 SDK 2.4.1

//uncomment to enable Arduino IDE Over The Air update code
#define OTA_ENABLE

#include <Wire.h>
#include "icons.h"
#include <TimeLib.h> // http://www.pjrc.com/teensy/td_libs_Time.html
#include <ssd1306_i2c.h>  // https://github.com/CuriousTech/WiFi_Doorbell/tree/master/Libraries/ssd1306_i2c

#include <EEPROM.h>
#include <ESP8266mDNS.h>
#include "WiFiManager.h"
#include <ESPAsyncWebServer.h> // https://github.com/me-no-dev/ESPAsyncWebServer
#include <UdpTime.h> // https://github.com/CuriousTech/ESP07_WiFiGarageDoor/tree/master/libraries/UdpTime

#ifdef OTA_ENABLE
#include <FS.h>
#include <ArduinoOTA.h>
#endif

#include <JsonClient.h> // https://github.com/CuriousTech/WiFi_Doorbell/tree/master/Libraries/jsonClient
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
UdpTime utime;

const char *pIcon[4] ={icon11,NULL};
uint16_t nWeatherID[4];
char szWeather[4][32] = {"NA"}; // short description
char szWeatherLoc[32];
char szWeatherDesc[4][64];   // Severe Thunderstorm Warning, Dense Fog, etc.
float fTemp;
float fTempMin;
float fTempMax;
uint8_t rh;
uint16_t bp;
uint8_t nCloud;
float fWindSpeed;
uint16_t windDeg;
float fWindGust;
bool bStartOLED;

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
AsyncWebSocket ws("/ws"); // access at ws://[esp ip]/ws

PushBullet pb;

eeMem eemem;

void sendState(void);
void Scroller(String s);
void convertIcon(uint8_t ni, bool bNight);
// WebSocket
void jsonCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue);
JsonClient jsonParse(jsonCallback);
JsonClient jsNotif(jsonCallback); // notifier (push to another device)

// OpenWeatherMap api
void owCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue);
JsonClient owClient(owCallback, 2200);
void innerCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue);
JsonClient innerParse(innerCallback, 1200);
void owCall(void);

String dataJson() // timed/instant pushed data
{
  String s = "{";
  s += "\"t\":";  s += ( now() - ((ee.tz + utime.getDST()) * 3600) );
  s += ",\"tz\":"; s += ee.tz;
  s += ",\"weather\":\"";
  for(int i = 0; szWeather[i][0] && i < 4; i++)
  {
    if(i) s += ",";
    s += szWeather[i];
  }
  s += "\",\"pir\":";  s += pirTime;
  s += ",\"bellCount\":"; s += doorbellTimeIdx;
  s += ",\"o\":"; s += ee.bEnableOLED;
  s += ",\"pbdb\":"; s += ee.bEnablePB[0];
  s += ",\"pbm\":"; s += ee.bEnablePB[1];
  s += ",\"loc\":";  s += ee.location;
  s += ",\"alert\":\""; s += szWeatherDesc[0]; s += "\"";
  s += ",\"temp\":\""; s += String(fTemp,1); s += "\"";
  s += ",\"rh\":\""; s += rh; s += "\"";
  s += ",\"wind\":\""; s += String(fWindSpeed,1) + " "; s += windDeg; s += "deg\"";
  s += ",\"db\":[";
  for(int i = 0; i < DB_CNT; i++)
  {
    if(i) s += ",";
    s += doorbellTimes[i];
  }
  s += "]}";
  return s;
}

void parseParams(AsyncWebServerRequest *request)
{
  char temp[100];
  char password[64] = "";
  int val;
  bool bRemote = false;
  bool ipSet = false;

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
    String data = "hack;{\"ip\":\"";
    data += request->client()->remoteIP().toString();
    data += "\",\"pass\":\"";
    data += password; // a String object here adds a NULL.  Possible bug in SDK
    data += "\"}";
    ws.textAll(data);
    data = String();
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
          if(bValue) bStartOLED = true;
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
          break;
      case 'L': // location
          s.toCharArray(ee.location, sizeof(ee.location));
          break;
      case 'm':  // message
          sMessage = p->value();
          if(ee.bEnableOLED == false && sMessage.length())
          {
            displayOnTimer = 60;
          }
          break;
      case 'w': // openweathermap key
          s.toCharArray(ee.owKey, sizeof(ee.owKey));
          break;
      case 'b': // pushbullet token
          s.toCharArray(ee.pbToken, sizeof(ee.pbToken));
          break;
      case 's': // ssid
          s.toCharArray(ee.szSSID, sizeof(ee.szSSID));
          break;
      case 'p': // pass
          wifi.setPass(s.c_str());
          break;
      case 'n': // notifier   /s?key=password&ni="192.168.0.102"&np="/test"&no=82
          switch(p->name().charAt(1))
          {
            case 'i': // ni=
              strncpy(ee.szNotifIP, s.c_str(), sizeof(ee.szNotifIP));
              break;
            case 'p': // np=
              strncpy(ee.szNotifPath, s.c_str(), sizeof(ee.szNotifPath));
              break;
            case 'o': // no=
              ee.NotifPort = atoi(s.c_str());
              break;
          }
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
  breakTime(t + (3600 * (ee.tz + utime.getDST())), tm);
 
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
  String s = "request;{\"remote\":\"";
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
//  events.send(s.c_str(), "request");
  ws.textAll(s);
  s = String();
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
  page = String();
}

// JSON format for initial or full data read
void handleJson(AsyncWebServerRequest *request)
{
  String s = "{";
  s += "\"weather\": \""; s += szWeather[0];
  s += "\",\"location\": \""; s += ee.location;
  s += "\",\"bellCount\": "; s += doorbellTimeIdx;
  s += ",\"display\": "; s += ee.bEnableOLED;
  s += ",\"temp\": \""; s += String(fTemp,1);
  s += "\",\"rh\": "; s += rh;
  s += ",\"time\": "; s += ( now() - ((ee.tz + utime.getDST()) * 3600) );
  s += ",\"pir\": "; s += pirTime;
  s += ",\"t\":[";
  for(int i = 0; i < DB_CNT; i++)
  {
    s += doorbellTimes[i];
    s += ",";
  }
  s += "]}";
  request->send( 200, "text/json", s );
  s = String();
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
  "TZ",
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
          if(iValue) bStartOLED = true;
          break;
        case 6: // msg
          sMessage = psValue;
          if(ee.bEnableOLED == false && sMessage.length())
          {
            displayOnTimer = 60;
          }
          break;
        case 7: // loc
          strncpy(ee.location, psValue, sizeof(ee.location));
          break;
        case 8: // TZ
          ee.tz = iValue;
          utime.start();
          break;
      }
      break;
  }
}

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len)
{  //Handle WebSocket event
  static bool bReset = true;
  String s;

  switch(type)
  {
    case WS_EVT_CONNECT:      //client connected
      if(bReset)
      {
        client->text("alert;Restarted");
        bReset = false;
      }
      s = String("state;") + dataJson().c_str();
      client->text(s);
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

static int ssCnt = 30;
void sendState()
{
  String s = dataJson();
  ws.textAll(String("state;") + s);
  ssCnt = 59 - second();
  if(ssCnt < 20) ssCnt = 59;
}

// called when doorbell rings (or test)
void doorBell()
{
  if(dbTime == 0) // date isn't set yet
    return;

  unsigned long newtime = now() - (3600 * (ee.tz + utime.getDST()));

  if( newtime - dbTime < 10) // ignore bounces and double taps
    return;

  doorbellTimes[doorbellTimeIdx] = newtime;

  String s = "Doorbell " + timeToTxt( doorbellTimes[doorbellTimeIdx]);
  ws.textAll(String("alert;") + s);
  s = String();
  sendState();
  if(ee.szNotifIP[0])
    jsNotif.begin(ee.szNotifIP, ee.szNotifPath, ee.NotifPort, false);
  // make sure it's more than 3 mins between triggers to send a PB
  if( newtime - dbTime > 3 * 60)
  {
    if(ee.bEnablePB[0])
    {
      if(strlen(ee.pbToken) < 30)
        Serial.println("PB token is missing");
      else pb.send("Doorbell", "The doorbell rang at " + timeToTxt( doorbellTimes[doorbellTimeIdx]), ee.pbToken );
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
  unsigned long newtime = now() - (3600 * (ee.tz + utime.getDST()));

  if( newtime - pirTime < 30) // limit triggers
    return;

  String s = String("alert;Motion ") + timeToTxt( newtime );
  ws.textAll(s);
  s = String();
  sendState();
  // make sure it's more than 3 mins between triggers to send a PB
  if( newtime - pirTime > 3 * 60)
  {
    if(ee.bEnablePB[1])
       pb.send("Doorbell", "Motion at " + timeToTxt(newtime), ee.pbToken );
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
  const char doorbell[] = "Doorbell";
  Serial.begin(115200);
  Serial.println();

  pinMode(ESP_LED, OUTPUT);
  pinMode(DOORBELL, INPUT_PULLUP);
  attachInterrupt(DOORBELL, doorbellISR, FALLING);
  pinMode(PIR, INPUT);
  attachInterrupt(PIR, pirISR, FALLING);

  // initialize dispaly
  display.init();
  display.clear();
  display.display();

  WiFi.hostname(doorbell);
  wifi.autoConnect(doorbell, controlPassword);

  if(!wifi.isCfg())
  {
    if ( !MDNS.begin ( doorbell ) )
      Serial.println ( "MDNS responder error" );
  }

  // attach AsyncWebSocket
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on ( "/", HTTP_GET | HTTP_POST, [](AsyncWebServerRequest *request){ // main page (avoids call from root)
    if(wifi.isCfg())
      request->send( 200, "text/html", wifi.page() );
    reportReq(request);
  });

  server.on ( "/iot", HTTP_GET | HTTP_POST, [](AsyncWebServerRequest *request){ // main page (avoids call from root)
    parseParams(request);
    request->send_P( 200, "text/html", page1 );
    reportReq(request);
  });

  server.on ( "/s", HTTP_GET | HTTP_POST, handleS );
  server.on ( "/json", HTTP_GET, handleJson );

  server.onNotFound([](AsyncWebServerRequest *request){ // root and exploits will be called here with *no response* sent back
    //Handle Unknown Request
    reportReq(request);  // Remove if you don't want all kinds of traffic from hacker attempts
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
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse_P(200, "image/x-icon", favicon, sizeof(favicon));
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });
  server.onFileUpload([](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
  });
  server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  });

  server.begin();
  jsonParse.addList(jsonListCmd);

  MDNS.addService("http", "tcp", serverPort);

#ifdef OTA_ENABLE
  ArduinoOTA.begin();
#endif
  if(wifi.isCfg() == false)
    utime.start();
}

int8_t msgCnt;

void loop()
{
  static uint8_t sec_save, min_save, hour_save;
  static bool bPulseLED = false;

  MDNS.update();
#ifdef OTA_ENABLE
  ArduinoOTA.handle();
#endif
  if(!wifi.isCfg())
    utime.check(ee.tz);

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

      if (hour_save != hour())  // update our time daily (at 2AM for DST)
      {
        if( (hour_save = hour()) == 2)
          utime.start();
        eemem.update(); // update EEPROM if needed while we're at it (give user time to make many adjustments)
      }
      if (min_save == 0) // hourly stuff
      {
        unsigned long t = now() - (3600 * ee.tz);
        if(doorbellTimeIdx && doorbellTimes[0] < t - (3600 * 24 *7) ) // 1 week old
        {
          doorbellTimeIdx--;
          if(doorbellTimeIdx)
            memcpy(doorbellTimes, &doorbellTimes[doorbellTimeIdx], sizeof(unsigned long) * (DB_CNT-doorbellTimeIdx) );
        }
      }
      static uint8_t owCnt = 1;
      if(--owCnt == 0)
      {
        owCnt = 5; // every 5 mins
        owCall();
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
    if(iconX){ iconX = 0;   infoX = 64; }
    else     { infoX = 0;   iconX = 64; }
  }

  // draw the screen here
  display.clear();

  if( (millis() - last_blnk) > 400) // 400ms toggle for blinky things
  {
    last_blnk = millis();
    blnk = !blnk;
  }
  if(bStartOLED)
  {
    bStartOLED = false;
    display.init();
  }
  if( (ee.bEnableOLED || displayOnTimer) && (bAutoClear == false)) // skip for motion enabled display
  {
    static uint8_t nIcon;
    static uint8_t nSec;
    if(nSec != second())
    {
      nSec != second();
      nIcon = (nIcon + 1) & 3; // alternate multiple icons
      if(pIcon[nIcon] == NULL)
        nIcon = 0;
    }

    display.drawXbm(iconX+10, 20, 44, 42, pIcon[nIcon]);
  
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
      for(int i = 0; szWeather[i] && i < 4; i++)
      {
        s += szWeather[i];
        s += "  ";
      }
    }
    Scroller(s);
    s = String();

    display.drawPropString(infoX, 23, String(fTemp, 1) + "]" );
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
  if(w > 40) w = 10; // bug
  String sPart = s.substring(ind, ind + 18);
  display.drawPropString(x, 0, sPart );

  if( --x <= -(w))
  {
    x = 0;
    if(++ind >= len) // reset at last char
    {
      ind = 0;
      if(msgCnt) // end of custom message display
      {
        if(--msgCnt == 0) // decrement times to repeat it
        {
          sMessage = String();
        }
      }
    }
  }
}

///////////////
// things we want from conditions request
const char *jsonListOw[] = { "",
  "weather",   // 0
  "main",      // 
  "wind",      //
  "rain",
  "snow",
  "clouds",    //
  "dt",        //
  "sys",       //
  "id",        //
  "name",      //
  "cod",      // 10
  NULL
};

uint8_t nWeatherIdx;

void owCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue)
{
  if(iEvent == -1) // connection events
    return;

  switch(iName)
  {
    case 0: // weather [{},{},{}] array
      nWeatherIdx = 0;
      memset(pIcon, 0, sizeof(pIcon));
      memset(nWeatherID, 0, sizeof(nWeatherID));
      memset(szWeather, 0, sizeof(szWeather));
      memset(szWeatherDesc, 0, sizeof(szWeatherDesc));
      innerParse.process("weather", psValue);
      break;
    case 1: // main
      innerParse.process("main", psValue);
      break;
    case 2: // wind
      innerParse.process("wind", psValue);
      break;
    case 3: // rain
      innerParse.process("rain", psValue);
      break;
    case 4: // snow
      innerParse.process("snow", psValue);
      break;
    case 5: // clouds
      innerParse.process("clouds", psValue);
      break;
    case 6: // dt
//      ws.textAll(String("print;dt ") + psValue);
      break;
    case 7: // sys {"type":1,"id":1128,"message":0.0033,"country":"US","sunrise":1542284504,"sunset":1542320651}
//      ws.textAll(String("print;sys ") + psValue);
      break;
    case 8: // id
      break;
    case 9: // name
      strncpy(szWeatherLoc, psValue, strlen(szWeatherLoc));
      break;
    case 10: // cod
      if(iValue != 200)
        ws.textAll(String("print;errcode ") + iValue);
      break;
  }
}

const char *jsonListWeather[] = { "weather",
  "id",      // 0
  "main",
  "description",
  "icon",
  NULL
};
const char *jsonListMain[] = { "main",
  "temp",      // 0
  "pressure",
  "humidity",
  "temp_min",
  "temp_max",
  NULL
};
const char *jsonListWind[] = { "wind",
  "speed",      // 0
  "deg",
  "gust",
  NULL
};
const char *jsonListRain[] = { "rain",
  "1h",      // 0
  "3h",
  NULL
};
const char *jsonListSnow[] = { "snow",
  "1h",      // 0
  "3h",
  NULL
};
const char *jsonListClouds[] = { "clouds",
  "all",      // 0
  NULL
};

// Call OpenWeathermap API
void owCall()
{
  String path = "/data/2.5/weather?id=";

  path += ee.location;
  path += "&APPID=";
  path += ee.owKey;
  path += "&units=imperial";

  owClient.begin("api.openweathermap.org", path.c_str(), 80, false);
  owClient.addList(jsonListOw);
  static bool bAdded;
  if(!bAdded){
    innerParse.addList(jsonListWeather);
    innerParse.addList(jsonListMain);
    innerParse.addList(jsonListWind);
    innerParse.addList(jsonListRain);
    innerParse.addList(jsonListSnow);
    innerParse.addList(jsonListClouds);
    bAdded = true;
  }
}

void innerCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue)
{
  if(iEvent == -1) // connection events
    return;

  String s;

  switch(iEvent)
  {
    case 0:// weather
      switch(iName)
      {
        case 0: // id
          nWeatherID[nWeatherIdx] = iValue;
          break;
        case 1: // main
          strncpy(szWeather[nWeatherIdx], psValue, sizeof(szWeather[0]));
          break;
        case 2: // description
          strncpy(szWeatherDesc[nWeatherIdx], psValue, sizeof(szWeatherDesc[0]));
          break;
        case 3: // icon
          convertIcon(iValue, (psValue[2] == 'n')?true:false);
          if(nWeatherIdx < 3) nWeatherIdx++;
          break;
      }
      break;
    case 1:// main
      switch(iName)
      {
        case 0: // temp
          fTemp = atof(psValue);
          break;
        case 1: // pressure
          bp = iValue;
          break;
        case 2: // humidity
          rh = iValue;
          break;
        case 3: // temp_min
          fTempMin = atof(psValue);
          break;
        case 4: // temp_max
          fTempMax = atof(psValue);
          break;
      }
      break;
    case 2:// wind
      switch(iName)
      {
        case 0: // speed
          fWindSpeed = atof(psValue);
          fWindGust = 0; // reset in case it's missing
          break;
        case 1: // deg
          windDeg = iValue;
          break;
        case 2: // gust
          fWindGust = atof(psValue);
          break;
      }
      break;
    case 3:// rain
      switch(iName)
      {
        case 0: // 1h
          break;
        case 1: // 3h
          break;
      }
      break;
    case 4:// snow
      switch(iName)
      {
        case 0: // 1h
          break;
        case 1: // 3h
          break;
      }
      break;
    case 5:// clouds
      switch(iName)
      {
        case 0: // all
          nCloud = iValue;
          break;
      }
      break;
  }
}

//////////////

struct cond2icon
{
  const uint8_t num;
  const char *pIconDay;
  const char *pIconNight;
};
const cond2icon cdata[] = { // row column from image at http://www.alessioatzeni.com/meteocons/
  { 1, icon11, icon12},// clear sky     codes at https://openweathermap.org/weather-conditions
  { 2, icon21, icon22}, // few clouds
  { 3, icon32, icon71}, // scattered clouds
  { 4, icon51, icon75}, // broken clouds
  { 9, icon36, icon65}, // shower rain
  {10, icon35, icon64},// rain
  {11, icon33, icon76}, // thunderstorm
  {13, icon43, icon71}, // snow
  {50, icon24, icon31}, // mist
  {NULL, NULL}
};

void convertIcon(uint8_t ni, bool bNight)
{
  for(int i = 0; cdata[i].num; i++)
  {
    if(ni == cdata[i].num)
    {
      pIcon[nWeatherIdx] = (bNight) ? cdata[i].pIconNight:cdata[i].pIconDay;
      break;
    }
  }
}
