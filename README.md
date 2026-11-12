
const int tlacidla[4] = {2, 3, 4, 5};
const int ledky[4] = {8, 9, 10, 11};
const int bzuciak = 6;

// Ton pre kazde tlacidlo
const int tony[4] = {262, 330, 392, 523};

bool predtym[4] = {false, false, false, false};
bool showSpustena = false;

void setup() {
  for (int i = 0; i < 4; i++) {
    pinMode(tlacidla[i], INPUT_PULLUP);
    pinMode(ledky[i], OUTPUT);
  }

  pinMode(bzuciak, OUTPUT);
}

void vypniLED() {
  for (int i = 0; i < 4; i++) {
    digitalWrite(ledky[i], LOW);
  }
}

void lightShow() {
  // Vlastna vitazna melodia
  int melodia[] = {
    523, 659, 784, 1047,
    988, 784, 659, 523,
    392, 523, 659, 784,
    1047, 784, 1047, 1319
  };

  // Efekt 1: Beziace svetla + melodia
  for (int i = 0; i < 16; i++) {
    vypniLED();

    digitalWrite(ledky[i % 4], HIGH);
    tone(bzuciak, melodia[i], 140);
    delay(160);
  }

  // Efekt 2: Striedanie dvojic
  for (int i = 0; i < 8; i++) {
    vypniLED();

    digitalWrite(ledky[i % 2], HIGH);
    digitalWrite(ledky[(i % 2) + 2], HIGH);

    tone(bzuciak, i % 2 == 0 ? 784 : 1047, 110);
    delay(130);
  }

  // Efekt 3: Rychly stroboskop
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 4; j++) {
      digitalWrite(ledky[j], HIGH);
    }

    tone(bzuciak, 1000 + i * 100, 65);
    delay(80);

    vypniLED();
    delay(65);
  }

  // Finale!
  for (int i = 0; i < 4; i++) {
    digitalWrite(ledky[i], HIGH);
  }

  tone(bzuciak, 1319, 500);
  delay(550);

  noTone(bzuciak);
  vypniLED();
}

void loop() {
  bool stlacene[4];
  bool vsetky = true;

  for (int i = 0; i < 4; i++) {
    stlacene[i] = digitalRead(tlacidla[i]) == LOW;

    if (!stlacene[i]) {
      vsetky = false;
    }
  }

  // Vsetky 4 = LIGHT SHOW
  if (vsetky && !showSpustena) {
    showSpustena = true;
    lightShow();
  }

  if (!vsetky) {
    showSpustena = false;
  }

  // Normalny rezim tlacidiel
  for (int i = 0; i < 4; i++) {
    digitalWrite(ledky[i],
                 stlacene[i] ? HIGH : LOW);

    // Pipne iba pri novom stlaceni
    if (stlacene[i] && !predtym[i] && !vsetky) {
      tone(bzuciak, tony[i], 150);
    }

    predtym[i] = stlacene[i];
  }
}
