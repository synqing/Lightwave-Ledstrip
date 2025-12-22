#ifndef FLASH_DATA_MANAGER_H
#define FLASH_DATA_MANAGER_H

#include <Arduino.h>
#include <esp_partition.h>
#include "../config/hardware_config.h"

/**
 * Flash Data Manager - Utilize custom data partition for pre-calculated effects
 * 
 * With our custom partition table, we have:
 * - 14.6MB app partition (vs 6.25MB default) for code + const data
 * - 1MB custom data partition for runtime-loadable pre-calculated effects
 * 
 * This allows us to store massive amounts of pre-calculated data:
 * - Pre-rendered effect sequences
 * - Large color gradient tables
 * - Complex transition animations
 * - HDR lookup tables
 * - Pre-calculated physics simulations
 */

class FlashDataManager {
public:
    // Data partition constants
    static constexpr size_t DATA_PARTITION_SIZE = 0x100000;  // 1MB
    static constexpr const char* DATA_PARTITION_LABEL = "data";
    static constexpr uint8_t DATA_PARTITION_TYPE = 0x99;     // Custom type
    
    // Pre-calculated data types that can be stored
    enum DataType {
        EFFECT_SEQUENCE = 0x01,
        COLOR_GRADIENT = 0x02,
        TRANSITION_FRAMES = 0x03,
        HDR_LUT = 0x04,
        PHYSICS_SIMULATION = 0x05,
        WAVE_PATTERN = 0x06,
        PALETTE_SEQUENCE = 0x07
    };
    
    // Header for stored data blocks
    struct DataHeader {
        uint32_t magic;       // 0xLED5DA7A
        uint16_t type;        // DataType enum
        uint16_t version;     // Format version
        uint32_t size;        // Data size in bytes
        uint32_t checksum;    // Simple checksum
        char name[16];        // Human-readable name
    };
    
    // Initialize and verify data partition
    bool init() {
        partition = esp_partition_find_first(
            ESP_PARTITION_TYPE_DATA,
            (esp_partition_subtype_t)DATA_PARTITION_TYPE,
            DATA_PARTITION_LABEL
        );
        
        if (!partition) {
            Serial.println("❌ Custom data partition not found!");
            return false;
        }
        
        Serial.printf("✅ Data partition found: %s, size: %d KB\n", 
                      partition->label, partition->size / 1024);
        
        return verifyPartition();
    }
    
    // Load pre-calculated effect sequence from flash
    bool loadEffectSequence(const char* name, uint8_t* buffer, size_t maxSize) {
        return loadData(EFFECT_SEQUENCE, name, buffer, maxSize);
    }
    
    // Load HDR lookup table from flash
    bool loadHDRLut(const char* name, uint16_t* buffer, size_t maxSize) {
        return loadData(HDR_LUT, name, (uint8_t*)buffer, maxSize);
    }
    
    // Store pre-calculated data to flash (for development/setup)
    bool storeData(DataType type, const char* name, const uint8_t* data, size_t size) {
        if (!partition || size > DATA_PARTITION_SIZE - sizeof(DataHeader)) {
            return false;
        }
        
        DataHeader header;
        header.magic = 0xLED5DA7A;
        header.type = type;
        header.version = 1;
        header.size = size;
        header.checksum = calculateChecksum(data, size);
        strncpy(header.name, name, sizeof(header.name) - 1);
        
        // Find free space (simple allocation)
        size_t offset = findFreeSpace(sizeof(DataHeader) + size);
        if (offset == SIZE_MAX) {
            Serial.println("❌ No free space in data partition");
            return false;
        }
        
        // Write header + data
        esp_err_t err = esp_partition_write(partition, offset, &header, sizeof(header));
        if (err != ESP_OK) return false;
        
        err = esp_partition_write(partition, offset + sizeof(header), data, size);
        return err == ESP_OK;
    }
    
    // Get available space in data partition
    size_t getAvailableSpace() {
        // Simple implementation - could be improved with proper allocation table
        return DATA_PARTITION_SIZE - usedSpace;
    }
    
