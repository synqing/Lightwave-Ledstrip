// Zone Energy Debug Test
// This standalone test helps diagnose why all zone energies are stuck at 1.0

#include <iostream>
#include <cmath>
#include <cstring>

// Forward declaration
void calculateAndPrintZones(float* bins, const char* description);

// Simulate the zone energy calculation
void testZoneCalculation() {
    // Simulate AGC output bins
    float agc_bins[96];
    
    // Test case 1: All bins at moderate level
    std::cout << "\n=== Test Case 1: Moderate uniform signal ===\n";
    for (int i = 0; i < 96; i++) {
        agc_bins[i] = 5.0f;  // Moderate AGC output
    }
    calculateAndPrintZones(agc_bins, "Uniform 5.0");
    
    // Test case 2: Descending signal
    std::cout << "\n=== Test Case 2: Descending signal ===\n";
    for (int i = 0; i < 96; i++) {
        agc_bins[i] = 10.0f * (1.0f - i / 96.0f);  // 10 down to ~0
    }
    calculateAndPrintZones(agc_bins, "Descending");
    
    // Test case 3: Bass-heavy signal
    std::cout << "\n=== Test Case 3: Bass-heavy signal ===\n";
    for (int i = 0; i < 96; i++) {
        agc_bins[i] = (i < 24) ? 20.0f : 1.0f;  // Strong bass
    }
    calculateAndPrintZones(agc_bins, "Bass-heavy");
    
    // Test case 4: High AGC values
    std::cout << "\n=== Test Case 4: High AGC output ===\n";
    for (int i = 0; i < 96; i++) {
        agc_bins[i] = 50.0f + (rand() % 50);  // 50-100 range
    }
    calculateAndPrintZones(agc_bins, "High AGC");
}

void calculateAndPrintZones(float* bins, const char* description) {
    float zones[8] = {0};
    float raw_zones[8] = {0};
    
    // Calculate raw zone averages
    for (int i = 0; i < 12; i++) raw_zones[0] += bins[i];
    raw_zones[0] = raw_zones[0] / 12.0f;
    
    for (int i = 12; i < 24; i++) raw_zones[1] += bins[i];
    raw_zones[1] = raw_zones[1] / 12.0f;
    
    for (int i = 24; i < 36; i++) raw_zones[2] += bins[i];
    raw_zones[2] = raw_zones[2] / 12.0f;
    
    for (int i = 36; i < 48; i++) raw_zones[3] += bins[i];
    raw_zones[3] = raw_zones[3] / 12.0f;
    
    for (int i = 48; i < 60; i++) raw_zones[4] += bins[i];
    raw_zones[4] = raw_zones[4] / 12.0f;
    
    for (int i = 60; i < 72; i++) raw_zones[5] += bins[i];
    raw_zones[5] = raw_zones[5] / 12.0f;
    
    for (int i = 72; i < 84; i++) raw_zones[6] += bins[i];
    raw_zones[6] = raw_zones[6] / 12.0f;
    
    for (int i = 84; i < 96; i++) raw_zones[7] += bins[i];
    raw_zones[7] = raw_zones[7] / 12.0f;
    
    // Apply boost factors
    const float boost_factors[8] = {
        2.0f, 1.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.2f, 1.5f
    };
    
    // Find max for normalization
    float max_zone = 0.0f;
    for (int z = 0; z < 8; z++) {
        float boosted = raw_zones[z] * boost_factors[z];
        if (boosted > max_zone) {
            max_zone = boosted;
        }
    }
    
    float zone_norm = (max_zone > 0.01f) ? (0.95f / max_zone) : 1.0f;
    
    // Apply normalization and clamp
    for (int z = 0; z < 8; z++) {
        zones[z] = raw_zones[z] * boost_factors[z] * zone_norm;
        zones[z] = (zones[z] < 0.0f) ? 0.0f : ((zones[z] > 1.0f) ? 1.0f : zones[z]);
    }
    
    // Print results
    std::cout << description << ":\n";
    std::cout << "  Input range: [" << bins[0] << " - " << bins[95] << "]\n";
    std::cout << "  Raw zones: ";
    for (int z = 0; z < 8; z++) std::cout << raw_zones[z] << " ";
    std::cout << "\n  Max zone (after boost): " << max_zone << ", Norm factor: " << zone_norm;
    std::cout << "\n  Final zones: ";
    for (int z = 0; z < 8; z++) std::cout << zones[z] << " ";
    std::cout << "\n";
}

int main() {
    std::cout << "Zone Energy Calculation Debug Test\n";
    std::cout << "==================================\n";
    testZoneCalculation();
    return 0;
}