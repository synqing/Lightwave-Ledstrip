/**
 * UDP Packet Helpers Implementation
 */

#include "udp_packets.h"

// Simple FNV-1a hash for token strings
uint32_t lw_token_hash32(const char* token) {
    if (!token) return 0;
    
    uint32_t hash = 2166136261u;  // FNV offset basis
    
    while (*token) {
        hash ^= (uint32_t)(*token++);
        hash *= 16777619u;  // FNV prime
    }
    
    return hash;
}
