#include <Arduino.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiManager.h>
#include "DigitLedDisplay.h"
#include "display.h"

/* Arduino Pin to Display Pin
   7 to DIN,
   6 to CS,
   5 to CLK */
#define DIN 2
#define CS 5
#define CLK 4
DigitLedDisplay ld = DigitLedDisplay(DIN, CS, CLK);

enum DisplayData
{
  PriceAndHeight,
  Price,
  BlockHeight,
  MempoolFees
};

DisplayData displayData = DisplayData::PriceAndHeight;

#define BUZZER_PIN 20     // Buzzer pin
#define CLICK_DURATION 20 // Click duration in ms

#define TACTILE_SWITCH_PIN 10 // Tactile switch pin

const bool isFeesDisplayEnabled = true;

int32_t blockHeight = 0;
int32_t bitcoinPrice = 0;

int32_t lastBlockHeight = 0;
int32_t i = 0;

void click(int period);
String getEndpointData(String url);
void writeTickTock();
void configureAccessPoint();
void animateClear();

WebSocketsClient coinbaseWebSocket;
WebSocketsClient mempoolWebSocket;

void printNumberCentreish(int number)
{
  // get number of digits in height
  int digits = floor(log10(abs(number))) + 1;
  // differeence
  int diff = 8 - digits;
  // half it and round down
  int halfDiff = floor(diff / 2);
  ld.printDigit(number, halfDiff);
}

void handleIncomingMessage(char *message)
{
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, message);

  const char *type = doc["type"];
  if (strcmp(type, "ticker") == 0)
  {
    const char *price = doc["price"];
    Serial.print("BTC-USD Price: ");
    Serial.println(price);

    if(displayData == DisplayData::PriceAndHeight || displayData == DisplayData::Price) {
      bitcoinPrice = atoi(price);
      ld.clear();
      // print from first LED
      printNumberCentreish(bitcoinPrice);
    }
  }
}

void parseBlockHeight(char *payload)
{
  click(100);
  // parse json
  StaticJsonDocument<1024> doc;
  deserializeJson(doc, (char *)payload);
  // get height
  int height = doc["block"]["height"];
  if (displayData == DisplayData::PriceAndHeight ||
      displayData == DisplayData::Price)
  {
    ld.clear();
    printNumberCentreish(bitcoinPrice);
    delay(3000);
  }
  ld.clear();
  printNumberCentreish(height);
  delay(3000);
}

int32_t fastestFee = 0;
int32_t halfHourFee = 0;
int32_t hourFee = 0;
int32_t economyFee = 0;
int32_t minimumFee = 0;

