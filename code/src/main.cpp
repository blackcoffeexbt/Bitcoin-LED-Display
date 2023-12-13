#include <Arduino.h>
#include <ArduinoOTA.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiManager.h>
#include "DigitLedDisplay.h"
#include "display.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

const char* firmwareVersion = "0.0.6";  // Current firmware version
const char* firmwareJsonUrl = "https://sx6.store/bitkoclock/firmware.json";

String textToWrite = "";
extern int textPos;

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
int32_t lastBitcoinPrice = 0;

int32_t lastBlockHeight = 0;
int32_t i = 0;

void click(int period);
String getEndpointData(String url);
void writeTickTock();
void configureAccessPoint();
void animateClear();

WebSocketsClient coinbaseWebSocket;
WebSocketsClient mempoolWebSocket;

// task for writeText
void writeTextTask(void *pvParameters) {
    // Cast pvParameters to the appropriate type if needed
    // For example, if pvParameters is a String:
    // String text = *((String*) pvParameters);

    // Infinite loop to continuously update the display
    for (;;) {
        writeText(textToWrite);
        vTaskDelay(pdMS_TO_TICKS(100)); // Delay for 100ms
    }
}

void updateFirmware(String firmwareUrl) {
  textToWrite = "Updating...";
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
          textToWrite = "Update done";
          delay(2000);
          Serial.println("Update successfully completed. Rebooting...");
          textToWrite = "Rebooting";
          delay(1500);
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

void parseFirmwareJson(String json) {
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
  parseFirmwareJson(payload);
}

void handleIncomingMessage(char *message)
{
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, message);

  const char *type = doc["type"];
  if (strcmp(type, "ticker") == 0)
  {
    const char *currentBitcoinPrice = doc["price"];
    Serial.print("BTC-USD Price: ");
    Serial.println(currentBitcoinPrice);

    if(
      (displayData == DisplayData::PriceAndHeight || displayData == DisplayData::Price)
      && lastBitcoinPrice != atoi(currentBitcoinPrice)
      ) {
      lastBitcoinPrice = atoi(currentBitcoinPrice);
      textToWrite = String(lastBitcoinPrice);
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
    textToWrite = String(lastBlockHeight);

    if(displayData == DisplayData::PriceAndHeight) {
      delay(5000);
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
  String fees = String(halfHourFee) + " - " + String(hourFee) + " - " + String(fastestFee);
  if(fees != lastFees) {
    textToWrite = fees;
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
  Serial.println("Fees: Fastest: " + String(fastestFee) + " Half Hour: " + String(halfHourFee) + " Hour: " + String(hourFee) + " Economy: " + String(economyFee) + " Minimum: " + String(minimumFee));
  
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
    // Serial.println("Received: ");
    // Serial.println((char *)payload);
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

  lastBitcoinPrice = atoi(amount);
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
  textToWrite = "Config";
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
  Serial.println("Click!");
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
      textToWrite = "Price - Height";
      delay(3500);
      // show height, then price ticker
      textToWrite = String(lastBlockHeight);
      delay(2000);
      textToWrite = String(lastBitcoinPrice);
      break;
    case DisplayData::Price:
      Serial.println("Price");
      textToWrite = "Price";
      delay(1000);
      textToWrite = String(lastBitcoinPrice);
      break;
    case DisplayData::BlockHeight:
      textToWrite = "Height";
      Serial.println("BlockHeight");
      delay(1000);
      textToWrite = String(lastBlockHeight);
      break;
    case DisplayData::MempoolFees:
      Serial.println("MempoolFees");
      textToWrite = "FEES";
      delay(1000);
      displayFees();
      break;
    default:
      textToWrite = "---";
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

    // set up the tasks
  xTaskCreate(
    writeTextTask, // Function that should be called
    "writeTextTask", // Name of the task (for debugging)
    10000, // Stack size (bytes)
    NULL, // Parameter to pass
    1, // Task priority
    NULL // Task handle
  );
  // task loop

  ld.setBright(0);
  ld.setDigitLimit(8);

  pinMode(BUZZER_PIN, OUTPUT); // Set the buzzer pin as an output.
  click(225);
  delay(250);

  animateClear();
  textToWrite = String(firmwareVersion);

  pinMode(TACTILE_SWITCH_PIN, INPUT_PULLUP);

  Serial.println("initing wifi");
  textToWrite = "Connecting";
  initWiFi();

  textToWrite = "Connected";
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