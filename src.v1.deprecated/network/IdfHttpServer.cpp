#include "IdfHttpServer.h"

#include <cstring>

#include "../config/features.h"

#if FEATURE_NETWORK
#include "../config/network_config.h"
#endif

IdfHttpServer* IdfHttpServer::s_lastInstance = nullptr;

static bool isAllowedCorsOrigin(const char* origin) {
    if (!origin || !origin[0]) return false;

    const char* scheme = nullptr;
    if (strncmp(origin, "http://", 7) == 0) scheme = origin + 7;
    if (strncmp(origin, "https://", 8) == 0) scheme = origin + 8;
    if (!scheme) return false;

    const char* hostport = scheme;
    const size_t hostLen = strcspn(hostport, ":/");
    if (hostLen == 0) return false;

    auto hostEquals = [&](const char* expected) -> bool {
        const size_t expectedLen = strlen(expected);
        return hostLen == expectedLen && strncmp(hostport, expected, hostLen) == 0;
    };

    if (hostEquals("localhost") || hostEquals("127.0.0.1") || hostEquals("0.0.0.0")) {
        return true;
    }

#if FEATURE_NETWORK
    if (hostEquals(NetworkConfig::MDNS_HOSTNAME)) {
        return true;
    }

    char mdnsLocal[64];
    snprintf(mdnsLocal, sizeof(mdnsLocal), "%s.local", NetworkConfig::MDNS_HOSTNAME);
    if (hostEquals(mdnsLocal)) {
        return true;
    }
#endif

    return false;
}

IdfHttpServer::IdfHttpServer() {
    for (size_t i = 0; i < MAX_WS_CLIENTS; i++) {
        m_wsClientFds[i] = -1;
    }
}

IdfHttpServer::~IdfHttpServer() {
    stop();
}

bool IdfHttpServer::begin(const Config& cfg) {
    if (m_server != nullptr) {
        return true;
    }

    m_cfg = cfg;

    httpd_config_t httpCfg = HTTPD_DEFAULT_CONFIG();
    httpCfg.server_port = cfg.port;
    httpCfg.max_uri_handlers = cfg.maxUriHandlers;
    httpCfg.max_open_sockets = cfg.maxOpenSockets;
    httpCfg.lru_purge_enable = cfg.enableLruPurge;

    if (httpd_start(&m_server, &httpCfg) != ESP_OK) {
        m_server = nullptr;
        return false;
    }

    // Register 404 handler for diagnostics
    httpd_register_err_handler(m_server, HTTPD_404_NOT_FOUND, &IdfHttpServer::handle404);

    s_lastInstance = this;
    return registerInternalHandlers();
}

void IdfHttpServer::stop() {
    if (m_server) {
        httpd_stop(m_server);
        m_server = nullptr;
    }
    for (size_t i = 0; i < MAX_WS_CLIENTS; i++) {
        m_wsClientFds[i] = -1;
    }
}

void IdfHttpServer::setWsHandlers(WsClientEventHandler onClientEvent,
                                  WsMessageHandler onMessage,
                                  void* ctx) {
    m_onWsClientEvent = onClientEvent;
    m_onWsMessage = onMessage;
    m_wsCtx = ctx;
}

bool IdfHttpServer::registerInternalHandlers() {
    if (!m_server) return false;

    // WebSocket endpoint
    httpd_uri_t wsUri{};
    wsUri.uri = "/ws";
    wsUri.method = HTTP_GET;
    wsUri.handler = &IdfHttpServer::wsHandler;
    wsUri.user_ctx = this;
    wsUri.is_websocket = true;

    if (httpd_register_uri_handler(m_server, &wsUri) != ESP_OK) {
        return false;
    }

    m_routes.push_back(String("GET ") + wsUri.uri);
    return true;
}

