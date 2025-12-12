#include <ESPAsyncWebServer.h>
#include <WiFi.h>

const char *ssid = "openws";
const char *password = "ithurtswhenip";

AsyncWebServer server(80);

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Connected: ");
  Serial.println(WiFi.localIP());

  server.on("/on", HTTP_GET, [](AsyncWebServerRequest *req) {
    digitalWrite(LED_BUILTIN, HIGH);
    req->send(200, "text/plain", "LED ON");
  });

  server.on("/off", HTTP_GET, [](AsyncWebServerRequest *req) {
    digitalWrite(LED_BUILTIN, LOW);
    req->send(200, "text/plain", "LED OFF");
  });

  server.begin();
}

void loop() {}
