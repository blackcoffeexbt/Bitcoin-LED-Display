/* 
 *  8 Digits LED Seven Segment Display Module based on MAX7219 using Arduino IDE
 *  
 *  updated by Ahmad Shamshiri for Robojax 
 *  On Monday Sep 16, 2019 in Ajax, Ontario, Canada
 *  You can get this code from Robojax.com
 *  
 *  Original Libary source: https://github.com/ozhantr/DigitLedDisplay
 *  
 *  Watch video instruction for this video: https://youtu.be/R5ste5UHmQk
 *  At the moments, it doesn't display decimal points. It needs a little work to make it work

You can get the wiring diagram from my Arduino Course at Udemy.com
Learn Arduino step by step with all library, codes, wiring diagram all in one place
visit my course now http://robojax.com/L/?id=62

If you found this tutorial helpful, please support me so I can continue creating 
content like this. You can support me on Patreon http://robojax.com/L/?id=63
or make donation using PayPal http://robojax.com/L/?id=64

 * Code is available at http://robojax.com/learn/arduino

 * This code is "AS IS" without warranty or liability. Free to be used as long as you keep this note intact.* 
 * This code has been download from Robojax.com
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>. 
    

 */
#include <ArduinoJson.h>
#include <AutoConnect.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "DigitLedDisplay.h"
#include "ching.h"
#include "XT_DAC_Audio.h"

/* Arduino Pin to Display Pin
   7 to DIN,
   6 to CS,
   5 to CLK */
#define DIN 5
#define CS 17
#define CLK 16   
DigitLedDisplay ld = DigitLedDisplay(DIN, CS, CLK);

WebServer server;
AutoConnect portal(server);
AutoConnectConfig config;
AutoConnectAux elementsAux;
AutoConnectAux saveAux;

String apPassword = "ToTheMoon1"; //default WiFi AP password

const bool isFeesDisplayEnabled = true;

int32_t blockHeight = 0;
int32_t bitcoinPrice = 0;

XT_Wav_Class ChingWav(ching_wav);     // create an object of type XT_Wav_Class that is used by 
XT_DAC_Audio_Class DacAudio(25,0);    // Create the main player class object. 

uint32_t DemoCounter=0;

int32_t lastBlockHeight = 0;
int32_t i = 0;

String getEndpointData(const char* host, String endpointUrl);
void playAudioChing();
void writeTickTock();
void configureAccessPoint();
void animateClear();

void displayMempoolFees() {
  uint16_t pagingDelay = 2000;
  StaticJsonDocument<3000> doc;
  const String line = getEndpointData("mempool.space", "/api/v1/fees/recommended");

  DeserializationError error = deserializeJson(doc, line);
  if (error)
  {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.f_str());
  }

  animateClear();
  ld.write(8, B1000111); // f
  ld.write(7, B1001111); // e
  ld.write(6, B1001111); // e
  ld.write(5, B1011011); // s
  delay(1000 );
  animateClear();

  uint16_t fee;
  // none
  fee = doc["minimumFee"];
  // none
  ld.clear();
  ld.write(8, B01110110);
  ld.write(7, B01111110);
  ld.write(6, B01110110);
  ld.write(5, B01001111);
  ld.printDigit(fee);
  delay(pagingDelay);

  // economy
  fee = doc["economyFee"];
  // economy
  ld.clear();
  ld.write(8, B01001111);
  ld.write(7, B01001110);
  ld.write(6, B01111110);
  ld.printDigit(fee);
  delay(pagingDelay);

  // hour
  fee = doc["hourFee"];
  // hour
  ld.clear();
  ld.write(8, B00010111);
  ld.write(7, B01111110);
  ld.write(6, B00111110);
  ld.write(5, B00000101);
  ld.printDigit(fee);
  delay(pagingDelay);

  // fast
  fee = doc["fastestFee"];
  // fast
  ld.clear();
  ld.write(8, B01000111);
  ld.write(7, B01110111);
  ld.write(6, B01011011);
  ld.write(5, B00001111);
  ld.printDigit(fee);
  
}

