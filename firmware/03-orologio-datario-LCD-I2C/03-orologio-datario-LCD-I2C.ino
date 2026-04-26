/*
   ------------------------------------------------------------
   Orologio Multifuzione con Datario - V2
   Arduino Nano + RTC DS3231 + LCD 16x2 I2C
   ------------------------------------------------------------

   Breve descrizione:
   Orologio digitale multifunzione con visualizzazione di ora,
   data e giorno della settimana, dotato di timer countdown,
   cronometro, sveglia giornaliera e menu di navigazione tramite
   encoder rotativo. Il dispositivo utilizza un modulo RTC
   DS3231 per mantenere l'orario con elevata precisione e un
   buzzer attivo per le segnalazioni acustiche.

   Video del progetto:
   https://youtu.be/ws_6DP3VXj8

   Autore:
   Giuseppe Romano

   Canale YouTube:
   Electronic & CNC Lab
   https://www.youtube.com/@Electronic.CNCLab

   ------------------------------------------------------------
   Progetto realizzato a scopo didattico e divulgativo.
   ------------------------------------------------------------
*/

#include <Wire.h>
#include <RTClib.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

/*
  Orologio + Datario + Timer + Sveglia giornaliera
  RTC: DS3231
  LCD: 16x2 I2C
  Encoder: KY-040
  Buzzer: attivo LOW

  Collegamenti:
    Encoder CLK -> D4
    Encoder DT  -> D3
    Encoder SW  -> D2
    Encoder +   -> 5V
    Encoder GND -> GND

    LCD I2C / DS3231:
      SDA -> A4
      SCL -> A5
      VCC -> 5V
      GND -> GND

    Buzzer attivo LOW:
      IN  -> D6
      GND -> GND

    DS3231 INT/SQW -> D7
*/

#define PIN_ENC_CLK 4
#define PIN_ENC_DT 3
#define PIN_ENC_SW 2

#define BUZZER_PIN 6  // buzzer attivo LOW
#define ALARM_PIN 7   // DS3231 INT/SQW -> LOW quando scatta la sveglia

#define LCD_ADDR 0x27
#define LCD_COLS 16
#define LCD_ROWS 2


const unsigned long LONG_PRESS_MS = 1000;
const int ENCODER_DIRECTION = +1;

const unsigned long REFRESH_MS = 120;
const unsigned long BLINK_MS = 350;
const unsigned long PARAM_TIMEOUT_MS = 8000;

bool inInfoFw = false;

RTC_DS3231 rtc;
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

const char *giorniITA[7] = { "dom", "lun", "mar", "mer", "gio", "ven", "sab" };

byte bellChar[8] = {
  B00100,
  B01110,
  B01110,
  B01110,
  B11111,
  B00100,
  B00000,
  B00100
};



// ------------------- STATI -------------------
enum Mode {
  RUN,
  MENU,
  SET_TIME,
  SET_DATE,
  SET_TIMER,
  TIMER_RUNNING,
  PARAM_MENU,
  ALARM_MENU,
  SET_ALARM,
  STOPWATCH_RUNNING
};

Mode mode = RUN;

// ------------------- PARAMETRI RUN -------------------
enum TimeAlign : uint8_t { ALIGN_LEFT = 0,
                           ALIGN_CENTER = 1,
                           ALIGN_RIGHT = 2 };
TimeAlign timeAlign = ALIGN_CENTER;

bool backlightEnabled = true;

// ------------------- SVEGLIA -------------------
bool alarmEnabled = false;
uint8_t alarmHour = 7;
uint8_t alarmMin = 30;

// ------------------- TIMER -------------------
uint8_t timerSetH = 0;
uint8_t timerSetM = 0;
uint8_t timerSetS = 0;

bool timerActive = false;
uint32_t timerDurationSec = 0;
unsigned long timerEndMillis = 0;
bool timerBeepDone = false;
unsigned long timerBeepShownAt = 0;
bool timerPaused = false;
uint32_t timerRemainingSec = 0;

// ------------------- CRONOMETRO -------------------
bool stopwatchRunning = false;
bool stopwatchPaused = false;
unsigned long stopwatchStartMillis = 0;
unsigned long stopwatchAccumulatedMs = 0;

// ------------------- MENU INDEX -------------------
int menuIndex = 0;       // MENU principale
int paramIndex = 0;      // PARAM_MENU
int alarmMenuIndex = 0;  // ALARM_MENU

// ------------------- EDITING -------------------
int setHour, setMin, setSec;
int setDay, setMonth, setYear;
int fieldIndex = 0;  // TIME/DATE/TIMER/ALARM

uint8_t setAlarmHour, setAlarmMin;

// ------------------- BLINK -------------------
unsigned long lastBlinkToggle = 0;
bool blinkOn = true;

