#include <Servo.h>
#include <string.h>   // strcmp, strtok, strlen
#include <stdlib.h>   // atol, atoi
#include <stdio.h>    // sprintf

// ─── Pines ───────────────────────────────────────────────────
const uint8_t SERVO_PIN = 9;
const uint8_t TEMP_PIN  = A0;   // Potenciómetro conectado a A0

// ─── Límites RAM ATmega328P ───────────────────────────────────
const int RAM_MIN = 0;
const int RAM_MAX = 2047;   // 2 KB de SRAM total

// ─── Buffer de entrada ───────────────────────────────────────
#define BUF_MAX 64
char    inputBuf[BUF_MAX + 1];
uint8_t bufLen = 0;

// ─── Estado del terminal ─────────────────────────────────────
Servo    myServo;
bool     waitingForAngle = false;
bool     readingTemp     = false;
uint32_t tempInterval    = 0;
uint32_t lastTempRead    = 0;
uint32_t tempStartTime   = 0;

// ─── Timeout para fin de línea ───────────────────────────────
#define CMD_TIMEOUT_MS 80
uint32_t lastCharTime = 0;

// ─── Utilidades ──────────────────────────────────────────────
void printPrompt() {
  Serial.print(F("\r\narduino> "));
}

void printSeparator() {
  Serial.println(F("--------------------------------------------"));
}

void strToLower(char *s) {
  for (; *s; s++) {
    if (*s >= 'A' && *s <= 'Z') *s += 32;
  }
}

char* strTrim(char *s) {
  while (*s == ' ' || *s == '\t') s++;
  if (*s == '\0') return s;

  char *end = s + strlen(s) - 1;

  while (end > s && (*end == ' ' || *end == '\t')) end--;

  *(end + 1) = '\0';

  return s;
}

// ─── COMANDO: servo ──────────────────────────────────────────
void handleServoCommand() {
  Serial.println(F("\r\n[servo] Control de servomotor"));
  printSeparator();
  Serial.println(F("Ingresa el angulo deseado (0 - 180 grados):"));
  waitingForAngle = true;
}

void applyServoAngle(char *raw) {
  raw = strTrim(raw);

  if (strlen(raw) == 0) {
    Serial.println(F("[servo] ERROR: entrada vacia. Ingresa un numero (0-180):"));
    return;
  }

  for (uint8_t i = 0; raw[i]; i++) {
    if (raw[i] < '0' || raw[i] > '9') {
      Serial.println(F("[servo] ERROR: solo numeros. Ingresa angulo (0-180):"));
      return;
    }
  }

  int angle = atoi(raw);

  angle = constrain(angle, 0, 180);

  myServo.write(angle);

  Serial.print(F("[servo] Moviendo a "));
  Serial.print(angle);
  Serial.println(F(" grados... OK"));

  waitingForAngle = false;

  printPrompt();
}

// ─── COMANDO: pot <ms> ───────────────────────────────────────
void handleTempCommand(char *args) {
  args = strTrim(args);

  if (strlen(args) == 0) {
    Serial.println(F("\r\nUso: pot <intervalo_ms>"));
    Serial.println(F("Ej:  pot 1000"));
    printPrompt();
    return;
  }

  long interval = atol(args);

  if (interval < 100) {
    Serial.println(F("[pot] Intervalo minimo: 100 ms. Usando 100."));
    interval = 100;
  }

  tempInterval  = (uint32_t)interval;
  tempStartTime = millis();
  lastTempRead  = tempStartTime - tempInterval;

  readingTemp = true;

  Serial.println(F("\r\n[pot] Leyendo potenciometro..."));
  Serial.println(F("      Presiona cualquier tecla para detener.\r\n"));
  Serial.println(F("Tiempo(ms)    Valor"));
  printSeparator();
}

void doTempReading() {
  uint32_t now = millis();

  if (now - lastTempRead < tempInterval) return;

  lastTempRead = now;

  int raw = analogRead(TEMP_PIN);

  char line[48];

  sprintf(line,
          "%10lu    %4d",
          (unsigned long)(now - tempStartTime),
          raw);

  Serial.println(line);
}

// ─── COMANDO: dump <ini> <fin> ───────────────────────────────
void handleDumpCommand(char *args) {
  args = strTrim(args);

  char *tok1 = strtok(args, " ");
  char *tok2 = strtok(NULL, " ");

  if (!tok1 || !tok2) {
    Serial.println(F("\r\nUso: dump <dir_inicio> <dir_fin>"));
    Serial.println(F("Ej:  dump 0 255"));
    Serial.print(F("RAM valida: 0 - "));
    Serial.println(RAM_MAX);
    printPrompt();
    return;
  }

  int startAddr = atoi(tok1);
  int endAddr   = atoi(tok2);

  if (startAddr < RAM_MIN) startAddr = RAM_MIN;
  if (endAddr > RAM_MAX) endAddr = RAM_MAX;

  if (startAddr > endAddr) {
    Serial.println(F("[dump] ERROR: dir_inicio debe ser <= dir_fin."));
    printPrompt();
    return;
  }

  Serial.println(F("\r\n[dump] RAM — formato: ADDR  HH HH ... | ASCII"));
  printSeparator();

  const int COLS = 16;

  for (int addr = startAddr; addr <= endAddr; addr += COLS) {

    char addrBuf[8];

    sprintf(addrBuf, "%04X  ", (unsigned int)addr);

    Serial.print(addrBuf);

    int rowEnd = min(addr + COLS - 1, endAddr);

    for (int i = addr; i <= rowEnd; i++) {
      char hx[4];
      sprintf(hx, "%02X ", (uint8_t)(*((volatile uint8_t *)i)));
      Serial.print(hx);
    }

    int pad = (addr + COLS - 1) - rowEnd;

    for (int p = 0; p < pad; p++) {
      Serial.print(F("   "));
    }

    Serial.print(F(" | "));

    for (int i = addr; i <= rowEnd; i++) {
      uint8_t c = *((volatile uint8_t *)i);
      Serial.print((char)((c >= 0x20 && c < 0x7F) ? c : '.'));
    }

    Serial.println();
  }

  printSeparator();

  char summary[32];

  sprintf(summary, "[dump] %d bytes volcados.",
          endAddr - startAddr + 1);

  Serial.println(summary);

  printPrompt();
}