bool IdfHttpServer::registerGet(const char* uri, esp_err_t (*handler)(httpd_req_t* req)) {
    if (!m_server) return false;
    httpd_uri_t u{};
    u.uri = uri;
    u.method = HTTP_GET;
    u.handler = handler;
    bool ok = httpd_register_uri_handler(m_server, &u) == ESP_OK;
    recordRoute("GET", uri, ok);
    return ok;
}

bool IdfHttpServer::registerPost(const char* uri, esp_err_t (*handler)(httpd_req_t* req)) {
    if (!m_server) return false;
    httpd_uri_t u{};
    u.uri = uri;
    u.method = HTTP_POST;
    u.handler = handler;
    bool ok = httpd_register_uri_handler(m_server, &u) == ESP_OK;
    recordRoute("POST", uri, ok);
    return ok;
}

bool IdfHttpServer::registerPut(const char* uri, esp_err_t (*handler)(httpd_req_t* req)) {
    if (!m_server) return false;
    httpd_uri_t u{};
    u.uri = uri;
    u.method = HTTP_PUT;
    u.handler = handler;
    bool ok = httpd_register_uri_handler(m_server, &u) == ESP_OK;
    recordRoute("PUT", uri, ok);
    return ok;
}

bool IdfHttpServer::registerPatch(const char* uri, esp_err_t (*handler)(httpd_req_t* req)) {
    if (!m_server) return false;
    httpd_uri_t u{};
    u.uri = uri;
    u.method = HTTP_PATCH;
    u.handler = handler;
    bool ok = httpd_register_uri_handler(m_server, &u) == ESP_OK;
    recordRoute("PATCH", uri, ok);
    return ok;
}

bool IdfHttpServer::registerDelete(const char* uri, esp_err_t (*handler)(httpd_req_t* req)) {
    if (!m_server) return false;
    httpd_uri_t u{};
    u.uri = uri;
    u.method = HTTP_DELETE;
    u.handler = handler;
    bool ok = httpd_register_uri_handler(m_server, &u) == ESP_OK;
    recordRoute("DELETE", uri, ok);
    return ok;
}

bool IdfHttpServer::registerOptions(const char* uri, esp_err_t (*handler)(httpd_req_t* req)) {
    if (!m_server) return false;
    httpd_uri_t u{};
    u.uri = uri;
    u.method = HTTP_OPTIONS;
    u.handler = handler;
    bool ok = httpd_register_uri_handler(m_server, &u) == ESP_OK;
    recordRoute("OPTIONS", uri, ok);
    return ok;
}

void IdfHttpServer::addCorsHeaders(httpd_req_t* req) {
    char origin[192];
    const bool hasOrigin = httpd_req_get_hdr_value_str(req, "Origin", origin, sizeof(origin)) == ESP_OK;
    if (hasOrigin && isAllowedCorsOrigin(origin)) {
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", origin);
        httpd_resp_set_hdr(req, "Vary", "Origin");
    }
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, PUT, PATCH, DELETE, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type, Authorization, X-OTA-Token");
    httpd_resp_set_hdr(req, "Access-Control-Max-Age", "86400");
}

esp_err_t IdfHttpServer::corsOptionsHandler(httpd_req_t* req) {
    addCorsHeaders(req);
    httpd_resp_set_status(req, "204 No Content");
    return httpd_resp_send(req, nullptr, 0);
}

void IdfHttpServer::recordRoute(const char* method, const char* uri, bool ok) {
    if (!method || !uri) return;
    String entry = String(method) + " " + uri;
    if (ok) {
        m_routes.push_back(entry);
    } else {
        Serial.printf("[HTTPD] Failed to register route: %s\n", entry.c_str());
        m_routes.push_back(String("[FAILED] ") + entry);
    }
}