void parseFees(char *payload)
{
  StaticJsonDocument<1024> doc;
  deserializeJson(doc, (char *)payload);
  // get fees with serial logging
  fastestFee = doc["fees"]["fastestFee"];
  halfHourFee = doc["fees"]["halfHourFee"];
  hourFee = doc["fees"]["hourFee"];
  economyFee = doc["fees"]["economyFee"];
  minimumFee = doc["fees"]["minimumFee"];
  Serial.println("Fees");
  Serial.println(fastestFee);
  Serial.println(halfHourFee);
  Serial.println(hourFee);
  Serial.println(economyFee);
  Serial.println(minimumFee);
  // display fees
  if (displayData == DisplayData::MempoolFees)
  {
    ld.clear();
    // display economy fee, hour fee, fastest fee on the same screen with the dp on the right of each number, numbered 0 = far right, 8 = far left
    ld.write(8, B00000001); // .
    ld.printDigit(economyFee, 7);
    ld.write(6, B00000001); // .
    ld.printDigit(hourFee, 5);
    ld.write(4, B00000001); // .
    ld.printDigit(fastestFee, 3);

    delay(3000);
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

    mempoolWebSocket.sendTXT("{\"action\": \"want\", \"data\": [\"stats\", \"blocks\"]}");
    // tft show "connected"
    break;
  case WStype_TEXT:
    Serial.println("Received: ");
    Serial.println((char *)payload);
    // if payload includes the word ["block"]["height"] then parse it for the block height
    if (strstr((char *)payload, "block") && strstr((char *)payload, "height"))
    {
      parseBlockHeight((char *)payload);
    }
    // else if contains "mempoolInfo"
    else if (strstr((char *)payload, "mempoolInfo"))
    {
      parseFees((char *)payload);
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

void coinbaseWebSocketEvent(WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
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
    handleIncomingMessage((char *)payload);
    break;
  case WStype_BIN:
    Serial.printf("[WSc] get binary length: %u\n", length);
    break;
  }
}

void displayMempoolFees()
{
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
  ld.write(8, B1110110);  // N
  ld.write(7, B1111110);  // O
  ld.write(6, B1110110);  // N
  ld.write(5, B01001111); // E
  ld.printDigit(fee);
  delay(pagingDelay);

  // economy
  fee = doc["economyFee"];
  // economy
  animateClear();
  ld.clear();
  ld.write(8, B01001111); // E
  ld.write(7, B0001101);  // c
  ld.write(6, B0011101);  // o
  ld.printDigit(fee);
  delay(pagingDelay);

  // hour
  fee = doc["hourFee"];
  // hour
  animateClear();
  ld.clear();
  ld.write(8, B0110111);  // H
  ld.write(7, B0011101);  // o
  ld.write(6, B0011100);  // u
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

void displayBlockHeight()
{
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

void animateClear()
{
  uint16_t animDelay = 50;
  // for segment 8 to 1
  for (uint8_t i = 8; i >= 1; i--)
  {
    if (i <= 8)
    {
      ld.write(i + 1, B00000000);
    }
    ld.write(i, B0001000);
    delay(animDelay);
  }
  ld.clear();
  delay(animDelay);
}

void displayBitcoinPrice()
{
  // Get block height
  const String line = getEndpointData("https://api.coinbase.com/v2/prices/BTC-USD/buy");
  // data will look like this, get the amount value {"data":{"amount":"26602.105","base":"BTC","currency":"USD"}} using ArduinoJson
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, line);
  const char *amount = doc["data"]["amount"];

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
String getEndpointData(String url)
{
  if (WiFi.status() == WL_CONNECTED)
  {
    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode > 0)
    {
      String payload = http.getString();
      return payload;
    }
    else
    {
      Serial.println("Error in HTTP request");
    }
    http.end();
  }
  else
  {
    Serial.println("Not connected to WiFi");
  }
  return "";
}

void configModeCallback(WiFiManager *myWiFiManager)
{
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  Serial.println(myWiFiManager->getConfigPortalSSID());
  writeText("Config");
}

void initWiFi()
{
  // WiFiManager for auto-connecting to Wi-Fi
  WiFiManager wifiManager;
  wifiManager.setAPCallback(configModeCallback);
  // set timeout
  wifiManager.setConnectRetries(5);
  wifiManager.setConnectTimeout(10);
  wifiManager.autoConnect("AutoConnectAP", "ToTheMoon1");

  Serial.println("Connected to WiFi");
}

void scrollWord(String word)
{
  uint16_t animDelay = 50;
  for (uint8_t i = 8; i >= 1; i--)
  {
    if (i <= 8)
    {
      ld.write(i + 1, B00000000);
    }
    ld.write(i, B0001000);
    delay(animDelay);
  }
  ld.clear();
  delay(animDelay);
}

void writeBitcoin()
{
  ld.write(7, B01111111); // B
  ld.write(6, B00110000); // i
  ld.write(5, B00001111); // t
  ld.write(4, B01001110); // c
  ld.write(3, B01111110); // o
  ld.write(2, B00110000); // i
  ld.write(1, B01110110); // n
}

void writeTickTock()
{
  const uint16_t animDelay = 250;
  ld.clear();

  writeLetterAtPos(7, 'T');
  delay(50);
  writeLetterAtPos(6, 'i');
  delay(50);
  writeLetterAtPos(5, 'c');
  delay(animDelay);

  writeLetterAtPos(4, 't');
  delay(50);
  writeLetterAtPos(3, 'o');
  delay(50);
  writeLetterAtPos(2, 'c');
  delay(animDelay);

  // for(uint8_t i = 7; i >= 2; i--) {
  //   ld.write(i, B10000000);
  // }
  // delay(animDelay);
}

void showLoadingAnim()
{
  const uint8_t animDelay = 100;
  for (int8_t i = 0; i < 6; i++)
  {
    for (int8_t j = 0; j <= 8; j++)
    {
      switch (i)
      {
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

void setup()
{
  delay(2000);
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
  // writeBitcoin();
  animateClear();
  writeTickTock();

  // read state of tactile switch
  pinMode(TACTILE_SWITCH_PIN, INPUT_PULLUP);
  Serial.println("Tactile switch state");
  Serial.println(digitalRead(TACTILE_SWITCH_PIN));

  Serial.println("initing wifi");
  writeText("Init");
  initWiFi();
  // say connected to internet in 8 chars
  writeText("Lets go");
  Serial.println("wifi connected");

  // Setup coinbaseWebSocket
  coinbaseWebSocket.beginSSL("ws-feed.exchange.coinbase.com", 443, "/");
  coinbaseWebSocket.onEvent(coinbaseWebSocketEvent);
  coinbaseWebSocket.setReconnectInterval(5000);
  coinbaseWebSocket.enableHeartbeat(15000, 3000, 2);

  mempoolWebSocket.beginSSL("mempool.space", 443, "/api/v1/ws");
  mempoolWebSocket.onEvent(mempoolWebSocketEvent);
}

void loop()
{
  coinbaseWebSocket.loop();
  mempoolWebSocket.loop();

  // watch for button press, on each press, delay 300ms and set DisplayData to next enum value
  if (digitalRead(TACTILE_SWITCH_PIN) == LOW)
  {
    Serial.println("Button pressed");
    delay(300);
    // increment DisplayData
    i++;
    if (i > 3)
    {
      i = 0;
    }
    displayData = static_cast<DisplayData>(i);
    // show on the display what is being displayed
    switch (displayData)
    {
    case DisplayData::PriceAndHeight:
      writeText("USD-Height");
      break;
    case DisplayData::Price:
      writeText("Price");
      break;
    case DisplayData::BlockHeight:
      writeText("Height");
      break;
    case DisplayData::MempoolFees:
      writeText("Fees");
      break;
    }

  }

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