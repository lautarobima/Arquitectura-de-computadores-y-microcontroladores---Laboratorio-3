#include <Servo.h>
#include <string.h>   // strcmp, strtok, strlen
#include <stdlib.h>   // atol, atoi
#include <stdio.h>    // sprintf

// ─── Pines ───────────────────────────────────────────────────
const uint8_t SERVO_PIN = 9;
const uint8_t TEMP_PIN  = A0;

// ─── Constantes TMP36 ────────────────────────────────────────
// V_out = 10 mV/°C,  0 °C → 500 mV,  -40 °C → 100 mV
// temp = (V_mV - 500) / 10 + 25  =  V*100 - 50  (con Vref=5V)
const float VREF = 5.0f;

// ─── Límites RAM ATmega328P ───────────────────────────────────
const int RAM_MIN = 0;
const int RAM_MAX = 2047;   // 2 KB de SRAM total

// ─── Buffer de entrada ───────────────────────────────────────
// CRÍTICO: usar char[] en lugar de String para evitar
// fragmentación del heap que corrompe el buffer en runtime.
#define BUF_MAX 64
char    inputBuf[BUF_MAX + 1];  // +1 para el '\0'
uint8_t bufLen = 0;

// ─── Estado del terminal ─────────────────────────────────────
Servo    myServo;
bool     waitingForAngle = false;
bool     readingTemp     = false;
uint32_t tempInterval    = 0;
uint32_t lastTempRead    = 0;
uint32_t tempStartTime   = 0;

// ─── Timeout para fin de línea (compatibilidad Tinkercad) ────
// Tinkercad no envía '\n' ni '\r'. Si el buffer tiene contenido
// y no llega ningún byte nuevo en CMD_TIMEOUT ms, se procesa
// el comando automáticamente. También funciona con hardware real
// que sí envía '\n' (ambos métodos coexisten sin conflicto).
#define CMD_TIMEOUT_MS 80
uint32_t lastCharTime = 0;   // millis() del último carácter recibido

// ─── Utilidades ──────────────────────────────────────────────
void printPrompt() {
  Serial.print(F("\r\narduino> "));
}

void printSeparator() {
  Serial.println(F("--------------------------------------------"));
}

// Convierte un char[] a minúsculas in-place
void strToLower(char *s) {
  for (; *s; s++) {
    if (*s >= 'A' && *s <= 'Z') *s += 32;
  }
}

