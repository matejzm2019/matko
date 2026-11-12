/*
  UNO WiFi Rev2: 4 physical buttons + 4 LEDs + passive piezo buzzer
  and a responsive local web dashboard.

  Wiring:
    Button outputs -> D2, D3, D4, D5 (each button closes to GND)
    LEDs (each with 220-330 ohm resistor) -> D8, D9, D10, D11, cathodes -> GND
    Passive piezo (+) -> D6, (-) -> GND

  Physical buttons: LED stays on only while pressed; press = short note.
  Web buttons: toggle the corresponding LED on/off; enabling = short note.
  All 4 physical buttons together: toggle PARTY MODE.
  Web PARTY button: toggle PARTY MODE.
  In PARTY MODE, the lighting animation and original melody loop until stopped.

  WiFi: Arduino itself creates a WPA2 access point. No router needed.
  Install 'WiFiNINA' and Arduino megaAVR Boards; select Arduino Uno WiFi Rev2.
*/
#include <SPI.h>
#include <WiFiNINA.h>

// Arduino creates this WiFi network. Change password before use!
char ssid[] = "ArduinoParty";
char pass[] = "arduino1234";  // Minimum 8 characters

const uint8_t BUTTON_PINS[4] = {2, 3, 4, 5};
const uint8_t LED_PINS[4] = {8, 9, 10, 11};
const uint8_t BUZZER_PIN = 6;
const uint16_t BUTTON_NOTES[4] = {262, 330, 392, 523};
const uint16_t SONG[16] = {
  523, 659, 784, 1047, 988, 784, 659, 523,
  392, 523, 659, 784, 1047, 784, 1047, 1319
};

WiFiServer server(80);
bool webOn[4] = {false, false, false, false};
bool pressed[4] = {false, false, false, false};
bool rawPrevious[4] = {false, false, false, false};
bool ledState[4] = {false, false, false, false};
unsigned long buttonChangedAt[4] = {0, 0, 0, 0};
bool allFourLatch = false;
bool party = false;
uint8_t partyStep = 0;
unsigned long partyLastStepAt = 0;

void setParty(bool enabled) {
  party = enabled;
  partyStep = 0;
  partyLastStepAt = millis();
  if (party) tone(BUZZER_PIN, SONG[0], 130);
  else noTone(BUZZER_PIN);
}

void readPhysicalButtons() {
  unsigned long now = millis();
  for (uint8_t i = 0; i < 4; i++) {
    bool raw = digitalRead(BUTTON_PINS[i]) == LOW;
    if (raw != rawPrevious[i]) {
      rawPrevious[i] = raw;
      buttonChangedAt[i] = now;
    }
    // Small debounce, so a single press does not create repeated triggers.
    if (now - buttonChangedAt[i] >= 25 && pressed[i] != raw) {
      pressed[i] = raw;
      if (pressed[i] && !party) tone(BUZZER_PIN, BUTTON_NOTES[i], 100);
    }
  }

  bool allFour = pressed[0] && pressed[1] && pressed[2] && pressed[3];
  if (allFour && !allFourLatch) setParty(!party);
  allFourLatch = allFour;
}

void updateParty() {
  if (!party) return;
  unsigned long now = millis();
  if (now - partyLastStepAt >= 165) {
    partyLastStepAt = now;
    partyStep = (partyStep + 1) % 24;
    tone(BUZZER_PIN, SONG[partyStep % 16], 125);
  }
}

void updateLEDs() {
  for (uint8_t i = 0; i < 4; i++) {
    if (!party) {
      ledState[i] = webOn[i] || pressed[i];
    } else if (partyStep < 12) {  // Running lights
      ledState[i] = (i == partyStep % 4);
    } else if (partyStep < 20) {  // Alternating pairs
      ledState[i] = ((i + partyStep) % 2 == 0);
    } else {                     // Strobe
      ledState[i] = (partyStep % 2 == 0);
    }
    digitalWrite(LED_PINS[i], ledState[i] ? HIGH : LOW);
  }
}

void sendHeaders(WiFiClient &client, bool json) {
  client.println(F("HTTP/1.1 200 OK"));
  if (json) client.println(F("Content-Type: application/json; charset=utf-8"));
  else client.println(F("Content-Type: text/html; charset=utf-8"));
  client.println(F("Cache-Control: no-store"));
  client.println(F("Connection: close"));
  client.println();
}

void sendStatus(WiFiClient &client) {
  sendHeaders(client, true);
  client.print(F("{\"party\":"));
  client.print(party ? '1' : '0');
  client.print(F(",\"web\":["));
  for (uint8_t i = 0; i < 4; i++) {
    if (i) client.print(',');
    client.print(webOn[i] ? '1' : '0');
  }
  client.print(F("],\"pressed\":["));
  for (uint8_t i = 0; i < 4; i++) {
    if (i) client.print(',');
    client.print(pressed[i] ? '1' : '0');
  }
  client.print(F("],\"led\":["));
  for (uint8_t i = 0; i < 4; i++) {
    if (i) client.print(',');
    client.print(ledState[i] ? '1' : '0');
  }
  client.println(F("]}"));
}

