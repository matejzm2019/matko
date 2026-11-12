/* Arduino UNO WiFi Rev2 • OscilloLab V3
   AP: OscilloLab / osciloskop123 • URL: http://192.168.4.1
   Sound square wave on D6 (OSCILLOSCOPE TIP), GND (SCOPE GROUND)
   LEDs D8-D11 each with 220-330 ohm resistor
   IMPORTANT: Don't connect a 4/8-ohm loudspeaker directly to Arduino D6!
   Use an amplifier/driver suitable for the speaker, with common GND.
   WiFiNINA library + Arduino megaAVR Boards required.
*/
#include <SPI.h>
#include <WiFiNINA.h>
#include <avr/pgmspace.h>
#include <stdlib.h>
#include <string.h>

char ssid[] = "OscilloLab";
char password[] = "osciloskop123";
WiFiServer server(80);
const byte AUDIO = 6;
const byte LEDS[] = {8,9,10,11};

struct Note { uint16_t frequency, duration; };
// All five tunes are traditional/public-domain melodies.
const Note tune0[] PROGMEM={{262,290},{262,290},{392,290},{392,290},{440,290},{440,290},{392,580},{349,290},{349,290},{330,290},{330,290},{294,290},{294,290},{262,590}};
const Note tune1[] PROGMEM={{330,270},{330,270},{349,270},{392,270},{392,270},{349,270},{330,270},{294,270},{262,270},{262,270},{294,270},{330,270},{330,400},{294,150},{294,530},{0,140},{330,270},{330,270},{349,270},{392,270},{392,270},{349,270},{330,270},{294,270},{262,270},{262,270},{294,270},{330,270},{294,400},{262,150},{262,590}};
const Note tune2[] PROGMEM={{262,280},{294,280},{330,280},{262,280},{262,280},{294,280},{330,280},{262,280},{330,280},{349,280},{392,560},{330,280},{349,280},{392,560},{392,180},{440,180},{392,180},{349,180},{330,280},{262,280}};
const Note tune3[] PROGMEM={{330,220},{330,220},{330,450},{0,140},{330,220},{330,220},{330,450},{0,140},{330,220},{392,220},{262,320},{294,220},{330,650},{349,220},{349,220},{349,300},{349,220},{349,220},{330,220},{330,220},{330,220},{330,220},{294,220},{294,220},{330,270},{294,440},{392,580}};
const Note tune4[] PROGMEM={{330,290},{294,290},{262,290},{294,290},{330,290},{330,290},{330,570},{294,290},{294,290},{294,560},{330,290},{392,290},{392,570},{330,290},{294,290},{262,290},{294,290},{330,290},{330,290},{330,290},{330,290},{294,290},{294,290},{330,290},{294,290},{262,630}};
const byte songLengths[]={sizeof(tune0)/sizeof(Note),sizeof(tune1)/sizeof(Note),sizeof(tune2)/sizeof(Note),sizeof(tune3)/sizeof(Note),sizeof(tune4)/sizeof(Note)};

enum Mode { OFF, SINGLE, SONG };
Mode mode=OFF;
byte songId=0, noteIndex=0;
uint16_t hz=0;
unsigned long noteStart=0, noteDuration=0;
bool soundActive=false;

void stopAudio(){ noTone(AUDIO); mode=OFF; hz=0; soundActive=false; noteIndex=0; }
void startSingle(uint16_t f){if(f<50||f>5000)return;stopAudio();mode=SINGLE;hz=f;soundActive=true;tone(AUDIO,f);}
Note readNote(byte id,byte idx){Note n={0,200};switch(id){
case 0:memcpy_P(&n,&tune0[idx],sizeof(n));break;
case 1:memcpy_P(&n,&tune1[idx],sizeof(n));break;
case 2:memcpy_P(&n,&tune2[idx],sizeof(n));break;
case 3:memcpy_P(&n,&tune3[idx],sizeof(n));break;
default:memcpy_P(&n,&tune4[idx],sizeof(n));break;
}return n;}
void nextNote(unsigned long now){
 if(noteIndex>=songLengths[songId]){stopAudio();return;}
 Note n=readNote(songId,noteIndex);
 noteStart=now;noteDuration=n.duration;hz=n.frequency;
 noTone(AUDIO);
 soundActive=(hz!=0);
 if(soundActive)tone(AUDIO,hz);
}
void startSong(byte id){if(id>=5)return;stopAudio();mode=SONG;songId=id;noteIndex=0;nextNote(millis());}
void updateMusic(){
 if(mode!=SONG)return;
 unsigned long elapsed=millis()-noteStart;
 if(elapsed>=noteDuration){++noteIndex;nextNote(millis());return;}
 unsigned long gap=noteDuration/6;
 if(gap<30)gap=30;
 if(gap>75)gap=75;
 if(soundActive && elapsed>=noteDuration-gap){noTone(AUDIO);soundActive=false;}
}
void updateLEDs(){
 bool on=(mode==SONG)?soundActive:(mode==SINGLE?((millis()/180)%2==0):((millis()/650)%2==0));
 for(byte i=0;i<4;i++)digitalWrite(LEDS[i],on?HIGH:LOW);
}

