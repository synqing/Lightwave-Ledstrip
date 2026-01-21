// ============================================================================
// HttpClient Implementation - Tab5.encoder
// ============================================================================

#include "HttpClient.h"

#if ENABLE_WIFI

#include <ESP.h>
#include <cstring>
#include "../config/network_config.h"

HttpClient::HttpClient()
    : _serverIP(INADDR_NONE)
    , _serverHostname(LIGHTWAVE_HOST)
{
    // Try to resolve hostname on initialization
    resolveHostname();
}

HttpClient::~HttpClient() {
    _client.stop();
}

bool HttpClient::resolveHostname() {
    // NOTE: WiFi.hostByName() is DNS-only and typically won't resolve .local mDNS names
    // The Tab5 WiFiManager already performs mDNS resolution via MDNS.queryHost()
    // and provides the resolved IP via getResolvedIP()
    
    // Attempt DNS resolution first (will likely fail for .local hostnames)
    IPAddress resolved;
    if (WiFi.hostByName(_serverHostname, resolved)) {
        _serverIP = resolved;
        Serial.printf("[HTTP] Resolved %s to %s (via DNS)\n", _serverHostname, resolved.toString().c_str());
        return true;
    }
    
    Serial.printf("[HTTP] DNS resolution failed for %s\n", _serverHostname);
    
    // Fall back to v2 SoftAP IP (192.168.4.1) - this is the guaranteed baseline path
    _serverIP = IPAddress(192, 168, 4, 1);
    Serial.printf("[HTTP] Using v2 SoftAP fallback IP: %s\n", _serverIP.toString().c_str());
    return true; // Return true since fallback IP is valid
}

bool HttpClient::connectToServer() {
    // Resolve hostname if IP is not set
    if (_serverIP == INADDR_NONE) {
        if (!resolveHostname()) {
            return false;
        }
    }

    // Connect to server
    if (!_client.connect(_serverIP, HTTP_PORT)) {
        Serial.printf("[HTTP] Failed to connect to %s:%d\n", _serverIP.toString().c_str(), HTTP_PORT);
        return false;
    }

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

    // Wait for response with timeout
    uint32_t startTime = millis();
    while (!_client.available() && (millis() - startTime) < HTTP_TIMEOUT_MS) {
        delay(10);
    }

    if (!_client.available()) {
        response.errorMessage = "Timeout waiting for response";
        _client.stop();
        return false;
    }

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

    // Wait for response with timeout
    uint32_t startTime = millis();
    while (!_client.available() && (millis() - startTime) < HTTP_TIMEOUT_MS) {
        delay(10);
    }

    if (!_client.available()) {
        response.errorMessage = "Timeout waiting for response";
        _client.stop();
        return false;
    }

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

    // Wait for response with timeout
    uint32_t startTime = millis();
    while (!_client.available() && (millis() - startTime) < HTTP_TIMEOUT_MS) {
        delay(10);
    }

    if (!_client.available()) {
        response.errorMessage = "Timeout waiting for response";
        _client.stop();
        return false;
    }

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
    if (!doc["data"].is<JsonObject>()) {
        Serial.printf("[HTTP] Response missing 'data' wrapper\n");
        return -1;
    }

    JsonObject data = doc["data"].as<JsonObject>();
    if (!data["networks"].is<JsonArray>()) {
        return 0;
    }

    JsonArray networkArray = data["networks"].as<JsonArray>();
    uint8_t count = 0;
    for (JsonObject network : networkArray) {
        if (count >= maxNetworks) break;

        if (network["ssid"].is<String>()) {
            networks[count].ssid = network["ssid"].as<String>();
        }
        if (network["password"].is<String>()) {
            networks[count].password = network["password"].as<String>();
        }
        if (network["isSaved"].is<bool>()) {
            networks[count].isSaved = network["isSaved"].as<bool>();
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

bool HttpClient::startScan(uint32_t& jobId) {
    HttpResponse response;
    if (!get("/api/v1/network/scan", response)) {
        return false;
    }

    JsonDocument doc;
    if (!parseJsonResponse(response, doc)) {
        return false;
    }

    // v2 API wraps response under "data" key
    if (!doc["data"].is<JsonObject>()) {
        Serial.printf("[HTTP] Response missing 'data' wrapper\n");
        return false;
    }

    JsonObject data = doc["data"].as<JsonObject>();
    if (!data["jobId"].is<uint32_t>()) {
        return false;
    }

    jobId = data["jobId"].as<uint32_t>();
    return true;
}

bool HttpClient::getScanStatus(ScanStatus& status) {
    HttpResponse response;
    if (!get("/api/v1/network/scan/status", response)) {
        return false;
    }

    JsonDocument doc;
    if (!parseJsonResponse(response, doc)) {
        return false;
    }

    // v2 API wraps response under "data" key
    if (!doc["data"].is<JsonObject>()) {
        Serial.printf("[HTTP] Response missing 'data' wrapper\n");
        return false;
    }

    JsonObject data = doc["data"].as<JsonObject>();
    
    // Parse scan status - v2 uses "status" string (not "inProgress" boolean)
    if (data["status"].is<String>()) {
        String statusStr = data["status"].as<String>();
        status.inProgress = (statusStr == "in_progress" || statusStr == "started");
    } else {
        status.inProgress = false;
    }
    
    status.jobId = data["jobId"].is<uint32_t>() ? data["jobId"].as<uint32_t>() : 0;

    if (data["networks"].is<JsonArray>()) {
        JsonArray networkArray = data["networks"].as<JsonArray>();
        status.networkCount = 0;
        for (JsonObject network : networkArray) {
            if (status.networkCount >= 20) break;

            if (network["ssid"].is<String>()) {
                status.networks[status.networkCount].ssid = network["ssid"].as<String>();
            }
            if (network["rssi"].is<int32_t>()) {
                status.networks[status.networkCount].rssi = network["rssi"].as<int32_t>();
            }
            if (network["channel"].is<uint8_t>()) {
                status.networks[status.networkCount].channel = network["channel"].as<uint8_t>();
            }
            // v2 uses "encryption" (numeric) not "encrypted"/"encryptionType"
            if (network["encryption"].is<uint8_t>()) {
                uint8_t encType = network["encryption"].as<uint8_t>();
                status.networks[status.networkCount].encrypted = (encType != 0); // 0 = WIFI_AUTH_OPEN
                // Map encryption type to string for UI display
                switch(encType) {
                    case 0: status.networks[status.networkCount].encryptionType = "Open"; break;
                    case 2: status.networks[status.networkCount].encryptionType = "WPA"; break;
                    case 3: status.networks[status.networkCount].encryptionType = "WPA2"; break;
                    case 4: status.networks[status.networkCount].encryptionType = "WPA/WPA2"; break;
                    case 5: status.networks[status.networkCount].encryptionType = "WPA2-Enterprise"; break;
                    case 6: status.networks[status.networkCount].encryptionType = "WPA3"; break;
                    case 7: status.networks[status.networkCount].encryptionType = "WPA2/WPA3"; break;
                    case 8: status.networks[status.networkCount].encryptionType = "WAPI"; break;
                    default: status.networks[status.networkCount].encryptionType = "Unknown"; break;
                }
            }

            status.networkCount++;
        }
    } else {
        status.networkCount = 0;
    }

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
