/*
    Code by: Peter Javorsky OM7TEK
    Mail: tekk.sk@gmail.com
    Date: 19.5.2021
    License: GPL
*/

// HMC5883
#include <QMC5883LCompass.h>
// Over the air upgrades
#include <ArduinoOTA.h>
// ESP32 basic functionality
#ifdef ESP32
#include <FS.h>
#include <SPIFFS.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#elif defined(ESP8266) // backward compatibility with ESP8266
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESP8266mDNS.h>
#endif
// Async Webserver
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFSEditor.h>
// Servo - for future purposes
// #include <ESP32Servo.h>
// Event Scheduler
#include "Tasker.h"

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
AsyncEventSource events("/events");

Tasker tasker;
QMC5883LCompass compass;
// Servo servo; // TODO: servo

const char *ssid = "COMPASS";
const char *password = "";
const char *hostName = "compass";
const char *http_username = "admin";
const char *http_password = "admin";
const int MAGNETOMETER_STEPS = 10;
const bool MAGNETOMETER_ADVANCED_SMOOTHING = true;

const int frequency = 5;

int x = 0, y = 0, z = 0, azimuth = 0, bearing = 0; // these are the results from the compass
char direction[3];                                 // i.e. NNW
int angle = 0;                                     // used for controlling the rotator from the frontend app
bool ledState = false;
// const int servoPin = 13; // TODO: servo

void getCompassData()
{
  compass.read();
  // Get Azimuth, Bearing, Direction
  azimuth = compass.getAzimuth();
  bearing = compass.getBearing(azimuth);
  compass.getDirection(direction, azimuth);
}

void printCompassData()
{
  if (!azimuth && !bearing)
  {
    Serial.println("# NO COMPASS DATA #");
    Serial.println();
    return;
  }

  Serial.print("Azimuth: ");
  Serial.print(azimuth);
  Serial.print(" Bearing: ");
  Serial.print(bearing);
  Serial.print(" Direction: ");
  Serial.print(direction[0]);
  Serial.print(direction[1]);
  Serial.println(direction[2]);
}

void sendData()
{
  ws.textAll(String(azimuth));
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
  {
    data[len] = 0;
    if (strcmp((char *)data, "toggle") == 0)
    {
      ledState = !ledState;
      sendData();
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, 
            void *arg, uint8_t *data, size_t len)
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

void setup()
{
  Serial.begin(115200);
  Serial.setDebugOutput(true);

  compass.init();
  /*
   *   call setSmoothing(STEPS, ADVANCED);
   *   
   *   STEPS     = int   The number of steps to smooth the results by. Valid 1 to 10.
   *                     Higher steps equals more smoothing but longer process time.
   *                     
   *   ADVANCED  = bool  Turn advanced smmothing on or off. True will remove the max and min values from each step and then process as normal.
   *                     Turning this feature on will results in even more smoothing but will take longer to process.
   *                     
   */
  compass.setSmoothing(MAGNETOMETER_STEPS, MAGNETOMETER_ADVANCED_SMOOTHING);

  WiFi.softAP(ssid);

  Serial.println();
  Serial.print("IP address: ");
  Serial.println(WiFi.softAPIP());

  // TODO: servo
  //servo.setPeriodHertz(50);
  //servo.attach(servoPin, 1000, 2000);

  // Task for getting the values of compass
  tasker.setInterval(getCompassData, frequency);
  tasker.setInterval(sendData, frequency);
  tasker.setInterval(printCompassData, 3000);

  //Send OTA events to the browser
  ArduinoOTA.onStart([]() { events.send("Update Start", "ota"); });
  ArduinoOTA.onEnd([]() { events.send("Update End", "ota"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    char p[32];
    sprintf(p, "Progress: %u%%\n", (progress / (total / 100)));
    events.send(p, "ota");
  });

  ArduinoOTA.onError([](ota_error_t error) {
    if (error == OTA_AUTH_ERROR)
      events.send("Auth Failed", "ota");
    else if (error == OTA_BEGIN_ERROR)
      events.send("Begin Failed", "ota");
    else if (error == OTA_CONNECT_ERROR)
      events.send("Connect Failed", "ota");
    else if (error == OTA_RECEIVE_ERROR)
      events.send("Recieve Failed", "ota");
    else if (error == OTA_END_ERROR)
      events.send("End Failed", "ota");
  });

  ArduinoOTA.setHostname(hostName);
  ArduinoOTA.begin();

  MDNS.addService("http", "tcp", 80);

  SPIFFS.begin();

  ws.onEvent(onEvent);
  server.addHandler(&ws);
  server.addHandler(&events);

#ifdef ESP32
  server.addHandler(new SPIFFSEditor(SPIFFS, http_username, http_password));
#elif defined(ESP8266)
  server.addHandler(new SPIFFSEditor(http_username, http_password));
#endif

  server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });

  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

  server.onNotFound([](AsyncWebServerRequest *request) {
    Serial.printf("NOT_FOUND: ");
    if (request->method() == HTTP_GET)
      Serial.printf("GET");
    else if (request->method() == HTTP_POST)
      Serial.printf("POST");
    else if (request->method() == HTTP_DELETE)
      Serial.printf("DELETE");
    else if (request->method() == HTTP_PUT)
      Serial.printf("PUT");
    else if (request->method() == HTTP_PATCH)
      Serial.printf("PATCH");
    else if (request->method() == HTTP_HEAD)
      Serial.printf("HEAD");
    else if (request->method() == HTTP_OPTIONS)
      Serial.printf("OPTIONS");
    else
      Serial.printf("UNKNOWN");
    Serial.printf(" http://%s%s\n", request->host().c_str(), request->url().c_str());

    if (request->contentLength())
    {
      Serial.printf("_CONTENT_TYPE: %s\n", request->contentType().c_str());
      Serial.printf("_CONTENT_LENGTH: %u\n", request->contentLength());
    }

    int headers = request->headers();
    int i;
    for (i = 0; i < headers; i++)
    {
      AsyncWebHeader *h = request->getHeader(i);
      Serial.printf("_HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
    }

    int params = request->params();
    for (i = 0; i < params; i++)
    {
      AsyncWebParameter *p = request->getParam(i);
      if (p->isFile())
      {
        Serial.printf("_FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
      }
      else if (p->isPost())
      {
        Serial.printf("_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
      }
      else
      {
        Serial.printf("_GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
      }
    }

    request->send(404);
  });

  server.onFileUpload([](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!index)
      Serial.printf("UploadStart: %s\n", filename.c_str());
    Serial.printf("%s", (const char *)data);
    if (final)
      Serial.printf("UploadEnd: %s (%u)\n", filename.c_str(), index + len);
  });

  server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!index)
      Serial.printf("BodyStart: %u\n", total);
    Serial.printf("%s", (const char *)data);
    if (index + len == total)
      Serial.printf("BodyEnd: %u\n", total);
  });

  server.begin();
}

void loop()
{
  ArduinoOTA.handle();
  tasker.loop();
  ws.cleanupClients();
  //servo.write(angle); // TODO: steering of the rotator, probably not with servo :D
}