// ─── COMANDO: help ───────────────────────────────────────────
void printHelp() {
  Serial.println(F("\r\n============ AYUDA ============"));

  Serial.println(F("servo"));
  Serial.println(F("    Mueve el servomotor."));
  Serial.println(F("    Pide el angulo (0-180)."));

  Serial.println(F(""));

  Serial.println(F("pot <ms>"));
  Serial.println(F("    Lee el potenciometro cada <ms> ms."));
  Serial.println(F("    Muestra valores entre 0 y 1023."));
  Serial.println(F("    Presiona cualquier tecla para detener."));

  Serial.println(F(""));

  Serial.println(F("dump <inicio> <fin>"));
  Serial.println(F("    Vuelca la RAM en hex + ASCII."));
  Serial.println(F("    Rango valido: 0 - 2047"));

  Serial.println(F(""));

  Serial.println(F("help"));
  Serial.println(F("    Muestra esta ayuda."));

  Serial.println(F("================================"));
}

// ─── Procesar comando completo ───────────────────────────────
void processCommand() {
  char *buf = strTrim(inputBuf);

  if (strlen(buf) == 0) {
    printPrompt();
    return;
  }

  char *cmdName = buf;
  char *args    = buf;

  while (*args && *args != ' ') args++;

  if (*args == ' ') {
    *args = '\0';
    args++;
  }

  strToLower(cmdName);

  if      (strcmp(cmdName, "servo") == 0) handleServoCommand();
  else if (strcmp(cmdName, "pot")   == 0) handleTempCommand(args);
  else if (strcmp(cmdName, "dump")  == 0) handleDumpCommand(args);
  else if (strcmp(cmdName, "help")  == 0) {
    printHelp();
    printPrompt();
  }
  else {
    Serial.print(F("\r\nComando desconocido: '"));
    Serial.print(cmdName);
    Serial.println(F("'  ->  escribe 'help'"));
    printPrompt();
  }
}

// ─── setup ───────────────────────────────────────────────────
void setup() {

  Serial.begin(9600);

  myServo.attach(SERVO_PIN);

  myServo.write(90);

  memset(inputBuf, 0, sizeof(inputBuf));

  Serial.println(F("\r\n"));
  Serial.println(F("====================================="));
  Serial.println(F("  Arduino UNO  |  Terminal Interactivo"));
  Serial.println(F("====================================="));
  Serial.println(F("Escribe 'help' para ver los comandos."));
  Serial.println(F("Configura el Monitor Serie en 9600 baud"));
  Serial.println(F("con terminacion 'Nueva linea' (\\n) o 'CR+LF'."));

  printPrompt();
}

// ─── Ejecuta comando y limpia buffer ─────────────────────────
void flushCommand() {

  if (bufLen == 0) return;

  inputBuf[bufLen] = '\0';

  Serial.println();

  if (waitingForAngle) {
    applyServoAngle(inputBuf);
  }
  else {
    processCommand();
  }

  memset(inputBuf, 0, sizeof(inputBuf));

  bufLen = 0;

  lastCharTime = 0;
}

// ─── loop ────────────────────────────────────────────────────
void loop() {

  // ── Lectura del potenciometro ────────────────────────────
  if (readingTemp) {

    doTempReading();

    if (Serial.available()) {

      while (Serial.available()) {
        Serial.read();
      }

      readingTemp = false;

      Serial.println(F("\r\n[pot] Lectura detenida."));

      printPrompt();
    }

    return;
  }

  // ── Lectura serial ──────────────────────────────────────
  while (Serial.available()) {

    char c = (char)Serial.read();

    if (c == '\r' || c == '\n') {

      flushCommand();
    }

    else if (c == '\b' || c == 127) {

      if (bufLen > 0) {

        bufLen--;

        inputBuf[bufLen] = '\0';

        Serial.print(F("\b \b"));
      }
    }

    else if (c >= 0x20 && c < 0x7F) {

      if (bufLen < BUF_MAX) {

        inputBuf[bufLen++] = c;

        inputBuf[bufLen] = '\0';

        Serial.print(c);
      }

      lastCharTime = millis();
    }
  }

  // ── Timeout para Tinkercad ──────────────────────────────
  if (bufLen > 0 &&
      lastCharTime > 0 &&
      (millis() - lastCharTime) >= CMD_TIMEOUT_MS) {

    flushCommand();
  }
}
