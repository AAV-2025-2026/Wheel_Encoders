# Wheel Encoders
Collin Dang, Alex Tempel

## Summary

## Required HW
- Raspberry Pi Pico 2
- WizNet Ethernet HAT (connected to Pico 2)

## Setup
1. Install Arduino IDE
2. Open Arduino IDE, go "File" -> "Preferences", add https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json to "Additional Boards Manager URLs" field
3. Go "Tools" -> "Board" -> "Boards Manager", search "RP2350" install "Raspberry Pi Pico/RP2040/RP2350"
4. Once installed, go "Tools" -> "Board", select "Raspberry Pi Pico 2"
5. Download ZIP (under Code button) https://github.com/WIZnet-ArduinoEthernet/Ethernet
6. Once downloaded, go "Sketch" -> "Include Library" -> "Add .ZIP Library", select the downloaded zip file

You should then be able to build and upload this code to a Raspberry Pi Pico 2 over USB
