/*
  Arduino Oscillo Lab - Arduino UNO WiFi Rev2
  - Creates its own WiFi access point, no router required
  - D8-D11: automatic LED patterns, no physical-button controls
  - D6: passive piezo buzzer / square-wave measurement output
  - Web UI: 24 musical notes, custom frequency, 3 original melodies, STOP

  Wiring:
    LED anodes -> through individual 220-330 ohm resistors -> D8,D9,D10,D11
    LED cathodes -> GND
    Passive piezo + -> D6, - -> GND
    Scope probe tip -> D6, probe ground -> Arduino GND

  Libraries: WiFiNINA (Library Manager), Arduino megaAVR Boards
  Board: Arduino Uno WiFi Rev2
*/

#include <SPI.h>
#include <WiFiNINA.h>
#include <avr/pgmspace.h>
#include <string.h>
#include <stdlib.h>

char ssid[] = "OscilloLab";
char pass[] = "osciloskop123"; // at least 8 characters
WiFiServer server(80);

const uint8_t LED_PINS[4] = {8, 9, 10, 11};
const uint8_t BUZZER_PIN = 6;

struct Step {
  uint16_t hz;
  uint16_t ms;
};

// Three original small test tunes; 0 Hz means a deliberate pause.
const Step SONG0[] PROGMEM = {
  {262, 260}, {330, 260}, {392, 260}, {523, 460},
  {392, 240}, {330, 240}, {294, 240}, {262, 560},
  {0, 150}, {392, 220}, {523, 220}, {659, 360},
  {523, 260}, {392, 260}, {330, 260}, {262, 650}
};
const Step SONG1[] PROGMEM = {
  {440, 200}, {880, 200}, {660, 200}, {1320, 220},
  {550, 200}, {1100, 200}, {440, 240}, {0, 130},
  {523, 200}, {1047, 200}, {784, 230}, {1568, 260},
  {659, 240}, {880, 240}, {1047, 490}
};
const Step SONG2[] PROGMEM = {
  {220, 400}, {330, 400}, {440, 400}, {660, 400},
  {880, 650}, {0, 200}, {880, 240}, {660, 240},
  {440, 240}, {330, 240}, {220, 700}
};

const uint8_t SONG_LENGTHS[3] = {
  sizeof(SONG0) / sizeof(SONG0[0]),
  sizeof(SONG1) / sizeof(SONG1[0]),
  sizeof(SONG2) / sizeof(SONG2[0])
};

// 0: silent, 1: sustained note, 2: preset melody.
uint8_t audioMode = 0;
uint16_t currentHz = 0;
uint8_t activeSong = 255;
uint8_t songStep = 0;
unsigned long songNextAt = 0;

void stopSound() {
  audioMode = 0;
  currentHz = 0;
  activeSong = 255;
  songStep = 0;
  noTone(BUZZER_PIN);
}

void startNote(uint16_t hz) {
  if (hz < 50 || hz > 5000) return;
  stopSound();
  audioMode = 1;
  currentHz = hz;
  tone(BUZZER_PIN, hz); // Sustained signal until STOP (best for oscilloscope)
}

void startMelody(uint8_t id) {
  if (id >= 3) return;
  stopSound();
  audioMode = 2;
  activeSong = id;
  songStep = 0;
  songNextAt = 0;
}

void updateAudio() {
  if (audioMode != 2) return;
  unsigned long now = millis();
  if (songNextAt != 0 && (long)(now - songNextAt) < 0) return;
  if (songStep >= SONG_LENGTHS[activeSong]) {
    stopSound();
    return;
  }

  Step note;
  if (activeSong == 0) memcpy_P(&note, &SONG0[songStep], sizeof(note));
  else if (activeSong == 1) memcpy_P(&note, &SONG1[songStep], sizeof(note));
  else memcpy_P(&note, &SONG2[songStep], sizeof(note));

  currentHz = note.hz;
  if (note.hz) tone(BUZZER_PIN, note.hz);
  else noTone(BUZZER_PIN);
  songNextAt = now + note.ms;
  ++songStep;
}

