#pragma once

#include <AsyncWebSocket.h>
#include <ArduinoJson.h>

// Audio sync instance
extern class AudioSync audioSync;

/**
 * Handle audio-related WebSocket commands
 */
inline void handleAudioCommand(AsyncWebSocketClient* client, const JsonDocument& doc) {
    const char* subCmd = doc["subCommand"];
    
    if (!subCmd) {
        // Send error response
        DynamicJsonDocument response(256);
        response["type"] = "audio";
        response["status"] = "error";
        response["message"] = "Missing subCommand";
        
        String output;
        serializeJson(response, output);
        client->text(output);
        return;
    }
    
    DynamicJsonDocument response(512);
    response["type"] = "audio";
    response["subCommand"] = subCmd;
    
    if (strcmp(subCmd, "loadData") == 0) {
        const char* filename = doc["filename"];
        if (filename && audioSync.loadAudioData(filename)) {
            response["status"] = "success";
            response["duration"] = audioSync.getDuration();
        } else {
            response["status"] = "error";
            response["message"] = "Failed to load audio data";
        }
    }
    else if (strcmp(subCmd, "startSync") == 0) {
        unsigned long clientTime = doc["clientTime"] | 0;
        audioSync.startPlayback(clientTime);
        response["status"] = "success";
        response["serverTime"] = millis();
    }
    else if (strcmp(subCmd, "stopSync") == 0) {
        audioSync.stopPlayback();
        response["status"] = "success";
    }
    else if (strcmp(subCmd, "getStatus") == 0) {
        response["status"] = "success";
        response["isPlaying"] = audioSync.isPlaying();
        response["currentTime"] = audioSync.getCurrentTime();
        
        if (audioSync.isPlaying()) {
            auto frame = audioSync.getCurrentFrame();
            response["bassEnergy"] = frame.bass_energy;
            response["midEnergy"] = frame.mid_energy;
            response["highEnergy"] = frame.high_energy;
        }
    }
    else if (strcmp(subCmd, "ping") == 0) {
        // Network latency measurement
        response["status"] = "success";
        response["clientTime"] = doc["clientTime"];
        response["serverTime"] = millis();
    }
    else {
        response["status"] = "error";
        response["message"] = "Unknown subCommand";
    }
    
    String output;
    serializeJson(response, output);
    client->text(output);
}