// ============================================================================
// HttpClient Implementation - Tab5.encoder
// ============================================================================

#include "HttpClient.h"

#if ENABLE_WIFI

#include <ESP.h>
#include <ESPmDNS.h>
#include <esp_task_wdt.h>
#include <cstring>
#include "../config/network_config.h"

HttpClient::HttpClient()
    : _serverIP(INADDR_NONE)
    , _serverHostname(LIGHTWAVE_HOST)
{
    // Resolution deferred to first HTTP request via connectToServer()
    // This prevents blocking the main loop during construction
    // (blocking here caused Task Watchdog crashes when navigating to ConnectivityTab)
}

HttpClient::~HttpClient() {
    _client.stop();
}

bool HttpClient::resolveHostname() {
    if (_serverIP != INADDR_NONE) {
        return true;
    }

    if (_discoveryState == DiscoveryState::SUCCESS) {
        _serverIP = _discoveryResult;
        return true;
    }

    if (_discoveryState != DiscoveryState::RUNNING) {
        startDiscovery();
    }

    Serial.println("[HTTP] Discovery in progress; deferring hostname resolution.");
    return false;
}

bool HttpClient::startDiscovery() {
    if (_discoveryState == DiscoveryState::RUNNING) {
        return true;
    }

    _discoveryState = DiscoveryState::RUNNING;
    _discoveryResult = IPAddress(0, 0, 0, 0);

    BaseType_t taskOk = xTaskCreate(
        &HttpClient::discoveryTask,
        "lw_discovery",
        4096,
        this,
        1,
        &_discoveryTaskHandle
    );

    if (taskOk != pdPASS) {
        _discoveryTaskHandle = nullptr;
        _discoveryState = DiscoveryState::FAILED;
        Serial.println("[HTTP] Failed to start discovery task.");
        return false;
    }

    return true;
}

void HttpClient::discoveryTask(void* parameter) {
    auto* client = static_cast<HttpClient*>(parameter);
    bool success = client->runDiscovery();
    client->_discoveryState = success ? DiscoveryState::SUCCESS : DiscoveryState::FAILED;
    client->_discoveryTaskHandle = nullptr;
    vTaskDelete(nullptr);
}