void updateLEDs() {
  // Always-on autonomous light show. Intentionally independent of the web UI.
  unsigned long now = millis();
  uint16_t section = (now / 5000UL) % 3;
  uint8_t step = 0;
  if (section == 0) step = (now / 140UL) % 4;         // running light
  else if (section == 1) step = (now / 230UL) % 2;    // alternating pairs
  else step = (now / 85UL) % 2;                      // fast flash

  for (uint8_t i = 0; i < 4; ++i) {
    bool on = false;
    if (section == 0) on = (i == step);
    else if (section == 1) on = ((i % 2) == step);
    else on = (step == 0);
    digitalWrite(LED_PINS[i], on ? HIGH : LOW);
  }
}

void sendHeaders(WiFiClient& client, bool json) {
  client.println(F("HTTP/1.1 200 OK"));
  if (json) client.println(F("Content-Type: application/json; charset=utf-8"));
  else client.println(F("Content-Type: text/html; charset=utf-8"));
  client.println(F("Cache-Control: no-store"));
  client.println(F("Connection: close"));
  client.println();
}

void sendStatus(WiFiClient& client) {
  sendHeaders(client, true);
  client.print(F("{\"mode\":")); client.print(audioMode);
  client.print(F(",\"hz\":")); client.print(currentHz);
  client.print(F(",\"song\":"));
  client.print(audioMode == 2 ? activeSong : 255);
  client.println(F("}"));
}

