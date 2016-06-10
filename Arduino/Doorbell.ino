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

// Build with Arduino IDE 1.6.9, esp8266 SDK 2.2.0

#include <Wire.h>
#include "icons.h"
#include <DHT.h> // http://www.github.com/markruys/arduino-DHT
#include <TimeLib.h> // http://www.pjrc.com/teensy/td_libs_Time.html
#include <ssd1306_i2c.h> // https://github.com/CuriousTech/WiFi_Doorbell/Libraries/ssd1306_i2c

#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <EEPROM.h>
#include <ESP8266mDNS.h>
#include "WiFiManager.h"
#include <ESP8266WebServer.h>
#include <Event.h>  // https://github.com/CuriousTech/ESP8266-HVAC/tree/master/Libraries/Event
#include <JsonClient.h> // https://github.com/CuriousTech/ESP8266-HVAC/tree/master/Libraries/JsonClient
 
const char *controlPassword = "password"; // device password for modifying any settings
int serverPort = 80;
const char *pbToken = "pushbullet token here";
const char *wuKey = "wunderground key goes here";

#define ESP_LED    2  // low turns on ESP blue LED
#define DOORBELL   5
#define SCL        13
#define SDA        12

uint32_t lastIP;
int nWrongPass;

const char *pIcon = icon12;
char szWeatherCond[32] = "NA";
char szWind[32] = "NA";
char szAlertType[8];
char szAlertDescription[32];
char szAlertMessage[4096];
unsigned long alert_expire = 0;
float TempF  = 82.6;
int rh = 50;
int8_t TZ;
int8_t DST;

struct timeStamp{
  uint8_t x,wd,h,m,s,a;
};

timeStamp doorbellTimes[16];
int doorbellTimeIdx = 0;

SSD1306 display(0x3c, SDA, SCL); // Initialize the oled display for address 0x3c, sda=12, sdc=13
int displayOnTimer;
String sMessage;

WiFiManager wifi(0);  // AP page:  192.168.4.1
ESP8266WebServer server( serverPort );

int httpPort = 80; // may be modified by open AP scan

struct eeSet // EEPROM backed data
{
  uint16_t size;          // if size changes, use defauls
  uint16_t sum;           // if sum is diiferent from memory struct, write
  char    location[32];   // location for wunderground
  bool    bEnablePB;
  bool    bEnableOLED;
  char    reserved[64];
};
eeSet ee = { sizeof(eeSet), 0xAAAA,
  "41042", // "KKYFLORE10"
  true,   // Enable pushbullet
  true,   // Enable OLED
  ""
};

String dataJson()
{
    String s = "{\"weather\": \"";
    s += szWeatherCond;
    s += "\"}";
    return s;
}

eventHandler event(dataJson);
void wuCondCallback(uint16_t iEvent, uint16_t iName, int iValue, char *psValue);
JsonClient wuClient1(wuCondCallback);
void wuConditions(void);
void wuAlertsCallback(uint16_t iEvent, uint16_t iName, int iValue, char *psValue);
JsonClient wuClient2(wuAlertsCallback);
void wuAlerts(void);

const char days[7][4] = {"Sun","Mon","Tue","Wed","Thr","Fri","Sat"};
const char months[12][4] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

String ipString(IPAddress ip) // Convert IP to string
{
  String sip = String(ip[0]);
  sip += ".";
  sip += ip[1];
  sip += ".";
  sip += ip[2];
  sip += ".";
  sip += ip[3];
  return sip;
}

bool parseArgs()
{
  char temp[100];
  String password;
  int val;
  bool bRemote = false;
  bool ipSet = false;
  eeSet save;
  memcpy(&save, &ee, sizeof(ee));

  Serial.println("parseArgs");

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    server.arg(i).toCharArray(temp, 100);
    String s = wifi.urldecode(temp);
//    Serial.println( i + " " + server.argName ( i ) + ": " + s);
    bool which = (tolower(server.argName(i).charAt(1) ) == 'n') ? 1:0;
 
    switch( server.argName(i).charAt(0)  )
    {
      case 'k': // key
          password = s;
          break;
      case 'O': // OLED
          ee.bEnableOLED = (s == "true") ? true:false;
          break;
      case 'P': // PushBullet
          ee.bEnablePB = (s == "true") ? true:false;
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
      case 't': // test
          doorBell();
          break;
    }
  }

  uint32_t ip = server.client().remoteIP();

  if(server.args() && (password != controlPassword) )
  {
    if(nWrongPass == 0) // it takes at least 10 seconds to recognize a wrong password
      nWrongPass = 10;
    else if((nWrongPass & 0xFFFFF000) == 0 ) // time doubles for every high speed wrong password attempt.  Max 1 hour
      nWrongPass <<= 1;
    if(ip != lastIP)  // if different IP drop it down
       nWrongPass = 10;
    String data = "{ip:\"";
    data += ipString(ip);
    data += "\",pass:\"";
    data += password;
    data += "\"}";
    event.push("hack", data); // log attempts
  }

  if(nWrongPass) memcpy(&ee, &save, sizeof(ee)); // undo any changes

  lastIP = ip;
}