// ------------------- REFRESH -------------------
unsigned long lastRefresh = 0;
unsigned long paramLastAction = 0;

// ------------------- BUTTON EVENTS -------------------
bool btnPressed = false;
bool btnPrevPressed = false;
unsigned long btnPressStart = 0;
bool longPressFired = false;
bool evtShortClick = false;
bool evtLongPress = false;

// ------------------- ENCODER PCINT -------------------
volatile int16_t encDelta = 0;
volatile uint8_t encPrevPins = 0;

// ------------------- BUZZER NON BLOCCANTE -------------------
bool buzzerActive = false;
unsigned long buzzerOffAt = 0;
bool alarmBeepSequenceActive = false;
uint8_t alarmBeepCount = 0;
bool alarmBeepPhaseOn = false;
unsigned long alarmBeepNextMs = 0;

// ------------------- EEPROM -------------------
const int EEPROM_ADDR_MAGIC = 0;
const int EEPROM_ADDR_BACKLIGHT = 1;
const int EEPROM_ADDR_ALIGN = 2;
const int EEPROM_ADDR_ALARM_EN = 3;
const int EEPROM_ADDR_ALARM_HH = 4;
const int EEPROM_ADDR_ALARM_MM = 5;
const uint8_t EEPROM_MAGIC = 0xA5;

// ------------------- UTIL -------------------
String twoDigits(int v) {
  if (v < 10) return "0" + String(v);
  return String(v);
}

void clearLine(uint8_t row) {
  lcd.setCursor(0, row);
  for (int i = 0; i < LCD_COLS; i++) lcd.print(" ");
}

String pad16(const String &s) {
  if ((int)s.length() >= LCD_COLS) return s.substring(0, LCD_COLS);
  String out = s;
  while ((int)out.length() < LCD_COLS) out += ' ';
  return out;
}

String lastLine0 = "";
String lastLine1 = "";

void lcdPrintLineBuffered(uint8_t row, const String &s) {
  String p = pad16(s);

  if (row == 0) {
    if (p == lastLine0) return;
    lastLine0 = p;
  } else {
    if (p == lastLine1) return;
    lastLine1 = p;
  }

  lcd.setCursor(0, row);
  lcd.print(p);
}

int wrapInt(int value, int minV, int maxV) {
  if (value < minV) return maxV;
  if (value > maxV) return minV;
  return value;
}

bool isLeap(int y) {
  return ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0));
}

int daysInMonth(int y, int m) {
  if (m == 2) return isLeap(y) ? 29 : 28;
  if (m == 4 || m == 6 || m == 9 || m == 11) return 30;
  return 31;
}

void blankRange(String &s, int start, int len) {
  for (int i = 0; i < len; i++) {
    int idx = start + i;
    if (idx >= 0 && idx < (int)s.length()) s.setCharAt(idx, ' ');
  }
}

String onOff(bool v) {
  return v ? "ON" : "OFF";
}

String alignToStr(TimeAlign a) {
  switch (a) {
    case ALIGN_LEFT: return "SX";
    case ALIGN_CENTER: return "CTR";
    case ALIGN_RIGHT: return "DX";
    default: return "SX";
  }
}

void updateBlink() {
  if (millis() - lastBlinkToggle >= BLINK_MS) {
    lastBlinkToggle = millis();
    blinkOn = !blinkOn;
  }
}

void resetBlink() {
  blinkOn = true;
  lastBlinkToggle = millis();
}

// ------------------- EEPROM -------------------
void loadSettingsFromEEPROM() {
  uint8_t magic = EEPROM.read(EEPROM_ADDR_MAGIC);

  if (magic != EEPROM_MAGIC) {
    backlightEnabled = true;
    timeAlign = ALIGN_CENTER;
    alarmEnabled = false;
    alarmHour = 7;
    alarmMin = 30;

    EEPROM.update(EEPROM_ADDR_MAGIC, EEPROM_MAGIC);
    EEPROM.update(EEPROM_ADDR_BACKLIGHT, (uint8_t)backlightEnabled);
    EEPROM.update(EEPROM_ADDR_ALIGN, (uint8_t)timeAlign);
    EEPROM.update(EEPROM_ADDR_ALARM_EN, (uint8_t)alarmEnabled);
    EEPROM.update(EEPROM_ADDR_ALARM_HH, alarmHour);
    EEPROM.update(EEPROM_ADDR_ALARM_MM, alarmMin);
    return;
  }

  backlightEnabled = (EEPROM.read(EEPROM_ADDR_BACKLIGHT) != 0);

  uint8_t a = EEPROM.read(EEPROM_ADDR_ALIGN);
  if (a > ALIGN_RIGHT) a = ALIGN_LEFT;
  timeAlign = (TimeAlign)a;

  alarmEnabled = (EEPROM.read(EEPROM_ADDR_ALARM_EN) != 0);
  alarmHour = EEPROM.read(EEPROM_ADDR_ALARM_HH);
  alarmMin = EEPROM.read(EEPROM_ADDR_ALARM_MM);

  if (alarmHour > 23) alarmHour = 7;
  if (alarmMin > 59) alarmMin = 30;
}

