#ifndef WEB_API_H
#define WEB_API_H

/*
 * web_api.h — Web Server API
 * HTTP endpoints de cau hinh WiFi va kiem tra trang thai.
 */

// NOTE: wifiSSID, wifiPASS, connectionStatus, connectStartTime
//       are declared in connectivity.h (included before this file)

// ===== GET / =====
void handleRoot() {
  server.send(200, "text/plain", "ESP32 Smart Apartment - Running");
}

// ===== POST /config — Cau hinh WiFi moi =====
void handleConfigWifi() {
  if (server.method() != HTTP_POST) {
    server.send(405, "application/json", "{\"error\":\"Method not allowed\"}");
    return;
  }

  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));

  if (error) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  wifiSSID = doc["ssid"].as<String>();
  wifiPASS = doc["password"].as<String>();

  Serial.printf("[API] WiFi config received: %s\n", wifiSSID.c_str());

  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(wifiSSID.c_str(), wifiPASS.c_str());

  connectionStatus = "connecting";
  connectStartTime = millis();

  server.send(200, "application/json", "{\"status\":\"connecting\"}");
}

// ===== GET /status =====
void handleStatusApi() {
  StaticJsonDocument<200> doc;
  doc["status"] = connectionStatus;

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

// ===== Dang ky tat ca routes =====
void setupWebRoutes() {
  server.on("/", handleRoot);
  server.on("/config", HTTP_POST, handleConfigWifi);
  server.on("/status", HTTP_GET, handleStatusApi);
  server.begin();
  Serial.println("[API] Web server started");
}

#endif
