// off Decimal: 832832776 (32Bit) Binary: 00110001101001000000010100001000 Tri-State: not applicable PulseLength: 254 microseconds Protocol: 1
// on Decimal: 832836016 (32Bit) Binary: 00110001101001000001000110110000 Tri-State: not applicable PulseLength: 253 microseconds Protocol: 1
// +  Decimal: 832833776 (32Bit) Binary: 00110001101001000000100011110000 Tri-State: not applicable PulseLength: 253 microseconds Protocol: 1
// - Decimal: 832832136 (32Bit) Binary: 00110001101001000000001010001000 Tri-State: not applicable PulseLength: 254 microseconds Protocol: 1

#include <RCSwitch.h>

// Uncomment the following line to enable serial debug output
//#define ENABLE_DEBUG

#ifdef ENABLE_DEBUG
  #define DEBUG_ESP_PORT Serial
  #define NODEBUG_WEBSOCKETS
  #define NDEBUG
#endif 

#include <Arduino.h>
#if defined(ESP8266)
  #include <ESP8266WiFi.h>
#elif defined(ESP32) || defined(ARDUINO_ARCH_RP2040)
  #include <WiFi.h>
#endif
#include <SPI.h>
#include <Wire.h>
#include <U8x8lib.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#include "SinricPro.h"
#include "SinricProFanUS.h"

RCSwitch mySwitch = RCSwitch();

#define WIFI_SSID         ""    
#define WIFI_PASS         ""
#define APP_KEY           ""      // Should look like "de0bxxxx-1x3x-4x3x-ax2x-5dabxxxxxxxx"
#define APP_SECRET        ""   // Should look like "5f36xxxx-x3x7-4x3x-xexe-e86724a9xxxx-4c4axxxx-3x3x-x5xe-x9x3-333d65xxxxxx"
#define FAN_ID            ""    // Should look like "5dc1564130xxxxxxxxxxxxxx"
#define BAUD_RATE         115200                // Change baudrate to your need

U8X8_SSD1306_128X64_NONAME_SW_I2C u8x8(/* clock=*/ SCL, /* data=*/ SDA, /* reset=*/ U8X8_PIN_NONE);



bool lamp=true; // variabile per puntini attesa connessione wifi
bool trasmetto = false; //per sapere quando devo trasmettere il comando +- velocita ventola
bool diminuisco = false; // variabile se devo aumentare false diminuire true
int n_invi; // quante volte devo tx
String ip;  // per stampare ip su display


// we use a struct to store all states and values for our fan
// fanSpeed (1..3)

struct {
  bool powerState = false;
  int fanSpeed = 10;
} device_state;



void accendi(){
 // mySwitch.setPulseLength(253);
  mySwitch.send("00110001101001000001000110110000");
}
void spegni(){
 // mySwitch.setPulseLength(254);
  device_state.fanSpeed = 10;
  sendFanSpeed(10);
  mySwitch.send("00110001101001000000010100001000");
}
void sendFanSpeed(int speed) {
  SinricProFanUS &myFan = SinricPro[FAN_ID];
  myFan.sendRangeValueEvent(speed);
}


bool onPowerState(const String &deviceId, bool &state) {
  Serial.printf("Fan turned %s\r\n", state?"on":"off");
  u8x8.clear();
  u8x8.setFont(u8x8_font_profont29_2x3_r);
  u8x8.drawString(0, 2, "TX");
  device_state.powerState = state;
  if (state) {
    accendi();
  }else{
    spegni();
  }
  u8x8.clear();
  u8x8.setFont(u8x8_font_victoriabold8_r);
  const char *indirizzo = ip.c_str();
  u8x8.drawString(0, 0, indirizzo);
  u8x8.setFont(u8x8_font_profont29_2x3_r);
  u8x8.drawString(0, 2, "SERVER");
  u8x8.drawString(0, 5, "ON");
  return true; // request handled properly
}

