#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>

// LCD I2C 16x2
LiquidCrystal_I2C lcd(0x20, 16, 2);

// Teclado 4x4
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {9, 8, 7, 6};
byte colPins[COLS] = {5, 4, 3, 2};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Buffers
#define INPUT_MAX 14
char    inputBuf[INPUT_MAX + 1];
uint8_t inputLen = 0;

#define RX_MAX 32
char    rxBuf[RX_MAX + 1];
uint8_t rxLen = 0;

// ─────────────────────────────────────────────────────
// Helpers LCD
// ─────────────────────────────────────────────────────
void lcdClearLine(uint8_t row) {
  lcd.setCursor(0, row);
  lcd.print(F("                "));
}

void lcdShowStatus(const char *msg) {
  lcd.setCursor(0, 0);
  lcd.print(F("                "));
  lcd.setCursor(0, 0);
  for (uint8_t i = 0; i < 16 && msg[i]; i++) lcd.print(msg[i]);
}

void lcdShowInput() {
  lcdClearLine(1);
  lcd.setCursor(0, 1);
  lcd.print(F("> "));
  lcd.print(inputBuf);
}

// ─────────────────────────────────────────────────────
// Envío al terminal vía Serial hardware
// ─────────────────────────────────────────────────────
void sendRawLine(const char *s) {
  Serial.print(s);
  Serial.print('\n');
}

void clearInput() {
  inputLen = 0;
  inputBuf[0] = '\0';
  lcdShowInput();
}

// ─────────────────────────────────────────────────────
// Manejo de teclas
// ─────────────────────────────────────────────────────
void handleKey(char k) {
  if (k == 'A') { sendRawLine("servo"); clearInput(); return; }
  if (k == 'C') { sendRawLine("help");  clearInput(); return; }

  if (k == 'B') {
    if (inputLen == 0) {
      lcdShowStatus("Tipea ms y B");
    } else {
      char cmd[24];
      snprintf(cmd, sizeof(cmd), "pot %s", inputBuf);
      sendRawLine(cmd);
      clearInput();
    }
    return;
  }

  if (k == 'D') {
    if (inputLen == 0) {
      Serial.print('\n');
    } else {
      sendRawLine(inputBuf);
      clearInput();
    }
    return;
  }

  if (k == '*') {
    if (inputLen > 0) {
      inputLen--;
      inputBuf[inputLen] = '\0';
      lcdShowInput();
    }
    return;
  }

  if (k == '#') {
    if (inputLen < INPUT_MAX) {
      inputBuf[inputLen++] = ' ';
      inputBuf[inputLen] = '\0';
      lcdShowInput();
    }
    return;
  }

  if (k >= '0' && k <= '9') {
    if (inputLen < INPUT_MAX) {
      inputBuf[inputLen++] = k;
      inputBuf[inputLen] = '\0';
      lcdShowInput();
    }
  }
}

// ─────────────────────────────────────────────────────
// setup
// ─────────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);

  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print(F("UI Arduino"));
  lcd.setCursor(0, 1);
  lcd.print(F("Conectando..."));
  delay(1500);

  lcd.clear();
  delay(200);
  lcdShowStatus("A=srv B=pot C=?");

  memset(inputBuf, 0, sizeof(inputBuf));
  memset(rxBuf,    0, sizeof(rxBuf));
  lcdShowInput();
}

// ─────────────────────────────────────────────────────
// loop
// ─────────────────────────────────────────────────────
void loop() {
  // Teclado
  char k = keypad.getKey();
  if (k) handleKey(k);

  // Lectura desde el terminal (línea a línea)
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      rxBuf[rxLen] = '\0';
      if (rxLen > 0) {
        lcdShowStatus(rxBuf);
      }
      rxLen = 0;
    } else if (c >= 0x20 && c < 0x7F) {
      if (rxLen < RX_MAX) rxBuf[rxLen++] = c;
    }
  }
}