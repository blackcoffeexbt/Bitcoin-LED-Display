#include <Arduino.h>
#include <ArduinoOTA.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiManager.h>
#include "DigitLedDisplay.h"
#include "display.h"

const char* firmwareVersion = "1.0.9";  // Current firmware version
const char* firmwareJsonUrl = "https://sx6.store/bitkoclock/firmware.json";

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

void updateFirmware(String firmwareUrl) {
  writeTextCentered("UPDATING");
  HTTPClient http;
  http.begin(firmwareUrl);
  int httpCode = http.GET();

  if (httpCode == 200) {
    int contentLength = http.getSize();
    bool canBegin = Update.begin(contentLength);

    if (canBegin) {
      WiFiClient *client = http.getStreamPtr();
      size_t written = Update.writeStream(*client);
      
      if (written == contentLength) {
        Serial.println("Written : " + String(written) + " successfully");
      } else {
        Serial.println("Write failed. Written only : " + String(written));
      }

      if (Update.end()) {
        if (Update.isFinished()) {
          writeTextCentered("UPDATED");
          delay(500);
          Serial.println("Update successfully completed. Rebooting...");
          ESP.restart();
        } else {
          Serial.println("Update not finished? Something went wrong!");
        }
      } else {
        Serial.println("Error Occurred. Error #: " + String(Update.getError()));
      }
    } else {
      Serial.println("Not enough space to begin OTA");
    }
  } else {
    Serial.println("Failed to fetch firmware");
  }

  http.end();
}

void parseJson(String json) {
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, json);

  const char* serverVersion = doc["version"];
  String firmwareUrl = doc["firmwareUrl"];

  if (String(firmwareVersion) != serverVersion) {
    updateFirmware(firmwareUrl);
  }
}

void checkForUpdates() {
  String payload = getEndpointData(firmwareJsonUrl);
  Serial.println("firmware");
  Serial.println(payload);
  parseJson(payload);
}

void printNumberCentreish(int number)
{
  ld.clear();
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

    if(
      (displayData == DisplayData::PriceAndHeight || displayData == DisplayData::Price)
      && bitcoinPrice != atoi(price)
      ) {
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
  lastBlockHeight = doc["block"]["height"];
  if (displayData == DisplayData::PriceAndHeight ||
      displayData == DisplayData::Price)
  {
    ld.clear();
    printNumberCentreish(lastBlockHeight);

    if(displayData == DisplayData::PriceAndHeight) {
      delay(3000);
    }
  }
}

int32_t fastestFee = 0;
int32_t halfHourFee = 0;
int32_t hourFee = 0;
int32_t economyFee = 0;
int32_t minimumFee = 0;

String lastFees = "";
void displayFees()
{
  Serial.println("displayFees called");
  String fees = String(economyFee) + "." + String(hourFee) + "." + String(fastestFee);
  if(fees != lastFees) {
    writeTextCentered(fees);
    lastFees = fees;
  }
}

void parseFees(char *payload)
{
  StaticJsonDocument<2000> doc;
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
    displayFees();
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
    // Serial.printf("[WSc] get text: %s\n", payload);
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

void setBlockHeight()
{
  const String line = getEndpointData("https://mempool.space/api/blocks/tip/height");
  lastBlockHeight = line.toInt();
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

void getBitcoinPrice()
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
  writeTextCentered("Config");
}

void initWiFi()
{
  // WiFiManager for auto-connecting to Wi-Fi
  WiFiManager wifiManager;
  wifiManager.setAPCallback(configModeCallback);
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

/**
 * @brief Display the current data name and then the data
 * 
 * @param displayData 
 */
void showCurrentData(DisplayData displayData) {
  switch (displayData)
    {
    case DisplayData::PriceAndHeight:
      Serial.println("PriceAndHeight");
      writeTextCentered("USDHeight");
      delay(1000);
      // show height, then price ticker
      printNumberCentreish(lastBlockHeight);
      delay(1000);
      printNumberCentreish(bitcoinPrice);
      break;
    case DisplayData::Price:
      Serial.println("Price");
      writeTextCentered("Price");
      delay(1000);
      printNumberCentreish(bitcoinPrice);
      break;
    case DisplayData::BlockHeight:
      writeTextCentered("HEIGHT");
      Serial.println("BlockHeight");
      delay(1000);
      printNumberCentreish(lastBlockHeight);
      break;
    case DisplayData::MempoolFees:
      Serial.println("MempoolFees");
      writeTextCentered("FEES");
      delay(1000);
      displayFees();
      break;
    default:
      writeTextCentered("---");
      delay(1000);
      Serial.println("default");
      Serial.println(i);
      break;
    }
  }

void setup()
{
  delay(1000);
  Serial.begin(115200);
  Serial.println("Boot");

  ld.setBright(0);
  ld.setDigitLimit(8);
  writeTextCentered(String(firmwareVersion));
  click(225);
  delay(500);

  pinMode(BUZZER_PIN, OUTPUT); // Set the buzzer pin as an output.

  animateClear();
  writeTickTock();

  pinMode(TACTILE_SWITCH_PIN, INPUT_PULLUP);

  Serial.println("initing wifi");
  writeTextCentered("INIT NET");
  initWiFi();

  writeTextCentered("HAS NET");
  Serial.println("wifi connected");

  checkForUpdates();

  setBlockHeight();
  getBitcoinPrice();
  showCurrentData(displayData);

  coinbaseWebSocket.beginSSL("ws-feed.exchange.coinbase.com", 443, "/");
  coinbaseWebSocket.onEvent(coinbaseWebSocketEvent);
  coinbaseWebSocket.setReconnectInterval(5000);
  coinbaseWebSocket.enableHeartbeat(15000, 3000, 2);

  mempoolWebSocket.beginSSL("mempool.space", 443, "/api/v1/ws");
  mempoolWebSocket.onEvent(mempoolWebSocketEvent);
  mempoolWebSocket.setReconnectInterval(5000);
  mempoolWebSocket.enableHeartbeat(15000, 3000, 2);
}

void loop()
{
  coinbaseWebSocket.loop();
  mempoolWebSocket.loop();

  // watch for button press, on each press, delay 300ms and set DisplayData to next enum value
  if (digitalRead(TACTILE_SWITCH_PIN) == LOW)
  {
    i++;

    if (i > 3)
    {
      i = 0;
    }
    displayData = static_cast<DisplayData>(i);
    showCurrentData(displayData);
  }
}