// Fan rangeValue is from 1..3
bool onRangeValue(const String &deviceId, int& rangeValue) {
   trasmetto = true;
   if (rangeValue > device_state.fanSpeed ){
      n_invi = rangeValue - device_state.fanSpeed;
      diminuisco = false;
   } else {
      n_invi = device_state.fanSpeed - rangeValue;
      diminuisco = true;
   }
  device_state.fanSpeed = rangeValue;
  
  Serial.printf("Fan speed changed to %d\r\n", device_state.fanSpeed);
  Serial.printf("numero invi %d\r\n", n_invi);
  return true;
}

void setupWiFi() {
  Serial.printf("\r\n[Wifi]: Connecting");
  
  u8x8.begin();
  u8x8.setPowerSave(0);  
  u8x8.clear();
  u8x8.setFont(u8x8_font_victoriabold8_r);
  u8x8.drawString(0, 0, "CONNESSIONE");
  
  #if defined(ESP8266)
    WiFi.setSleepMode(WIFI_NONE_SLEEP); 
    WiFi.setAutoReconnect(true);
  #elif defined(ESP32)
    WiFi.setSleep(false); 
    WiFi.setAutoReconnect(true);
  #endif
  
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    if (lamp){
      u8x8.setFont(u8x8_font_profont29_2x3_r);
      u8x8.drawString(0, 4, "....");
    }else{
      u8x8.setFont(u8x8_font_profont29_2x3_r);
      u8x8.drawString(0, 4, "    ");
    }
    delay(100);
    !lamp;
  }
  u8x8.clear();
  IPAddress localIP = WiFi.localIP();
  ip.concat((String)localIP[0]);
  ip.concat(".");
  ip.concat((String)localIP[1]);
  ip.concat(".");
  ip.concat((String)localIP[2]);
  ip.concat(".");
  ip.concat((String)localIP[3]);
  const char *indirizzo = ip.c_str();
  u8x8.setFont(u8x8_font_victoriabold8_r);
  u8x8.drawString(0, 0, indirizzo);
  mySwitch.setPulseLength(253);
  mySwitch.setRepeatTransmit(8);
  Serial.printf("connected!\r\n[WiFi]: IP-Address is %s\r\n", WiFi.localIP().toString().c_str());
}

void setupSinricPro() {
  SinricProFanUS &myFan = SinricPro[FAN_ID];

  // set callback function to device
  myFan.onPowerState(onPowerState);
  myFan.onRangeValue(onRangeValue);
 // myFan.onAdjustRangeValue(onAdjustRangeValue);

  // setup SinricPro
  SinricPro.onConnected([](){ u8x8.setFont(u8x8_font_profont29_2x3_r);
                              u8x8.drawString(0, 2, "SERVER");
                              u8x8.drawString(0, 5, "ON"); }); 
  SinricPro.onDisconnected([](){ u8x8.drawString(0, 2, "SERVER");
                                 u8x8.drawString(0, 5, "OFF"); }); 
  SinricPro.begin(APP_KEY, APP_SECRET);
  
  sendFanSpeed(10);
}

void setup() {
  pinMode(2, OUTPUT);
  
  // pin per trasmissione 433.92
  mySwitch.enableTransmit(2);

  Serial.begin(BAUD_RATE); Serial.printf("\r\n\r\n");
  setupWiFi();
  setupSinricPro();
}

void loop() {
   if (trasmetto){  // aumento o diminuisco velocita ventola 
    int n_tx=0;
     while ((n_tx <= n_invi-1)&&(n_invi>=0)){
          n_tx++;
          Serial.printf("numero invi %d\r\n", n_tx);
          if (diminuisco){
            mySwitch.send(832832136,32);
          }else{
            mySwitch.send(832833776,32);
          }
         
     }
     trasmetto = false;
   }
  // put your main code here, to run repeatedly:
  SinricPro.handle();
}
