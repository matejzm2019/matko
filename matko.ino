/*
  Arduino UNO WiFi Rev2: Oscillo Lab v2
  -----------------------------------
  Creates its own Wi-Fi: OscilloLab / osciloskop123
  Default website: http://192.168.4.1 (check Serial Monitor)
  D6  = passive piezo buzzer + oscilloscope signal
  D8, D9, D10, D11 = 4 LEDs, each with its OWN 220-330 ohm resistor
  LEDs flash together with every melody note, with a short dark gap.
  STOP responds as soon as the HTTP request line arrives; no delay() in loop.
  No push buttons are required.

  IDE: Tools > Board > Arduino megaAVR Boards > Arduino Uno WiFi Rev2
  Library: WiFiNINA
*/

#include <SPI.h>
#include <WiFiNINA.h>
#include <avr/pgmspace.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

char ssid[] = "OscilloLab";
char pass[] = "osciloskop123";
WiFiServer server(80);

const uint8_t BUZZER_PIN = 6;
const uint8_t LED_PINS[4] = {8, 9, 10, 11};

struct NoteStep {
  uint16_t hz;
  uint16_t durationMs;
};

// Five familiar, public-domain melodies. Zero Hz = a pause.
// 0: Twinkle Twinkle Little Star (traditional)
const NoteStep SONG0[] PROGMEM = {
  {262,290},{262,290},{392,290},{392,290},{440,290},{440,290},{392,580},
  {349,290},{349,290},{330,290},{330,290},{294,290},{294,290},{262,660}
};
// 1: Ode to Joy (Beethoven)
const NoteStep SONG1[] PROGMEM = {
  {330,270},{330,270},{349,270},{392,270},{392,270},{349,270},{330,270},{294,270},
  {262,270},{262,270},{294,270},{330,270},{330,400},{294,150},{294,530},
  {0,140},{330,270},{330,270},{349,270},{392,270},{392,270},{349,270},{330,270},{294,270},
  {262,270},{262,270},{294,270},{330,270},{294,400},{262,150},{262,590}
};
// 2: Frere Jacques (traditional)
const NoteStep SONG2[] PROGMEM = {
  {262,280},{294,280},{330,280},{262,280},{262,280},{294,280},{330,280},{262,280},
  {330,280},{349,280},{392,560},{330,280},{349,280},{392,560},
  {392,180},{440,180},{392,180},{349,180},{330,280},{262,280}
};
// 3: Jingle Bells (James Lord Pierpont)
const NoteStep SONG3[] PROGMEM = {
  {330,220},{330,220},{330,450},{0,140},
  {330,220},{330,220},{330,450},{0,140},
  {330,220},{392,220},{262,320},{294,220},{330,650},
  {349,220},{349,220},{349,300},{349,220},{349,220},{330,220},{330,220},
  {330,220},{330,220},{294,220},{294,220},{330,270},{294,440},{392,580}
};
// 4: Mary Had a Little Lamb (traditional)
const NoteStep SONG4[] PROGMEM = {
  {330,290},{294,290},{262,290},{294,290},{330,290},{330,290},{330,570},
  {294,290},{294,290},{294,560},{330,290},{392,290},{392,570},
  {330,290},{294,290},{262,290},{294,290},{330,290},{330,290},{330,290},
  {330,290},{294,290},{294,290},{330,290},{294,290},{262,630}
};

const uint8_t SONG_LENGTHS[5] = {
  sizeof(SONG0)/sizeof(SONG0[0]),
  sizeof(SONG1)/sizeof(SONG1[0]),
  sizeof(SONG2)/sizeof(SONG2[0]),
  sizeof(SONG3)/sizeof(SONG3[0]),
  sizeof(SONG4)/sizeof(SONG4[0])
};

enum AudioMode : uint8_t { SILENT=0, CONTINUOUS=1, MELODY=2 };
AudioMode audioMode = SILENT;
uint16_t currentHz = 0;
uint8_t activeSong = 255;
uint8_t songStep = 0;
unsigned long stepEndAt = 0;
unsigned long pulseEndAt = 0;
bool pulsePlaying = false;

// All actions are non-blocking. STOP always shuts down the buzzer first.
void stopSound() {
  noTone(BUZZER_PIN);
  audioMode = SILENT;
  currentHz = 0;
  activeSong = 255;
  songStep = 0;
  pulsePlaying = false;
}

