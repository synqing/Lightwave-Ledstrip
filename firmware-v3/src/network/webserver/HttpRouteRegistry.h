// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file HttpRouteRegistry.h
 * @brief HTTP route registration abstraction
 *
 * Provides a clean interface for registering HTTP routes without exposing
 * AsyncWebServer implementation details to handler modules.
 */

#pragma once

#include <ESPAsyncWebServer.h>
#include <functional>

namespace lightwaveos {
namespace network {
namespace webserver {

/**
 * @brief HTTP route registry abstraction
 *
 * Wraps AsyncWebServer route registration to provide a cleaner interface
 * for handler modules.
 */
class HttpRouteRegistry {
public:
    explicit HttpRouteRegistry(AsyncWebServer* server) : m_server(server) {}
    
    /**
     * @brief Register a GET route
     */
    void onGet(const char* path, ArRequestHandlerFunction handler) {
        m_server->on(path, HTTP_GET, handler);
    }
    
    /**
     * @brief Register a POST route without body handling
     */
    void onPost(const char* path, ArRequestHandlerFunction handler) {
        m_server->on(path, HTTP_POST, handler);
    }

    /**
     * @brief Register a POST route with body handler
     */
    void onPost(const char* path,
                ArRequestHandlerFunction onRequest,
                ArUploadHandlerFunction onUpload,
                ArBodyHandlerFunction onBody) {
        m_server->on(path, HTTP_POST, onRequest, onUpload, onBody);
    }

    /**
     * @brief Register a POST route with upload handler only (no body handler)
     */
    void onPost(const char* path,
                ArRequestHandlerFunction onRequest,
                ArUploadHandlerFunction onUpload) {
        m_server->on(path, HTTP_POST, onRequest, onUpload, nullptr);
    }
    
    /**
     * @brief Register a PUT route with body handler
     */
    void onPut(const char* path,
               ArRequestHandlerFunction onRequest,
               ArUploadHandlerFunction onUpload,
               ArBodyHandlerFunction onBody) {
        m_server->on(path, HTTP_PUT, onRequest, onUpload, onBody);
    }

    /**
     * @brief Register a PUT route without body handling
     */
    void onPut(const char* path, ArRequestHandlerFunction handler) {
        m_server->on(path, HTTP_PUT, handler);
    }
    
    /**
     * @brief Register a PATCH route with body handler
     */
    void onPatch(const char* path,
                 ArRequestHandlerFunction onRequest,
                 ArUploadHandlerFunction onUpload,
                 ArBodyHandlerFunction onBody) {
        m_server->on(path, HTTP_PATCH, onRequest, onUpload, onBody);
    }
    
    /**
     * @brief Register a DELETE route
     */
    void onDelete(const char* path, ArRequestHandlerFunction handler) {
        m_server->on(path, HTTP_DELETE, handler);
    }
    
    /**
     * @brief Register a route handler for any method
     */
    void onNotFound(ArRequestHandlerFunction handler) {
        m_server->onNotFound(handler);
    }
    
    // ========================================================================
    // Regex Route Support
    // ========================================================================
    
    /**
     * @brief Register a GET route with regex pattern
     */
    void onGetRegex(const char* pattern, ArRequestHandlerFunction handler) {
        m_server->on(pattern, HTTP_GET, handler);
    }
    
    /**
     * @brief Register a POST route with regex pattern (no body handler)
     */
    void onPostRegex(const char* pattern, ArRequestHandlerFunction handler) {
        m_server->on(pattern, HTTP_POST, handler);
    }
    
    /**
     * @brief Register a POST route with regex pattern and body handler
     */
    void onPostRegex(const char* pattern,
                     ArRequestHandlerFunction onRequest,
                     ArUploadHandlerFunction onUpload,
                     ArBodyHandlerFunction onBody) {
        m_server->on(pattern, HTTP_POST, onRequest, onUpload, onBody);
    }
    
    /**
     * @brief Register a PUT route with regex pattern (no body handler)
     */
    void onPutRegex(const char* pattern, ArRequestHandlerFunction handler) {
        m_server->on(pattern, HTTP_PUT, handler);
    }
    
    /**
     * @brief Register a PUT route with regex pattern and body handler
     */
    void onPutRegex(const char* pattern,
                    ArRequestHandlerFunction onRequest,
                    ArUploadHandlerFunction onUpload,
                    ArBodyHandlerFunction onBody) {
        m_server->on(pattern, HTTP_PUT, onRequest, onUpload, onBody);
    }
    
    /**
     * @brief Register a PATCH route with regex pattern and body handler
     */
    void onPatchRegex(const char* pattern,
                      ArRequestHandlerFunction onRequest,
                      ArUploadHandlerFunction onUpload,
                      ArBodyHandlerFunction onBody) {
        m_server->on(pattern, HTTP_PATCH, onRequest, onUpload, onBody);
    }
    
    /**
     * @brief Register a DELETE route with regex pattern
     */
    void onDeleteRegex(const char* pattern, ArRequestHandlerFunction handler) {
        m_server->on(pattern, HTTP_DELETE, handler);
    }
    
private:
    AsyncWebServer* m_server;
};

} // namespace webserver
} // namespace network
} // namespace lightwaveos

