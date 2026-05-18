#include <LiquidCrystal.h>
#include <Wire.h>
#include <RTClib.h>
#include <DS3232RTC.h>
#include <Keypad.h>

#define ROWS 4
#define COLS 4

//Keypad Pin Init
byte rowPins[ROWS] = { 36, 38, 40, 42 };
byte colPins[COLS] = { 44, 46, 48, 50 };

char keys[ROWS][COLS] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', '#', 'D' }
};

//Object declarations
LiquidCrystal lcd(22, 24, 26, 28, 30, 32);
RTC_DS3231 rtc;
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

//Alarm FSM states
enum AlarmState {
  CLOCK_MODE,
  MANUAL_RESET,
  RE_SETUP,
  ALARM_MODE,
  ALARM_SETUP,
  ALARM_FINISHED,
  SNOOZE,
  SNOOZE_SETUP,
  SNOOZE_FINISHED
  ERROR
};

//Pin declarations
const int buzzPin = 11;
const int sqwPin = 2;

/* Flags */

volatile bool alarmTriggered = false;  //set for ISR attachment

//Alarm Flags:
bool alarmMode = false;
bool settingMinutes = false;
bool settingHours = false;
bool showBootUp = false;
bool alarmFin = false;

//Snooze Flags:
bool snoozeMode = false;
bool snoozeFin = false;

//Reset Flags:
bool resetMode = false;
bool resetFin = false;


//State variables:
String mins = "";
String hrs = "";
unsigned long prevClockUpdate = 0;
const unsigned long clockUpdateInterval = 1000;
unsigned long bootUpStartTime = 0;
const unsigned long bootUpDuration = 4000;

void setup() {
  Serial.begin(9600);
  pinMode(buzzPin, OUTPUT);
  pinMode(sqwPin, INPUT_PULLUP);

  lcd.begin(16, 2);

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
  // AlarmState nextState = CLOCK_MODE;
  switch (currState) {

    case CLOCK_MODE:
      displayClock();
      currState = CLOCK_MODE;
      if(resetMode){
        currState = MANUAL_RESET;
      }
      if(alarmMode){
        currState = ALARM_MODE;
      }
      break;

    case MANUAL_RESET:
      reset_boot();
      currState = RE_SETUP;
      break;

    case RE_SETUP:
      syncClock(); //note- automatic for now, might introduce manual 'manual' reset later
      if(resetFin){
        currState = CLOCK_MODE;
      }
      currState = ERROR;
      break;
    
    case ALARM_MODE:
      alarm_boot();
      currState = ALARM_SETUP;
      break;

    case ALARM_SETUP:
      alarmSetup();
      if(alarmFin){
        currState = ALARM_FINISHED;
      }
      break;

    case ALARM_FINISHED:
      if(snoozeMode){
        currState = SNOOZE;
      }
      currState = CLOCK_MODE;
      break;

    case SNOOZE:
      snooze_boot();
      currState = SNOOZE_SETUP;
      break;

    case SNOOZE_SETUP:
      snoozeSetup();
      if(snoozeFin){
        currState = SNOOZE_FINISHED;
      }
      break;
    case SNOOZE_FINISHED:
      snoozeFinished();
      if(snoozeMode){
        currState = SNOOZE;
      }
      currState = CLOCK_MODE;
      break;
  }
}

void syncClock() {
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
}

void displayClock() {
}

void alarmSetup() {
}

void reset_boot() {
}

void alarm_boot() {
}

void snooze_boot() {
}

void snoozeSetup() {
}

void alarmFinished() {
}

void snoozeFinished() {
}