esp_err_t IdfHttpServer::handle404(httpd_req_t* req, httpd_err_code_t err) {
    // Log details for diagnostics
    const char* method = "UNKNOWN";
    switch (req->method) {
        case HTTP_GET: method = "GET"; break;
        case HTTP_POST: method = "POST"; break;
        case HTTP_PUT: method = "PUT"; break;
        case HTTP_PATCH: method = "PATCH"; break;
        case HTTP_DELETE: method = "DELETE"; break;
        case HTTP_OPTIONS: method = "OPTIONS"; break;
        default: break;
    }

    Serial.printf("[HTTP 404] %s %s (err=%d)\n", method, req->uri, (int)err);
    httpd_resp_set_status(req, "404 Not Found");
    httpd_resp_set_type(req, "application/json");
    addCorsHeaders(req);

    cJSON* root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", false);
    cJSON* errObj = cJSON_AddObjectToObject(root, "error");
    cJSON_AddStringToObject(errObj, "code", "NOT_FOUND");
    cJSON_AddStringToObject(errObj, "message", "Route not found");
    cJSON_AddStringToObject(root, "path", req->uri);
    cJSON_AddStringToObject(root, "method", method);

    if (s_lastInstance) {
        cJSON* routes = cJSON_AddArrayToObject(root, "registeredRoutes");
        size_t count = 0;
        for (const auto& r : s_lastInstance->m_routes) {
            if (count++ >= 256) break; // cap response size
            cJSON_AddItemToArray(routes, cJSON_CreateString(r.c_str()));
        }
        cJSON_AddNumberToObject(root, "totalRoutes", (double)s_lastInstance->m_routes.size());
    }

    char* out = cJSON_PrintUnformatted(root);
    if (!out) {
        cJSON_Delete(root);
        static constexpr const char* body = "{\"success\":false,\"error\":{\"code\":\"NOT_FOUND\",\"message\":\"Route not found\"}}";
        return httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
    }
    esp_err_t res = httpd_resp_send(req, out, HTTPD_RESP_USE_STRLEN);
    cJSON_free(out);
    cJSON_Delete(root);
    return res;
}

char* IdfHttpServer::readBody(httpd_req_t* req, size_t* outLen) const {
    if (!req || !outLen) return nullptr;
    *outLen = 0;

    const size_t total = req->content_len;
    if (total == 0) return nullptr;
    if (total > m_cfg.maxReqBodyBytes) return nullptr;

    char* buf = (char*)malloc(total + 1);
    if (!buf) return nullptr;

    size_t read = 0;
    while (read < total) {
        const int r = httpd_req_recv(req, buf + read, total - read);
        if (r <= 0) {
            free(buf);
            return nullptr;
        }
        read += (size_t)r;
    }
    buf[read] = '\0';
    *outLen = read;
    return buf;
}

esp_err_t IdfHttpServer::sendJson(httpd_req_t* req, int statusCode, cJSON* root) const {
    if (!req || !root) return ESP_FAIL;

    addCorsHeaders(req);
    httpd_resp_set_type(req, "application/json");

    // Status line (ESP-IDF expects full token, e.g. "200 OK")
    const char* statusLine = "200 OK";
    switch (statusCode) {
        case 200: statusLine = "200 OK"; break;
        case 201: statusLine = "201 Created"; break;
        case 204: statusLine = "204 No Content"; break;
        case 400: statusLine = "400 Bad Request"; break;
        case 401: statusLine = "401 Unauthorized"; break;
        case 404: statusLine = "404 Not Found"; break;
        case 429: statusLine = "429 Too Many Requests"; break;
        case 500: statusLine = "500 Internal Server Error"; break;
        default:  statusLine = "200 OK"; break;
    }
    httpd_resp_set_status(req, statusLine);

    char* out = cJSON_PrintUnformatted(root);
    if (!out) {
        return httpd_resp_send(req,
                               "{\"success\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"message\":\"JSON encode failed\"}}",
                               HTTPD_RESP_USE_STRLEN);
    }

    const esp_err_t res = httpd_resp_send(req, out, HTTPD_RESP_USE_STRLEN);
    cJSON_free(out);
    return res;
}

