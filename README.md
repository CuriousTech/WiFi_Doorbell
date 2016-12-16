# WiFi Doorbell Notifier

This is an ESP07 based doorbell notifier.  

The 3 terminals connect to the existing 12~24VAC (up to 40V) transformer used to power the bell solenoids and light the neon bulb in the button.  

Btn-DB connects to the wire coming from the button and connects to the bell.<br/>
Btn-TF is the one with a wire cap between the transformer and button.  <br/>
TF-DB is the one coming from the transformer and connects to the bell.<br/>

This uses PushBullet for notifications, which can be enabled from the control page as well as a short list of ring times and last PIR motion trigger time.  It also uses ESPAsyncWebServer for pages, events, WebSockets, Weather Underground, PushBullet and the AP config.  

Wunderground is used for forcast and weather alerts.  If the OLED is truned off, it will only turn on for flashing a bell icon with time, when the PIR motion sensor is triggered (doorbell times will clear after it times out), or a weather alert is active. If the OLED is on, it will show a current condition icon, and scroll the date, time and condition text unless an alert is active.  The doorbell count bar also flashes.  

There's also analog and a few more raw I/O pins broken out for more sensors or controls.  The regulator is 400mA max so it has a bit of room for expansion.  The PIR sensor is supposed to be 5V, but there was a diode and 3V3 regulator that was removed to get it to work with this.  

![esp07doorbell](http://www.curioustech.net/images/doorbell.jpg)  

![esp07doorbellInside](http://www.curioustech.net/images/db2.png)  

The web page.  
![doorbellweb](http://www.curioustech.net/images/doorbellweb2.png)  

![doorbellr1](http://www.curioustech.net/images/doorbellr1.jpg)  
