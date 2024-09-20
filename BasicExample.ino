#include <WiFiNINA.h>
#include "HttpParser.h"
#include "WiFiSecrets.h"

const unsigned int PORT = 80;

WiFiServer server(PORT);

void setup() {
  Serial.begin(9600);
  int status = WL_IDLE_STATUS;
  do {
    status = WiFi.begin(SSID, PASSWORD);
    delay(2000);
  }
  while (WiFi.status() != WL_CONNECTED);
  
  Serial.println("WIFI connected!");
  Serial.print("Address: ");
  Serial.println(WiFi.localIP());

  server.begin();
}

void loop() {
  WiFiClient client = server.available();
  if (client && client.connected()) {
    HttpParser clientParser(client, 1024);
    if (clientParser.receive()) {
      Serial.println("------ BASIC INFO ------");
      Serial.println("Method: " + clientParser.getMethod());
      Serial.println("Path: " + clientParser.getPath());
      Serial.println("Version: " + clientParser.getVersion());
      Serial.println("---- REQUEST HEADERS ----");
      vector<KeyValueEntry> headers = clientParser.getHeaders();
      for (int i = 0; i < headers.size(); i++) {
        KeyValueEntry current = headers.at(i);
        Serial.println("Header " + current.key + " -> " + current.value);
      }
      Serial.println("------ PATH PARAMS ------");
      vector<KeyValueEntry> params = clientParser.getPathParameters();
      for (int i = 0; i < params.size(); i++) {
        KeyValueEntry current = params.at(i);
        Serial.println("Parameter " + current.key + " -> " + current.value);
      }
      Serial.println("--------- DATA ---------");
      Serial.println(clientParser.getData());
      Serial.println("------------------------");
      clientParser.transmit("Thank you for your request! The server is handling it now :)");
    }
    client.stop();
    clientParser.end();
  }
}