void startTone(uint16_t hz) {
  if (hz < 50 || hz > 5000) return;
  stopSound();
  currentHz = hz;
  audioMode = CONTINUOUS;
  tone(BUZZER_PIN, hz);
}

void loadStep(uint8_t song, uint8_t index, NoteStep &step) {
  switch (song) {
    case 0: memcpy_P(&step, &SONG0[index], sizeof(step)); break;
    case 1: memcpy_P(&step, &SONG1[index], sizeof(step)); break;
    case 2: memcpy_P(&step, &SONG2[index], sizeof(step)); break;
    case 3: memcpy_P(&step, &SONG3[index], sizeof(step)); break;
    default: memcpy_P(&step, &SONG4[index], sizeof(step)); break;
  }
}

void playCurrentStep(unsigned long now) {
  NoteStep step;
  loadStep(activeSong, songStep, step);
  noTone(BUZZER_PIN);
  currentHz = step.hz;
  stepEndAt = now + step.durationMs;
  // Short gap separates consecutive notes visually and acoustically.
  uint16_t gapMs = step.durationMs / 5;
  if (gapMs < 40) gapMs = 40;
  if (gapMs > 90) gapMs = 90;
  pulseEndAt = stepEndAt - gapMs;
  pulsePlaying = step.hz != 0;
  if (pulsePlaying) tone(BUZZER_PIN, step.hz);
}

void startSong(uint8_t id) {
  if (id >= 5) return;
  stopSound();
  audioMode = MELODY;
  activeSong = id;
  songStep = 0;
  playCurrentStep(millis());
}

void updateAudio() {
  if (audioMode != MELODY) return;
  unsigned long now = millis();
  if ((long)(now - stepEndAt) >= 0) {
    ++songStep;
    if (songStep >= SONG_LENGTHS[activeSong]) {
      stopSound();
    } else {
      playCurrentStep(now);
    }
  } else if (pulsePlaying && (long)(now - pulseEndAt) >= 0) {
    pulsePlaying = false;
    noTone(BUZZER_PIN);
    currentHz = 0; // Actual output is silent during the small gap.
  }
}

void updateLEDs() {
  unsigned long now = millis();
  bool on;
  if (audioMode == MELODY) {
    // All four LEDs flash in sync with the real sound, per note.
    on = pulsePlaying;
  } else if (audioMode == CONTINUOUS) {
    // Slow visual pulse while a frequency is generated continuously.
    on = (now / 200UL) % 2 == 0;
  } else {
    // Gentle idle blink, no web LED controls needed.
    on = (now / 650UL) % 2 == 0;
  }
  for (uint8_t i = 0; i < 4; ++i) {
    digitalWrite(LED_PINS[i], on ? HIGH : LOW);
  }
}