// The HTML page lives in flash, not the 6 KB data RAM.
const char HTML_PAGE[] PROGMEM = R"PAGE(<!doctype html><html lang="sk"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#0b0b16"><title>UNO Light Control</title>
<style>
*{box-sizing:border-box}body{margin:0;background:radial-gradient(ellipse at 50% -10%,#25204c 0,#0b0b16 55%);color:#f7f6ff;font-family:system-ui,-apple-system,Segoe UI,sans-serif;min-height:100vh}
main{max-width:750px;margin:auto;padding:38px 20px 60px}.top{display:flex;justify-content:space-between;align-items:center;gap:12px}.chip{font-size:11px;font-weight:850;letter-spacing:2px;color:#a9a2ff;background:#24203d;padding:9px 13px;border:1px solid #39305e;border-radius:99px}.net{font-size:12px;color:#8f8ca7}.net:before{content:'●';color:#54f0a3;margin-right:7px}h1{font-size:clamp(34px,7vw,56px);line-height:1.03;margin:34px 0 12px;letter-spacing:-2px}.intro{color:#a2a0b6;font-size:15px;margin-bottom:28px;line-height:1.5}
.grid{display:grid;grid-template-columns:repeat(2,1fr);gap:14px}.tile{appearance:none;border:1px solid #302e49;background:#19182a;border-radius:22px;text-align:left;color:inherit;padding:21px;min-height:176px;cursor:pointer;transition:.18s transform,.18s border-color,.18s background}.tile:active{transform:scale(.97)}.tile.on{border-color:#8277c6;background:#232039}.tile:disabled{opacity:.55;cursor:default}.orb{display:block;width:57px;height:57px;border-radius:50%;background:var(--c);opacity:.25;box-shadow:0 0 0 6px #ffffff04;transition:.2s}.tile.on .orb{opacity:1;box-shadow:0 0 22px 5px var(--glow),0 0 0 7px #ffffff09}.nm{display:block;margin-top:17px;font-weight:750;font-size:16px}.small{display:block;margin-top:5px;font-size:12px;color:#a09cb6}
.party{margin-top:22px;width:100%;border:1px solid #7d66e5;border-radius:20px;padding:20px 18px;background:linear-gradient(115deg,#7050db,#462fb0);color:#fff;text-align:left;cursor:pointer;display:flex;justify-content:space-between;align-items:center;gap:10px}.party strong{font-size:19px;letter-spacing:.3px}.party small{display:block;opacity:.78;margin-top:5px}.party.active{background:linear-gradient(115deg,#ce3bcf,#6329db);box-shadow:0 0 27px #9738dc55;animation:pulse 1.2s infinite alternate}@keyframes pulse{to{box-shadow:0 0 42px #bc3df188}}.ico{font-size:28px}.hint{margin-top:24px;color:#77738e;font-size:12px;line-height:1.7}.err{color:#ff8699!important}.footer{margin-top:34px;color:#68637b;font-size:11px}
</style></head><body><main><div class="top"><span class="chip">UNO / WIFI REV2</span><span id="net" class="net">Pripája sa...</span></div>
<h1>Light Control<span style="color:#967dff">.</span></h1><p class="intro">Štyri svetlá, štyri tóny. Ovládaj ich z mobilu alebo fyzickými tlačidlami.</p>
<div class="grid">
<button class="tile" id="b0" onclick="toggle(0)" style="--c:#f6f7ff;--glow:#f6f7ff66"><span class="orb"></span><span class="nm">Biela</span><span class="small" id="s0">Vypnutá</span></button>
<button class="tile" id="b1" onclick="toggle(1)" style="--c:#ff4a66;--glow:#ff4a6680"><span class="orb"></span><span class="nm">Červená</span><span class="small" id="s1">Vypnutá</span></button>
<button class="tile" id="b2" onclick="toggle(2)" style="--c:#ffe04d;--glow:#ffe04d88"><span class="orb"></span><span class="nm">Žltá</span><span class="small" id="s2">Vypnutá</span></button>
<button class="tile" id="b3" onclick="toggle(3)" style="--c:#4e9fff;--glow:#4e9fff88"><span class="orb"></span><span class="nm">Modrá</span><span class="small" id="s3">Vypnutá</span></button>
</div><button class="party" id="party" onclick="partyToggle()"><span><strong>✦ PARTY MODE</strong><small id="pt">Spusti svetelnú show + melódiu</small></span><span class="ico">♫</span></button>
<p class="hint">Klikni na farbu a prepni LED. Fyzické tlačidlá fungujú tiež. Keď stlačíš všetky štyri naraz, PARTY MODE sa prepne.</p><div class="footer">AP MODE · WiFiNINA · UNO WiFi Rev2</div></main>
<script>
let state={party:0,web:[0,0,0,0],pressed:[0,0,0,0],led:[0,0,0,0]};
let busy=false;
async function command(path){if(busy)return;busy=true;try{const r=await fetch(path,{cache:'no-store'});if(!r.ok)throw Error();state=await r.json();paint()}catch(e){offline()}finally{busy=false}}
function toggle(i){if(!state.party)command('/toggle?i='+i)}
function partyToggle(){command('/party')}
function paint(){document.getElementById('net').textContent='Online · Arduino WiFi';document.getElementById('net').classList.remove('err');for(let i=0;i<4;i++){const b=document.getElementById('b'+i);b.classList.toggle('on',!!state.led[i]);b.disabled=!!state.party;document.getElementById('s'+i).textContent=state.party?'Party efekt':state.pressed[i]?'Fyzicky stlačené':state.web[i]?'Zapnutá':'Vypnutá'}document.getElementById('party').classList.toggle('active',!!state.party);document.getElementById('pt').textContent=state.party?'Hrá sa! Klikni pre vypnutie':'Spusti svetelnú show + melódiu'}
function offline(){const x=document.getElementById('net');x.textContent='Arduino nedostupné';x.classList.add('err')}
async function refresh(){if(busy)return;try{const r=await fetch('/status',{cache:'no-store'});if(!r.ok)throw Error();state=await r.json();paint()}catch(e){offline()}}
setInterval(refresh,700);refresh();
</script></body></html>)PAGE";

void sendPage(WiFiClient &client) {
  sendHeaders(client, false);
  // Send in small blocks so WiFiNINA does not have to send each char
  // over SPI separately; preserve the tiny SRAM of the Uno WiFi Rev2.
  const uint16_t total = strlen_P(HTML_PAGE);
  char chunk[96];
  for (uint16_t offset = 0; offset < total; offset += sizeof(chunk)) {
    uint8_t count = ((total - offset) < sizeof(chunk))
                     ? (uint8_t)(total - offset)
                     : (uint8_t)sizeof(chunk);
    for (uint8_t i = 0; i < count; i++) {
      chunk[i] = (char)pgm_read_byte(HTML_PAGE + offset + i);
    }
    client.write((const uint8_t*)chunk, count);
  }
}

void handleClient(WiFiClient &client) {
  // Read the first HTTP request line only. No heap-heavy String allocation.
  char request[72];
  uint8_t pos = 0;
  unsigned long start = millis();
  while (client.connected() && millis() - start < 700) {
    if (client.available()) {
      char ch = (char)client.read();
      if (ch == '\n') break;
      if (ch != '\r' && pos < sizeof(request) - 1) request[pos++] = ch;
    }
  }
  request[pos] = '\0';

  if (strncmp(request, "GET /toggle?i=", 14) == 0 &&
      request[14] >= '0' && request[14] <= '3') {
    uint8_t i = request[14] - '0';
    if (!party) {
      webOn[i] = !webOn[i];
      if (webOn[i]) tone(BUZZER_PIN, BUTTON_NOTES[i], 110);
    }
    updateLEDs();
    sendStatus(client);
  } else if (strncmp(request, "GET /party", 10) == 0) {
    setParty(!party);
    updateLEDs();
    sendStatus(client);
  } else if (strncmp(request, "GET /status", 11) == 0) {
    sendStatus(client);
  } else {
    sendPage(client);
  }
  delay(2);
  client.stop();
}

void setup() {
  Serial.begin(115200);
  for (uint8_t i = 0; i < 4; i++) {
    pinMode(BUTTON_PINS[i], INPUT_PULLUP);
    pinMode(LED_PINS[i], OUTPUT);
    digitalWrite(LED_PINS[i], LOW);
  }
  pinMode(BUZZER_PIN, OUTPUT);

  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println(F("CHYBA: WiFi modul nebol najdeny. Skontroluj typ dosky."));
    while (true) delay(1000);
  }
  Serial.println(F("Vytvaram vlastnu Arduino WiFi siet..."));
  int status = WiFi.beginAP(ssid, pass);
  if (status != WL_AP_LISTENING && status != WL_AP_CONNECTED) {
    Serial.println(F("CHYBA: WiFi AP sa nepodarilo vytvorit."));
    Serial.println(F("Over heslo (min. 8 znakov) a firmware WiFiNINA."));
    while (true) delay(1000);
  }
  delay(200);
  server.begin();
  Serial.print(F("NAZOV WIFI: "));
  Serial.println(ssid);
  Serial.print(F("HESLO WIFI: "));
  Serial.println(pass);
  Serial.print(F("OTVOR V MOBILE: http://"));
  Serial.println(WiFi.localIP());
}

void loop() {
  readPhysicalButtons();
  updateParty();
  updateLEDs();
  WiFiClient client = server.available();
  if (client) handleClient(client);
}
