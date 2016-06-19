# WiFi_Doorbell

This is an ESP07 based doorbell notifier.  

The 3 terminals connect to the existing 12~24VAC transformer used to power the bell solenoids and light the neon bulb in the button.  

Btn-DB connects to the wire comming from the button and connects to the bell.<br/>
Btn-TF is the one with a wire cap between the transformer and button.  <br/>
FT-DB is the one coming from the transformer and connect to the bell.<br/>

This uses PushBullet for notifications, which can be enabled from the control page as well as a short list of ring times and last PIR motion trigger time.  It also uses my event library for logging all triggers, but the PB notifications have to by 5 minutes apart.  The motion triggers should probably be longer.  

The rest of the code is primarily OLED and wunderground for forcast and weather alerts.  If the OLED is truned off, it will only turn on for flashing a bell icon with time.  I think alerts are flashed with a text scroller as well.  If the OLED is on, it will show a current condition icon, and scroll the date, time and condition text unless an alert is active.  The doorbell count bar also flashes.

There's also analog and a few more raw I/O pins broken out for more sensors or controls.  The regulator is 400mA max so it has a bit of room for expansion.  I added a small PIR sensor.  It's supposed to be 5V, but there was a diode and 3V3 regulator that was removed to get it to work with this.  

![esp07tool](http://www.curioustech.net/images/doorbell.jpg)  