void IdfHttpServer::wsClientAdd(int fd) {
    if (fd < 0) return;
    if (wsClientHas(fd)) return;
    for (size_t i = 0; i < MAX_WS_CLIENTS; i++) {
        if (m_wsClientFds[i] < 0) {
            m_wsClientFds[i] = fd;
            return;
        }
    }
}

void IdfHttpServer::wsClientRemove(int fd) {
    for (size_t i = 0; i < MAX_WS_CLIENTS; i++) {
        if (m_wsClientFds[i] == fd) {
            m_wsClientFds[i] = -1;
        }
    }
}

bool IdfHttpServer::wsClientHas(int fd) const {
    for (size_t i = 0; i < MAX_WS_CLIENTS; i++) {
        if (m_wsClientFds[i] == fd) return true;
    }
    return false;
}

void IdfHttpServer::onWsConnect(int fd) {
    wsClientAdd(fd);
    if (m_onWsClientEvent) {
        m_onWsClientEvent(fd, true, m_wsCtx);
    }
}

void IdfHttpServer::onWsDisconnect(int fd) {
    wsClientRemove(fd);
    if (m_onWsClientEvent) {
        m_onWsClientEvent(fd, false, m_wsCtx);
    }
}

void IdfHttpServer::onWsData(httpd_req_t* req) {
    if (!req) return;
    if (!httpd_ws_get_fd_info(req->handle, httpd_req_to_sockfd(req))) return;

    httpd_ws_frame_t frame{};
    frame.type = HTTPD_WS_TYPE_TEXT;

    // Get length first
    if (httpd_ws_recv_frame(req, &frame, 0) != ESP_OK) {
        return;
    }
    if (frame.len == 0) return;
    if (frame.len > m_cfg.maxWsFrameBytes) return;

    char* buf = (char*)malloc(frame.len + 1);
    if (!buf) return;
    memset(buf, 0, frame.len + 1);
    frame.payload = (uint8_t*)buf;

    if (httpd_ws_recv_frame(req, &frame, frame.len) != ESP_OK) {
        free(buf);
        return;
    }
    buf[frame.len] = '\0';

    const int fd = httpd_req_to_sockfd(req);
    if (!wsClientHas(fd)) {
        onWsConnect(fd);
    }

    if (m_onWsMessage) {
        m_onWsMessage(fd, buf, frame.len, m_wsCtx);
    }

    free(buf);
}

esp_err_t IdfHttpServer::wsHandler(httpd_req_t* req) {
    if (!req) return ESP_FAIL;
    IdfHttpServer* self = (IdfHttpServer*)req->user_ctx;
    if (!self) return ESP_FAIL;

    if (req->method == HTTP_GET) {
        // Handshake / initial connect path
        const int fd = httpd_req_to_sockfd(req);
        self->onWsConnect(fd);
        return ESP_OK;
    }

    // Data frames
    self->onWsData(req);
    return ESP_OK;
}

void IdfHttpServer::wsBroadcastText(const char* msg, size_t len) {
    if (!m_server || !msg || len == 0) return;

    httpd_ws_frame_t frame{};
    frame.type = HTTPD_WS_TYPE_TEXT;
    frame.payload = (uint8_t*)msg;
    frame.len = len;

    for (size_t i = 0; i < MAX_WS_CLIENTS; i++) {
        const int fd = m_wsClientFds[i];
        if (fd < 0) continue;
        (void)httpd_ws_send_frame_async(m_server, fd, &frame);
    }
}

void IdfHttpServer::wsSendText(int clientFd, const char* msg, size_t len) {
    if (!m_server || clientFd < 0 || !msg || len == 0) return;

    httpd_ws_frame_t frame{};
    frame.type = HTTPD_WS_TYPE_TEXT;
    frame.payload = (uint8_t*)msg;
    frame.len = len;

    (void)httpd_ws_send_frame_async(m_server, clientFd, &frame);
}