// Small static page. Buttons are REAL HTTP links, not JavaScript-only fetches.
// This makes STOP and melodies work even if browser JavaScript is disabled.
const char PAGE[] PROGMEM=R"PAGE(<!doctype html><html lang="sk"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>OscilloLab V3</title><style>
*{box-sizing:border-box}body{margin:0;background:#0b1220;color:#eaf2ff;font:15px system-ui,sans-serif}main{max-width:790px;margin:auto;padding:20px 15px 60px}h1{font-size:40px;margin:13px 0 5px}h1 span{color:#69f6b0}p{color:#a8b8d1;line-height:1.5}section{background:#172337;border:1px solid #344661;padding:17px;border-radius:16px;margin:14px 0}h2{font-size:18px;margin:0 0 13px}.grid{display:grid;grid-template-columns:repeat(4,1fr);gap:9px}.songs{grid-template-columns:repeat(2,1fr)}a,.go{display:block;text-align:center;text-decoration:none;border:1px solid #537291;background:#253a57;border-radius:11px;padding:13px 8px;color:white;font-weight:700;font-size:14px;cursor:pointer}a:active{background:#42866f}.stop{background:#aa334a;border-color:#fa889b;padding:18px;font-size:22px}.row{display:flex;gap:8px}.row input{width:100%;min-width:50px;border:1px solid #678;background:#111d30;color:white;border-radius:10px;padding:12px;font-size:16px}.go{white-space:nowrap;background:#137a57}.small{font-size:12px}.hz{font-size:28px;font-weight:800;color:#8fffc8}#msg{color:#9dd8bb}@media(max-width:500px){.grid{grid-template-columns:repeat(3,1fr)}.songs{grid-template-columns:1fr}}
</style></head><body><main><small>ARDUINO UNO WIFI REV2 · AP MODE</small><h1>Oscillo<span>Lab V3</span></h1><p>Noty, 5 melodií a meranie frekvencie osciloskopom. Všetky LED blikajú podľa tónov.</p>
<section><h2>Aktuálny výstup D6</h2><div class="hz" id="hz">Načítavam...</div><p id="msg">Stav sa obnovuje automaticky</p><a class="stop" href="/stop">■ STOP · VYPNÚŤ ZVUK</a></section>
<section><h2>🎵 Päť melódií</h2><div class="grid songs"><a href="/song/0">1 · Hviezdička</a><a href="/song/1">2 · Óda na radosť</a><a href="/song/2">3 · Brat Jakub</a><a href="/song/3">4 · Jingle Bells</a><a href="/song/4">5 · Mary Had a Little Lamb</a></div></section>
<section><h2>🎹 Jednotlivé noty (C4 až B5)</h2><div class="grid" id="keyboard"></div></section>
<section><h2>📈 Vlastná frekvencia (50 až 5000 Hz)</h2><form action="/frequency" method="get" class="row"><input type="number" name="hz" min="50" max="5000" value="440" required><button class="go" type="submit">▶ Prehrať</button></form><p class="small">440 Hz: perióda približne 2,27 ms. Meraj D6 voči GND. Na výstupe je obdĺžnikový signál, nie sínusovka.</p></section>
<p class="small">Sieť OscilloLab · stránka 192.168.4.1 · nevyžaduje internet. Reproduktor 4/8 Ω pripájaj cez vhodný zosilňovač, nie priamo na pin.</p>
<script>
(function(){var names=['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'];var box=document.getElementById('keyboard');for(var o=4;o<=5;o++)for(var i=0;i<12;i++){var f=Math.round(440*Math.pow(2,((o+1)*12+i-69)/12));var a=document.createElement('a');a.href='/note/'+f;a.textContent=names[i]+o+' · '+f+' Hz';box.appendChild(a);}
function poll(){var x=new XMLHttpRequest();x.open('GET','/status?t='+Date.now(),true);x.timeout=2500;x.onreadystatechange=function(){if(x.readyState===4&&x.status===200){try{var s=JSON.parse(x.responseText);document.getElementById('hz').textContent=s.hz+' Hz';document.getElementById('msg').textContent=s.mode===0?'Ticho':s.mode===1?'Trvalý tón':'Melódia '+(s.song+1)+' · nota '+s.step+'/'+s.total;}catch(e){}}};x.send();}
poll();setInterval(poll,1600);})();
</script></main></body></html>)PAGE";

