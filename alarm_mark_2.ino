#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>
#include <RTClib.h>
#include <DS3232RTC.h>
#include <Keypad.h>

#define ROWS 4
#define COLS 4

// Keypad Pin Init
byte rowPins[ROWS] = {4, 5, 6, 7};
byte colPins[COLS] = {8, 9, 10, 11};

char keys[ROWS][COLS] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', '#', 'D' }
};

// Object declarations
hd44780_I2Cexp lcd;
RTC_DS3231 rtc;
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Alarm FSM states
enum AlarmState {
  CLOCK_MODE,
  MANUAL_RESET,
  RE_SETUP,
  ALARM_MODE,
  SET_MINUTES,
  SET_HOURS,
  SAVE_ALARM,
  ALARM_FINISHED,
  SNOOZE,
  SNOOZE_SETUP,
  SAVE_SNOOZE,
  SNOOZE_FINISHED,
  ERROR
};

// Pin declarations
const int buzzPin = 3;
const int sqwPin = 2;
const int pushButton = 12;
const int LCD_COLS = 16;
const int LCD_ROWS = 2;
char key;
int lastButtonState = HIGH;
AlarmState currState;

/* Flags */

volatile bool alarmTriggered = false;  // set for ISR attachment
bool buzzerState = LOW;
bool resetFin = false;

// State variables:
String mins = "";
String hrs = "";
String snooze_mins = "";
unsigned long prevClockUpdate = 0;
const unsigned long clockUpdateInterval = 1000;
unsigned long alarmBootStartTime = 0;
const unsigned long alarmBootDisplayTime = 5000;
unsigned long buttonHoldTime = 0;
const unsigned long buttonHoldLimit = 4000;
unsigned long buzzerRingTime = 0;
const unsigned long buzzerRingLimit = 10000;
unsigned long buzzerToggleTime = 0;
const unsigned long buzzerToggleLimit = 500;
unsigned long snoozeBootStartTime = 0;
const unsigned long snoozeBootDisplayTime = 3500;

void setup() {
  Serial.begin(9600);
  lcd.begin(LCD_COLS, LCD_ROWS);
  pinMode(buzzPin, OUTPUT);
  pinMode(pushButton, INPUT_PULLUP);
  pinMode(sqwPin, INPUT_PULLUP);

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1)
      ;
  }

  // Clear alarms and configure SQW pin for alarm interrupt mode
  rtc.clearAlarm(1);
  rtc.disableAlarm(2);
  rtc.writeSqwPinMode(DS3231_OFF);

  // Attach ISR for SQW pin (fires when alarm goes off, active LOW)
  attachInterrupt(digitalPinToInterrupt(sqwPin), alarmISR, FALLING);
  //syncClock();
  currState = CLOCK_MODE;
}

void loop() {
  key = keypad.getKey();  // keypad input

  if (key != NO_KEY) {  // routing from regular clock display to either alarm setup or manual reset
    if (key == 'A') {
      currState = ALARM_MODE;
      alarmBootStartTime = millis();
      lcd.setCursor(0, 0);
      lcd.print("C = Confirm");
      lcd.setCursor(0, 1);
      lcd.print("# = Cancel");
    } else if (key == 'B') {
      currState = MANUAL_RESET;
    }
  }

  if (alarmTriggered && currState != ALARM_FINISHED) {  //routing from regular clock display to alarm finishing state
    currState = ALARM_FINISHED;
    buzzerRingTime = millis();
  }

  switch (currState) {
    case CLOCK_MODE:  // displays regular 24hr clock
      displayClock();
      currState = CLOCK_MODE;
      break;

    case MANUAL_RESET:  // reset time of clock
      reset_boot();
      currState = RE_SETUP;
      break;

    case RE_SETUP:
      syncClock();  // note- automatic for now, might introduce manual 'manual' reset later
      if (resetFin) {
        currState = CLOCK_MODE;
      } else {
        currState = ERROR;
      }
      break;

    case ALARM_MODE:  // alarm mode gets prompted
      alarm_boot();
      break;

    case SET_MINUTES:  // alarm setup prompted
      setMinutes();
      break;

    case SET_HOURS:
      setHours();
      break;

    case SAVE_ALARM:
      saveAlarm();
      break;

    case ALARM_FINISHED:  // alarm rings and signals to either snooze or return to clock mode
      alarmFinished();
      break;

    case SNOOZE:  // snooze mode gets prompted
      snooze_boot();
      break;

    case SNOOZE_SETUP:  // snooze button mechanism
      snoozeSetup();
      break;

    case SAVE_SNOOZE:
      saveSnooze();
      break;

    case ERROR:
      lcd.setCursor(0, 0);
      lcd.print("Error occured. Please");
      lcd.setCursor(1, 0);
      lcd.print("try again.");
      currState = CLOCK_MODE;
      break;

    default:
      lcd.setCursor(0, 0);
      lcd.print("Error occured. Please");
      lcd.setCursor(1, 0);
      lcd.print("try again.");
      currState = CLOCK_MODE;
      break;
  }
}

