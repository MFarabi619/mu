#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include "esp_system.h"
#include "esp_heap_caps.h"
#include <ESP32Servo.h>

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

const char *ssid = "IO_GUEST_EVENT";
const char *password = "cyt5Z&Q&1i";

AsyncWebServer server(80);

Servo servo;
const int SERVO_PIN = 13;

enum class ServoMode { Idle, Set, Sweep };

ServoMode servoMode = ServoMode::Idle;
int currentAngle = 90;
int targetAngle = 90;
bool sweepDirUp = true;
unsigned long lastServoStepMs = 0;

static void handle_not_found(AsyncWebServerRequest *request) {
  digitalWrite(LED_BUILTIN, HIGH);
  String message = "404 — Nothing here\n\nURI: " + request->url();
  request->send(404, "text/plain; charset=utf-8", message);
  digitalWrite(LED_BUILTIN, LOW);
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.begin(115200);

  Serial.println("\n=== BOOT SEQUENCE ===");
  Serial.println("[Logger] Initializing...");

  Serial.println("\n=== HARDWARE BRING-UP SUMMARY ===");
  Serial.println("[Logger] OK");

  if (!SPIFFS.begin(true)) {
    Serial.println("[SPIFFS] ERROR: mount failed");
  } else {
    Serial.println("[SPIFFS] Mounted");
  }

  // servo init
  servo.attach(SERVO_PIN);
  currentAngle = 90;
  targetAngle = 90;
  servo.write(currentAngle);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[WiFi] Connected");
    Serial.print("[WiFi] IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(
        "[WiFi] ERROR: connect timeout. Check 2.4GHz/WPA2 and password.");
  }

  if (MDNS.begin("esp32")) {
    Serial.println("[mDNS] Responder started (esp32.local)");
  } else {
    Serial.println("[mDNS] ERROR: Failed to start responder");
  }

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    digitalWrite(LED_BUILTIN, HIGH);
    request->send(SPIFFS, "/index.html", "text/html; charset=utf-8");
    digitalWrite(LED_BUILTIN, LOW);
  });

  server.on("/on", HTTP_GET, [](AsyncWebServerRequest *req) {
    digitalWrite(LED_BUILTIN, HIGH);
    req->send(200, "text/plain", "LED ON");
  });

  server.on("/off", HTTP_GET, [](AsyncWebServerRequest *req) {
    digitalWrite(LED_BUILTIN, LOW);
    req->send(200, "text/plain", "LED OFF");
  });

  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *req) {
    int state = digitalRead(LED_BUILTIN) ? 1 : 0;
    String payload = String("{\"gpio_state\":") + state + "}";
    req->send(200, "application/json", payload);
  });

  server.on("/servo/left", HTTP_GET, [](AsyncWebServerRequest *req) {
    targetAngle = 30;
    servoMode = ServoMode::Set;
    req->send(200, "text/plain", "Servo LEFT");
  });

  server.on("/servo/right", HTTP_GET, [](AsyncWebServerRequest *req) {
    targetAngle = 150;
    servoMode = ServoMode::Set;
    req->send(200, "text/plain", "Servo RIGHT");
  });

  server.on("/servo/center", HTTP_GET, [](AsyncWebServerRequest *req) {
    targetAngle = 90;
    servoMode = ServoMode::Set;
    req->send(200, "text/plain", "Servo CENTER");
  });

  server.on("/servo/sweep", HTTP_GET, [](AsyncWebServerRequest *req) {
    servoMode = ServoMode::Sweep;
    req->send(200, "text/plain", "Servo SWEEP");
  });

  server.on("/servo/stop", HTTP_GET, [](AsyncWebServerRequest *req) {
    servoMode = ServoMode::Idle;
    req->send(200, "text/plain", "Servo STOP");
  });

  server.on("/servo/set", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (req->hasParam("value")) {
      String v = req->getParam("value")->value();
      int pos = v.toInt();
      if (pos < 0) pos = 0;
      if (pos > 180) pos = 180;
      targetAngle = pos;
      servoMode = ServoMode::Set;
      req->send(200, "text/plain", "Servo SET");
    } else {
      req->send(400, "text/plain", "Missing value");
    }
  });

  server.onNotFound(handle_not_found);
  server.begin();
}

void loop() {
  unsigned long now = millis();

  switch (servoMode) {
    case ServoMode::Set:
      servo.write(targetAngle);
      currentAngle = targetAngle;
      servoMode = ServoMode::Idle;
      break;

    case ServoMode::Sweep:
      if (now - lastServoStepMs >= 20) {
        lastServoStepMs = now;

        if (sweepDirUp) {
          currentAngle++;
          if (currentAngle >= 150) {
            currentAngle = 150;
            sweepDirUp = false;
          }
        } else {
          currentAngle--;
          if (currentAngle <= 30) {
            currentAngle = 30;
            sweepDirUp = true;
          }
        }

        servo.write(currentAngle);
      }
      break;

    case ServoMode::Idle:
    default:
      break;
  }
}