bool HttpClient::runDiscovery() {
    bool onLightwaveNetwork = false;
    String currentSSID = "";

    if (WiFi.status() == WL_CONNECTED) {
        currentSSID = WiFi.SSID();
        onLightwaveNetwork = currentSSID.startsWith("LightwaveOS") || currentSSID == WIFI_SSID2;
    }

    Serial.printf("[HTTP] Resolving v2 address (connected to: %s, onLightwaveNetwork: %s)\n",
                  currentSSID.c_str(), onLightwaveNetwork ? "YES" : "NO");

    if (onLightwaveNetwork) {
        IPAddress gateway = WiFi.gatewayIP();
        if (gateway != IPAddress(0, 0, 0, 0)) {
            _discoveryResult = gateway;
            Serial.printf("[HTTP] Using gateway IP: %s (on LightwaveOS network)\n",
                          _discoveryResult.toString().c_str());
            return true;
        }
        #ifdef LIGHTWAVE_IP
        {
            IPAddress compiledIP;
            if (compiledIP.fromString(LIGHTWAVE_IP)) {
                _discoveryResult = compiledIP;
                Serial.printf("[HTTP] Using compiled IP: %s (LightwaveOS fallback)\n",
                              _discoveryResult.toString().c_str());
                return true;
            }
        }
        #endif
    }

    Serial.printf("[HTTP] Attempting mDNS resolution for %s (with retries)...\n", _serverHostname);
    esp_task_wdt_reset();

    IPAddress resolved(0, 0, 0, 0);
    for (uint8_t attempt = 0; attempt < NetworkConfig::MDNS_MAX_ATTEMPTS; attempt++) {
        if (attempt > 0) {
            vTaskDelay(pdMS_TO_TICKS(NetworkConfig::MDNS_RETRY_DELAY_MS));
        }
        esp_task_wdt_reset();
        resolved = MDNS.queryHost(_serverHostname);
        esp_task_wdt_reset();

        if (resolved != IPAddress(0, 0, 0, 0)) {
            _discoveryResult = resolved;
            Serial.printf("[HTTP] Resolved %s to %s (via mDNS, attempt %d/%d)\n",
                          _serverHostname, resolved.toString().c_str(),
                          attempt + 1, NetworkConfig::MDNS_MAX_ATTEMPTS);
            return true;
        }
        Serial.printf("[HTTP] mDNS attempt %d/%d failed\n", attempt + 1, NetworkConfig::MDNS_MAX_ATTEMPTS);
    }

    Serial.printf("[HTTP] mDNS resolution failed after %d attempts\n", NetworkConfig::MDNS_MAX_ATTEMPTS);

    if (onLightwaveNetwork) {
        _discoveryResult = IPAddress(192, 168, 4, 1);
        Serial.printf("[HTTP] Using SoftAP fallback IP: %s (on LightwaveOS network)\n",
                      _discoveryResult.toString().c_str());
        return true;
    }

    Serial.printf("[HTTP] Scanning network to discover LightwaveOS device...\n");
    IPAddress localIP = WiFi.localIP();
    IPAddress subnet = WiFi.subnetMask();
    IPAddress gateway = WiFi.gatewayIP();

    IPAddress networkBase(localIP[0] & subnet[0],
                          localIP[1] & subnet[1],
                          localIP[2] & subnet[2],
                          localIP[3] & subnet[3]);

    IPAddress candidates[] = {
        gateway,
        IPAddress(gateway[0], gateway[1], gateway[2], gateway[3] + 1),
        IPAddress(gateway[0], gateway[1], gateway[2], gateway[3] + 2),
        IPAddress(localIP[0], localIP[1], localIP[2], 1),
        IPAddress(localIP[0], localIP[1], localIP[2], 100),
        IPAddress(localIP[0], localIP[1], localIP[2], 101),
        IPAddress(localIP[0], localIP[1], localIP[2], 102),
    };

    for (uint8_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        if (candidates[i] == IPAddress(0, 0, 0, 0) || candidates[i] == localIP) {
            continue;
        }

        WiFiClient testClient;
        testClient.setTimeout(500);
        if (testClient.connect(candidates[i], HTTP_PORT)) {
            testClient.print("GET /api/v1/device/info HTTP/1.1\r\n");
            testClient.print("Host: ");
            testClient.print(candidates[i].toString());
            testClient.print("\r\n");
            testClient.print("Connection: close\r\n\r\n");

            String response = "";
            unsigned long startTime = millis();
            while (millis() - startTime < 1000 && testClient.available()) {
                response += (char)testClient.read();
                if (response.length() > 200) break;
            }
            testClient.stop();

            if (response.indexOf("lightwaveos") >= 0 ||
                response.indexOf("LightwaveOS") >= 0 ||
                response.indexOf("\"board\":\"ESP32-S3\"") >= 0) {
                _discoveryResult = candidates[i];
                Serial.printf("[HTTP] Discovered LightwaveOS device at %s (network scan)\n",
                              _discoveryResult.toString().c_str());
                return true;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5));
        esp_task_wdt_reset();
    }

    Serial.printf("[HTTP] Scanning subnet %s.0/24 for LightwaveOS device...\n",
                  networkBase.toString().c_str());
    for (uint8_t host = 1; host < 255; host++) {
        if (host == localIP[3]) continue;

        IPAddress testIP(networkBase[0], networkBase[1], networkBase[2], host);

        WiFiClient testClient;
        testClient.setTimeout(300);
        if (testClient.connect(testIP, HTTP_PORT)) {
            testClient.print("GET /api/v1/device/info HTTP/1.1\r\n");
            testClient.print("Host: ");
            testClient.print(testIP.toString());
            testClient.print("\r\n");
            testClient.print("Connection: close\r\n\r\n");

            String response = "";
            unsigned long startTime = millis();
            while (millis() - startTime < 500 && testClient.available()) {
                response += (char)testClient.read();
                if (response.length() > 200) break;
            }
            testClient.stop();

            if (response.indexOf("lightwaveos") >= 0 ||
                response.indexOf("LightwaveOS") >= 0 ||
                response.indexOf("\"board\":\"ESP32-S3\"") >= 0) {
                _discoveryResult = testIP;
                Serial.printf("[HTTP] Discovered LightwaveOS device at %s (subnet scan)\n",
                              _discoveryResult.toString().c_str());
                return true;
            }
        }

        if (host % 20 == 0) {
            Serial.printf("[HTTP] Scanning... %d/254\n", host);
            esp_task_wdt_reset();
        }

        vTaskDelay(pdMS_TO_TICKS(2));
    }

    Serial.printf("[HTTP] ERROR: Could not discover LightwaveOS device on network!\n");
    Serial.printf("[HTTP] Tried mDNS (%d attempts) and network scan (254 IPs)\n",
                  NetworkConfig::MDNS_MAX_ATTEMPTS);
    return false;
}