void handleRoot() // Main webpage interface
{
  Serial.println("handleRoot");

  parseArgs();

  String page = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>"
      "<title>WiFi Doorbell</title>"
      "<style>div,input {margin-bottom: 5px;}body{width:260px;display:block;margin-left:auto;margin-right:auto;text-align:right;font-family: Arial, Helvetica, sans-serif;}}</style>"

      "<script src=\"http://ajax.googleapis.com/ajax/libs/jquery/1.3.2/jquery.min.js\" type=\"text/javascript\" charset=\"utf-8\"></script>"
      "<script type=\"text/javascript\">"
      "oledon=";
  page += ee.bEnableOLED;
  page += "pb=";
  page += ee.bEnablePB;
  page += ";function startEvents()"
      "{"
         "eventSource = new EventSource(\"events?i=60&p=1\");"
         "eventSource.addEventListener('open', function(e){},false);"
         "eventSource.addEventListener('error', function(e){},false);"
         "eventSource.addEventListener('alert', function(e){alert(e.data)},false);"
         "eventSource.addEventListener('state',function(e){"
           "d = JSON.parse(e.data);"
         "},false)"
      "}"
      "function reset(){"
        "$.post(\"s\", { R: 0, key: document.all.myKey.value });"
        "location.reload();"
      "}"
      "function oled(){"
        "oledon=!oledon;"
        "$.post(\"s\", { O: oledon, key: document.all.myKey.value });"
        "document.all.OLED.value=oledon?'OFF':'ON'"
      "}"
      "function pbToggle(){"
        "pb=!pb;"
        "$.post(\"s\", { P: pb, key: document.all.myKey.value });"
        "document.all.PB.value=pb?'OFF':'ON'"
      "}"
      "setInterval(timer,1000);"
      "t=";
  page += now() - (3600 * TZ);// - (DST * 3600); // set to GMT
  page +="000;function timer(){" // add 000 for ms
          "t+=1000;d=new Date(t);"
          "document.all.time.innerHTML=d.toLocaleTimeString()}"
          "</script>"

      "<body onload=\"{"
      "key=localStorage.getItem('key');if(key!=null) document.getElementById('myKey').value=key;"
      "for(i=0;i<document.forms.length;i++) document.forms[i].elements['key'].value=key;"
      "startEvents();}\">";

  page += "<h3>WiFi Doorbell</h3>"
          "<table align=\"right\">"
          "<tr><td><p id=\"time\">";
  page += timeFmt();
  page += "</p></td><td>";
  page += "<input type=\"button\" value=\"Reset\" id=\"resetBtn\" onClick=\"{reset()}\">"
          "</td>"
          "</tr>";
  if(doorbellTimeIdx)
  {
    page += "<tr><td colspan=2>Doorbell Rings:</td></tr>";
    for(int i = 0; i < doorbellTimeIdx; i++)
    {
      page += "<tr><td colspan=2>";
      page += timeToTxt(doorbellTimes[i]);
      page += "</tr></td>";
    }
  }
  page += "<tr><td>Display:</td><td>"
          "<input type=\"button\" value=\"";
  page += ee.bEnableOLED ? "OFF":"ON";
  page += "\" id=\"OLED\" onClick=\"{oled()}\">"
          "</td></tr>";
          "<tr><td>PushBullet:</td><td>"
          "<input type=\"button\" value=\"";
  page += ee.bEnablePB ? "OFF":"ON";
  page += "\" id=\"PB\" onClick=\"{pbToggle()}\">"
          "</td></tr>"
          "<tr><td>Message:</td><td>";
  page += valButton("M", "" );
  page += "</td></tr>"
          "<tr><td>Location:</td>";
  page += "<td>";  page += valButton("L", ee.location );
  page += "</td></tr>";

  page += "</table>"

          "<input id=\"myKey\" name=\"key\" type=text size=50 placeholder=\"password\" style=\"width: 150px\">"
          "<input type=\"button\" value=\"Save\" onClick=\"{localStorage.setItem('key', key = document.all.myKey.value)}\">"
          "<br><small>Logged IP: ";
  page += ipString(server.client().remoteIP());
  page += "</small><br></body></html>";

  server.send ( 200, "text/html", page );
}

