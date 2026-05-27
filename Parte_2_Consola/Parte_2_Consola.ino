#include <Servo.h>
#include <string.h>
#include <stdlib.h>

// Pines
const uint8_t SERVO_PIN = 9;
const uint8_t TEMP_PIN  = A0;

// Buffer
#define BUF_MAX 32
char    inputBuf[BUF_MAX + 1];
uint8_t bufLen = 0;

// Estado
Servo    myServo;
bool     waitingForAngle = false;
bool     readingTemp     = false;
uint32_t tempInterval    = 0;
uint32_t lastTempRead    = 0;
uint32_t tempStartTime   = 0;

// Utilidades
void strToLower(char *s) {
  for (; *s; s++) if (*s >= 'A' && *s <= 'Z') *s += 32;
}

char* strTrim(char *s) {
  while (*s == ' ' || *s == '\t') s++;
  if (*s == '\0') return s;
  char *end = s + strlen(s) - 1;
  while (end > s && (*end == ' ' || *end == '\t')) end--;
  *(end + 1) = '\0';
  return s;
}

// COMANDO: servo
void handleServoCommand() {
  Serial.println(F("[servo]"));
  Serial.println(F("Angulo 0-180+D"));
  waitingForAngle = true;
}

void applyServoAngle(char *raw) {
  raw = strTrim(raw);
  if (strlen(raw) == 0) {
    Serial.println(F("Vacio. 0-180:"));
    return;
  }
  for (uint8_t i = 0; raw[i]; i++) {
    if (raw[i] < '0' || raw[i] > '9') {
      Serial.println(F("Solo num 0-180:"));
      return;
    }
  }
  int angle = atoi(raw);
  angle = constrain(angle, 0, 180);
  myServo.write(angle);
  Serial.print(F("Servo OK: "));
  Serial.print(angle);
  Serial.println(F(" deg"));
  waitingForAngle = false;
}

// COMANDO: pot <ms>
void handleTempCommand(char *args) {
  args = strTrim(args);
  if (strlen(args) == 0) {
    Serial.println(F("Uso: pot <ms>"));
    return;
  }
  long interval = atol(args);
  if (interval < 100) {
    Serial.println(F("Min 100ms"));
    interval = 100;
  }
  tempInterval  = (uint32_t)interval;
  tempStartTime = millis();
  lastTempRead  = tempStartTime - tempInterval;
  readingTemp   = true;
  Serial.print(F("[pot] cada "));
  Serial.print(interval);
  Serial.println(F("ms"));
  Serial.println(F("D para detener"));
}

void doTempReading() {
  uint32_t now = millis();
  if (now - lastTempRead < tempInterval) return;
  lastTempRead = now;
  int raw = analogRead(TEMP_PIN);
  Serial.print(F("t="));
  Serial.print((unsigned long)(now - tempStartTime));
  Serial.print(F("ms v="));
  Serial.println(raw);
}

// COMANDO: help
void printHelp() {
  Serial.println(F("--- AYUDA ---"));   delay(700);
  Serial.println(F("A: servo"));         delay(700);
  Serial.println(F("B: pot <ms>"));      delay(700);
  Serial.println(F("C: help"));          delay(700);
  Serial.println(F("D: ENTER/STOP"));    delay(700);
  Serial.println(F("*: borrar"));        delay(700);
  Serial.println(F("#: espacio"));       delay(700);
  Serial.println(F("0-9: digitos"));     delay(700);
  Serial.println(F("-------------"));
}

// Procesar comando
void processCommand() {
  char *buf = strTrim(inputBuf);
  if (strlen(buf) == 0) return;

  char *cmdName = buf;
  char *args    = buf;
  while (*args && *args != ' ') args++;
  if (*args == ' ') { *args = '\0'; args++; }

  strToLower(cmdName);

  if      (strcmp(cmdName, "servo") == 0) handleServoCommand();
  else if (strcmp(cmdName, "pot")   == 0) handleTempCommand(args);
  else if (strcmp(cmdName, "help")  == 0) printHelp();
  else {
    Serial.print(F("? "));
    Serial.println(cmdName);
  }
}

// setup
void setup() {
  Serial.begin(9600);

  myServo.attach(SERVO_PIN);
  myServo.write(90);

  memset(inputBuf, 0, sizeof(inputBuf));

  delay(500);
  Serial.println(F("Terminal listo"));
  Serial.println(F("Pulsa A/B/C"));
}

// Ejecutar línea recibida
void flushCommand() {
  if (bufLen == 0) return;
  inputBuf[bufLen] = '\0';

  if (waitingForAngle) applyServoAngle(inputBuf);
  else                 processCommand();

  memset(inputBuf, 0, sizeof(inputBuf));
  bufLen = 0;
}

// loop
void loop() {
  // Modo lectura continua de potenciómetro
  if (readingTemp) {
    doTempReading();
    if (Serial.available()) {
      while (Serial.available()) Serial.read();
      readingTemp = false;
      Serial.println(F("[pot] detenido"));
    }
    return;
  }

  // Recepción de comandos línea a línea
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r' || c == '\n') {
      flushCommand();
    }
    else if (c == '\b' || c == 127) {
      if (bufLen > 0) { bufLen--; inputBuf[bufLen] = '\0'; }
    }
    else if (c >= 0x20 && c < 0x7F) {
      if (bufLen < BUF_MAX) {
        inputBuf[bufLen++] = c;
        inputBuf[bufLen] = '\0';
      }
    }
  }
}