// HTML resides in flash to protect the UNO WiFi Rev2's limited RAM.
const char PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="sk"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#0c1421"><title>OscilloLab | Arduino</title>
<style>
:root{color-scheme:dark}*{box-sizing:border-box}
body{margin:0;min-height:100vh;color:#edf3ff;background:radial-gradient(ellipse at 15% 0%,#253954 0,#111a2a 42%,#09101b 100%);font:15px system-ui,-apple-system,Segoe UI,sans-serif}
main{max-width:880px;margin:auto;padding:22px 14px 70px}
header{display:flex;align-items:center;justify-content:space-between;gap:10px;flex-wrap:wrap}
.brand{font-size:12px;font-weight:800;letter-spacing:1.3px;color:#a9f8d2}
.net{color:#a4b5d0;font-size:12px}
h1{font-size:clamp(35px,8vw,58px);line-height:1.05;margin:28px 0 8px;letter-spacing:-2px}h1 span{color:#7bffb6}
p{margin:0 0 19px;color:#9eafc6;line-height:1.55}
.card{background:#121d2dea;border:1px solid #32435d;border-radius:20px;padding:18px;margin:16px 0}
.bar{display:flex;align-items:center;justify-content:space-between;gap:10px;flex-wrap:wrap;margin-bottom:15px}
h2{font-size:17px;margin:0}.hint{font-size:12px;color:#98a9c4}
.readout{background:#0d2a24;border:1px solid #315c4c;border-radius:16px;padding:17px;display:flex;align-items:center;justify-content:space-between;gap:10px}
.readout small{display:block;font-size:11px;color:#aac6b7;text-transform:uppercase;letter-spacing:1px}
.hz{font-size:clamp(30px,7vw,44px);font-weight:850;letter-spacing:-1px;font-variant-numeric:tabular-nums;color:#8dffc2;margin:3px 0}
.btn{background:#266f58;border:1px solid #62b693;color:white;border-radius:12px;padding:13px 15px;font-size:14px;font-weight:750;cursor:pointer}
.btn:active{transform:scale(.98)}.stop{background:#813846;border-color:#ff8a9c}.stop:focus-visible{outline:3px solid white}
.readout .stop{min-width:102px;min-height:50px}
.keys{display:grid;grid-template-columns:repeat(6,minmax(0,1fr));gap:8px}
.key{border-radius:12px;min-height:61px;cursor:pointer;background:#27364d;border:1px solid #425671;color:#f4f7ff;font-weight:800;font-size:13px}
.key.sharp{background:#192232}.key.active{background:#21694d;border-color:#8bf7c4}
.key small{display:block;color:#adbed6;font-weight:500;font-size:10px;margin-top:4px}
.ctrl{display:flex;gap:8px;flex-wrap:wrap}.ctrl input{flex:1;min-width:120px;background:#0c1828;border:1px solid #405873;border-radius:12px;padding:13px;color:white;font-size:16px}
.songs{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px}.song{min-height:64px;background:#263951;border-color:#466487;text-align:left}
.song.active{background:#21694d;border-color:#8bf7c4}.song small{display:block;color:#bbcade;font-weight:500;margin-top:5px}
.status{margin-top:13px;font-size:12px;color:#a0b4cd}.notice{font-size:12px;margin-top:12px;color:#879bb8}
@media(max-width:520px){.keys{grid-template-columns:repeat(4,minmax(0,1fr))}.songs{grid-template-columns:1fr}.card{padding:14px}}
</style></head><body><main>
<header><span class="brand">ARDUINO · OSCILLO LAB V2</span><span class="net" id="conn">Pripájam sa...</span></header>
<h1>Music <span>Lab.</span></h1>
<p>Hraj noty, generuj frekvenciu a meraj na osciloskope. Vsetky 4 LED blikaju v rytme kazdej noty.</p>
<section class="readout"><div><small>Výstup D6</small><div class="hz" id="hz">0 Hz</div><small id="status">Ticho</small></div><button class="btn stop" onclick="command('stop')">■ STOP</button></section>
<section class="card"><div class="bar"><h2>🎹 Jednotlivé noty</h2><span class="hint">C4 – B5 · hrajú až do STOP</span></div><div class="keys" id="keys"></div></section>
<section class="card"><div class="bar"><h2>🎚️ Vlastná frekvencia</h2><span class="hint">50 až 5000 Hz</span></div>
<div class="ctrl"><input id="freq" type="number" min="50" max="5000" step="1" value="440" aria-label="Frekvencia v Hz"><button class="btn" onclick="customFreq()">▶ Generovať</button><button class="btn stop" onclick="command('stop')">■ STOP</button></div>
<p class="notice">Na osciloskope: pre 440 Hz je perióda asi 2,27 ms. Meraj medzi D6 a GND.</p></section>
<section class="card"><div class="bar"><h2>🎵 Päť jednoduchých melódií</h2><span class="hint">LED sa rozsvietia na každú notu</span></div>
<div class="songs">
<button class="btn song" id="song0" onclick="command('melody?id=0')">♪ Twinkle Twinkle<small>Hviezdička</small></button>
<button class="btn song" id="song1" onclick="command('melody?id=1')">♪ Óda na radosť<small>Beethoven</small></button>
<button class="btn song" id="song2" onclick="command('melody?id=2')">♪ Frère Jacques<small>Brat Jakub</small></button>
<button class="btn song" id="song3" onclick="command('melody?id=3')">♪ Jingle Bells<small>Vianočná</small></button>
<button class="btn song" id="song4" onclick="command('melody?id=4')">♪ Mary Had a Little Lamb<small>Jednoduchá detská</small></button>
</div><div class="status" id="progress">Vyber si melódiu alebo notu.</div></section>
<p class="notice">STOP preruší melódiu, ale LED potom pokračujú v pomalom blikaní naprázdno. Zvukový výstup je obdĺžnikový, približne 0–5 V (nie sínusovka). Sieť je lokálna, bez internetu.</p>
</main><script>
const names=['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'];
const keys=document.getElementById('keys');
let state={mode:0,hz:0,song:255,step:0,total:0};
let requestVersion=0,commandInFlight=false,pollInFlight=false;
for(let oct=4;oct<=5;oct++) for(let i=0;i<12;i++){
 const midi=(oct+1)*12+i,hz=Math.round(440*Math.pow(2,(midi-69)/12));
 const b=document.createElement('button');b.className='key'+(names[i].includes('#')?' sharp':'');
 b.id='n'+hz;b.innerHTML=names[i]+oct+'<small>'+hz+' Hz</small>';
 b.onclick=()=>command('tone?hz='+hz);keys.appendChild(b);
}
function paint(){
 document.getElementById('conn').textContent='● Spojené s Arduinom';
 document.getElementById('hz').textContent=state.hz+' Hz';
 document.getElementById('status').textContent=state.mode===0?'Ticho':state.mode===1?'Trvalý tón':'Melódia';
 document.getElementById('progress').textContent=state.mode===2?'Nota '+state.step+' z '+state.total:state.mode===1?'Stlač STOP pre vypnutie tónu.':'Vyber si melódiu alebo notu.';
 document.querySelectorAll('.key').forEach(b=>b.classList.toggle('active',state.mode===1&&b.id==='n'+state.hz));
 for(let i=0;i<5;i++)document.getElementById('song'+i).classList.toggle('active',state.mode===2&&state.song===i);
}
function offline(){document.getElementById('conn').textContent='● Výpadok spojenia'}
async function command(cmd){
 // Request line is handled immediately by the Arduino, even while a tune plays.
 const version=++requestVersion;commandInFlight=true;
 if(cmd==='stop')document.getElementById('status').textContent='Zastavujem...';
 try{
  const r=await fetch('/api/'+cmd+(cmd.includes('?')?'&':'?')+'_='+Date.now(),{cache:'no-store'});
  if(!r.ok)throw Error('HTTP '+r.status);
  const s=await r.json();if(version===requestVersion){state=s;paint()}
 }catch(e){if(version===requestVersion)offline()}
 finally{if(version===requestVersion)commandInFlight=false}
}
function customFreq(){const hz=Number(document.getElementById('freq').value);if(!Number.isInteger(hz)||hz<50||hz>5000){alert('Zadaj celé číslo medzi 50 a 5000 Hz.');return}command('tone?hz='+hz)}
async function poll(){
 if(commandInFlight||pollInFlight)return;const version=requestVersion;pollInFlight=true;
 try{
  const r=await fetch('/api/status?_='+Date.now(),{cache:'no-store'});if(!r.ok)throw Error('HTTP');
  const s=await r.json();if(version===requestVersion&&!commandInFlight){state=s;paint()}
 }catch(e){if(version===requestVersion&&!commandInFlight)offline()}
 finally{pollInFlight=false}
}
setInterval(poll,1300);poll();
</script></body></html>)HTML";

// Keep request parsing separate from playback: it NEVER waits for a melody.
WiFiClient httpClient;
bool httpActive = false;
bool requestLineDone = false;
bool requestActionApplied = false;
bool headerLineHasText = false;
char requestLine[110];
uint8_t requestLength = 0;
unsigned long requestStartedAt = 0;

void applyRequest() {
  if (requestActionApplied) return;
  requestActionApplied = true;
  if (strncmp(requestLine, "GET /api/stop", 13) == 0) {
    stopSound(); // IMPORTANT: Immediate; don't wait for remaining HTTP headers.
  } else if (strncmp(requestLine, "GET /api/tone?", 14) == 0) {
    const char *p = strstr(requestLine, "hz=");
    if (p) {
      long hz = atol(p + 3);
      if (hz >= 50 && hz <= 5000) startTone((uint16_t)hz);
    }
  } else if (strncmp(requestLine, "GET /api/melody?", 16) == 0) {
    const char *p = strstr(requestLine, "id=");
    if (p) {
      long id = atol(p + 3);
      if (id >= 0 && id < 5) startSong((uint8_t)id);
    }
  }
}

void printCommonHeaders(WiFiClient &c, const __FlashStringHelper* type, uint16_t len) {
  c.println(F("HTTP/1.1 200 OK"));
  c.print(F("Content-Type: ")); c.println(type);
  c.print(F("Content-Length: ")); c.println(len);
  c.println(F("Cache-Control: no-store, max-age=0"));
  c.println(F("Connection: close"));
  c.println();
}

void sendStatus(WiFiClient &c) {
  char json[100];
  uint8_t step = (audioMode == MELODY) ? songStep + 1 : 0;
  uint8_t total = (audioMode == MELODY) ? SONG_LENGTHS[activeSong] : 0;
  snprintf(json,sizeof(json),"{\"mode\":%u,\"hz\":%u,\"song\":%u,\"step\":%u,\"total\":%u}",
    (unsigned)audioMode,(unsigned)currentHz,(unsigned)activeSong,(unsigned)step,(unsigned)total);
  printCommonHeaders(c, F("application/json; charset=utf-8"), strlen(json));
  c.print(json);
}

void sendPage(WiFiClient &c) {
  uint16_t len = strlen_P(PAGE);
  printCommonHeaders(c,F("text/html; charset=utf-8"),len);
  char chunk[96];
  uint16_t pos = 0;
  while (pos < len) {
    uint16_t remaining = len - pos;
    uint8_t n = ((size_t)remaining < sizeof(chunk)) ? (uint8_t)remaining : (uint8_t)sizeof(chunk);
    for (uint8_t i=0;i<n;i++) chunk[i] = (char)pgm_read_byte(PAGE+pos+i);
    uint8_t sent = 0;
    while (sent < n) {
      size_t written = c.write((const uint8_t *)chunk+sent, n-sent);
      if (written == 0) return; // Lost connection: fail instead of hanging STOP.
      sent += (uint8_t)written;
      updateAudio();
      updateLEDs();
    }
    pos += n;
  }
}

void finishRequest() {
  if (!requestLineDone) return;
  if (strncmp(requestLine,"GET /api/",9)==0) {
    sendStatus(httpClient);
  } else if (strncmp(requestLine,"GET /favicon.ico",16)==0) {
    httpClient.println(F("HTTP/1.1 204 No Content\r\nContent-Length: 0\r\nConnection: close\r\n"));
  } else {
    sendPage(httpClient);
  }
  httpClient.stop();
  httpActive = false;
}

void handleHttp() {
  if (!httpActive) {
    WiFiClient incoming = server.available();
    if (!incoming) return;
    httpClient = incoming;
    httpActive = true;
    requestLineDone = false;
    requestActionApplied = false;
    headerLineHasText = false;
    requestLength = 0;
    requestStartedAt = millis();
  }

  // Process only a bounded number of bytes in this pass, keeping LED updates alive.
  uint8_t bytes = 0;
  while (httpClient.available() && bytes < 120) {
    char ch = (char)httpClient.read();
    ++bytes;
    if (!requestLineDone) {
      if (ch == '\n') {
        requestLine[requestLength] = 0;
        requestLineDone = true;
        applyRequest(); // STOP works even during the melody.
      } else if (ch != '\r' && requestLength < sizeof(requestLine)-1) {
        requestLine[requestLength++] = ch;
      }
    } else {
      if (ch == '\n') {
        if (!headerLineHasText) { finishRequest(); return; }
        headerLineHasText = false;
      } else if (ch != '\r') {
        headerLineHasText = true;
      }
    }
  }
  // Timeout never blocks the loop. For incomplete requests send a response
  // if the request line was received, or clean up if nothing arrived.
  if (millis() - requestStartedAt > 1200UL) {
    if (requestLineDone) finishRequest();
    else { httpClient.stop(); httpActive = false; }
  } else if (!httpClient.connected() && !httpClient.available()) {
    httpClient.stop(); httpActive = false;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN,OUTPUT);
  noTone(BUZZER_PIN);
  for(uint8_t i=0;i<4;i++){
    pinMode(LED_PINS[i], OUTPUT);
    digitalWrite(LED_PINS[i],LOW);
  }
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println(F("ERROR: WiFiNINA module not detected."));
    while(true) updateLEDs();
  }
  Serial.println(F("Starting OscilloLab access point..."));
  if (WiFi.beginAP(ssid,pass) != WL_AP_LISTENING) {
    Serial.println(F("ERROR: AP failed. Check WiFiNINA firmware and credentials."));
    while(true) updateLEDs();
  }
  delay(300);
  server.begin();
  Serial.print(F("WiFi SSID: ")); Serial.println(ssid);
  Serial.print(F("WiFi password: ")); Serial.println(pass);
  Serial.print(F("Web address: http://")); Serial.println(WiFi.localIP());
}

void loop() {
  updateAudio();
  updateLEDs();
  handleHttp();
}
