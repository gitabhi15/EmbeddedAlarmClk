#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>
#include <RTClib.h>
#include <DS3232RTC.h>
#include <Keypad.h>

#define ROWS 4
#define COLS 4

// Keypad Pin Init
byte rowPins[ROWS] = { 36, 38, 40, 42 };
byte colPins[COLS] = { 44, 46, 48, 50 };

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
  ALARM_SETUP,
  ALARM_FINISHED,
  SNOOZE,
  SNOOZE_SETUP,
  SNOOZE_FINISHED,
  ERROR
};

// Pin declarations
const int buzzPin = 11;
const int sqwPin = 2;
const int LCD_COLS = 16;
const int LCD_ROWS = 2;

/* Flags */

volatile bool alarmTriggered = false;  //set for ISR attachment

// Alarm Flags:
// bool alarmMode = false;
bool settingMinutes = false;
bool settingHours = false;
bool showBootUp = false;
bool alarmFin = false;

// Snooze Flags:
bool snoozeMode = false;
bool snoozeFin = false;

// Reset Flags:
// bool resetMode = false;
bool resetFin = false;


//State variables:
String mins = "";
String hrs = "";
// unsigned long prevClockUpdate = 0;
// const unsigned long clockUpdateInterval = 1000;
// unsigned long bootUpStartTime = 0;
// const unsigned long bootUpDuration = 4000;

void setup() {
  Serial.begin(9600);
  lcd.begin(LCD_COLS, LCD_ROWS);
  pinMode(buzzPin, OUTPUT);
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
}

void loop() {
  AlarmState currState = CLOCK_MODE;

  char key = keypad.getKey();
  if (key != NO_KEY) {
    if (key == 'A') {
      currState = ALARM_MODE;
    } else if (key == 'B') {
      currState = MANUAL_RESET;
    }
  }

  switch (currState) {

    case CLOCK_MODE:  // displays regular 24hr clock
      displayClock();
      currState = CLOCK_MODE;
      // if (resetMode) {
      //   currState = MANUAL_RESET;
      // }
      // if (alarmMode) {
      //   currState = ALARM_MODE;
      // }
      break;

    case MANUAL_RESET:  // reset time of clock
      reset_boot();
      currState = RE_SETUP;
      break;

    case RE_SETUP:
      syncClock();  // note- automatic for now, might introduce manual 'manual' reset later
      if (resetFin) {
        currState = CLOCK_MODE;
      }
      currState = ERROR;
      break;

    case ALARM_MODE:  // alarm mode gets prompted
      alarm_boot();
      currState = ALARM_SETUP;
      break;

    case ALARM_SETUP:  // alarm setup prompted
      alarmSetup();
      if (alarmFin) {
        currState = ALARM_FINISHED;
      }
      break;

    case ALARM_FINISHED:  // alarm rings and signals to either snooze or return to clock mode
      if (snoozeMode) {
        currState = SNOOZE;
      }
      currState = CLOCK_MODE;
      break;

    case SNOOZE:  // snooze mode gets prompted
      snooze_boot();
      currState = SNOOZE_SETUP;
      break;

    case SNOOZE_SETUP:  // snooze button mechanism
      snoozeSetup();
      if (snoozeFin) {
        currState = SNOOZE_FINISHED;
      }
      break;

    case SNOOZE_FINISHED:  // snooze period finished
      snoozeFinished();
      if (snoozeMode) {
        currState = SNOOZE;
      }
      currState = CLOCK_MODE;
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

void syncClock() {
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  resetFin = true;
}

void displayClock() {
  unsigned long prevClockUpdate = 0;
  const unsigned long clockUpdateInterval = 1000;
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

void alarmSetup() {
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

void alarm_boot() {
  unsigned long startTime = 0;
  unsigned long displayTime = 5000;
  lcd.setCursor(0, 0);
  lcd.print("C = Confirm");
  lcd.setCursor(1, 0);
  lcd.print("# = Cancel");

  while (millis() - startTime < displayTime) {
  }
  lcd.clear();
}

void snooze_boot() {
}

void snoozeSetup() {
}

void alarmFinished() {
}

void snoozeFinished() {
}
