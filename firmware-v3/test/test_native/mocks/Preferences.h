#pragma once

#ifdef NATIVE_BUILD
#include <cstdint>
#include <string>
#include <unordered_map>

class Preferences {
public:
    bool begin(const char*, bool = false) { return true; }
    void end() {}

    uint8_t getUChar(const char* key, uint8_t defaultValue = 0) {
        auto it = s_ucharStore.find(key);
        return it == s_ucharStore.end() ? defaultValue : it->second;
    }
    uint16_t getUShort(const char* key, uint16_t defaultValue = 0) {
        auto it = s_ushortStore.find(key);
        return it == s_ushortStore.end() ? defaultValue : it->second;
    }
    uint32_t getUInt(const char* key, uint32_t defaultValue = 0) {
        auto it = s_uintStore.find(key);
        return it == s_uintStore.end() ? defaultValue : it->second;
    }
    float getFloat(const char* key, float defaultValue = 0.0f) {
        auto it = s_floatStore.find(key);
        return it == s_floatStore.end() ? defaultValue : it->second;
    }
    bool getBool(const char* key, bool defaultValue = false) {
        auto it = s_boolStore.find(key);
        return it == s_boolStore.end() ? defaultValue : it->second;
    }

    void putUChar(const char* key, uint8_t value) { s_ucharStore[key] = value; }
    void putUShort(const char* key, uint16_t value) { s_ushortStore[key] = value; }
    void putUInt(const char* key, uint32_t value) { s_uintStore[key] = value; }
    void putFloat(const char* key, float value) { s_floatStore[key] = value; }
    void putBool(const char* key, bool value) { s_boolStore[key] = value; }

    static void resetMockStore() {
        s_ucharStore.clear();
        s_ushortStore.clear();
        s_uintStore.clear();
        s_floatStore.clear();
        s_boolStore.clear();
    }

private:
    static inline std::unordered_map<std::string, uint8_t> s_ucharStore;
    static inline std::unordered_map<std::string, uint16_t> s_ushortStore;
    static inline std::unordered_map<std::string, uint32_t> s_uintStore;
    static inline std::unordered_map<std::string, float> s_floatStore;
    static inline std::unordered_map<std::string, bool> s_boolStore;
};
#else
#include <Preferences.h>
#endif
