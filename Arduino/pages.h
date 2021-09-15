const char page1[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en"><head><meta name="viewport" content="width=device-width, initial-scale=1"/>
<title>WiFi Doorbell</title>
<style type="text/css">
div,table,input{
border-radius: 5px;
margin-bottom: 5px;
box-shadow: 2px 2px 12px #000000;
background-image: -moz-linear-gradient(top, #ffffff, #50a0ff);
background-image: -ms-linear-gradient(top, #ffffff, #50a0ff);
background-image: -o-linear-gradient(top, #ffffff, #50a0ff);
background-image: -webkit-linear-gradient(top, #efffff, #50a0ff);background-image: linear-gradient(top, #ffffff, #50a0ff);
background-clip: padding-box;
}
.dropdown {
    position: relative;
    display: inline-block;
}
.dropbtn {
    background-color: #50a0ff;
    padding: 1px;
    font-size: 12px;
  background-image: -webkit-linear-gradient(top, #efa0b0, #50a0ff);
  border-radius: 5px;
  margin-bottom: 5px;
  box-shadow: 2px 2px 12px #000000;
}
.btn {
    background-color: #50a0ff;
    padding: 1px;
    font-size: 12px;
    min-width: 50px;
    border: none;
}
.dropdown-content {
    display: none;
    position: absolute;
    background-color: #919191;
    min-width: 40px;
    min-height: 1px;
    z-index: 1;
}
.dropdown:hover .dropdown-content {display: block;}
.dropdown:hover .dropbtn {background-color: #3e8e41;}
.dropdown:hover .btn {  background-image: -webkit-linear-gradient(top, #efffff, #a0a0ff);}
body{width:240px;display:block;margin-left:auto;margin-right:auto;text-align:center;font-family: Arial, Helvetica, sans-serif;}}
</style>
<script type="text/javascript">
a=document.all
oledon=0
pb0=0
pb1=0
bmot=0
function startEvents(){
//ws = new WebSocket("ws://192.168.31.178/ws")
ws = new WebSocket("ws://"+window.location.host+"/ws")
ws.onopen = function(evt) { }
ws.onclose = function(evt) { alert("Connection closed."); }
ws.onmessage = function(evt) {
 lines = evt.data.split(';')
 event=lines[0]
 data=lines[1]
 console.log(data)
 if(event == 'state')
 {
  d=JSON.parse(data)
  dt=new Date(d.t*1000)
  a.time.innerHTML=dt.toLocaleTimeString()
  for(i=0;i<16;i++){
   dt=new Date(d.db[i]*1000)
   document.getElementById('t'+i).innerHTML=dt.toString().split(' ')[0]+' '+dt.toLocaleTimeString()
   document.getElementById('r'+i).setAttribute('style',d.db[i]?'':'display:none')
  }
  oledon=d.o
  pb0=d.pbdb
  pb1=d.pbm
  dt=new Date(d.pir*1000)
  a.mot.innerHTML=dt.toString().split(' ')[0]+' '+dt.toLocaleTimeString()
  a.loc.value=d.loc
  a.OLED.value=oledon?'ON ':'OFF'
  a.MOT.value=bmot?'ON ':'OFF'
  a.PB0.value=pb0?'ON ':'OFF'
  a.PB1.value=pb1?'ON ':'OFF'
  a.br.value=d.br
  a.temp.innerHTML=d.temp+'&deg '+d.rh+'%'
  a.wea.innerHTML=d.weather
  a.alert.innerHTML=d.alert
  a.wind.innerHTML='Wind '+d.wind
 }
 else if(event == 'alert')
 {
  alert(data)
 }
}
}
function setVar(varName, value)
{
 ws.send('cmd;{"key":"'+a.myKey.value+'","'+varName+'":'+value+'}')
}
function reset(){setVar('reset', 0)}
function oled(){
 oledon=!oledon
 setVar('oled', oledon?1:0)
 a.OLED.value=oledon?'ON':'OFF'
}
function pbToggle0(){
pb0=!pb0
setVar('pbdb', pb0?1:0)
a.PB0.value=pb0?'ON ':'OFF'
}
function pbToggle1(){
pb1=!pb1
setVar('pbm', pb1?1:0)
a.PB1.value=pb1?'ON ':'OFF'
}
function motTog(){
bmot=!bmot
setVar('mot', bmot?1:0)
a.MOT.value=bmot?'ON ':'OFF'
}
function setLoc(){setVar('loc', a.loc.value)}
function setBr(){setVar('br', a.br.value)}
function sendMus(n){setVar('play', n)}
function setEff(n){setVar('ef', n)}
function setCnt(n){setVar('cnt', n)}
</script>
</head>
<body onload="{
key=localStorage.getItem('key')
if(key!=null) document.getElementById('myKey').value=key
startEvents()
}">
<div><h3>WiFi Doorbell</h3>
<table align="center">
<tr><td><div id="time"></div></td><td><div id="temp"></div></td></tr>
<tr><td colspan=2>Doorbell Rings: <input type="button" value="Clear" id="resetBtn" onClick="{reset()}"></td></tr>
<tr id=r0 style="display:none"><td></td><td><div id="t0"></div></td></tr>
<tr id=r1 style="display:none"><td></td><td><div id="t1"></div></td></tr>
<tr id=r2 style="display:none"><td></td><td><div id="t2"></div></td></tr>
<tr id=r3 style="display:none"><td></td><td><div id="t3"></div></td></tr>
<tr id=r4 style="display:none"><td></td><td><div id="t4"></div></td></tr>
<tr id=r5 style="display:none"><td></td><td><div id="t5"></div></td></tr>
<tr id=r6 style="display:none"><td></td><td><div id="t6"></div></td></tr>
<tr id=r7 style="display:none"><td></td><td><div id="t7"></div></td></tr>
<tr id=r8 style="display:none"><td></td><td><div id="t8"></div></td></tr>
<tr id=r9 style="display:none"><td></td><td><div id="t9"></div></td></tr>
<tr id=r10 style="display:none"><td></td><td><div id="t10"></div></td></tr>
<tr id=r11 style="display:none"><td></td><td><div id="t11"></div></td></tr>
<tr id=r12 style="display:none"><td></td><td><div id="t12"></div></td></tr>
<tr id=r13 style="display:none"><td></td><td><div id="t13"></div></td></tr>
<tr id=r14 style="display:none"><td></td><td><div id="t14"></div></td></tr>
<tr id=r15 style="display:none"><td></td><td><div id="t15"></div></td></tr>
<tr><td>Motion:</td><td><div id="mot"></div></td></tr>
<tr><td>Display:</td><td><input type="button" value="OFF" id="OLED" onClick="{oled()}"> Mot <input type="button" value="OFF" id="MOT" onClick="{motTog()}"></td></tr>
<tr><td>PushBullet:</td><td><input type="button" value="OFF" id="PB0" onClick="{pbToggle0()}"> Mot <input type="button" value="OFF" id="PB1" onClick="{pbToggle1()}"></td></tr>
<tr><td>Settings:</td><td>
<div class="dropdown">
  <button class="dropbtn">Music</button>
  <div class="dropdown-content">
  <button class="btn" id="m0" onclick="sendMus(0)">Doorbell</button>
  <button class="btn" id="m1" onclick="sendMus(1)">Song1</button>
  <button class="btn" id="m2" onclick="sendMus(2)">Song2</button>
  <button class="btn" id="m3" onclick="sendMus(3)">Song3</button>
  <button class="btn" id="m4" onclick="sendMus(4)">Song4</button>
  </div>
</div>
<div class="dropdown">
  <button class="dropbtn">Effect</button>
  <div class="dropdown-content">
  <button class="btn" id="e0" onclick="setEff(0)">None</button>
  <button class="btn" id="e1" onclick="setEff(1)">Hold</button>
  <button class="btn" id="e2" onclick="setEff(2)">Off</button>
  <button class="btn" id="e3" onclick="setEff(3)">rainbowCycle</button>
  <button class="btn" id="e4" onclick="setEff(4)">rainbow</button>
  <button class="btn" id="e5" onclick="setEff(5)">alert</button>
  <button class="btn" id="e6" onclick="setEff(6)">glow</button>
  <button class="btn" id="e7" onclick="setEff(7)">chaser</button>
  <button class="btn" id="e8" onclick="setEff(8)">pendulum</button>
  <button class="btn" id="e9" onclick="setEff(9)">orbit</button>
  <button class="btn" id="e10" onclick="setEff(10)">clock</button>
  <button class="btn" id="e11" onclick="setEff(11)">spiral</button>
  <button class="btn" id="e12" onclick="setEff(12)">doubleSpiral</button>
  <button class="btn" id="e13" onclick="setEff(13)">wings</button>
  </div>
</div>
<div class="dropdown">
  <button class="dropbtn">IndCnt</button>
  <div class="dropdown-content">
  <button class="btn" id="c0" onclick="setCnt(0)">0</button>
  <button class="btn" id="c1" onclick="setCnt(1)">1</button>
  <button class="btn" id="c2" onclick="setCnt(2)">2</button>
  <button class="btn" id="c3" onclick="setCnt(3)">3</button>
  </div>
</div>
</td></tr>
<tr><td>Location:</td><td><input id='loc' type=text size=8 value=''><input value="Set" type='button' onclick="{setLoc()}"></td></tr>
<tr><td>Brightnes:</td><td><input id='br' type=text size=8 value=''><input value="Set" type='button' onclick="{setBr()}"></td></tr>
</table>
<div id="wea"></div>
<div id="alert"></div>
<div id="wind"></div>
<input id="myKey" name="key" type=text size=50 placeholder="password" style="width: 150px"><input type="button" value="Save" onClick="{localStorage.setItem('key', key = document.all.myKey.value)}">
<br></div>
<small>Copyright &copy 2016 CuriousTech.net</small>
</body>
</html>
)rawliteral";

const uint8_t favicon[] PROGMEM = {
  0x1F, 0x8B, 0x08, 0x08, 0x70, 0xC9, 0xE2, 0x59, 0x04, 0x00, 0x66, 0x61, 0x76, 0x69, 0x63, 0x6F, 
  0x6E, 0x2E, 0x69, 0x63, 0x6F, 0x00, 0xD5, 0x94, 0x31, 0x4B, 0xC3, 0x50, 0x14, 0x85, 0x4F, 0x6B, 
  0xC0, 0x52, 0x0A, 0x86, 0x22, 0x9D, 0xA4, 0x74, 0xC8, 0xE0, 0x28, 0x46, 0xC4, 0x41, 0xB0, 0x53, 
  0x7F, 0x87, 0x64, 0x72, 0x14, 0x71, 0xD7, 0xB5, 0x38, 0x38, 0xF9, 0x03, 0xFC, 0x05, 0x1D, 0xB3, 
  0x0A, 0x9D, 0x9D, 0xA4, 0x74, 0x15, 0x44, 0xC4, 0x4D, 0x07, 0x07, 0x89, 0xFA, 0x3C, 0x97, 0x9C, 
  0xE8, 0x1B, 0xDA, 0x92, 0x16, 0x3A, 0xF4, 0x86, 0x8F, 0x77, 0x73, 0xEF, 0x39, 0xEF, 0xBD, 0xBC, 
  0x90, 0x00, 0x15, 0x5E, 0x61, 0x68, 0x63, 0x07, 0x27, 0x01, 0xD0, 0x02, 0xB0, 0x4D, 0x58, 0x62, 
  0x25, 0xAF, 0x5B, 0x74, 0x03, 0xAC, 0x54, 0xC4, 0x71, 0xDC, 0x35, 0xB0, 0x40, 0xD0, 0xD7, 0x24, 
  0x99, 0x68, 0x62, 0xFE, 0xA8, 0xD2, 0x77, 0x6B, 0x58, 0x8E, 0x92, 0x41, 0xFD, 0x21, 0x79, 0x22, 
  0x89, 0x7C, 0x55, 0xCB, 0xC9, 0xB3, 0xF5, 0x4A, 0xF8, 0xF7, 0xC9, 0x27, 0x71, 0xE4, 0x55, 0x38, 
  0xD5, 0x0E, 0x66, 0xF8, 0x22, 0x72, 0x43, 0xDA, 0x64, 0x8F, 0xA4, 0xE4, 0x43, 0xA4, 0xAA, 0xB5, 
  0xA5, 0x89, 0x26, 0xF8, 0x13, 0x6F, 0xCD, 0x63, 0x96, 0x6A, 0x5E, 0xBB, 0x66, 0x35, 0x6F, 0x2F, 
  0x89, 0xE7, 0xAB, 0x93, 0x1E, 0xD3, 0x80, 0x63, 0x9F, 0x7C, 0x9B, 0x46, 0xEB, 0xDE, 0x1B, 0xCA, 
  0x9D, 0x7A, 0x7D, 0x69, 0x7B, 0xF2, 0x9E, 0xAB, 0x37, 0x20, 0x21, 0xD9, 0xB5, 0x33, 0x2F, 0xD6, 
  0x2A, 0xF6, 0xA4, 0xDA, 0x8E, 0x34, 0x03, 0xAB, 0xCB, 0xBB, 0x45, 0x46, 0xBA, 0x7F, 0x21, 0xA7, 
  0x64, 0x53, 0x7B, 0x6B, 0x18, 0xCA, 0x5B, 0xE4, 0xCC, 0x9B, 0xF7, 0xC1, 0xBC, 0x85, 0x4E, 0xE7, 
  0x92, 0x15, 0xFB, 0xD4, 0x9C, 0xA9, 0x18, 0x79, 0xCF, 0x95, 0x49, 0xDB, 0x98, 0xF2, 0x0E, 0xAE, 
  0xC8, 0xF8, 0x4F, 0xFF, 0x3F, 0xDF, 0x58, 0xBD, 0x08, 0x25, 0x42, 0x67, 0xD3, 0x11, 0x75, 0x2C, 
  0x29, 0x9C, 0xCB, 0xF9, 0xB9, 0x00, 0xBE, 0x8E, 0xF2, 0xF1, 0xFD, 0x1A, 0x78, 0xDB, 0x00, 0xEE, 
  0xD6, 0x80, 0xE1, 0x90, 0xFF, 0x90, 0x40, 0x1F, 0x04, 0xBF, 0xC4, 0xCB, 0x0A, 0xF0, 0xB8, 0x6E, 
  0xDA, 0xDC, 0xF7, 0x0B, 0xE9, 0xA4, 0xB1, 0xC3, 0x7E, 0x04, 0x00, 0x00, 
};
