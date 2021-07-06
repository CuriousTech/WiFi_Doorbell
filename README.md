# WiFi Doorbell Chime

This is a very small doorbell chime based closely on the older version for displaying weather and sending notifications, but eliminates the old mechanical chime. The additional parts are a MOSFET to produce PWM sound, a resistor to eliminate the chime coil and allow parasitic power to supply a smart doorbell or button light, and a WS281B LED ring, but is still compatible with the old project. The resistor should be about 100 ohms to allow power to the smart doorbell, but I don't know the current rating for them all. A 5W should be sufficient. It's connected to the transformer while the button is pressed, so it needs to handle the current for that time. I have a 12VAC transformoer, but many will have 24V, which is much higher.

Simple operation for detection: The way this circuit operates is when the button is pressed, the optocoupler is disalbled, causing the GPIO port (with internal pullup) to go high for longer than 60Hz. The optocoupler vf is 1.5V up to 50mA, but should be 10mA. The limiting resistor value will be different for 12V or 24V, so use the proper value.  

The 3D printed parts have 2 options: A plastic grill or a retainer ring to wrap cloth around and glue. I prefer the latter. The diffuser ring is printed in clear.  

![esp07doorbell2](http://www.curioustech.net/images/wifidoorbellchime.jpg)  
![esp07doorbellparts](http://www.curioustech.net/images/wifidbparts.jpg)  
![esp07doorbellcolor](http://www.curioustech.net/images/wifidbcolor.jpg)  


Previous version below.  

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
