/*********
  Rui Santos
  Complete instructions at https:///RandomNerdTutorials.com/esp32-wi-fi-manager-asyncwebserver/

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*********/

/**
 * @file main.cpp
 * @brief Main entry point for the application.
 */

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

/// SPIFFS (SPI Flash File System) allows you to access the flash memory like you would do in a normal filesystem in your computer.
#include "SPIFFS.h"

/// Libraries for SD card
#include "FS.h"
#include "SD.h"
#include <SPI.h>

/// JSON Library
#include <Arduino_JSON.h>

/// DS18B20 libraries
#include <OneWire.h>
#include <DallasTemperature.h>

/// Libraries to get time from NTP Server
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

/// prototypes
void writeFile(fs::FS &fs, const char *path, const char *message);
void getTimeStamp();
void logSDCard();
void appendFile(fs::FS &fs, const char *path, const char *message);
void handleDownload(AsyncWebServerRequest *request);
String getSensorReadings();

/// Define CS pin for the SD card module
#define SD_CS 5

/// Create a WebSocket object
AsyncWebSocket ws("/ws");

/// Save reading number on RTC memory
RTC_DATA_ATTR int readingID = 0;

/// Notify clients about new sensor readings
void notifyClients(String sensorReadings)
{
  ws.textAll(sensorReadings);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
  {
    String sensorReadings = getSensorReadings();
    notifyClients(sensorReadings);
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  switch (type)
  {
  case WS_EVT_CONNECT:
    Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    break;
  case WS_EVT_DISCONNECT:
    Serial.printf("WebSocket client #%u disconnected\n", client->id());
    break;
  case WS_EVT_DATA:
    handleWebSocketMessage(arg, data, len);
    break;
  case WS_EVT_PONG:
  case WS_EVT_ERROR:
    break;
  }
}

String dataMessage;

/// Temperature Sensor variables
float temperature;
bool initWifi = false;

/// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

/// Variables to save date and time
String formattedDate;
String dayStamp;
String timeStamp;

/**
 * @brief initializes the NTPClient
 *
 */
void initializeTimeClient()
{
  /// Initialize a NTPClient to get time data
  timeClient.begin();

  /// GMT +1 = 3600
  timeClient.setTimeOffset(3600);
}

const char *filename = "/data.csv";

/**
 * @brief initializes the SD card
 *
 */
void initializeSDCard()
{
  /// Initialize SD card
  SD.begin(SD_CS);
  if (!SD.begin(SD_CS))
  {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE)
  {
    Serial.println("No SD card attached");
    return;
  }
  Serial.println("Initializing SD card...");
  if (!SD.begin(SD_CS))
  {
    Serial.println("ERROR - SD card initialization failed!");
    return; /// init failed
  }

  /// If the data.txt file doesn't exist
  /// Create a file on the SD card and write the data labels
  File file = SD.open(filename);
  if (!file)
  {
    Serial.println("File doens't exist");
    Serial.println("Creating file...");
    writeFile(SD, filename, "Date,HH:mm:ss,Temperature in Celsius \r\n");
  }
  else
  {
    Serial.println("File already exists");
  }
  file.close();
}

/**
 * @brief handles the download of the data.csv file
 *
 * @param request
 */
void handleDownload(AsyncWebServerRequest *request)
{
  /// Handle the file download
  File file = SD.open(filename);

  Serial.println("Initializing download of file...");
  if (file)
  {
    Serial.println("File exists");
    AsyncWebServerResponse *response = request->beginResponse(SD, filename, "text/csv", false);

    request->send(response);
    file.close();
  }
  else
  {
    Serial.println("File does not exists");
    request->send(404, "text/plain", "File not found");
  }
}

/**
 * @brief sets the date and time variables from the NTPClient for the data.csv file
 *
 */
void getTimeStamp()
{
  while (!timeClient.update())
  {
    timeClient.forceUpdate();
  }
  /// The formattedDate comes with the following format:
  /// 2018-05-28T16:00:13Z
  formattedDate = timeClient.getFormattedDate();
  Serial.println(formattedDate);

  /// Extract date
  int splitT = formattedDate.indexOf("T");
  dayStamp = formattedDate.substring(0, splitT);
  Serial.println(dayStamp);

  /// Extract time
  timeStamp = formattedDate.substring(splitT + 1, formattedDate.length() - 1);
  Serial.println(timeStamp);
}

/**
 * @brief logs the data to the data.csv file on the SD card
 *
 */
void logSDCard()
{
  dataMessage = String(dayStamp) + "," + String(timeStamp) + "," +
                String(temperature) + "\r\n";
  Serial.print("Save data: ");
  Serial.println(dataMessage);
  appendFile(SD, filename, dataMessage.c_str());
}

/**
 * @brief appends a file
 *
 * @param fs file system
 * @param path
 * @param message
 */
void appendFile(fs::FS &fs, const char *path, const char *message)
{
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file)
  {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (file.print(message))
  {
    Serial.println("Message appended");
  }
  else
  {
    Serial.println("Append failed");
  }
  file.close();
}

/**
 * @brief initializes the web server
 *
 */
AsyncWebServer server(80);

/**
 * @brief initializes the web socket
 *
 */
void initWebSocket()
{
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

/// Search for parameter in HTTP POST request
const char *PARAM_INPUT_1 = "ssid";
const char *PARAM_INPUT_2 = "pass";
const char *PARAM_INPUT_3 = "ip";
const char *PARAM_INPUT_4 = "gateway";

/// Variables to save values from HTML form
String ssid;
String pass;
String ip;
String gateway;

/// File paths to save input values permanently
const char *ssidPath = "/ssid.txt";
const char *passPath = "/pass.txt";
const char *ipPath = "/ip.txt";
const char *gatewayPath = "/gateway.txt";

IPAddress localIP;

IPAddress localGateway;

IPAddress subnet(255, 255, 0, 0);

/// Timer variables
unsigned long previousMillis = 0;
const long interval = 10000; /// interval to wait for Wi-Fi connection (milliseconds)

DeviceAddress temperatureSensor = {0x28, 0xFF, 0x64, 0x1E, 0x30, 0x7B, 0xE2, 0x75};

/// Create an Event Source on /events
AsyncEventSource events("/events");

/// Json Variable to Hold Sensor Readings
JSONVar readings;

/// Timer variables
unsigned long lastTime = 0;
unsigned long timerDelay = 10000;

/// GPIO4 where the DS18B20 sensors are connected to
const int oneWireBus = 4;

/// Setup a oneWire instance to communicate with OneWire devices (DS18B20)
OneWire oneWire(oneWireBus);

/// Pass our oneWire reference to Dallas Temperature sensor
DallasTemperature sensors(&oneWire);

/**
 * @brief gets the sensor readings
 *
 * @return String with the sensor readings in json format
 */
String getSensorReadings()
{
  sensors.requestTemperatures();
  temperature = sensors.getTempC(temperatureSensor);
  readings["sensor1"] = String(temperature);

  String jsonString = JSON.stringify(readings);
  return jsonString;
}

/**
 * @brief initializes the SPIFFS
 *
 */
void initSPIFFS()
{
  if (!SPIFFS.begin(true))
  {
    Serial.println("An error has occurred while mounting SPIFFS");
  }
  Serial.println("SPIFFS mounted successfully");
}

/**
 * @brief reads a file from SPIFFS
 *
 * @param fs file system
 * @param path
 * @return String
 */
String readFile(fs::FS &fs, const char *path)
{
  Serial.printf("Reading file: %s\r\n", path);

  File file = fs.open(path);
  if (!file || file.isDirectory())
  {
    Serial.println("- failed to open file for reading");
    return String();
  }

  String fileContent;
  while (file.available())
  {
    fileContent = file.readStringUntil('\n');
    break;
  }
  return fileContent;
}

/**
 * @brief writes a file to SPIFFS
 *
 * @param fs file system
 * @param path
 * @param message
 */
void writeFile(fs::FS &fs, const char *path, const char *message)
{
  Serial.printf("Writing file: %s\r\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file)
  {
    Serial.println("- failed to open file for writing");
    return;
  }
  if (file.print(message))
  {
    Serial.println("- file written");
  }
  else
  {
    Serial.println("- write failed");
  }
}

/**
 * @brief initializes the Wi-Fi connection
 *
 * @return true if Wi-Fi connection is successful
 * @return false if Wi-Fi connection is unsuccessful
 */
bool initWiFi()
{
  if (ssid == "" || ip == "")
  {
    Serial.println("Undefined SSID or IP address.");
    return false;
  }

  WiFi.mode(WIFI_STA);
  localIP.fromString(ip.c_str());
  localGateway.fromString(gateway.c_str());

  if (!WiFi.config(localIP, localGateway, subnet, IPAddress(8, 8, 8, 8)))
  {
    Serial.println("STA Failed to configure");
    return false;
  }
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.println("Connecting to WiFi...");

  unsigned long currentMillis = millis();
  previousMillis = currentMillis;

  while (WiFi.status() != WL_CONNECTED)
  {
    currentMillis = millis();
    if (currentMillis - previousMillis >= interval)
    {
      Serial.println("Failed to connect.");
      return false;
    }
  }

  Serial.println(WiFi.localIP());
  return true;
}

void setup()
{
  /// Serial port for debugging purposes
  Serial.begin(115200);

  initSPIFFS();

  /// Load values saved in SPIFFS
  ssid = readFile(SPIFFS, ssidPath);
  pass = readFile(SPIFFS, passPath);
  ip = readFile(SPIFFS, ipPath);
  gateway = readFile(SPIFFS, gatewayPath);
  Serial.println(ssid);
  Serial.println(pass);
  Serial.println(ip);
  Serial.println(gateway);

  initWifi = initWiFi();
  if (initWifi)
  {
    /// Web Server Root URL
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/index.html", "text/html"); });

    server.serveStatic("/", SPIFFS, "/");

    server.on("/download", HTTP_GET, handleDownload);

    /// Start server
    server.begin();

    initializeTimeClient();
    initializeSDCard();

    /// Start DS18B20 sensors
    sensors.begin();

    initWebSocket();
  }
  else
  {
    /// Connect to Wi-Fi network with SSID and password
    Serial.println("Setting AP (Access Point)");
    /// NULL sets an open Access Point
    WiFi.softAP("ESP-WIFI-MANAGER-LASSE-JON", NULL);

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    /// Web Server Root URL
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/wifimanager.html", "text/html"); });

    server.serveStatic("/", SPIFFS, "/");

    server.on("/", HTTP_POST, [](AsyncWebServerRequest *request)
              {
      int params = request->params();
      for(int i=0;i<params;i++){
        AsyncWebParameter* p = request->getParam(i);
        if(p->isPost()){
          /// HTTP POST ssid value
          if (p->name() == PARAM_INPUT_1) {
            ssid = p->value().c_str();
            Serial.print("SSID set to: ");
            Serial.println(ssid);
            /// Write file to save value
            writeFile(SPIFFS, ssidPath, ssid.c_str());
          }
          /// HTTP POST pass value
          if (p->name() == PARAM_INPUT_2) {
            pass = p->value().c_str();
            Serial.print("Password set to: ");
            Serial.println(pass);
            /// Write file to save value
            writeFile(SPIFFS, passPath, pass.c_str());
          }
          /// HTTP POST ip value
          if (p->name() == PARAM_INPUT_3) {
            ip = p->value().c_str();
            Serial.print("IP Address set to: ");
            Serial.println(ip);
            /// Write file to save value
            writeFile(SPIFFS, ipPath, ip.c_str());
          }
          /// HTTP POST gateway value
          if (p->name() == PARAM_INPUT_4) {
            gateway = p->value().c_str();
            Serial.print("Gateway set to: ");
            Serial.println(gateway);
            /// Write file to save value
            writeFile(SPIFFS, gatewayPath, gateway.c_str());
          }
        }
      }
      request->send(200, "text/plain", "Done. ESP will restart, connect to your router and go to IP address: " + ip);
      delay(3000);
      ESP.restart(); });
    server.begin();
  }
}

void loop()
{
  if (initWifi && (millis() - lastTime) > timerDelay)
  {
    String sensorReadings = getSensorReadings();

    lastTime = millis();

    Serial.print("Sending Sensor Readings: ");
    Serial.println(sensorReadings);

    notifyClients(sensorReadings);

    getTimeStamp();
    logSDCard();
  }
}