void saveSettingsToEEPROM() {
  EEPROM.update(EEPROM_ADDR_MAGIC, EEPROM_MAGIC);
  EEPROM.update(EEPROM_ADDR_BACKLIGHT, (uint8_t)backlightEnabled);
  EEPROM.update(EEPROM_ADDR_ALIGN, (uint8_t)timeAlign);
  EEPROM.update(EEPROM_ADDR_ALARM_EN, (uint8_t)alarmEnabled);
  EEPROM.update(EEPROM_ADDR_ALARM_HH, alarmHour);
  EEPROM.update(EEPROM_ADDR_ALARM_MM, alarmMin);
}

// ------------------- BACKLIGHT -------------------
void applyBacklight() {
  if (backlightEnabled) lcd.backlight();
  else lcd.noBacklight();
}

void setBacklightRaw(bool on) {
  if (on) lcd.backlight();
  else lcd.noBacklight();
}
// ------------------- BUZZER -------------------
void alarmBeepSequenceUpdate() {
  if (!alarmBeepSequenceActive) return;

  if ((long)(millis() - alarmBeepNextMs) < 0) return;

  if (!alarmBeepPhaseOn) {
    // Inizio beep
    digitalWrite(BUZZER_PIN, LOW);  // buzzer attivo LOW

    // Lampeggio display in sincronia col beep:
    // inverte lo stato normale della retroilluminazione
    setBacklightRaw(!backlightEnabled);

    alarmBeepPhaseOn = true;
    alarmBeepNextMs = millis() + 300;  // beep ON per 300 ms
  } else {
    // Fine beep
    digitalWrite(BUZZER_PIN, HIGH);  // buzzer spento

    // Ripristina lo stato normale impostato dall'utente
    applyBacklight();

    alarmBeepPhaseOn = false;
    alarmBeepCount++;

    if (alarmBeepCount >= 5) {
      alarmBeepSequenceActive = false;

      // Sicurezza finale: ripristina la retroilluminazione corretta
      applyBacklight();
    } else {
      alarmBeepNextMs = millis() + 300;  // pausa 300 ms
    }
  }
}

void buzzerStart(unsigned long durationMs) {
  digitalWrite(BUZZER_PIN, LOW);  // attivo LOW
  buzzerActive = true;
  buzzerOffAt = millis() + durationMs;
}

void buzzerUpdate() {
  if (buzzerActive && (long)(millis() - buzzerOffAt) >= 0) {
    digitalWrite(BUZZER_PIN, HIGH);  // spento
    buzzerActive = false;
  }
}

void beepTimerEnd() {
  buzzerStart(500);
}

void beepAlarmDaily() {
  alarmBeepSequenceActive = true;
  alarmBeepCount = 0;
  alarmBeepPhaseOn = false;
  alarmBeepNextMs = millis();
}

// ------------------- DS3231 ALARM -------------------
void disableDailyAlarmDS3231() {
  rtc.disableAlarm(1);
  rtc.clearAlarm(1);
}

void programDailyAlarmDS3231() {
  // Spegni square wave e pulisci eventuali flag
  rtc.writeSqwPinMode(DS3231_OFF);
  rtc.clearAlarm(1);
  rtc.clearAlarm(2);
  rtc.disableAlarm(1);
  rtc.disableAlarm(2);

  if (!alarmEnabled) return;

  // Alarm1 giornaliera su HH:MM:SS = alarmHour:alarmMin:00
  // Con DS3231_A1_Hour la data viene ignorata, l'orario viene confrontato ogni giorno
  rtc.setAlarm1(
    DateTime(2000, 1, 1, alarmHour, alarmMin, 0),
    DS3231_A1_Hour);

  rtc.clearAlarm(1);
}

void handleAlarmInterruptIfAny() {
  if (!alarmEnabled) return;

  if (digitalRead(ALARM_PIN) == LOW) {
    beepAlarmDaily();
    rtc.clearAlarm(1);
  }
}

