# Daisy Seed Guitar Pedal Project

An Arduino-based digital audio effects pedal built using the Electro-Smith Daisy Seed microcontroller.
https://docs.daisy.audio/hardware/Seed/

## Features
* Soft-clip distortion
* Low-Pass filter (LPF)
* Delay with additional stutter effect
* Strum-detecting arpeggiator with maj, minor, & maj7 modes
* Volume control

## Hardware & Breadboard Layout

### Audio I/O:
* Input: Routed to Daisy Pin 16 with a hardware op-amp boost to amplify the guitar signal to 3V
* Output: Routed from Daisy Pin 18 to output jack.

### Physical Controls (Knobs & Switches):
* Potentiometer 1 (Pin 22): Distortion gain
* Potentiometer 2 (Pin 23): Filtered frequencies for the LPF
* Potentiometer 3 (Pin 24): Delay timing. Adds stutter effect at 80% (or about ~1 sec delay)
* Potentiometer 4 (Pin 25): Volume
* Button (Pin 25): Toggles between arpeggiator modes (off, major chord, minor chord, major 7 chord).

## Software Dependencies & Setup

To compile this script, Arduino IDE must be configured with the following board packages and libraries:

### 1. Board Support Package (BSP)
* STM32 Cores by STMicroelectronics: Add STM32 board URL to Arduino Preferences and install boards manager. Allows Arduino IDE to compile code for the Daisy's ARM Cortex-M7 processor.

### 2. Required Libraries
* DaisyDuino (1.7.0): Primary Arduino wrapper library for the Daisy hardware platform. Handles background audio DMA streams and includes:
  * `DaisyDuino.h` (Hardware abstraction and audio callback setup)
  * `daisysp' namespace
