#pragma once 

//Every wire on the breadboard 

//LED and BUZZERS
#define PIN_LED_STATUS 2 //onboard led
#define PIN_LED_B 13 // scanned LED 
#define PIN_LED_C 14 // "Pouring / done" LED
#define PIN_BUZZER 25 

//Buttons, wired button -> GND, internal pull up on 
#define PIN_BTN_POUR 4 
// PIN_BTN_HALF removed for now: half/full will become a mode toggle later

//Relay module IN pin , Active HIGH 1 = pump on, 0 = pump off 
#define PIN_RELAY 32 //not GPIO 15: its boot-time pull-up would switch an active-high relay on

//HC-SR04. ECHOs come through 1k/2k divider 
#define PIN_TRIG 5
#define PIN_ECHO 18 

// PN532 on I2C
#define PIN_I2C_SDA 21
#define PIN_I2C_SCL 22 

//DFD PLayer Mini on UART2, PIN_DF_TX goes to the DFPlayers RX pin 
#define PIN_DF_TX 17
#define PIN_DF_RX 16 