// ------------------- DISPLAY -------------------
void showRun() {
  DateTime now = rtc.now();

  String timeStr = twoDigits(now.hour()) + ":" + twoDigits(now.minute()) + ":" + twoDigits(now.second());
  int padLeft = 0;
  if (timeAlign == ALIGN_CENTER) padLeft = (LCD_COLS - 8) / 2;
  else if (timeAlign == ALIGN_RIGHT) padLeft = (LCD_COLS - 8);

  String r1 = "";
  for (int i = 0; i < padLeft; i++) r1 += " ";
  r1 += timeStr;

  int dow = now.dayOfTheWeek();
  String r2 = String(giorniITA[dow]) + " " + twoDigits(now.day()) + "-" + twoDigits(now.month()) + "-" + String(now.year());

  r2 = pad16(r2);

  lcdPrintLineBuffered(0, r1);

  // Riga 2 scritta direttamente
  lcd.setCursor(0, 1);
  lcd.print(r2.substring(0, 15));  // primi 15 caratteri

  if (alarmEnabled) {
    lcd.write(byte(0));  // campanella
  } else {
    lcd.print(" ");
  }

  lastLine1 = r2;  // opzionale, per non perdere coerenza col buffering
}

void showMenu() {
  String l0 = "MENU";
  String l1;

  switch (menuIndex) {
    case 0: l1 = ">ORA"; break;
    case 1: l1 = ">DATA"; break;
    case 2: l1 = ">TIMER"; break;
    case 3: l1 = ">SVEGLIA"; break;
    case 4: l1 = ">CRONOMETRO"; break;
    default: l1 = ">ORA"; break;
  }

  lcdPrintLineBuffered(0, l0);
  lcdPrintLineBuffered(1, l1);
}

void showParamMenu() {
  String l0 = "PARAM/INFO";
  String l1;

  switch (paramIndex) {

    case 0:
      l1 = String(">BACKLIGHT:") + onOff(backlightEnabled);
      break;

    case 1:
      l1 = String(">ORARIO:") + alignToStr(timeAlign);
      break;

    case 2:
      l1 = ">TEST BUZZER";
      break;

    case 3:
      l1 = ">INFO FW";
      break;

    case 4:
      l1 = ">RESET SETTINGS";
      break;

    default:
      l1 = ">-";
      break;
  }

  lcdPrintLineBuffered(0, l0);
  lcdPrintLineBuffered(1, l1);
}

void showAlarmMenu() {
  String l0 = "SVEGLIA";
  String l1;

  switch (alarmMenuIndex) {
    case 0:
      l1 = String(">ATTIVA:") + onOff(alarmEnabled);
      break;

    case 1:
      l1 = String(">ORA:") + twoDigits(alarmHour) + ":" + twoDigits(alarmMin);
      break;

    default:
      l1 = ">ATTIVA";
      break;
  }

  lcdPrintLineBuffered(0, l0);
  lcdPrintLineBuffered(1, l1);
}

void showSetTime() {
  updateBlink();

  String r1 = "SET ORA";
  String r2 = twoDigits(setHour) + ":" + twoDigits(setMin) + ":" + twoDigits(setSec);

  if (!blinkOn) {
    if (fieldIndex == 0) blankRange(r2, 0, 2);
    if (fieldIndex == 1) blankRange(r2, 3, 2);
    if (fieldIndex == 2) blankRange(r2, 6, 2);
  }

  lcdPrintLineBuffered(0, r1);
  lcdPrintLineBuffered(1, r2);
}

void showSetDate() {
  updateBlink();

  String r1 = "SET DATA";
  String r2 = twoDigits(setDay) + "-" + twoDigits(setMonth) + "-" + String(setYear);

  if (!blinkOn) {
    if (fieldIndex == 0) blankRange(r2, 0, 2);
    if (fieldIndex == 1) blankRange(r2, 3, 2);
    if (fieldIndex == 2) blankRange(r2, 6, 4);
  }

  lcdPrintLineBuffered(0, r1);
  lcdPrintLineBuffered(1, r2);
}

void showSetTimer() {
  updateBlink();

  String l0 = "SET TIMER";
  String hh = twoDigits(timerSetH);
  String mm = twoDigits(timerSetM);
  String ss = twoDigits(timerSetS);

  if (!blinkOn) {
    if (fieldIndex == 0) hh = "  ";
    if (fieldIndex == 1) mm = "  ";
    if (fieldIndex == 2) ss = "  ";
  }

  String l1 = hh + ":" + mm + ":" + ss;

  lcdPrintLineBuffered(0, l0);
  lcdPrintLineBuffered(1, l1);
}

