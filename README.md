# STM32 Low Power Metronome With Tap Tempo

This repository contains the firmware and configuration for a standalone, feature-rich digital metronome built on the NUCLEO-STM32L432KC microcontroller. Designed to replace limited freemium mobile apps and reduce screen time during long acoustic flatpicking and fingerstyle practice sessions, the system prioritizes tactile hardware controls, precise audio generation, and aggressive power management.

## Core Capabilities

* **Dynamic Tempo & Time Signatures:** Adjustable tempo from 30 to 240 bpm with selectable beat lengths for various time signatures.

* **Rhythmic Subdivisions:** Real-time generation of quarter, eighth, and sixteenth note internal subdivisions.

* **Tap Tempo Engine:** Calculates an average bpm from multiple user inputs within a 2.5-second timeout window.

* **Audio Generation:** Outputs custom inverted-parabola DAC pulses for distinct downbeat, regular, and subdivision accents, scaled by a multi-level volume control.

## Hardware Architecture

The system relies on hardware interrupts to ensure precise timing and responsive controls without blocking the main execution loop.

<img width="50%"  src="https://github.com/user-attachments/assets/dcc868b6-d59d-4bcf-9718-d38d8bebb917" />

* **MCU:** STMicroelectronics NUCLEO-STM32L432KC

* **User Interface:** KY-040 Rotary Encoder for menu navigation and SSD1306 OLED via I2C for visual feedback

* **Audio Output:** Hardware DAC (12-bit) routed through a PAM8403 amplifier to a generic speaker

* **External Interrupts (EXTI):** Dedicated tactile push-buttons for Tap Tempo and Mute toggling

## Power Optimization Strategy

To maximize battery life for portable operation, the firmware dynamically scales the microcontroller's power consumption based on user interaction states.

* **Run Mode:** Fully active at 80 MHz during encoder or menu interactions, timing out after 3 seconds of inactivity.

* **Sleep Mode:** Activated via `WFI()` when the metronome is ticking but the UI is idle, keeping the CPU halted while the timers and DAC continue operating.

* **Low-Power Sleep Mode:** Engaged when the device is muted; drops the MSI clock to 2 MHz, switches to the low-power voltage regulator, and disables non-essential EXTIs until the unmute button is pressed.

## Repository Structure & Third-Party Libraries

This project was built using the Keil MDK-ARM IDE and standard STMicroelectronics STM32L4xx HAL drivers.

* `Drivers/`: Contains the necessary CMSIS, BSP, and HAL driver source files.

* `Projects/NUCLEO-L432KC/Templates/`: Contains the active project files.

  * `MDK-ARM/`: Contains the `.uvprojx` Keil project and main application source (`metronome_final_version.c`).

  * `ssd1306/`: An open-source [SSD1306 OLED library by afiskon](https://github.com/afiskon/stm32-ssd1306) utilized for the graphical interface (licenses retained within).

## How to Build

1. Clone the repository to your local machine.

2. Open the `Project.uvprojx` file located in `Projects/NUCLEO-L432KC/Templates/MDK-ARM/` using Keil uVision.

3. Compile and flash to the NUCLEO-L432KC board.