// Fixed small request buffer. No String concatenation, delay or blocking tune playback.
WiFiClient client;
bool hasClient=false, firstLineFinished=false;
char request[85]; byte requestLen=0;
unsigned long clientSince=0;

void respondStatus(WiFiClient &c){
 c.println(F("HTTP/1.1 200 OK"));c.println(F("Content-Type: application/json"));c.println(F("Cache-Control: no-store"));c.println(F("Connection: close"));c.println();
 c.print(F("{\"mode\":"));c.print((int)mode);
 c.print(F(",\"hz\":"));c.print(hz);
 c.print(F(",\"song\":"));c.print(songId);
 c.print(F(",\"step\":"));c.print((int)(mode==SONG?noteIndex+1:0));
 c.print(F(",\"total\":"));c.print((int)(mode==SONG?songLengths[songId]:0));
 c.println(F("}"));
}
void sendPage(WiFiClient &c){
 c.println(F("HTTP/1.1 200 OK"));c.println(F("Content-Type: text/html; charset=utf-8"));
 c.print(F("Content-Length: "));c.println(strlen_P(PAGE));
 c.println(F("Cache-Control: no-store"));c.println(F("Connection: close"));c.println();
 // Only one browser request at a time; small chunks keep RAM usage low.
 for(size_t p=0;p<strlen_P(PAGE);p+=64){
  char b[64];size_t n=strlen_P(PAGE)-p;if(n>sizeof(b))n=sizeof(b);
  memcpy_P(b,PAGE+p,n);
  if(c.write((uint8_t*)b,n)!=n)break;
  updateMusic();updateLEDs();
 }
}
void redirectHome(WiFiClient &c){
 c.println(F("HTTP/1.1 303 See Other"));
 c.println(F("Location: /"));
 c.println(F("Cache-Control: no-store"));
 c.println(F("Content-Length: 0"));
 c.println(F("Connection: close"));c.println();
}
void route(WiFiClient &c){
 // Requests are parsed immediately on end of FIRST line. No dependence on HTTP headers.
 if(strncmp(request,"GET /stop ",10)==0 || strncmp(request,"GET /stop?",10)==0){
  stopAudio();Serial.println(F("STOP"));redirectHome(c);
 }else if(strncmp(request,"GET /song/",10)==0){
  int id=atoi(request+10);if(id>=0&&id<5){startSong((byte)id);Serial.print(F("SONG "));Serial.println(id);}redirectHome(c);
 }else if(strncmp(request,"GET /note/",10)==0){
  int f=atoi(request+10);if(f>=50&&f<=5000){startSingle((uint16_t)f);Serial.print(F("NOTE "));Serial.println(f);}redirectHome(c);
 }else if(strncmp(request,"GET /frequency?",15)==0){
  char *q=strstr(request,"hz=");if(q){int f=atoi(q+3);if(f>=50&&f<=5000)startSingle((uint16_t)f);}redirectHome(c);
 }else if(strncmp(request,"GET /status",11)==0){respondStatus(c);
 }else if(strncmp(request,"GET /favicon.ico",16)==0){
  c.println(F("HTTP/1.1 204 No Content\r\nContent-Length: 0\r\nConnection: close\r\n"));
 }else{sendPage(c);}
}
void serviceWeb(){
 if(!hasClient){WiFiClient incoming=server.available();if(!incoming)return;client=incoming;hasClient=true;firstLineFinished=false;requestLen=0;clientSince=millis();}
 byte count=0;
 while(client.available()&&count++<110){
  char c=(char)client.read();
  if(c=='\n'){
   request[requestLen]=0;
   route(client); // command already applied BEFORE sending HTML/redirect
   client.stop();hasClient=false;return;
  }
  if(c!='\r'&&requestLen<sizeof(request)-1)request[requestLen++]=c;
 }
 if(millis()-clientSince>1300UL||(!client.connected()&&!client.available())){
  client.stop();hasClient=false;
 }
}
void setup(){
 Serial.begin(115200);
 pinMode(AUDIO,OUTPUT);noTone(AUDIO);
 for(byte i=0;i<4;i++){pinMode(LEDS[i],OUTPUT);digitalWrite(LEDS[i],LOW);}
 if(WiFi.status()==WL_NO_MODULE){Serial.println(F("WiFiNINA module not found"));while(true)updateLEDs();}
 if(WiFi.beginAP(ssid,password)!=WL_AP_LISTENING){Serial.println(F("Failed to start WiFi AP"));while(true)updateLEDs();}
 server.begin();Serial.print(F("Open http://"));Serial.println(WiFi.localIP());
}
void loop(){updateMusic();updateLEDs();serviceWeb();}
