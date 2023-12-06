#include <Arduino.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiManager.h>
#include "DigitLedDisplay.h"


/* Arduino Pin to Display Pin
   7 to DIN,
   6 to CS,
   5 to CLK */
#define DIN 2
#define CS 5
#define CLK 4 
DigitLedDisplay ld = DigitLedDisplay(DIN, CS, CLK);

#define BUZZER_PIN 20 // Buzzer pin
#define CLICK_DURATION 20 // Click duration in ms

#define TACTILE_SWITCH_PIN 10 // Tactile switch pin

const bool isFeesDisplayEnabled = true;

int32_t blockHeight = 0;
int32_t bitcoinPrice = 0;

uint32_t DemoCounter=0;

int32_t lastBlockHeight = 0;
int32_t i = 0;

void click(int period);
String getEndpointData(String url);
void writeTickTock();
void configureAccessPoint();
void animateClear();

WebSocketsClient coinbaseWebSocket;
WebSocketsClient mempoolWebSocket;

void handleIncomingMessage(char* message) {
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, message);
  
  const char* type = doc["type"]; 
  if (strcmp(type, "ticker") == 0) {
    const char* price = doc["price"]; 
    Serial.print("BTC-USD Price: ");
    Serial.println(price);

    // write to display
    bitcoinPrice = atoi(price);

    ld.clear();
    // print from first LED 
    ld.printDigit(bitcoinPrice);
  }
}

void mempoolWebSocketEvent(WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
  case WStype_DISCONNECTED:
    Serial.println("Disconnected!");
    // Attempt to reconnect
    mempoolWebSocket.beginSSL("mempool.space", 443, "/api/v1/ws");
    break;
  case WStype_CONNECTED:
    Serial.println("Connected to WebSocket");
    // Send message on connection
    click(225);
    delay(100);
    click(225);

    mempoolWebSocket.sendTXT("{\"action\": \"want\", \"data\": [\"blocks\"]}");
    // tft show "connected"
    break;
  case WStype_TEXT:
    Serial.println("Received: ");
    Serial.println((char *)payload);
    // if payload includes the word "block" then click
    if (strstr((char *)payload, "height"))
    {
      click(100);
      // get height from this payload: {"block": {"id":"00000000000000000003390f9da5e76dba42f8b3f147bcfdfc8092b4ba8007ac","height":810613,"version":648273920,"timestamp":1696424982,"bits":386197775,"nonce":1711818532,"difficulty":57321508229258.04,"merkle_root":"2f4d206b4ae8fa000f8a4bca8e204c7cfbe2401e796d19c31587fc5c9e9d8ef1","tx_count":2064,"size":1288841,"weight":3992804,"previousblockhash":"00000000000000000000af83bffb46f1de8a6b22e3822bb6cb0699ce0bd2ee0d","mediantime":1696420600,"stale":false,"extras":{"reward":647328442,"coinbaseRaw":"03755e0c0416641d652f466f756e6472792055534120506f6f6c202364726f70676f6c642f0d01c0361f01000000000000","orphans":[],"medianFee":16.050393700787403,"feeRange":[13.022222222222222,14.733532934131736,16.52953537240252,28.53715775749674,31.137777777777778,40.47144152311877,469.97389033942557],"totalFees":22328442,"avgFee":10823,"avgFeeRate":22,"utxoSetChange":-1647,"avgTxSize":624.29,"totalInputs":7045,"totalOutputs":5398,"totalOutputAmt":1519350914914,"segwitTotalTxs":1755,"segwitTotalSize":718500,"segwitTotalWeight":1711548,"feePercentiles":null,"virtualSize":998201,"coinbaseAddress":"bc1qxhmdufsvnuaaaer4ynz88fspdsxq2h9e9cetdj","coinbaseSignature":"OP_0 OP_PUSHBYTES_20 35f6de260c9f3bdee47524c473a6016c0c055cb9","coinbaseSignatureAscii":"\u0003u^\f\u0004\u0016d\u001de/Foundry USA Pool #dropgold/\r\u0001À6\u001f\u0001\u0000\u0000\u0000\u0000\u0000\u0000","header":"00e0a3260deed20bce9906cbb62b82e3226b8adef146fbbf83af00000000000000000000f18e9d9e5cfc8715c3196d791e40e2fb7c4c208eca4b8a0f00fae84a6b204d2f16641d650fe9041724470866","utxoSetSize":null,"totalInputAmt":null,"pool":{"id":111,"name":"Foundry USA","slug":"foundryusa"},"matchRate":100,"expectedFees":22854570,"expectedWeight":3991874,"similarity":0.9598855088210204}}}
      // parse json
      StaticJsonDocument<1024> doc;
      deserializeJson(doc, (char *)payload);
      // get height
      int height = doc["block"]["height"];
      // draw height on display
      click(100);
      ld.clear();
      ld.printDigit(height);
      delay(1000);
    }
    break;
  case WStype_BIN:
  case WStype_ERROR:
  case WStype_FRAGMENT_TEXT_START:
  case WStype_FRAGMENT_BIN_START:
  case WStype_FRAGMENT:
  case WStype_FRAGMENT_FIN:
    // Handle or ignore other cases
    break;
  }
}

void coinbaseWebSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[WSc] Disconnected!\n");
      // reconnect
      coinbaseWebSocket.beginSSL("ws-feed.exchange.coinbase.com", 443, "/");
      break;
    case WStype_CONNECTED:
      {
        Serial.printf("[WSc] Connected to url: %s\n", payload);
        // Send message to subscribe to ticker
        coinbaseWebSocket.sendTXT("{\"type\": \"subscribe\", \"product_ids\": [\"BTC-USD\"], \"channels\": [\"level2\", \"heartbeat\", {\"name\": \"ticker\", \"product_ids\": [\"BTC-USD\"]}]}");
      }
      break;
    case WStype_TEXT:
      Serial.printf("[WSc] get text: %s\n", payload);
      // Handle the received message
      handleIncomingMessage((char*)payload);
      break;
    case WStype_BIN:
      Serial.printf("[WSc] get binary length: %u\n", length);
      break;
  }
}

void displayMempoolFees() {
  uint16_t pagingDelay = 2000;
  DynamicJsonDocument doc(200);
  String line = getEndpointData("https://mempool.space/api/v1/fees/recommended");

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
  delay(1000);
  animateClear();

  uint16_t fee;
  // none
  fee = doc["minimumFee"];
  // none
  ld.clear();
  ld.write(8, B1110110); // N
  ld.write(7, B1111110); // O
  ld.write(6, B1110110); // N
  ld.write(5, B01001111); // E
  ld.printDigit(fee);
  delay(pagingDelay);

  // economy
  fee = doc["economyFee"];
  // economy
  animateClear();
  ld.clear();
  ld.write(8, B01001111); // E
  ld.write(7, B0001101); // c
  ld.write(6, B0011101); // o
  ld.printDigit(fee);
  delay(pagingDelay);

  // hour
  fee = doc["hourFee"];
  // hour
  animateClear();
  ld.clear();
  ld.write(8, B0110111);  // H
  ld.write(7, B0011101); // o
  ld.write(6, B0011100); // u
  ld.write(5, B00000101); // r
  ld.printDigit(fee);
  delay(pagingDelay);

  // fast
  fee = doc["fastestFee"];
  // fast
  animateClear();
  ld.clear();
  ld.write(8, B01000111); // F
  ld.write(7, B01110111); // A
  ld.write(6, B01011011); // S
  ld.write(5, B00001111); // t
  ld.printDigit(fee);
  
}

void displayBlockHeight() {
  // Get block height
  const String line = getEndpointData("https://mempool.space/api/blocks/tip/height");
  // const String line = getEndpointData("/bh");
  Serial.println("Block height");
  Serial.println(line);

  blockHeight = line.toInt();
  // blockHeight = 696969;

  animateClear();
  ld.write(8, B0110111); // H
  ld.write(7, B1001111); // E
  ld.write(6, B0000110); // I
  ld.write(5, B1011110); // G
  ld.write(4, B0110111); // H
  ld.write(3, B0001111); // t
  delay(1000);

  animateClear();

  // ld.write(8, B1111110); // square
  ld.printDigit(blockHeight);
  // ld.write(8, B00011101);
  // ld.write(7, B00001001);
}