bool HttpClient::connectToServer() {
    // Resolve hostname if IP is not set
    if (_serverIP == INADDR_NONE) {
        if (!resolveHostname()) {
            return false;
        }
    }

    // Connect to server (feed watchdog before blocking call)
    Serial.printf("[HTTP] Connecting to %s:%d\n", _serverIP.toString().c_str(), HTTP_PORT);
    esp_task_wdt_reset();
    if (!_client.connect(_serverIP, HTTP_PORT)) {
        Serial.printf("[HTTP] Failed to connect to %s:%d\n", _serverIP.toString().c_str(), HTTP_PORT);
        return false;
    }
    esp_task_wdt_reset();  // Feed after successful connect

    return true;
}

bool HttpClient::get(const String& path, HttpResponse& response) {
    response = HttpResponse();

    if (!connectToServer()) {
        response.errorMessage = "Connection failed";
        return false;
    }

    // Send HTTP GET request
    _client.printf("GET %s HTTP/1.1\r\n", path.c_str());
    _client.printf("Host: %s\r\n", _serverHostname);
    if (!_apiKey.isEmpty()) {
        _client.printf("X-API-Key: %s\r\n", _apiKey.c_str());
    }
    _client.printf("Connection: close\r\n\r\n");

    // Wait for response with timeout (CRITICAL: feed watchdog to prevent WDT crash)
    uint32_t startTime = millis();
    while (!_client.available() && (millis() - startTime) < HTTP_TIMEOUT_MS) {
        delay(10);
        esp_task_wdt_reset();  // Prevent watchdog timeout during HTTP wait
    }

    if (!_client.available()) {
        response.errorMessage = "Timeout waiting for response";
        _client.stop();
        return false;
    }

    esp_task_wdt_reset();  // Feed watchdog before parsing

    // Read HTTP status line
    String statusLine = _client.readStringUntil('\n');
    statusLine.trim();

    // Parse status code
    int spaceIndex = statusLine.indexOf(' ');
    if (spaceIndex > 0) {
        int spaceIndex2 = statusLine.indexOf(' ', spaceIndex + 1);
        if (spaceIndex2 > 0) {
            response.statusCode = statusLine.substring(spaceIndex + 1, spaceIndex2).toInt();
        }
    }

    // Read headers until empty line
    while (_client.available()) {
        String header = _client.readStringUntil('\n');
        header.trim();
        if (header.length() == 0) {
            break;
        }
    }

    esp_task_wdt_reset();  // Feed watchdog before reading body

    // Read body
    while (_client.available()) {
        response.body += (char)_client.read();
    }

    _client.stop();

    response.success = (response.statusCode >= 200 && response.statusCode < 300);
    if (!response.success) {
        response.errorMessage = "HTTP " + String(response.statusCode);
    }

    return response.success;
}

bool HttpClient::post(const String& path, const String& body, HttpResponse& response) {
    response = HttpResponse();

    if (!connectToServer()) {
        response.errorMessage = "Connection failed";
        return false;
    }

    // Send HTTP POST request
    _client.printf("POST %s HTTP/1.1\r\n", path.c_str());
    _client.printf("Host: %s\r\n", _serverHostname);
    _client.printf("Content-Type: application/json\r\n");
    _client.printf("Content-Length: %d\r\n", body.length());
    if (!_apiKey.isEmpty()) {
        _client.printf("X-API-Key: %s\r\n", _apiKey.c_str());
    }
    _client.printf("Connection: close\r\n\r\n");
    _client.print(body);

    // Wait for response with timeout (CRITICAL: feed watchdog to prevent WDT crash)
    uint32_t startTime = millis();
    while (!_client.available() && (millis() - startTime) < HTTP_TIMEOUT_MS) {
        delay(10);
        esp_task_wdt_reset();  // Prevent watchdog timeout during HTTP wait
    }

    if (!_client.available()) {
        response.errorMessage = "Timeout waiting for response";
        _client.stop();
        return false;
    }

    esp_task_wdt_reset();  // Feed watchdog before parsing

    // Read HTTP status line
    String statusLine = _client.readStringUntil('\n');
    statusLine.trim();

    // Parse status code
    int spaceIndex = statusLine.indexOf(' ');
    if (spaceIndex > 0) {
        int spaceIndex2 = statusLine.indexOf(' ', spaceIndex + 1);
        if (spaceIndex2 > 0) {
            response.statusCode = statusLine.substring(spaceIndex + 1, spaceIndex2).toInt();
        }
    }

    // Read headers until empty line
    while (_client.available()) {
        String header = _client.readStringUntil('\n');
        header.trim();
        if (header.length() == 0) {
            break;
        }
    }

    esp_task_wdt_reset();  // Feed watchdog before reading body

    // Read body
    while (_client.available()) {
        response.body += (char)_client.read();
    }

    _client.stop();

    response.success = (response.statusCode >= 200 && response.statusCode < 300);
    if (!response.success) {
        response.errorMessage = "HTTP " + String(response.statusCode);
    }

    return response.success;
}