// Elimina espacios al inicio y al final in-place; devuelve puntero al inicio
char* strTrim(char *s) {
  // Trim izquierdo
  while (*s == ' ' || *s == '\t') s++;
  if (*s == '\0') return s;
  // Trim derecho
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

  // Valida que sea un número
  if (strlen(raw) == 0) {
    Serial.println(F("[servo] ERROR: entrada vacia. Ingresa un numero (0-180):"));
    return;   // sigue esperando
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

// ─── COMANDO: temp <ms> ──────────────────────────────────────
void handleTempCommand(char *args) {
  args = strTrim(args);

  if (strlen(args) == 0) {
    Serial.println(F("\r\nUso: temp <intervalo_ms>"));
    Serial.println(F("Ej:  temp 1000   (lee cada 1 segundo)"));
    printPrompt();
    return;
  }

  long interval = atol(args);
  if (interval < 100) {
    Serial.println(F("[temp] Intervalo minimo: 100 ms. Usando 100."));
    interval = 100;
  }

  tempInterval  = (uint32_t)interval;
  tempStartTime = millis();
  lastTempRead  = tempStartTime - tempInterval;  // dispara lectura inmediata

  readingTemp = true;

  Serial.println(F("\r\n[temp] Leyendo temperatura..."));
  Serial.println(F("       Presiona cualquier tecla para detener.\r\n"));
  Serial.println(F("Tiempo(ms)    Temp(C)    Tension(mV)"));
  printSeparator();
}

void doTempReading() {
  uint32_t now = millis();
  if (now - lastTempRead < tempInterval) return;
  lastTempRead = now;

  int   raw     = analogRead(TEMP_PIN);
  float voltage = (float)raw * VREF / 1023.0f;
  float tempC   = voltage * 100.0f - 50.0f;   // TMP36: temp=(V*1000-500)/10
  float mv      = voltage * 1000.0f;

  char line[48];
  // dtostrf(valor, ancho_total, decimales, buffer)
  char tBuf[8], vBuf[8];
  dtostrf(tempC, 6, 2, tBuf);
  dtostrf(mv,    7, 1, vBuf);

  sprintf(line, "%10lu    %s    %s", (unsigned long)(now - tempStartTime), tBuf, vBuf);
  Serial.println(line);
}

// ─── COMANDO: dump <ini> <fin> ───────────────────────────────
void handleDumpCommand(char *args) {
  args = strTrim(args);

  // Espera dos tokens separados por espacio
  char *tok1 = strtok(args, " ");
  char *tok2 = strtok(NULL, " ");

  if (!tok1 || !tok2) {
    Serial.println(F("\r\nUso: dump <dir_inicio> <dir_fin>"));
    Serial.println(F("Ej:  dump 0 255"));
    Serial.print  (F("RAM valida: 0 - "));
    Serial.println(RAM_MAX);
    printPrompt();
    return;
  }

  int startAddr = atoi(tok1);
  int endAddr   = atoi(tok2);

  if (startAddr < RAM_MIN) startAddr = RAM_MIN;
  if (endAddr   > RAM_MAX) endAddr   = RAM_MAX;
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

    // Bytes en hex
    for (int i = addr; i <= rowEnd; i++) {
      char hx[4];
      sprintf(hx, "%02X ", (uint8_t)(*((volatile uint8_t *)i)));
      Serial.print(hx);
    }

    // Relleno para última fila incompleta
    int pad = (addr + COLS - 1) - rowEnd;
    for (int p = 0; p < pad; p++) Serial.print(F("   "));

    Serial.print(F(" | "));

    // Columna ASCII
    for (int i = addr; i <= rowEnd; i++) {
      uint8_t c = *((volatile uint8_t *)i);
      Serial.print((char)((c >= 0x20 && c < 0x7F) ? c : '.'));
    }

    Serial.println();
  }

  printSeparator();
  char summary[32];
  sprintf(summary, "[dump] %d bytes volcados.", endAddr - startAddr + 1);
  Serial.println(summary);
  printPrompt();
}

// ─── COMANDO: help ───────────────────────────────────────────
void printHelp() {
  Serial.println(F("\r\n============ AYUDA ============"));
  Serial.println(F("servo"));
  Serial.println(F("    Mueve el servomotor."));
  Serial.println(F("    Pide el angulo (0-180) de forma interactiva."));
  Serial.println(F(""));
  Serial.println(F("temp <ms>"));
  Serial.println(F("    Lee la temperatura cada <ms> milisegundos."));
  Serial.println(F("    Muestra tiempo transcurrido, Celsius y mV."));
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

// ─── Procesar comando completo ────────────────────────────────
// inputBuf ya esta null-terminado y tiene el comando completo.
void processCommand() {
  char *buf = strTrim(inputBuf);

  if (strlen(buf) == 0) {
    printPrompt();
    return;
  }

  // Separar nombre del comando de los argumentos
  // strtok modifica buf, así que guardamos puntero al inicio de args
  char *cmdName = buf;
  char *args    = buf;

  // Avanza args hasta el primer espacio (o fin de string)
  while (*args && *args != ' ') args++;

  if (*args == ' ') {
    *args = '\0';   // termina el nombre del comando
    args++;         // args apunta al primer char de los argumentos
  }
  // Si no había espacio, args apunta al '\0' final → args vacíos

  strToLower(cmdName);

  // ── Despacho de comandos ──────────────────────────────────
  // CRÍTICO: usar strcmp() no == para comparar char[]
  if      (strcmp(cmdName, "servo") == 0) handleServoCommand();
  else if (strcmp(cmdName, "temp")  == 0) handleTempCommand(args);
  else if (strcmp(cmdName, "dump")  == 0) handleDumpCommand(args);
  else if (strcmp(cmdName, "help")  == 0) { printHelp(); printPrompt(); }
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
  // NOTA: NO usar while(!Serial) en el UNO — ese guard es solo
  // para Leonardo/Micro (USB nativo). En el UNO congela el setup.

  myServo.attach(SERVO_PIN);
  myServo.write(90);   // posición central

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

// ─── Ejecuta el comando actual y limpia el buffer ─────────────
// Llamada tanto por '\n'/'\r' como por timeout de Tinkercad.
void flushCommand() {
  if (bufLen == 0) return;
  inputBuf[bufLen] = '\0';
  Serial.println();
  if (waitingForAngle) {
    applyServoAngle(inputBuf);
  } else {
    processCommand();
  }
  memset(inputBuf, 0, sizeof(inputBuf));
  bufLen = 0;
  lastCharTime = 0;
}

// ─── loop ────────────────────────────────────────────────────
void loop() {

  // ── Modo lectura de temperatura ──────────────────────────
  if (readingTemp) {
    doTempReading();
    if (Serial.available()) {
      while (Serial.available()) Serial.read();
      readingTemp = false;
      Serial.println(F("\r\n[temp] Lectura detenida."));
      printPrompt();
    }
    return;
  }

  // ── Lectura de bytes entrantes ────────────────────────────
  while (Serial.available()) {
    char c = (char)Serial.read();

    if (c == '\r' || c == '\n') {
      // Hardware real o Monitor Serie con terminador: procesa ya.
      flushCommand();

    } else if (c == '\b' || c == 127) {
      if (bufLen > 0) {
        bufLen--;
        inputBuf[bufLen] = '\0';
        Serial.print(F("\b \b"));
      }

    } else if (c >= 0x20 && c < 0x7F) {
      if (bufLen < BUF_MAX) {
        inputBuf[bufLen++] = c;
        inputBuf[bufLen]   = '\0';
        Serial.print(c);
      }
      lastCharTime = millis();
    }
    // Bytes de control desconocidos se descartan
  }

  // ── Timeout: fin de linea sin '\n' (Tinkercad) ───────────
  // Si llegaron caracteres pero no hay mas datos desde hace
  // CMD_TIMEOUT_MS ms, el usuario ya termino de escribir.
  if (bufLen > 0 && lastCharTime > 0 &&
      (millis() - lastCharTime) >= CMD_TIMEOUT_MS) {
    flushCommand();
  }
}

// prueba de compilacion 2