void displayBlockHeight() {
  // Get block height
  const String line = getEndpointData("mempool.space", "/api/blocks/tip/height");
  // const String line = getEndpointData("/bh");
  Serial.println("Block height");
  Serial.println(line);

  blockHeight = line.toInt();
  // blockHeight = 696969;

  animateClear();
  ld.write(8, B0110111); // h
  ld.write(7, B1001111); // e
  ld.write(6, B0000110); // i
  ld.write(5, B1011110); // g
  ld.write(4, B0110111); // h
  ld.write(3, B0001111); // t
  delay(1000);

  animateClear();

  if(blockHeight != lastBlockHeight) {
      lastBlockHeight = blockHeight;
      playAudioChing();
      delay(1000);
      writeTickTock();
    }

  ld.clear();
  ld.write(8, B1111111); // square
  ld.printDigit(blockHeight);
  // ld.write(8, B00011101);
  // ld.write(7, B00001001);
}

void animateClear() {
  // for segment 8 to 1
  for(uint8_t i = 8; i >= 1; i--) {
    if(i <= 8) {
      ld.write(i + 1, B00000000);
    }
    ld.write(i, B0001000);
    delay(100);
  }
  ld.clear();
}

void displayBitcoinPrice() {
  // Get block height
  const String line = getEndpointData("api.coinbase.com", "/v2/prices/BTC-USD/buy");
  // data will look like this, get the amount value {"data":{"amount":"26602.105","base":"BTC","currency":"USD"}} using ArduinoJson
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, line);
  const char* amount = doc["data"]["amount"];
  
  Serial.println("amount is");
  Serial.println(amount);

  bitcoinPrice = atoi(amount);

  Serial.println("bitcoinPrice is");
  Serial.println(bitcoinPrice);

  animateClear();
  ld.write(8, B1111101); // b
  ld.write(7, B0001111); // t
  ld.write(6, B1001110); // c
  ld.write(5, B0011100); // U
  ld.write(4, B1011011); // S
  ld.write(3, B0111101); // d
  delay(1000);

  animateClear();
  ld.clear();
  // print from first LED 
  ld.printDigit(bitcoinPrice);
}


/**
 * @brief GET data from an LNbits endpoint
 * 
 * @param endpointUrl 
 * @return String 
 */
String getEndpointData(const char* host, String endpointUrl) {
  WiFiClientSecure client;
  client.setInsecure(); //Some versions of WiFiClientSecure need this

  if (!client.connect(host, 443))
  {
    Serial.println("Server down");
    delay(3000);
  }

  const String request = String("GET ") + endpointUrl + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: BCLedDisplay\r\n" +
               "Content-Type: application/json\r\n" +
               "Connection: close\r\n\r\n";
  client.print(request);
  while (client.connected())
  {
    const String line = client.readStringUntil('\n');
    if (line == "\r")
    {
      break;
    }
  }

  const String line = client.readString();
  return line;
}

void writeBitcoin() {
  ld.write(7, B01111111); // B
  ld.write(6, B00110000); // i
  ld.write(5, B00001111); // t
  ld.write(4, B01001110); // c
  ld.write(3, B01111110); // o
  ld.write(2, B00110000); // i
  ld.write(1, B01110110); // n
}

void playAudioChing() {
  Serial.println("Ching");
  
  DacAudio.FillBuffer(); 
  DacAudio.Play(&ChingWav);

  while(ChingWav.Playing) {
    DacAudio.FillBuffer();                // Fill the sound buffer with data  
  }
}

void writeTickTock() {
  const uint16_t animDelay = 500;
  ld.clear();
  
  ld.write(7, B00001111); // t
  ld.write(6, B00110000); // i
  ld.write(5, B01001110); // c
  delay(animDelay);
  
  ld.write(4, B00001111); // t
  ld.write(3, B01111110); // o
  ld.write(2, B01001110); // c
  delay(animDelay);

  for(uint8_t i = 7; i >= 2; i--) {
    ld.write(i, B10000000);
  }
  delay(animDelay); 
}


