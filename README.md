# Bitcoin-LED-Display

## Parts list

1. MAX7219 8 Digit Seven Segment Display Module
1. ESP32
1. A micro USB cable
1. Two 3.5mm heat inset nuts, two m2.5 x 6mm screws.

## Instructions

1. Connect the MAX7219 board to the ESP32:
    1. DIN to pin 5
    1. CS to pin 17
    1. CLK to pin 16   
    1. +VE to 5V 
    1. GND to a GND pin
1. Copy libraries to Arduino/
1. Open .ino in Arduino IDE
1. Compile and upload
1. Power on and connect to WiFi using the AP "MehClock-xxxxx" and password "ToTheMoon1"
1. Menu > Configure new AP > Then set up the AP.