void reset_boot() {
  lcd.clear();
  lcd.setCursor(16, 0);
  lcd.autoscroll();

  String message = "Please reset your clock        ";
  int charIndex = 0;

  unsigned long startTime = millis();
  unsigned long lastCharTime = 0;
  const int scrollSpeed = 250;

  while (millis() - startTime < 3000) {
    if (millis() - lastCharTime >= scrollSpeed) {
      lastCharTime = millis();
      lcd.print(message[charIndex]);
      charIndex++;

      if (charIndex >= message.length()) {
        charIndex = 0;
        lcd.clear();
        lcd.setCursor(16, 0);
      }
    }
  }
  lcd.noAutoscroll();
  lcd.clear();
}

void syncClock() {
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  resetFin = true;
}

void displayClock() {
  if (millis() - prevClockUpdate >= clockUpdateInterval) {
    prevClockUpdate = millis();
    DateTime now = rtc.now();
    lcd.setCursor(0, 0);
    lcd.print("Time: ");
    lcd.setCursor(2, 1);
    if (now.hour() < 10) lcd.print("0");
    lcd.print(now.hour());
    lcd.print(" : ");
    if (now.minute() < 10) lcd.print("0");
    lcd.print(now.minute());
    lcd.print(" : ");
    if (now.second() < 10) lcd.print("0");
    lcd.print(now.second());
  }
}

void setMinutes() {
  if (isDigit(key) && mins.length() < 2) {
    mins += key;
  }

  lcd.setCursor(0, 0);
  lcd.print("Minutes: ");
  lcd.setCursor(9, 0);
  lcd.print(mins + "  ");

  if (key == 'C') {
    lcd.clear();
    currState = SET_HOURS;
  }
  if (key == '#') {
    clearRow(0);
    mins = "";
    currState = SET_MINUTES;
  }
  if (key == 'D') {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Alarm cancelled");
    resetAlarmState();
    currState = CLOCK_MODE;
  }
}

void setHours() {
  if (isDigit(key) && hrs.length() < 2) {
    hrs += key;
  }

  lcd.setCursor(0, 0);
  lcd.print("Hours: ");
  lcd.setCursor(9, 0);
  lcd.print(hrs + "  ");

  if (key == 'C') {
    lcd.clear();
    currState = SAVE_ALARM;
  }
  if (key == '#') {
    clearRow(0);
    hrs = "";
    currState = SET_HOURS;
  }
  if (key == 'D') {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Alarm cancelled");
    resetAlarmState();
    currState = CLOCK_MODE;
  }
}