bool HttpClient::del(const String& path, HttpResponse& response) {
    response = HttpResponse();

    if (!connectToServer()) {
        response.errorMessage = "Connection failed";
        return false;
    }

    // Send HTTP DELETE request
    _client.printf("DELETE %s HTTP/1.1\r\n", path.c_str());
    _client.printf("Host: %s\r\n", _serverHostname);
    if (!_apiKey.isEmpty()) {
        _client.printf("X-API-Key: %s\r\n", _apiKey.c_str());
    }
    _client.printf("Connection: close\r\n\r\n");

    // Wait for response with timeout (CRITICAL: feed watchdog to prevent WDT crash)
    uint32_t startTime = millis();
    while (!_client.available() && (millis() - startTime) < HTTP_TIMEOUT_MS) {
        delay(10);
        esp_task_wdt_reset();  // Prevent watchdog timeout during HTTP wait
    }

    if (!_client.available()) {
        response.errorMessage = "Timeout waiting for response";
        _client.stop();
        return false;
    }

    esp_task_wdt_reset();  // Feed watchdog before parsing

    // Read HTTP status line
    String statusLine = _client.readStringUntil('\n');
    statusLine.trim();

    // Parse status code
    int spaceIndex = statusLine.indexOf(' ');
    if (spaceIndex > 0) {
        int spaceIndex2 = statusLine.indexOf(' ', spaceIndex + 1);
        if (spaceIndex2 > 0) {
            response.statusCode = statusLine.substring(spaceIndex + 1, spaceIndex2).toInt();
        }
    }

    // Read headers until empty line
    while (_client.available()) {
        String header = _client.readStringUntil('\n');
        header.trim();
        if (header.length() == 0) {
            break;
        }
    }

    esp_task_wdt_reset();  // Feed watchdog before reading body

    // Read body
    while (_client.available()) {
        response.body += (char)_client.read();
    }

    _client.stop();

    response.success = (response.statusCode >= 200 && response.statusCode < 300);
    if (!response.success) {
        response.errorMessage = "HTTP " + String(response.statusCode);
    }

    return response.success;
}

bool HttpClient::parseJsonResponse(const HttpResponse& response, JsonDocument& doc) {
    DeserializationError error = deserializeJson(doc, response.body);
    if (error) {
        Serial.printf("[HTTP] JSON parse error: %s\n", error.c_str());
        Serial.printf("[HTTP] Response body: %s\n", response.body.c_str());
        return false;
    }
    return true;
}