void showTimerRunning(uint32_t remainingSec) {

  // Riga 1: orologio normale
  DateTime now = rtc.now();

  String timeStr = twoDigits(now.hour()) + ":" + twoDigits(now.minute()) + ":" + twoDigits(now.second());

  int padLeft = 0;
  if (timeAlign == ALIGN_CENTER) padLeft = (LCD_COLS - 8) / 2;
  else if (timeAlign == ALIGN_RIGHT) padLeft = (LCD_COLS - 8);

  String r1 = "";
  for (int i = 0; i < padLeft; i++) r1 += " ";
  r1 += timeStr;

  // Riga 2: timer
  String prefix = timerPaused ? "PAUSA " : "TIMER ";

  uint32_t h = remainingSec / 3600UL;
  uint32_t m = (remainingSec % 3600UL) / 60UL;
  uint32_t s = remainingSec % 60UL;

  String r2 = prefix + twoDigits((int)h) + ":" + twoDigits((int)m) + ":" + twoDigits((int)s);

  lcdPrintLineBuffered(0, r1);
  lcdPrintLineBuffered(1, r2);
}

void showTimerExpired() {
  lcdPrintLineBuffered(0, "TIMER SCADUTO");
  lcdPrintLineBuffered(1, "                ");
}

void showSetAlarm() {
  updateBlink();

  String l0 = "SET SVEGLIA";
  String hh = twoDigits(setAlarmHour);
  String mm = twoDigits(setAlarmMin);

  if (!blinkOn) {
    if (fieldIndex == 0) hh = "  ";
    if (fieldIndex == 1) mm = "  ";
  }

  String l1 = hh + ":" + mm;

  lcdPrintLineBuffered(0, l0);
  lcdPrintLineBuffered(1, l1);
}

// ------------------- FLOW -------------------
void enterMenu() {
  mode = MENU;
  menuIndex = 0;
  resetBlink();
}

void enterParamMenu() {
  mode = PARAM_MENU;
  paramIndex = 0;
  paramLastAction = millis();
}

void exitParamMenu() {
  inInfoFw = false;
  mode = RUN;
}

void enterSetTime() {
  mode = SET_TIME;
  fieldIndex = 0;

  DateTime now = rtc.now();
  setHour = now.hour();
  setMin = now.minute();
  setSec = now.second();

  resetBlink();
}

void enterSetDate() {
  mode = SET_DATE;
  fieldIndex = 0;

  DateTime now = rtc.now();
  setDay = now.day();
  setMonth = now.month();
  setYear = now.year();

  resetBlink();
}

void enterSetTimer() {
  mode = SET_TIMER;
  fieldIndex = 0;
  timerSetH = 0;
  timerSetM = 0;
  timerSetS = 0;
  resetBlink();
}

void enterAlarmMenu() {
  mode = ALARM_MENU;
  alarmMenuIndex = 0;
}

void enterSetAlarm() {
  mode = SET_ALARM;
  fieldIndex = 0;
  setAlarmHour = alarmHour;
  setAlarmMin = alarmMin;
  resetBlink();
}

// ------------------- RTC SAVE -------------------
void saveTimeToRTC() {
  DateTime now = rtc.now();
  rtc.adjust(DateTime(now.year(), now.month(), now.day(), setHour, setMin, setSec));
}

void saveDateToRTC() {
  DateTime now = rtc.now();
  int dim = daysInMonth(setYear, setMonth);
  if (setDay > dim) setDay = dim;

  rtc.adjust(DateTime(setYear, setMonth, setDay, now.hour(), now.minute(), now.second()));
}

// ------------------- TIMER -------------------
void startTimerFromSet() {
  timerDurationSec = (uint32_t)timerSetH * 3600UL + (uint32_t)timerSetM * 60UL + (uint32_t)timerSetS;

  if (timerDurationSec == 0) return;

  timerRemainingSec = timerDurationSec;
  timerPaused = false;
  timerActive = true;
  timerBeepDone = false;
  timerEndMillis = millis() + timerRemainingSec * 1000UL;
  mode = TIMER_RUNNING;
}

void stopTimer() {
  timerActive = false;
  timerPaused = false;
  timerRemainingSec = 0;
  mode = RUN;
}

void pauseTimer() {
  if (!timerActive || timerPaused) return;

  unsigned long nowMs = millis();

  if ((long)(timerEndMillis - nowMs) > 0) {
    timerRemainingSec = (uint32_t)((timerEndMillis - nowMs) / 1000UL);
    if (timerRemainingSec == 0) timerRemainingSec = 1;
  } else {
    timerRemainingSec = 0;
  }

  timerPaused = true;
}

void resumeTimer() {
  if (!timerActive || !timerPaused) return;

  timerEndMillis = millis() + timerRemainingSec * 1000UL;
  timerPaused = false;
}

// ------------------- STOPWATCH -------------------
void enterStopwatch() {
  mode = STOPWATCH_RUNNING;
  stopwatchRunning = false;
  stopwatchPaused = false;
  stopwatchAccumulatedMs = 0;
  stopwatchStartMillis = millis();
}