void showLoadingAnim() {
    const uint8_t animDelay = 100;
    for(int8_t i = 0; i < 6; i++) {
      for(int8_t j = 0; j <= 8; j++) {
        switch(i) {
          case 0:
            ld.write(j, B01000000);
          break;
          case 1:
            ld.write(j, B01100000);
          break;
          case 2:
            ld.write(j, B01110000);
          break;
          case 3:
            ld.write(j, B01111000);
          break;
          case 4:
            ld.write(j, B01111100);
          break;
          default:
            ld.write(j, B01111110);
          break;
        }
      }
    delay(animDelay);
  }
}


void demo() {
  
 

  /* Prints data to the display */
  ld.printDigit(12345678); 
  delay(3000);
  ld.clear();

  ld.printDigit(22222222);
  delay(500);
  ld.clear();

  ld.printDigit(44444444);
  delay(500);
  ld.clear();

  for (int i = 0; i < 100; i++) {
    ld.printDigit(i);

    /* Start From Digit 4 */
    ld.printDigit(i, 4);
    delay(50);

    
  }

  for (int i = 0; i <= 10; i++) {
    /* Display off */
    ld.off();
    delay(150);

    /* Display on */
    ld.on();
    delay(150);
  }

  /* Clear all display value */
  ld.clear();
  delay(500);

  for (long i = 0; i < 100; i++) {
    ld.printDigit(i);
    delay(25);
  }

  for (int i = 0; i <= 20; i++) {
    /* Select Digit 5 and write B01100011 */
    ld.write(5, B01100011);
    delay(200);

    /* Select Digit 5 and write B00011101 */
    ld.write(5, B00011101);
    delay(200);
  }

  /* Clear all display value */
  ld.clear();
  delay(500);

  ld.printDigit(2019, 3);
  delay(4000);
}

void initWiFi() {
  configureAccessPoint();
    
  WiFi.mode(WIFI_STA);
  while (WiFi.status() != WL_CONNECTED) {
    delay(3000);
  }
}


void configureAccessPoint() {
  // handle access point traffic
  server.on("/", []() {
    String content = "<h1>Bitcoin Block Height Display</h1>";
    content += AUTOCONNECT_LINK(COG_24);
    server.send(200, "text/html", content);
  });

  config.autoReconnect = true;
  config.reconnectInterval = 1; // 30s
  //  config.beginTimeout = 10000UL;

  config.immediateStart = false;
  config.hostName = "Mehclock";
  config.apid = "Mehclock-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  config.apip = IPAddress(6, 15, 6, 15);      // Sets SoftAP IP address
  config.gateway = IPAddress(6, 15, 6, 15);     // Sets WLAN router IP address
  config.psk = apPassword;
  config.menuItems = AC_MENUITEM_CONFIGNEW | AC_MENUITEM_OPENSSIDS | AC_MENUITEM_RESET;
  config.title = "Mehclock";
  config.portalTimeout = 120000;

  portal.join({elementsAux, saveAux});
  portal.config(config);

    // Establish a connection with an autoReconnect option.
  if (portal.begin()) {
    Serial.println("WiFi connected: " + WiFi.localIP().toString());
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();

  DacAudio.DacVolume=10;
  
  initWiFi();
  
  /* Set the brightness min:1, max:15 */
  ld.setBright(1);

  /* Set the digit count */
  ld.setDigitLimit(8);

  delay(500);
  writeBitcoin();
  delay(3000);
}

void loop() {
  if(isFeesDisplayEnabled) {
    displayMempoolFees();
    delay(2000);
  }
  displayBitcoinPrice();
  delay(30000);
  displayBlockHeight();
  delay(30000);
}// loop ends