int HttpClient::listNetworks(NetworkEntry* networks, uint8_t maxNetworks) {
    HttpResponse response;
    if (!get("/api/v1/network/networks", response)) {
        return -1;
    }

    JsonDocument doc;
    if (!parseJsonResponse(response, doc)) {
        return -1;
    }

    // v2 API wraps response under "data" key
    if (!doc.containsKey("data") || !doc["data"].is<JsonObject>()) {
        Serial.printf("[HTTP] Response missing 'data' wrapper\n");
        return -1;
    }

    JsonObject data = doc["data"].as<JsonObject>();
    if (!data.containsKey("networks") || !data["networks"].is<JsonArray>()) {
        return 0;
    }

    JsonArray networkArray = data["networks"].as<JsonArray>();
    uint8_t count = 0;
    for (JsonVariant network : networkArray) {
        if (count >= maxNetworks) break;

        if (network.is<const char*>()) {
            // v2 API returns saved networks as array of SSID strings
            networks[count].ssid = network.as<const char*>();
            networks[count].password = "";
            networks[count].isSaved = true;
            count++;
            continue;
        }

        if (!network.is<JsonObject>()) {
            continue;
        }

        JsonObject networkObj = network.as<JsonObject>();
        if (networkObj.containsKey("ssid")) {
            networks[count].ssid = networkObj["ssid"].as<String>();
        }
        if (networkObj.containsKey("password")) {
            networks[count].password = networkObj["password"].as<String>();
        }
        if (networkObj.containsKey("isSaved")) {
            networks[count].isSaved = networkObj["isSaved"].as<bool>();
        } else {
            networks[count].isSaved = true;  // Default to saved if not specified
        }

        count++;
    }

    return count;
}

bool HttpClient::addNetwork(const String& ssid, const String& password) {
    JsonDocument doc;
    doc["ssid"] = ssid;
    doc["password"] = password;

    String body;
    serializeJson(doc, body);

    HttpResponse response;
    return post("/api/v1/network/networks", body, response);
}

bool HttpClient::deleteNetwork(const String& ssid) {
    // URL-encode SSID (simple implementation - handle spaces and special chars)
    String encodedSsid = ssid;
    encodedSsid.replace(" ", "%20");
    encodedSsid.replace("/", "%2F");
    encodedSsid.replace("?", "%3F");
    encodedSsid.replace("&", "%26");
    encodedSsid.replace("=", "%3D");

    HttpResponse response;
    return del("/api/v1/network/networks/" + encodedSsid, response);
}

bool HttpClient::connectToNetwork(const String& ssid, const String& password) {
    JsonDocument doc;
    doc["ssid"] = ssid;
    if (!password.isEmpty()) {
        doc["password"] = password;
    }

    String body;
    serializeJson(doc, body);

    HttpResponse response;
    return post("/api/v1/network/connect", body, response);
}

bool HttpClient::disconnectFromNetwork() {
    HttpResponse response;
    return post("/api/v1/network/disconnect", "{}", response);
}

bool HttpClient::startScan(ScanStatus& status) {
    Serial.println("[HTTP] Starting network scan...");

    HttpResponse response;
    if (!get("/api/v1/network/scan", response)) {
        Serial.println("[HTTP] Scan request failed");
        return false;
    }

    JsonDocument doc;
    if (!parseJsonResponse(response, doc)) {
        Serial.println("[HTTP] Failed to parse scan response");
        return false;
    }

    // v2 API wraps response under "data" key
    if (!doc.containsKey("data") || !doc["data"].is<JsonObject>()) {
        Serial.println("[HTTP] Scan response missing 'data' wrapper");
        return false;
    }

    JsonObject data = doc["data"].as<JsonObject>();

    // v2 returns synchronous results - no jobId/polling needed
    status.inProgress = false;
    status.jobId = 0;
    status.networkCount = 0;

    if (data.containsKey("networks") && data["networks"].is<JsonArray>()) {
        JsonArray networkArray = data["networks"].as<JsonArray>();

        for (JsonObject network : networkArray) {
            if (status.networkCount >= 20) break;

            if (network.containsKey("ssid")) {
                status.networks[status.networkCount].ssid = network["ssid"].as<String>();
            }
            if (network.containsKey("rssi")) {
                status.networks[status.networkCount].rssi = network["rssi"].as<int32_t>();
            }
            if (network.containsKey("channel")) {
                status.networks[status.networkCount].channel = network["channel"].as<uint8_t>();
            }

            // v2 returns "encryption" as STRING (e.g., "WPA2", "OPEN")
            if (network.containsKey("encryption")) {
                String encStr = network["encryption"].as<String>();
                status.networks[status.networkCount].encrypted = (encStr != "OPEN");
                status.networks[status.networkCount].encryptionType = encStr;
            }

            status.networkCount++;
        }
    }

    Serial.printf("[HTTP] Scan complete, found %d networks\n", status.networkCount);
    return true;
}

bool HttpClient::getNetworkStatus(JsonDocument& doc) {
    HttpResponse response;
    if (!get("/api/v1/network/status", response)) {
        return false;
    }

    return parseJsonResponse(response, doc);
}

#endif // ENABLE_WIFI
