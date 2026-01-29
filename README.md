# Aircraft Monitoring System

**Embedded IoT Project for Real-Time Aircraft Parameter Monitoring using Arduino Microcontrollers**

## Overview
This project implements a dual-unit telemetry system consisting of a **Flight Control Unit (FCU)** and a **Ground Control Unit (GCU)** for monitoring and controlling aircraft systems.

## Hardware Components
- Arduino MEGA 2560 (x2)
- MPU6050 IMU (acceleration/gyroscope)
- Temperature sensors
- PIR motion sensor
- RFID reader (RC522)
- Ultrasonic sensor (HC-SR04)
- MAX7219 LED matrix display
- LCD display (16x2)
- DC motor
- Servo motors (propeller & landing gear)
- Rotatory encoder
- IR remote control
- Wireless communication modules

## Features
- Real-time telemetry data transmission between FCU and GCU  
- Motion and orientation tracking via IMU  
- Temperature monitoring  
- RFID authentication system  
- Visual feedback through LED matrix and LCDs  
- Remote control via IR  
- Servo-controlled propeller and landing gear actuation  
- Node-RED dashboard for web-based monitoring  

## Project Structure
```
FlightControlUnit/ - FCU firmware
GroundControlUnit/ - GCU firmware
[Sensor]_Test/ - Individual sensor test modules
node-red/ - Dashboard configuration
Documentation/ - Project documentation
CircuitConnections/ - Wiring diagrams
```

## Setup
1. Install **Arduino IDE**.  
2. Upload **FlightControlUnitv0.2** code to the first Arduino MEGA.  
3. Upload **GroundControlUnitv0.2** code to the second Arduino MEGA.  
4. Wire the components according to the diagrams in the `CircuitConnections/` folder.  
5. Configure the **Node-RED** dashboard (optional).  

## Development
Each sensor subsystem includes its own test folder for isolated testing before integration into the main flight and ground control units.

#### Contact 
Email: tishaapatel08@gmail.com
