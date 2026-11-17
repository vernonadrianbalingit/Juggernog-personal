#pragma once 

//Every wire on the breadboard 

//LED and BUZZERS
#define PIN_LED_STATUS 2 //onboard led
#define PIN_LED_B 13 // scanned LED 
#define PIN_LED_C 14 // "Pouring / done" LED
#define PIN_BUZZER 25 

//Buttons, wired button -> GND, internal pull up on 
#define PIN_BTN_POUR 4 
#define PIN_BTN HALF 27

//Relay module IN pin , Active Low 0 = pump on, 1 = pump off 
#define PIN_RELAY 15 

//HC-SR04. ECHOs come through 1k/2k divider 
#define PIN_TRIG 5
#define PIN_ECHO 18 

// PN532 on I2C
#define PIN_I2C_SDA 21
#define PIN_I2C_SCL 22 

//DFD PLayer Mini on UART2, PIN_DF_TX goes to the DFPlayers RX pin 
#define PIN_DF_TX 17
#define PIN_DF_RX 16 


