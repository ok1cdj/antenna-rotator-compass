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
#include <ESPAsyncWebServer.h>
#include <SPIFFSEditor.h>
// Servo - for future purposes
// #include <ESP32Servo.h>
// Event Scheduler
#include "Tasker.h"

Tasker tasker;
QMC5883LCompass compass;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
AsyncEventSource events("/events");
// Servo servo; // TODO: servo

const int MagnetometerSteps = 10;
const bool MagnetometerAdvancedSmoothing = true;

int x = 0, y = 0, z = 0, azimuth = 0, bearing = 0; // these are the results from the compass
char direction[3];                                 // i.e. NNW
int angle = 0;                                     // used for controlling the rotator from the frontend app
// const int servoPin = 13; // TODO: servo

void setupCompass(int steps, bool advanced)
{
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
  compass.setSmoothing(steps, advanced);
}

void getCompassData()
{
  // Read compass values
  compass.read();

  // Return XYZ readings
  // x = compass.getX();
  // y = compass.getY();
  // z = compass.getZ();

  // Get Azimuth, Bearing, Direction
  azimuth = compass.getAzimuth();
  bearing = compass.getBearing(azimuth);
  compass.getDirection(direction, azimuth);
}

void printCompassData()
{
  if (!azimuth && !bearing)
  {
    Serial.println();
    Serial.println("###############################");
    Serial.println("#        NO COMPASS DATA      #");
    Serial.println("###############################");
    Serial.println();
    return;
  }

  Serial.println("#################################");
  Serial.print("Azimuth: ");
  Serial.print(azimuth);
  Serial.print(" Bearing: ");
  Serial.print(bearing);
  Serial.print(" Direction: ");
  Serial.print(direction[0]);
  Serial.print(direction[1]);
  Serial.println(direction[2]);
}

// TODO: Maybe in the future use JSON
// char* getJsonResponse() { // TODO: Later we will use this istead of just passing one number (Azimuth)
//   String json = String("{\"x\":\"") + String(x) + String("\",") +
//   String("\"y\":\"") + String(y) + String("\",") +
//   String("\"z\":\"") + String(z) + String("\",") +
//   String("\"az\":\"") + String(azimuth) + String("\",") +
//   String("\"be\":\"") + String(bearing) + String("\",") +
//   String("\"dir\":\"") + String(direction[0]) + String(direction[1]) + String(direction[2]) + String("\"") +
//   String("}");

//   return (char*)json.c_str();
// }

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  if (type == WS_EVT_CONNECT)
  {
    Serial.printf("ws[%s][%u] connect\n", server->url(), client->id());
    client->printf("Hello Client %u :)", client->id());
    client->ping();
  }
  else if (type == WS_EVT_DISCONNECT)
  {
    Serial.printf("ws[%s][%u] disconnect\n", server->url(), client->id());
  }
  else if (type == WS_EVT_ERROR)
  {
    Serial.printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t *)arg), (char *)data);
  }
  else if (type == WS_EVT_PONG)
  {
    Serial.printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len) ? (char *)data : "");
  }
  else if (type == WS_EVT_DATA)
  {
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    String msg = "";
    if (info->final && info->index == 0 && info->len == len)
    {
      //the whole message is in a single frame and we got all of it's data
      Serial.printf("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT) ? "text" : "binary", info->len);

      if (info->opcode == WS_TEXT)
      {
        for (size_t i = 0; i < info->len; i++)
        {
          msg += (char)data[i];
        }
      }
      else
      {
        char buff[3];
        for (size_t i = 0; i < info->len; i++)
        {
          sprintf(buff, "%02x ", (uint8_t)data[i]);
          msg += buff;
        }
      }
      Serial.printf("%s\n", msg.c_str());

      if (msg == "GET")
      {
        //TODO: For now, let's suppose that we only have the angle from the compass and not from the frontend rotator control app
        //angle = azimuth;

        if (info->opcode == WS_TEXT)
          client->text(String(azimuth));
        else
          client->binary(String(azimuth));
      }
      else
      {
        /*
          CAUTION!
          This is very experimental and currently not supported by the frontend JS
          However, you can modify and use it as you like ;)
        */

        //angle = atoi(msg.c_str()); // get the angle (azimuth) where the rotator control motor should steer the antenna
        //Serial.println("New angle: " + String(angle));
        // now you can play with motors control as you like
      }
    }
    else
    {
      //message is comprised of multiple frames or the frame is split into multiple packets
      if (info->index == 0)
      {
        if (info->num == 0)
          Serial.printf("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
        Serial.printf("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
      }

      Serial.printf("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT) ? "text" : "binary", info->index, info->index + len);

      if (info->opcode == WS_TEXT)
      {
        for (size_t i = 0; i < len; i++)
        {
          msg += (char)data[i];
        }
      }
      else
      {
        char buff[3];
        for (size_t i = 0; i < len; i++)
        {
          sprintf(buff, "%02x ", (uint8_t)data[i]);
          msg += buff;
        }
      }
      Serial.printf("%s\n", msg.c_str());

      if (msg == "GET")
      {
        if (info->opcode == WS_TEXT)
          client->text(String(azimuth));
        else
          client->binary(String(azimuth));
      }
      else
      {
        // TODO: experimental as well, see upper comments
        angle = atoi(msg.c_str());
        Serial.println("New angle: " + String(angle));
      }

      if ((info->index + len) == info->len)
      {
        Serial.printf("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
        if (info->final)
        {
          Serial.printf("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
        }
      }
    }
  }
}

const char *ssid = "COMPASS";
const char *password = "";
const char *hostName = "compass";
const char *http_username = "admin";
const char *http_password = "admin";

void setup()
{
  Serial.begin(115200);
  Serial.setDebugOutput(true);

  setupCompass(MagnetometerSteps, MagnetometerAdvancedSmoothing);

  WiFi.softAP(ssid);

  Serial.println();
  Serial.print("IP address: ");
  Serial.println(WiFi.softAPIP());

  // TODO: servo
  //servo.setPeriodHertz(50);
  //servo.attach(servoPin, 1000, 2000);

  // Task for getting the values of compass
  tasker.setInterval(getCompassData, 50);
  tasker.setInterval(printCompassData, 300);

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
  MDNS.addService("https", "tcp", 443);

  SPIFFS.begin();

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  events.onConnect([](AsyncEventSourceClient *client) {
    client->send("hello!", NULL, millis(), 1000);
  });

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