void startStopwatch() {
  if (!stopwatchPaused && !stopwatchRunning) {
    stopwatchStartMillis = millis();
    stopwatchRunning = true;
    stopwatchPaused = false;
    return;
  }

  if (stopwatchPaused) {
    stopwatchStartMillis = millis();
    stopwatchRunning = true;
    stopwatchPaused = false;
  }
}

void pauseStopwatch() {
  if (!stopwatchRunning) return;

  stopwatchAccumulatedMs += millis() - stopwatchStartMillis;
  stopwatchRunning = false;
  stopwatchPaused = true;
}

void resetStopwatch() {
  stopwatchRunning = false;
  stopwatchPaused = false;
  stopwatchAccumulatedMs = 0;
}

unsigned long getStopwatchElapsedMs() {
  if (stopwatchRunning) {
    return stopwatchAccumulatedMs + (millis() - stopwatchStartMillis);
  }
  return stopwatchAccumulatedMs;
}

void showStopwatch() {
  // Riga 1: orologio corrente
  DateTime now = rtc.now();

  String timeStr = twoDigits(now.hour()) + ":" + twoDigits(now.minute()) + ":" + twoDigits(now.second());

  int padLeft = 0;
  if (timeAlign == ALIGN_CENTER) padLeft = (LCD_COLS - 8) / 2;
  else if (timeAlign == ALIGN_RIGHT) padLeft = (LCD_COLS - 8);

  String r1 = "";
  for (int i = 0; i < padLeft; i++) r1 += " ";
  r1 += timeStr;

  // Riga 2: cronometro MM:SS:CC
  unsigned long elapsed = getStopwatchElapsedMs();

  unsigned long totalCs = elapsed / 10;  // centesimi
  unsigned long mm = (totalCs / 6000UL) % 100UL;
  unsigned long ss = (totalCs / 100UL) % 60UL;
  unsigned long cc = totalCs % 100UL;

  String prefix = stopwatchRunning ? "CRONO " : "PAUSA ";
  String r2 = prefix + twoDigits((int)mm) + ":" + twoDigits((int)ss) + ":" + twoDigits((int)cc);

  lcdPrintLineBuffered(0, r1);
  lcdPrintLineBuffered(1, r2);
}

void showInfoFw() {
  lcdPrintLineBuffered(0, "FW v1.0");
  lcdPrintLineBuffered(1, "Electronic&CNC");
}

// ------------------- ENCODER PCINT -------------------
void setupEncoderPCINT() {
  pinMode(PIN_ENC_CLK, INPUT_PULLUP);
  pinMode(PIN_ENC_DT, INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);

  noInterrupts();

  encPrevPins = PIND & ((1 << PD4) | (1 << PD3));

  PCICR |= (1 << PCIE2);
  PCMSK2 |= (1 << PCINT20) | (1 << PCINT19);

  interrupts();
}

ISR(PCINT2_vect) {
  uint8_t pins = PIND & ((1 << PD4) | (1 << PD3));
  uint8_t changed = pins ^ encPrevPins;

  if (changed & (1 << PD4)) {
    bool clk = (pins & (1 << PD4)) != 0;
    bool dt = (pins & (1 << PD3)) != 0;

    if (dt != clk) encDelta++;
    else encDelta--;
  }

  encPrevPins = pins;
}

int16_t readEncDeltaAtomic() {
  static int16_t carry = 0;
  int16_t d;

  noInterrupts();
  d = encDelta;
  encDelta = 0;
  interrupts();

  carry += d;

  int16_t step = carry / 2;
  carry -= step * 2;

  return step;
}

// ------------------- BUTTON EVENTS -------------------
void updateButtonEvents() {
  evtShortClick = false;
  evtLongPress = false;

  btnPressed = (digitalRead(PIN_ENC_SW) == LOW);

  if (btnPressed && !btnPrevPressed) {
    btnPressStart = millis();
    longPressFired = false;
  }

  if (btnPressed && !longPressFired) {
    if (millis() - btnPressStart >= LONG_PRESS_MS) {
      longPressFired = true;
      evtLongPress = true;
    }
  }

  if (!btnPressed && btnPrevPressed) {
    unsigned long dur = millis() - btnPressStart;
    if (dur < LONG_PRESS_MS && !longPressFired) evtShortClick = true;
  }

  btnPrevPressed = btnPressed;
}

// ------------------- SETUP -------------------
void setup() {
  pinMode(PIN_ENC_SW, INPUT_PULLUP);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH);  // buzzer attivo LOW -> HIGH = spento

  pinMode(ALARM_PIN, INPUT_PULLUP);

  Wire.begin();

  loadSettingsFromEEPROM();

  lcd.init();
  applyBacklight();
  lcd.createChar(0, bellChar);

  if (!rtc.begin()) {
    clearLine(0);
    lcd.setCursor(0, 0);
    lcd.print("RTC NON TROVATO");
    clearLine(1);
    lcd.setCursor(0, 1);
    lcd.print("Controlla I2C");
    while (1) {}
  }

  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  programDailyAlarmDS3231();

  setupEncoderPCINT();

  clearLine(0);
  lcd.setCursor(0, 0);
  lcd.print("Orologio RTC");
  clearLine(1);
  lcd.setCursor(0, 1);
  lcd.print("Pronto...");
  delay(700);
}