String button(String id, String text) // Reset button
{
  String s = "<form method='post'><input name='";
  s += id;
  s += "' type='submit' value='";
  s += text;
  s += "'><input type=\"hidden\" name=\"key\" value=\"value\"></form>";
  return s;
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
    parseArgs();

    String page = "{\"ip\": \"";
    page += ipString(WiFi.localIP());
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
  event.set(server.client(), interval, nType); // copying the client before the send makes it work with SDK 2.2.0
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
  if(doorbellTimeIdx >= 16)
    return;

  doorbellTimes[doorbellTimeIdx].wd = weekday()-1;
  doorbellTimes[doorbellTimeIdx].h = hourFormat12();
  doorbellTimes[doorbellTimeIdx].m = minute();
  doorbellTimes[doorbellTimeIdx].s = second();
  doorbellTimes[doorbellTimeIdx].a = isPM();
  event.alert("Doorbell "  + timeToTxt( doorbellTimes[doorbellTimeIdx]) );
  if(ee.bEnablePB)
    pushBullet("Doorbell", "The doorbell rang at " + timeToTxt( doorbellTimes[doorbellTimeIdx]) );
  doorbellTimeIdx++;
}

volatile bool doorBellTriggered = false;

void doorbellISR()
{
  doorBellTriggered = true;
}

