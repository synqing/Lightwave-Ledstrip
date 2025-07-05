#pragma once

#include <AsyncWebSocket.h>
#include <ArduinoJson.h>
#include "audio_sync.h"

// Audio sync instance
extern AudioSynq audioSynq;

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
        if (filename && audioSynq.loadAudioData(filename)) {
            response["status"] = "success";
            response["duration"] = audioSynq.getDuration();
        } else {
            response["status"] = "error";
            response["message"] = "Failed to load audio data";
        }
    }
    else if (strcmp(subCmd, "startSync") == 0) {
        unsigned long clientTime = doc["clientTime"] | 0;
        audioSynq.startPlayback(clientTime);
        response["status"] = "success";
        response["serverTime"] = millis();
    }
    else if (strcmp(subCmd, "stopSync") == 0) {
        audioSynq.stopPlayback();
        response["status"] = "success";
    }
    else if (strcmp(subCmd, "getStatus") == 0) {
        response["status"] = "success";
        response["isPlaying"] = audioSynq.isPlaying();
        response["isMicActive"] = audioSynq.isMicrophoneActive();
        response["source"] = audioSynq.isUsingMicrophone() ? "microphone" : "vp_decoder";
        response["currentTime"] = audioSynq.getCurrentTime();
        
        if (audioSynq.isPlaying() || audioSynq.isMicrophoneActive()) {
            auto frame = audioSynq.getCurrentFrame();
            response["bassEnergy"] = frame.bass_energy;
            response["midEnergy"] = frame.mid_energy;
            response["highEnergy"] = frame.high_energy;
            response["overallEnergy"] = frame.total_energy;
            response["hasBeat"] = frame.beat_detected;
        }
    }
    else if (strcmp(subCmd, "ping") == 0) {
        // Network latency measurement
        response["status"] = "success";
        response["clientTime"] = doc["clientTime"];
        response["serverTime"] = millis();
    }
    else if (strcmp(subCmd, "startMic") == 0) {
        if (audioSynq.startMicrophone()) {
            response["status"] = "success";
            response["message"] = "Microphone started";
        } else {
            response["status"] = "error";
            response["message"] = "Failed to start microphone";
        }
    }
    else if (strcmp(subCmd, "stopMic") == 0) {
        audioSynq.stopMicrophone();
        response["status"] = "success";
        response["message"] = "Microphone stopped";
    }
    else if (strcmp(subCmd, "setSource") == 0) {
        bool useMic = doc["useMicrophone"] | false;
        audioSynq.setAudioSource(useMic);
        response["status"] = "success";
        response["source"] = useMic ? "microphone" : "vp_decoder";
    }
    else {
        response["status"] = "error";
        response["message"] = "Unknown subCommand";
    }
    
    String output;
    serializeJson(response, output);
    client->text(output);
}