// ------------------- LOOP -------------------
void loop() {
  updateButtonEvents();
  buzzerUpdate();
  alarmBeepSequenceUpdate();

  int step = (int)(ENCODER_DIRECTION * readEncDeltaAtomic());

  // Controllo sveglia hardware sempre attivo
  handleAlarmInterruptIfAny();

  switch (mode) {

    case RUN:
      if (evtLongPress) {
        enterMenu();
      }

      if (step != 0) {
        enterParamMenu();
        break;
      }

      if (millis() - lastRefresh >= REFRESH_MS) {
        lastRefresh = millis();
        showRun();
      }
      break;

    case MENU:
      if (step != 0) {
        int s = (step > 0) ? 1 : -1;
        menuIndex += s;
        if (menuIndex < 0) menuIndex = 4;
        if (menuIndex > 4) menuIndex = 0;
      }

      if (evtShortClick) {
        if (menuIndex == 0) enterSetTime();
        else if (menuIndex == 1) enterSetDate();
        else if (menuIndex == 2) enterSetTimer();
        else if (menuIndex == 3) enterAlarmMenu();
        else if (menuIndex == 4) enterStopwatch();
      }

      if (evtLongPress) mode = RUN;

      if (millis() - lastRefresh >= REFRESH_MS) {
        lastRefresh = millis();
        showMenu();
      }
      break;

    case SET_TIME:
      if (step != 0) {
        if (fieldIndex == 0) setHour = wrapInt(setHour + step, 0, 23);
        if (fieldIndex == 1) setMin = wrapInt(setMin + step, 0, 59);
        if (fieldIndex == 2) setSec = wrapInt(setSec + step, 0, 59);
      }

      if (evtShortClick) {
        if (fieldIndex < 2) {
          fieldIndex++;
          resetBlink();
        } else {
          saveTimeToRTC();
          mode = RUN;
        }
      }

      if (evtLongPress) mode = RUN;

      if (millis() - lastRefresh >= REFRESH_MS) {
        lastRefresh = millis();
        showSetTime();
      }
      break;

    case SET_DATE:
      if (step != 0) {
        if (fieldIndex == 0) {
          int dim = daysInMonth(setYear, setMonth);
          setDay = wrapInt(setDay + step, 1, dim);
        }

        if (fieldIndex == 1) {
          setMonth = wrapInt(setMonth + step, 1, 12);
          int dim = daysInMonth(setYear, setMonth);
          if (setDay > dim) setDay = dim;
        }

        if (fieldIndex == 2) {
          setYear += step;
          if (setYear < 2000) setYear = 2099;
          if (setYear > 2099) setYear = 2000;

          int dim = daysInMonth(setYear, setMonth);
          if (setDay > dim) setDay = dim;
        }
      }

      if (evtShortClick) {
        if (fieldIndex < 2) {
          fieldIndex++;
          resetBlink();
        } else {
          saveDateToRTC();
          mode = RUN;
        }
      }

      if (evtLongPress) mode = RUN;

      if (millis() - lastRefresh >= REFRESH_MS) {
        lastRefresh = millis();
        showSetDate();
      }
      break;

    case SET_TIMER:
      if (step != 0) {
        if (fieldIndex == 0) timerSetH = (uint8_t)wrapInt((int)timerSetH + step, 0, 99);
        if (fieldIndex == 1) timerSetM = (uint8_t)wrapInt((int)timerSetM + step, 0, 59);
        if (fieldIndex == 2) timerSetS = (uint8_t)wrapInt((int)timerSetS + step, 0, 59);
      }

      if (evtShortClick) {
        if (fieldIndex < 2) {
          fieldIndex++;
          resetBlink();
        } else {
          startTimerFromSet();
          resetBlink();
        }
      }

      if (evtLongPress) mode = RUN;

      if (millis() - lastRefresh >= REFRESH_MS) {
        lastRefresh = millis();
        showSetTimer();
      }
      break;

    case TIMER_RUNNING:
      {
        if (evtLongPress) {
          stopTimer();
          break;
        }

        if (evtShortClick) {
          if (timerPaused) resumeTimer();
          else pauseTimer();
        }

        unsigned long nowMs = millis();

        if (timerActive && !timerPaused) {
          if ((long)(nowMs - timerEndMillis) >= 0) {
            timerActive = false;
            timerRemainingSec = 0;

            if (!timerBeepDone) {
              beepTimerEnd();
              timerBeepDone = true;
              timerBeepShownAt = nowMs;
            }
          }
        }

        if (timerBeepDone) {
          showTimerExpired();
          if (nowMs - timerBeepShownAt > 2000UL) {
            mode = RUN;
          }
        } else {
          uint32_t remainingSec = 0;

          if (timerPaused) {
            remainingSec = timerRemainingSec;
          } else if (timerActive) {
            unsigned long msLeft = (unsigned long)(timerEndMillis - nowMs);
            remainingSec = (uint32_t)(msLeft / 1000UL);
          }

          if (millis() - lastRefresh >= REFRESH_MS) {
            lastRefresh = millis();
            showTimerRunning(remainingSec);
          }
        }
        break;
      }

case PARAM_MENU:
      if (millis() - paramLastAction > PARAM_TIMEOUT_MS) {
        exitParamMenu();
        break;
      }

      if (inInfoFw) {
        if (evtShortClick || evtLongPress) {
          inInfoFw = false;
          paramLastAction = millis();
          lastRefresh = 0;
        }

        if (millis() - lastRefresh >= REFRESH_MS) {
          lastRefresh = millis();
          lcdPrintLineBuffered(0, "FW v1.0");
          lcdPrintLineBuffered(1, "Electronic&CNC");
        }
        break;
      }

      if (step != 0) {
        paramLastAction = millis();
        int s = (step > 0) ? 1 : -1;
        paramIndex += s;

        const int maxIndex = 4;
        if (paramIndex < 0) paramIndex = maxIndex;
        if (paramIndex > maxIndex) paramIndex = 0;
      }

      if (evtShortClick) {
        paramLastAction = millis();

        if (paramIndex == 0) {
          backlightEnabled = !backlightEnabled;
          applyBacklight();
          saveSettingsToEEPROM();
        } else if (paramIndex == 1) {
          timeAlign = (TimeAlign)wrapInt((int)timeAlign + 1, (int)ALIGN_LEFT, (int)ALIGN_RIGHT);
          saveSettingsToEEPROM();
        } else if (paramIndex == 2) {
          buzzerStart(1000);  // TEST BUZZER: beep di 1 secondo
        } else if (paramIndex == 3) {
          inInfoFw = true;
          lastRefresh = 0;
        } else if (paramIndex == 4) {
          // RESET SETTINGS
          // anche questo per ora meglio lasciarlo vuoto,
          // poi aggiungiamo una conferma per non fare reset accidentali
        }
      }

      if (evtLongPress) {
        exitParamMenu();
      }

      if (millis() - lastRefresh >= REFRESH_MS) {
        lastRefresh = millis();
        showParamMenu();
      }
      break;

    case ALARM_MENU:
      if (step != 0) {
        int s = (step > 0) ? 1 : -1;
        alarmMenuIndex += s;
        if (alarmMenuIndex < 0) alarmMenuIndex = 1;
        if (alarmMenuIndex > 1) alarmMenuIndex = 0;
      }

      if (evtShortClick) {
        if (alarmMenuIndex == 0) {
          alarmEnabled = !alarmEnabled;
          saveSettingsToEEPROM();
          programDailyAlarmDS3231();
        } else if (alarmMenuIndex == 1) {
          enterSetAlarm();
        }
      }

      if (evtLongPress) mode = RUN;

      if (millis() - lastRefresh >= REFRESH_MS) {
        lastRefresh = millis();
        showAlarmMenu();
      }
      break;

    case SET_ALARM:
      if (step != 0) {
        if (fieldIndex == 0) setAlarmHour = (uint8_t)wrapInt((int)setAlarmHour + step, 0, 23);
        if (fieldIndex == 1) setAlarmMin = (uint8_t)wrapInt((int)setAlarmMin + step, 0, 59);
      }

      if (evtShortClick) {
        if (fieldIndex == 0) {
          fieldIndex = 1;
          resetBlink();
        } else {
          alarmHour = setAlarmHour;
          alarmMin = setAlarmMin;
          saveSettingsToEEPROM();
          programDailyAlarmDS3231();
          mode = ALARM_MENU;
        }
      }

      if (evtLongPress) mode = ALARM_MENU;

      if (millis() - lastRefresh >= REFRESH_MS) {
        lastRefresh = millis();
        showSetAlarm();
      }
      break;

    case STOPWATCH_RUNNING:
      if (evtShortClick) {
        if (stopwatchRunning) pauseStopwatch();
        else startStopwatch();
      }

      if (evtLongPress) {
        resetStopwatch();
        mode = RUN;
      }

      if (millis() - lastRefresh >= REFRESH_MS) {
        lastRefresh = millis();
        showStopwatch();
      }
      break;
  }
}