void saveAlarm() {
  int m = mins.toInt();
  int h = hrs.toInt();
  DateTime now = rtc.now();

  // Set Alarm1 for today at H:M:00
  rtc.setAlarm1(DateTime(now.year(), now.month(), now.day(), h, m, 0), DS3231_A1_Hour);

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Alarm Set to:");
  lcd.setCursor(4, 1);

  if (h < 10) lcd.print("0");
  lcd.print(h);

  lcd.print(" : ");

  if (m < 10) lcd.print("0");
  lcd.print(m);

  delay(2000);
  lcd.clear();
  resetAlarmState();

  currState = CLOCK_MODE;
}

void alarm_boot() {
  if (millis() - alarmBootStartTime >= alarmBootDisplayTime) {
    lcd.clear();
    currState = SET_MINUTES;
  }
}

void alarmFinished() {
  lcd.setCursor(0, 0);
  lcd.print("WAKE UP!!!");

  int currentButtonState = digitalRead(pushButton);

  if (lastButtonState == HIGH && currentButtonState == LOW) {
    buttonHoldTime = millis();
  }

  if (currentButtonState == LOW) {
    if (millis() - buttonHoldTime >= buttonHoldLimit) {
      digitalWrite(buzzPin, LOW);
      rtc.clearAlarm(1);
      alarmTriggered = false;
      currState = SNOOZE;
      snoozeBootStartTime = millis();

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Snooze mode");
      lcd.setCursor(0, 1);
      lcd.print("activated");
      return;
    }
  }

  lastButtonState = currentButtonState;

  if (millis() - buzzerRingTime >= buzzerRingLimit) {
    digitalWrite(buzzPin, LOW);
    rtc.clearAlarm(1);
    alarmTriggered = false;
    currState = CLOCK_MODE;
    return;
  }

  if (millis() - buzzerToggleTime >= buzzerToggleLimit) {
    buzzerToggleTime = millis();
    buzzerState = !buzzerState;
    digitalWrite(buzzPin, buzzerState);
  }
}

void resetAlarmState() {
  mins = "";
  hrs = "";
}

void snooze_boot() {
  unsigned long elapsedTime = millis() - snoozeBootStartTime;

  if (elapsedTime >= snoozeBootDisplayTime && elapsedTime < (snoozeBootDisplayTime * 2)) {
    lcd.setCursor(0, 0);
    lcd.print("Please press C  ");
    lcd.setCursor(0, 1);
    lcd.print("to setup snooze ");
  } else if (elapsedTime >= (snoozeBootDisplayTime * 2)) {
    lcd.clear();
    currState = SNOOZE_SETUP;
  }
}

void snoozeSetup() {
  if (isDigit(key) && snooze_mins.length() < 3) {
    snooze_mins += key;
  }

  lcd.setCursor(0, 0);
  lcd.print("Minutes: ");
  lcd.setCursor(9, 0);
  lcd.print(snooze_mins + "  ");

  if (key == 'C') {
    lcd.clear();
    currState = SAVE_SNOOZE;
  }
  if (key == '#') {
    clearRow(0);
    snooze_mins = "";
    currState = SNOOZE_SETUP;
  }
  if (key == 'D') {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Snooze cancelled");
    resetSnoozeState();
    currState = CLOCK_MODE;
  }
}

void saveSnooze() {
  int sm = snooze_mins.toInt();
  DateTime now = rtc.now();

  DateTime snoozeTime = now + TimeSpan(0, 0, sm, 0);
  rtc.setAlarm1(snoozeTime, DS3231_A1_Hour);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Snooze set to:");
  lcd.setCursor(4, 1);

  if (snoozeTime.hour() < 10) lcd.print("0");
  lcd.print(snoozeTime.hour());

  lcd.print(" : ");

  if (snoozeTime.minute() < 10) lcd.print("0");
  lcd.print(snoozeTime.minute());

  delay(2000);
  lcd.clear();
  resetSnoozeState();

  currState = CLOCK_MODE;
}

void resetSnoozeState() {
  snooze_mins = "";
}

void clearRow(int row) {
  lcd.setCursor(0, row);
  lcd.print("                    ");
}

void alarmISR() {
  alarmTriggered = true;
}