void setup()
{
  pinMode(ESP_LED, OUTPUT);
  pinMode(DOORBELL, INPUT_PULLUP);
  attachInterrupt(DOORBELL, doorbellISR, FALLING);

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

  MDNS.update();
  server.handleClient();
  wuClient1.service();
  wuClient2.service();

  if(doorBellTriggered)
  {
    doorBellTriggered = false;
    doorBell();
  }

  if(sec_save != second()) // only do stuff once per second (loop is maybe 20-30 Hz)
  {
    sec_save = second();

    if(min_save != minute())
    {
      min_save = minute();

      if(ee.location[0] && --wuMins == 0) // call wunderground API
      {
        switch(++wuCall) // put a list of wu callers here
        {
          case 0:
            if(ee.bEnableOLED)
            {
              wuConditions();
              break;
            } // fall through if not getting conditions
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

    if(displayOnTimer) // if alerts or messages turn the display on
      displayOnTimer--;

    if(nWrongPass)
      nWrongPass--;

    if(alert_expire && alert_expire > now()) // if active alert
    {
      alert_expire = 0;
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
  static long last;
 
  // draw the screen here
  display.clear();

  if( (millis() - last) > 400) // 400ms togle for blinker
  {
    last = millis();
    blnk = !blnk;
  }

  if(ee.bEnableOLED || displayOnTimer)
  {
    String s;

    if(alert_expire) // if theres an alert message
    {
      sMessage = szAlertMessage; // set the scroller message
      if(blnk) display.drawXbm(0,20, 44, 42, pIcon);
    }
    else
    {
      display.drawXbm(0,20, 44, 42, pIcon);
    }
  
    if(sMessage.length()) // message sent over wifi or alert
    {
      s = sMessage;
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

    display.drawPropString(60, 23, String(TempF, 1) + "]" );
    display.drawString(80, 42, String(rh) + "%");
    if(blnk && doorbellTimeIdx) // blink small gauge if doorbell logged
    { // up to 64 pixels (128x64 with 44 for icon+space = 82 left)
      display.fillRect(46, 61, doorbellTimeIdx << 2, 3);
    }
  }
  else if(doorbellTimeIdx) // blink a bell if doorbell logged and display is off
  {
    display.drawPropString(0, 0, timeToTxt(doorbellTimes[0]) ); // the first time
    if(blnk) display.drawXbm(0,20, 44, 42, bell);
    display.drawPropString(60, 23, String(doorbellTimeIdx) ); // count
  }
  else if(alert_expire) // blink the alert
  {
    if(blnk) display.drawXbm(0,20, 44, 42, pIcon);
    display.drawPropString(60, 23, szAlertType); // abrv alert

    Scroller(szAlertMessage);
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
      if(msgCnt) // end of custom message display
      {
        if(--msgCnt == 0) // decrement times to repeat it
            sMessage = "";
      }
    }
  }
}

void iconFromName(char *pName)
{
  const char *iconNames[] = {
    "chanceflurries", // 0
    "chancerain",
    "chancesleet",
    "chancesnow",
    "chancetstorms",
    "clear", // 5
    "cloudy",
    "flurries",
    "fog",
    "hazy",
    "mostlycloudy", // 10
    "mostlysunny",
    "partlycloudy",
    "partlysunny",
    "rain",
    "sleet", // 15
    "snow",
    "sunny",
    "tstorms",

    "nt_chanceflurries",
    "nt_chancerain", // 20
    "nt_chancesleet",
    "nt_chancesnow",
    "nt_chancetstorms",
    "nt_clear", // 24
    "nt_cloudy", // 25
    "nt_flurries",
    "nt_fog",
    "nt_hazy",
    "nt_mostlycloudy",
    "nt_mostlysunny", //30
    "nt_partlycloudy",
    "nt_partlysunny",
    "nt_rain",
    "nt_sleet",
    "nt_snow",
    "nt_sunny",
    "nt_tstorms",
    NULL
  };

  int nIcon;
  for(nIcon = 0; iconNames[nIcon]; nIcon++)
    if(!strcmp(pName, iconNames[nIcon]))
      break;

  switch(nIcon) // rown column from image
  {
    case 0:   pIcon = icon72; break;
    case 1:   pIcon = icon64; break;
    case 2:   pIcon = icon44; break;
    case 3:   pIcon = icon43; break;
    case 4:   pIcon = icon34; break;
    case 5:   pIcon = icon54; break;
    case 6:   pIcon = icon75; break;
    case 7:   pIcon = icon73; break;
    case 8:   pIcon = icon31; break;
    case 9:   pIcon = icon11; break;
    case 10:  pIcon = icon62; break;
    case 11:  pIcon = icon56; break;
    case 12:  pIcon = icon22; break;
    case 13:  pIcon = icon56; break;
    case 14:  pIcon = icon65; break;
    case 15:  pIcon = icon73; break;
    case 16:  pIcon = icon74; break;
    case 17:  pIcon = icon54; break;
    case 18:  pIcon = icon53; break;
    case 19:  pIcon = icon45; break;
    case 20:  pIcon = icon35; break;
    case 21:  pIcon = icon42; break;
    case 22:  pIcon = icon44; break;
    case 23:  pIcon = icon33; break;
    case 24:  pIcon = icon13; break;
    case 25:  pIcon = icon32; break;
    case 26:  pIcon = icon45; break;
    case 27:  pIcon = icon26; break;
    case 28:  pIcon = icon25; break;
    case 29:  pIcon = icon32; break;
    case 30:  pIcon = icon14; break;
    case 31:  pIcon = icon23; break;
    case 32:  pIcon = icon23; break;
    case 33:  pIcon = icon22; break;
    case 34:  pIcon = icon36; break;
    case 35:  pIcon = icon46; break;
    case 36:  pIcon = icon56; break;
    case 37:  pIcon = icon34; break;
  }
}

// things we want from conditions request
const char *jsonList1[] = { "",
  "weather",           // 0
  "temp_f",
  "relative_humidity",
  "wind_string",       //       Calm
  "wind_dir",          //       East, ENE
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
  NULL
};

void wuCondCallback(uint16_t iEvent, uint16_t iName, int iValue, char *psValue)
{
  static unsigned long epoch;

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
      break;
    case 16:
      iconFromName(psValue);
      break;
  }
}

void wuConditions()
{
  if(ee.location[0] == 0)
    return;
  String path = "/api/";
  path += wuKey;
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
  NULL
};

void wuAlertsCallback(uint16_t iEvent, uint16_t iName, int iValue, char *psValue)
{
  switch(iName)
  {
    case 0: // type
      strncpy(szAlertType, psValue, sizeof(szAlertType));
      break;
    case 1: // description
      strncpy(szAlertDescription, psValue, sizeof(szAlertDescription) );
      Serial.print("alert_desc ");
      Serial.println(szAlertDescription);
      break;
    case 2: // expires
      alert_expire = iValue;
      alert_expire += (3600 * TZ );

      Serial.print("alert_expires ");
      Serial.println(alert_expire);
      break;
    case 3: // message
      {
        int i;
        for(i = 0; i < sizeof(szAlertMessage)-1; i++)
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
        szAlertMessage[i] = 0; //null term
      }

      Serial.print("alert_message ");
      Serial.println(szAlertMessage);
      break;
  }
}
//http://api.wunderground.com/api/3cc233e283d04bf7/alerts/q/41042.json

void wuAlerts()
{
  if(ee.location[0] == 0)
    return;

  String path = "/api/";
  path += wuKey;
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

  String data = "{\"type\": \"note\", \"title\": \"";
  data += pTitle;
  data += "\", \"body\": \"";
  data += sBody;
  data += "\"}";

  client.print(String("POST ") + url + " HTTP/1.1\r\n" +
              "Host: " + host + "\r\n" +
              "Content-Type: application/json\r\n" +
              "Access-Token: " + pbToken + "\r\n" +
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