// Stored in program flash instead of the UNO WiFi Rev2's limited SRAM.
const char PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="sk"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="theme-color" content="#0d1019"><title>Oscillo Lab | Arduino</title>
<style>
:root{color-scheme:dark}*{box-sizing:border-box}body{margin:0;background:radial-gradient(ellipse at top left,#22324e 0,#101522 38%,#080c14 100%);color:#eaf0f9;font:15px system-ui,Segoe UI,Arial,sans-serif;min-height:100vh}
main{max-width:820px;padding:26px 16px 60px;margin:auto}.top{display:flex;align-items:center;justify-content:space-between;gap:12px}.tag{color:#9bffcf;background:#16443855;border:1px solid #39785e;border-radius:99px;padding:7px 11px;font-size:11px;font-weight:800;letter-spacing:1.4px}.online{font-size:12px;color:#b1bdd5}
h1{font-size:clamp(34px,7vw,58px);letter-spacing:-2px;margin:25px 0 8px;line-height:1.07}h1 span{color:#70f4ba}p{color:#9baac3;line-height:1.55;margin:0 0 22px}
.panel{border:1px solid #2b374d;background:#121a29e8;border-radius:20px;margin:16px 0;padding:20px;box-shadow:0 10px 35px #0002}.ph{display:flex;align-items:center;justify-content:space-between;gap:8px;margin-bottom:14px;flex-wrap:wrap}h2{font-size:17px;margin:0}.hint{color:#8c9cb5;font-size:12px}
.readout{border:1px solid #3a6857;border-radius:16px;background:#0d271f;margin-bottom:20px;padding:17px;display:flex;align-items:center;justify-content:space-between;gap:8px}.readout .digit{font-variant-numeric:tabular-nums;font-size:clamp(25px,7vw,39px);letter-spacing:-1px;font-weight:800;color:#80ffb7}.readout small{color:#9bb8a9;display:block;margin-bottom:3px;font-size:11px;text-transform:uppercase;letter-spacing:1px}.pill{border:1px solid #47745d;background:#16392b;border-radius:10px;padding:8px;color:#baffd2;font-size:11px}
.keys{display:grid;grid-template-columns:repeat(6,minmax(0,1fr));gap:8px}.key{background:#222f41;border:1px solid #3a4861;color:#eef6ff;border-radius:12px;min-height:58px;cursor:pointer;font-size:13px;font-weight:780;transition:background .1s,transform .1s}.key:active,.key.active{background:#286a57;border-color:#7cf6b4;transform:scale(.98)}.key.sharp{background:#141a28;color:#c4cee1}.key.sharp.active{background:#286a57}.key small{display:block;color:#94a2ba;font-size:10px;font-weight:500;margin-top:4px}.key.active small{color:#c6ffe2}
.freq{display:flex;gap:9px;align-items:stretch;flex-wrap:wrap}input{background:#09111f;border:1px solid #3a4861;border-radius:12px;padding:12px;color:#fff;font-size:17px;min-width:0;width:150px;flex:1}.btn{background:#317b65;color:white;border:1px solid #52ae8c;border-radius:12px;padding:12px 16px;font-weight:800;cursor:pointer}.btn:active{transform:scale(.98)}.btn.stop{background:#552e39;border-color:#b7687c}.melodies{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:9px}.melodies .btn{background:#25354d;border-color:#405b81;min-height:65px}.melodies .btn.active{background:#225d48;border-color:#7af9b9}
footer{font-size:12px;color:#71819b;line-height:1.6;padding:12px 3px}.waves{width:100%;height:42px;margin-top:11px;opacity:.55}@media(max-width:520px){.keys{grid-template-columns:repeat(4,minmax(0,1fr))}.melodies{grid-template-columns:1fr}.panel{padding:15px}}
</style></head><body><main>
<div class="top"><span class="tag">ARDUINO · OSCILLO LAB</span><span id="conn" class="online">Pripájam sa...</span></div>
<h1>Sound <span>Lab.</span></h1><p>LEDky blikajú samy. Ty cez web generuješ tóny a sleduješ priebeh na osciloskope.</p>
<div class="readout"><div><small>Frekvencia na pine D6</small><div class="digit" id="hz">0 Hz</div><small id="status">Ticho</small></div><span class="pill">SQUARE WAVE<br>~50% DUTY</span></div>
<div class="panel"><div class="ph"><h2>Hudobné noty</h2><span class="hint">C4 až B5 · tón hrá až do STOP</span></div><div id="keys" class="keys"></div></div>
<div class="panel"><div class="ph"><h2>Vlastná frekvencia</h2><span class="hint">50 – 5000 Hz</span></div><div class="freq"><input id="freq" type="number" min="50" max="5000" value="440" step="1" aria-label="Frekvencia v Hz"><button class="btn" onclick="customFreq()">▶ Generovať</button><button class="btn stop" onclick="command('/api/stop')">■ STOP</button></div><p style="font-size:12px;margin:14px 0 0">Skús 100 Hz, 440 Hz alebo 1000 Hz. Na osciloskope uvidíš rozdiel v perióde.</p></div>
<div class="panel"><div class="ph"><h2>Testovacie melódie</h2><span class="hint">Vlastné krátke sekvencie</span></div><div class="melodies"><button class="btn" id="song0" onclick="command('/api/melody?id=0')">♪ Vzostup a návrat</button><button class="btn" id="song1" onclick="command('/api/melody?id=1')">♫ Dvojité frekvencie</button><button class="btn" id="song2" onclick="command('/api/melody?id=2')">♬ Pomalé schody</button></div><p style="font-size:12px;margin:13px 0 0">Pri melódii sa frekvencia postupne mení. STOP funguje okamžite po doručení požiadavky.</p></div>
<footer>Pripojenie: hrot sondy na D6, uzemnenie sondy na GND. Signál je digitálny približne 0–5 V, nie sínusovka. Prehrávanie vyžaduje pasívny piezo bzučiak.</footer>
</main><script>
const notes=['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'];
const keys=document.getElementById('keys');let busy=false, state={mode:0,hz:0,song:255};
for(let octave=4;octave<=5;octave++)for(let i=0;i<12;i++){
 const midi=(octave+1)*12+i,hz=Math.round(440*Math.pow(2,(midi-69)/12));
 const b=document.createElement('button');b.className='key'+(notes[i].includes('#')?' sharp':'');b.id='n'+hz;b.innerHTML=notes[i]+octave+'<small>'+hz+' Hz</small>';b.onclick=()=>command('/api/tone?hz='+hz);keys.appendChild(b);
}
async function command(path){try{busy=true;const r=await fetch(path,{cache:'no-store'});if(!r.ok)throw Error('HTTP');state=await r.json();paint()}catch(e){offline()}finally{busy=false}}
function customFreq(){let hz=Number(document.getElementById('freq').value);if(!Number.isInteger(hz)||hz<50||hz>5000){alert('Zadaj frekvenciu 50 až 5000 Hz.');return}command('/api/tone?hz='+hz)}
function paint(){document.getElementById('conn').textContent='● Pripojené k Arduinu';document.getElementById('hz').textContent=state.hz+' Hz';document.getElementById('status').textContent=state.mode===0?'Ticho':state.mode===1?'Trvalý tón (do STOP)':'Prehráva sa melódia';document.querySelectorAll('.key').forEach(b=>b.classList.toggle('active',state.mode===1&&b.id==='n'+state.hz));for(let i=0;i<3;i++)document.getElementById('song'+i).classList.toggle('active',state.mode===2&&state.song===i)}
function offline(){document.getElementById('conn').textContent='● Spojenie prerušené'}
async function poll(){if(busy)return;try{const r=await fetch('/api/status',{cache:'no-store'});if(!r.ok)throw Error('HTTP');state=await r.json();paint()}catch(e){offline()}}
setInterval(poll,900);poll();
</script></body></html>)HTML";

void sendPage(WiFiClient& client) {
  sendHeaders(client, false);
  const uint16_t total = strlen_P(PAGE);
  char buf[96];
  for (uint16_t offset = 0; offset < total; offset += sizeof(buf)) {
    uint8_t n = ((total - offset) < sizeof(buf)) ? (uint8_t)(total - offset) : (uint8_t)sizeof(buf);
    for (uint8_t i = 0; i < n; ++i) buf[i] = (char)pgm_read_byte(PAGE + offset + i);
    client.write((const uint8_t*)buf, n);
    updateAudio();
    updateLEDs();
  }
}

void serveClient(WiFiClient& client) {
  char request[96];
  uint8_t n = 0;
  unsigned long started = millis();
  while (client.connected() && millis() - started < 650UL) {
    if (!client.available()) {
      updateAudio();
      updateLEDs();
      continue;
    }
    char ch = (char)client.read();
    if (ch == '\n') break;
    if (ch != '\r' && n < sizeof(request) - 1) request[n++] = ch;
  }
  request[n] = '\0';
  // Consume the remaining HTTP headers before replying. Closing a TCP
  // connection with unread request data can make browsers report a reset.
  uint8_t lineEndCount = 0;
  unsigned long headerStart = millis();
  while (client.connected() && millis() - headerStart < 600UL) {
    if (client.available()) {
      char ch = (char)client.read();
      if (ch == '\n') {
        if (++lineEndCount >= 2) break;
      } else if (ch != '\r') {
        lineEndCount = 0;
      }
    }
    updateAudio();
    updateLEDs();
  }
  if (strncmp(request, "GET /api/tone?", 14) == 0) {
    const char* p = strstr(request, "hz=");
    if (p) {
      long hz = atol(p + 3);
      if (hz >= 50 && hz <= 5000) startNote((uint16_t)hz);
    }
    sendStatus(client);
  } else if (strncmp(request, "GET /api/melody?", 16) == 0) {
    const char* p = strstr(request, "id=");
    if (p) {
      int id = atoi(p + 3);
      if (id >= 0 && id <= 2) startMelody((uint8_t)id);
    }
    updateAudio();
    sendStatus(client);
  } else if (strncmp(request, "GET /api/stop", 13) == 0) {
    stopSound();
    sendStatus(client);
  } else if (strncmp(request, "GET /api/status", 15) == 0) {
    sendStatus(client);
  } else {
    sendPage(client);
  }
  delay(1);
  client.stop();
}

void setup() {
  Serial.begin(115200);
  for (uint8_t i = 0; i < 4; ++i) {
    pinMode(LED_PINS[i], OUTPUT);
    digitalWrite(LED_PINS[i], LOW);
  }
  pinMode(BUZZER_PIN, OUTPUT);
  noTone(BUZZER_PIN);

  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println(F("ERROR: WiFiNINA module not found"));
    while (true) {
      updateLEDs();
    }
  }

  Serial.println(F("Starting local WiFi access point..."));
  int status = WiFi.beginAP(ssid, pass);
  if (status != WL_AP_LISTENING) {
    Serial.println(F("ERROR: could not start AP. Check WiFiNINA firmware."));
    while (true) {
      updateLEDs();
    }
  }
  delay(800);
  server.begin();
  Serial.print(F("Network: ")); Serial.println(ssid);
  Serial.print(F("Password: ")); Serial.println(pass);
  Serial.print(F("Open: http://")); Serial.println(WiFi.localIP());
}

void loop() {
  updateLEDs();
  updateAudio();
  WiFiClient client = server.available();
  if (client) serveClient(client);
}
