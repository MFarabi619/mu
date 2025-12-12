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
  Serial.print("[HTTP] 404 Not Found: ");
  Serial.println(request->url());
  request->send(404, "text/plain; charset=utf-8", message);
  digitalWrite(LED_BUILTIN, LOW);
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("=== BOOT SEQUENCE ===");
  Serial.println("[Logger] Initializing...");

  Serial.println("\n=== HARDWARE BRING-UP SUMMARY ===");
  Serial.println("[Logger] OK");

  Serial.println("[SPIFFS] Mounting...");
  if (!SPIFFS.begin(true)) {
    Serial.println("[SPIFFS] ERROR: mount failed");
  } else {
    Serial.println("[SPIFFS] Mounted OK");
  }

  Serial.print("[Servo] Attaching to pin ");
  Serial.println(SERVO_PIN);
  int ch = servo.attach(SERVO_PIN);  // ESP32Servo returns channel
  Serial.print("[Servo] attach() returned channel: ");
  Serial.println(ch);
  currentAngle = 90;
  targetAngle = 90;
  servo.write(currentAngle);
  Serial.print("[Servo] Initial angle set to ");
  Serial.println(currentAngle);

  WiFi.mode(WIFI_STA);
  Serial.print("[WiFi] Connecting to SSID: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  Serial.print("[WiFi] Connecting");
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
    Serial.println("[WiFi] ERROR: connect timeout. Check 2.4GHz/WPA2 and password.");
  }

  Serial.print("[mDNS] Starting responder as 'esp32.local'... ");
  if (MDNS.begin("esp32")) {
    Serial.println("OK");
  } else {
    Serial.println("ERROR");
  }

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("[HTTP] GET / -> /index.html");
    digitalWrite(LED_BUILTIN, HIGH);
    if (SPIFFS.exists("/index.html")) {
      Serial.println("[SPIFFS] /index.html found, sending");
      request->send(SPIFFS, "/index.html", "text/html; charset=utf-8");
    } else {
      Serial.println("[SPIFFS] ERROR: /index.html not found");
      request->send(500, "text/plain", "index.html not found");
    }
    digitalWrite(LED_BUILTIN, LOW);
  });

  server.on("/on", HTTP_GET, [](AsyncWebServerRequest *req) {
    Serial.println("[HTTP] GET /on");
    digitalWrite(LED_BUILTIN, HIGH);
    servoMode = ServoMode::Sweep;
    Serial.println("[Servo] Sweep mode enabled");
    req->send(200, "text/plain", "Servo SWEEP");
    // req->send(200, "text/plain", "LED ON");
  });

  server.on("/off", HTTP_GET, [](AsyncWebServerRequest *req) {
    Serial.println("[HTTP] GET /off");
    digitalWrite(LED_BUILTIN, LOW);

    Serial.println("[HTTP] GET /servo/stop");
    servoMode = ServoMode::Idle;
    Serial.println("[Servo] Sweep/Set stopped, Idle mode");

    req->send(200, "text/plain", "LED OFF");
  });

  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *req) {
    int state = digitalRead(LED_BUILTIN) ? 1 : 0;
    String payload = String("{\"gpio_state\":") + state + "}";
    Serial.print("[HTTP] GET /api/status -> ");
    Serial.println(payload);
    req->send(200, "application/json", payload);
  });

  server.on("/left", HTTP_GET, [](AsyncWebServerRequest *req) {
    Serial.println("[HTTP] GET /left");
    targetAngle = 30;
    servoMode = ServoMode::Set;
    Serial.print("[Servo] targetAngle set to ");
    Serial.println(targetAngle);
    req->send(200, "text/plain", "Servo LEFT");
  });

  server.on("/right", HTTP_GET, [](AsyncWebServerRequest *req) {
    Serial.println("[HTTP] GET /right");
    targetAngle = 150;
    servoMode = ServoMode::Set;
    Serial.print("[Servo] targetAngle set to ");
    Serial.println(targetAngle);
    req->send(200, "text/plain", "Servo RIGHT");
  });

  server.on("/center", HTTP_GET, [](AsyncWebServerRequest *req) {
    Serial.println("[HTTP] GET /center");
    targetAngle = 90;
    servoMode = ServoMode::Set;
    Serial.print("[Servo] targetAngle set to ");
    Serial.println(targetAngle);
    req->send(200, "text/plain", "Servo CENTER");
  });

  server.on("/sweep", HTTP_GET, [](AsyncWebServerRequest *req) {
    Serial.println("[HTTP] GET /sweep");
    servoMode = ServoMode::Sweep;
    Serial.println("[Servo] Sweep mode enabled");
    req->send(200, "text/plain", "Servo SWEEP");
  });

  server.on("/stop", HTTP_GET, [](AsyncWebServerRequest *req) {
    Serial.println("[HTTP] GET /servo/stop");
    servoMode = ServoMode::Idle;
    Serial.println("[Servo] Sweep/Set stopped, Idle mode");
    req->send(200, "text/plain", "Servo STOP");
  });

  // Direct set endpoint (if you want to test numeric positions)
  server.on("/set", HTTP_GET, [](AsyncWebServerRequest *req) {
    Serial.println("[HTTP] GET /servo/set");
    if (req->hasParam("value")) {
      String v = req->getParam("value")->value();
      int pos = v.toInt();
      Serial.print("[Servo] /servo/set value param = ");
      Serial.println(v);
      if (pos < 0) pos = 0;
      if (pos > 180) pos = 180;
      targetAngle = pos;
      servoMode = ServoMode::Set;
      Serial.print("[Servo] targetAngle set to ");
      Serial.println(targetAngle);
      req->send(200, "text/plain", "Servo SET");
    } else {
      Serial.println("[Servo] ERROR: /servo/set missing 'value' param");
      req->send(400, "text/plain", "Missing value");
    }
  });

  server.onNotFound(handle_not_found);

  Serial.println("[HTTP] Starting AsyncWebServer on port 80");
  server.begin();
}

void loop() {
  unsigned long now = millis();

  switch (servoMode) {
    case ServoMode::Set:
      Serial.print("[Servo] Applying Set mode, angle = ");
      Serial.println(targetAngle);
      servo.write(targetAngle);
      currentAngle = targetAngle;
      servoMode = ServoMode::Idle;
      break;

    case ServoMode::Sweep:
      if (now - lastServoStepMs >= 1) {
        lastServoStepMs = now;

        if (sweepDirUp) {
          currentAngle++;
          if (currentAngle >= 180) {
            currentAngle = 180;
            sweepDirUp = false;
            Serial.println("[Servo] Sweep reached max, reversing");
          }
        } else {
          currentAngle--;
          if (currentAngle <= 0) {
            currentAngle = 0;
            sweepDirUp = true;
            Serial.println("[Servo] Sweep reached min, reversing");
          }
        }

        servo.write(currentAngle);
        Serial.print("[Servo] Sweep angle = ");
        Serial.println(currentAngle);
      }
      break;

    case ServoMode::Idle:
    default:
      break;
  }
}