    // List all stored data entries
    void listStoredData() {
        Serial.println("\n📦 Stored Data in Flash:");
        Serial.println("Type | Name            | Size  ");
        Serial.println("-----|-----------------|-------");
        
        size_t offset = 0;
        DataHeader header;
        
        while (offset < DATA_PARTITION_SIZE - sizeof(DataHeader)) {
            esp_partition_read(partition, offset, &header, sizeof(header));
            
            if (header.magic != 0xLED5DA7A) {
                offset += 4096;  // Skip to next sector
                continue;
            }
            
            const char* typeName = getTypeName(header.type);
            Serial.printf("%-4s | %-15s | %5d\n", 
                          typeName, header.name, header.size);
            
            offset += sizeof(header) + header.size;
            offset = (offset + 4095) & ~4095;  // Align to 4KB
        }
    }
    
private:
    const esp_partition_t* partition = nullptr;
    size_t usedSpace = 0;
    
    bool verifyPartition() {
        // Quick scan to determine used space
        size_t offset = 0;
        DataHeader header;
        
        while (offset < DATA_PARTITION_SIZE - sizeof(DataHeader)) {
            esp_partition_read(partition, offset, &header, sizeof(header));
            
            if (header.magic == 0xLED5DA7A) {
                usedSpace = offset + sizeof(header) + header.size;
                offset = usedSpace;
                offset = (offset + 4095) & ~4095;  // Align to 4KB
            } else {
                break;
            }
        }
        
        Serial.printf("📊 Data partition usage: %d/%d KB\n", 
                      usedSpace / 1024, DATA_PARTITION_SIZE / 1024);
        return true;
    }
    
    bool loadData(DataType type, const char* name, uint8_t* buffer, size_t maxSize) {
        size_t offset = 0;
        DataHeader header;
        
        while (offset < DATA_PARTITION_SIZE - sizeof(DataHeader)) {
            esp_partition_read(partition, offset, &header, sizeof(header));
            
            if (header.magic != 0xLED5DA7A) {
                offset += 4096;
                continue;
            }
            
            if (header.type == type && strcmp(header.name, name) == 0) {
                if (header.size > maxSize) {
                    Serial.printf("❌ Data too large: %d > %d\n", header.size, maxSize);
                    return false;
                }
                
                esp_partition_read(partition, offset + sizeof(header), buffer, header.size);
                
                // Verify checksum
                uint32_t checksum = calculateChecksum(buffer, header.size);
                if (checksum != header.checksum) {
                    Serial.println("❌ Checksum mismatch!");
                    return false;
                }
                
                return true;
            }
            
            offset += sizeof(header) + header.size;
            offset = (offset + 4095) & ~4095;  // Align to 4KB
        }
        
        return false;
    }
    
    size_t findFreeSpace(size_t size) {
        // Simple first-fit allocation
        size_t alignedSize = (size + 4095) & ~4095;  // Align to 4KB
        
        if (usedSpace + alignedSize <= DATA_PARTITION_SIZE) {
            return (usedSpace + 4095) & ~4095;  // Return next aligned offset
        }
        
        return SIZE_MAX;  // No space
    }
    
    uint32_t calculateChecksum(const uint8_t* data, size_t size) {
        uint32_t sum = 0;
        for (size_t i = 0; i < size; i++) {
            sum = (sum << 1) ^ data[i];
        }
        return sum;
    }
    
    const char* getTypeName(uint16_t type) {
        switch (type) {
            case EFFECT_SEQUENCE: return "FX";
            case COLOR_GRADIENT: return "GRAD";
            case TRANSITION_FRAMES: return "TRAN";
            case HDR_LUT: return "HDR";
            case PHYSICS_SIMULATION: return "PHYS";
            case WAVE_PATTERN: return "WAVE";
            case PALETTE_SEQUENCE: return "PAL";
            default: return "UNK";
        }
    }
};

// Global instance
extern FlashDataManager flashData;

/**
 * Example Usage:
 * 
 * // In setup()
 * flashData.init();
 * 
 * // Load pre-calculated wave pattern
 * uint8_t waveData[1024];
 * if (flashData.loadEffectSequence("ocean_wave", waveData, sizeof(waveData))) {
 *     // Use pre-calculated wave data
 * }
 * 
 * // Store new pre-calculated data (during development)
 * uint16_t hdrLut[256];
 * calculateHDRLut(hdrLut);  // Your calculation function
 * flashData.storeData(FlashDataManager::HDR_LUT, "hdr_sunset", 
 *                     (uint8_t*)hdrLut, sizeof(hdrLut));
 */

#endif // FLASH_DATA_MANAGER_H