void animateClear() {
  uint16_t animDelay = 50;
  // for segment 8 to 1
  for(uint8_t i = 8; i >= 1; i--) {
    if(i <= 8) {
      ld.write(i + 1, B00000000);
    }
    ld.write(i, B0001000);
    delay(animDelay);
  }
  ld.clear();
  delay(animDelay);
}

void displayBitcoinPrice() {
  // Get block height
  const String line = getEndpointData("https://api.coinbase.com/v2/prices/BTC-USD/buy");
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
  ld.write(8, B0011111); // b
  ld.write(7, B0001111); // t
  ld.write(6, B0001101); // c
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
String getEndpointData(String url) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();
      return payload;
    } else {
      Serial.println("Error in HTTP request");
    }
    http.end();
  } else {
    Serial.println("Not connected to WiFi");
  }
  return "";
}

void initWiFi() {
  // WiFiManager for auto-connecting to Wi-Fi
  WiFiManager wifiManager;
  wifiManager.autoConnect("AutoConnectAP");

  Serial.println("Connected to WiFi");
}

void scrollWord(String word) {
  uint16_t animDelay = 50;
  for(uint8_t i = 8; i >= 1; i--) {
    if(i <= 8) {
      ld.write(i + 1, B00000000);
    }
    ld.write(i, B0001000);
    delay(animDelay);
  }
  ld.clear();
  delay(animDelay);
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

void writeTickTock() {
  const uint16_t animDelay = 250;
  ld.clear();
  
  ld.write(7, B00001111); // t
  delay(50);
  ld.write(6, B00110000); // i
  delay(50);
  ld.write(5, B01001110); // c
  delay(animDelay);
  
  ld.write(4, B00001111); // t
  delay(50);
  ld.write(3, B01111110); // o
  delay(50);
  ld.write(2, B01001110); // c
  delay(animDelay);

  // for(uint8_t i = 7; i >= 2; i--) {
  //   ld.write(i, B10000000);
  // }
  // delay(animDelay); 
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


void click(int period)
{
  Serial.println("NEW BLOCK Click!");
  for (int i = 0; i < CLICK_DURATION; i++)
  {
    digitalWrite(BUZZER_PIN, HIGH);
    delayMicroseconds(period); // Half period of 1000Hz tone.
    digitalWrite(BUZZER_PIN, LOW);
    delayMicroseconds(period); // Other half period of 1000Hz tone.
  }
}

void setup() {
  delay(5000);
  Serial.begin(115200);
  Serial.println("Booted up");

    /* Set the brightness min:1, max:15 */
  ld.setBright(1);

  /* Set the digit count */
  ld.setDigitLimit(8);

  delay(1000);

  pinMode(BUZZER_PIN, OUTPUT); // Set the buzzer pin as an output.
  // click on boot
  click(225);

  animateClear();
  writeBitcoin();
  animateClear();
  writeTickTock();
  
  // read state of tactile switch
  pinMode(TACTILE_SWITCH_PIN, INPUT_PULLUP);
  Serial.println("Tactile switch state");
  Serial.println(digitalRead(TACTILE_SWITCH_PIN));

  Serial.println("initing wifi");
  initWiFi();
  Serial.println("wifi connected");

  // Setup coinbaseWebSocket
  coinbaseWebSocket.beginSSL("ws-feed.exchange.coinbase.com", 443, "/");
  coinbaseWebSocket.onEvent(coinbaseWebSocketEvent);
  coinbaseWebSocket.setReconnectInterval(5000);
  coinbaseWebSocket.enableHeartbeat(15000, 3000, 2);

  mempoolWebSocket.beginSSL("mempool.space", 443, "/api/v1/ws");
  mempoolWebSocket.onEvent(mempoolWebSocketEvent);
  
}

void loop() {
    coinbaseWebSocket.loop();
    mempoolWebSocket.loop();
  
  // if(isFeesDisplayEnabled) {
  //   displayMempoolFees();
  //   delay(2000);
  // }
  // displayBitcoinPrice();
  // delay(20000);

  // if(isFeesDisplayEnabled) {
  //   displayMempoolFees();
  //   delay(2000);
  // }
  // displayBlockHeight();
  // delay(20000);
}