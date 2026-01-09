# Embedded Alarm Clock Prototype

A reliable, time-aware alarm clock system built as an embedded prototype with persistent timekeeping and clearly defined operational states.

## What this project does
- Displays real-time clock information using an RTC module
- Triggers alarm events through a buzzer based on user-defined schedules
- Maintains correct time and alarm behavior across power loss
- Uses explicit state management to handle normal, alarm, and idle modes

## System overview
An Arduino microcontroller interfaces with an RTC module to maintain accurate time. The system logic manages alarm states and triggers a buzzer when conditions are met. An LCD provides continuous feedback to the user.

## Tech stack
- Arduino
- C/C++ (Arduino)
- RTC module
- LCD display
- Buzzer

## Current status
This repository contains a functional embedded prototype with stable timekeeping and alarm logic. Networked control and advanced features are not implemented yet.
