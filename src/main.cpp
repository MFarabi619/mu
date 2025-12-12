#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <SPIFFS.h>
#include <WiFi.h>

const char *ssid = "IO_GUEST_EVENT";
const char *password = "cyt5Z&Q&1i";

AsyncWebServer server(80);

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

  server.onNotFound(handle_not_found);
  server.begin();
}

void loop() {}
