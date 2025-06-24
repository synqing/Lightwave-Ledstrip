#ifndef PLASMA_FIELD_EFFECT_H
#define PLASMA_FIELD_EFFECT_H

#include "LightGuideBase.h"

#if LED_STRIPS_MODE && FEATURE_LIGHT_GUIDE_MODE

// Plasma particle structure
struct PlasmaParticle {
    float x, y;              // Position in plate coordinates (0.0 - 1.0)
    float vx, vy;            // Velocity components
    float charge;            // Particle charge (-1.0 to +1.0)
    float energy;            // Particle energy level
    uint8_t hue;            // Color hue
    bool active;            // Particle active state
    uint32_t birth_time;    // When particle was created
};

class PlasmaFieldEffect : public LightGuideEffectBase {
private:
    // Plasma simulation parameters
    static constexpr uint8_t MAX_PARTICLES = 12;
    static constexpr float FIELD_STRENGTH = 0.8f;
    static constexpr float PARTICLE_DECAY_TIME = 3000.0f;  // 3 seconds
    static constexpr float EDGE_REPULSION = 0.5f;
    
    // Particle system
    PlasmaParticle particles[MAX_PARTICLES];
    uint8_t active_particle_count = 0;
    uint32_t last_spawn_time = 0;
    uint32_t spawn_interval = 300;  // milliseconds between spawns
    
    // Field visualization
    float field_intensity_map[32][16];  // Reduced resolution for performance
    uint32_t last_field_calc = 0;
    
    // Animation parameters
    float field_animation_speed = 1.0f;
    float charge_separation = 1.0f;
    float turbulence_strength = 0.3f;
    float field_decay_rate = 0.1f;
    
public:
    PlasmaFieldEffect() : LightGuideEffectBase("Plasma Field", 200, 20, 30) {
        // Initialize particle system
        for (uint8_t i = 0; i < MAX_PARTICLES; i++) {
            particles[i].active = false;
        }
        
        // Clear field map
        memset(field_intensity_map, 0, sizeof(field_intensity_map));
        
        // Set sync mode to interference for best plasma effect
        sync_mode = LightGuideSyncMode::INTERFERENCE;
    }
    
    void renderLightGuideEffect() override {
        uint32_t current_time = millis();
        
        // Update particle system
        updateParticles(current_time);
        
        // Spawn new particles
        spawnParticles(current_time);
        
        // Calculate electric field
        calculateElectricField();
        
        // Render field to LEDs
        renderFieldToLEDs();
        
        // Add edge electrode effects
        renderElectrodes();
    }
    
private:
    void updateParticles(uint32_t current_time) {
        for (uint8_t i = 0; i < MAX_PARTICLES; i++) {
            if (!particles[i].active) continue;
            
            PlasmaParticle& p = particles[i];
            
            // Check particle lifetime
            if (current_time - p.birth_time > PARTICLE_DECAY_TIME) {
                p.active = false;
                active_particle_count--;
                continue;
            }
            
            // Calculate forces from other particles (simplified)
            float fx = 0.0f, fy = 0.0f;
            
            for (uint8_t j = 0; j < MAX_PARTICLES; j++) {
                if (i == j || !particles[j].active) continue;
                
                PlasmaParticle& other = particles[j];
                float dx = p.x - other.x;
                float dy = p.y - other.y;
                float distance_sq = dx * dx + dy * dy + 0.01f;  // Avoid division by zero
                float distance = sqrt(distance_sq);
                
                // Coulomb force (simplified)
                float force_magnitude = (p.charge * other.charge) / distance_sq;
                force_magnitude *= FIELD_STRENGTH * 0.1f;
                
                fx += force_magnitude * (dx / distance);
                fy += force_magnitude * (dy / distance);
            }
            
            // Add edge repulsion to keep particles in bounds
            if (p.x < 0.1f) fx += EDGE_REPULSION * (0.1f - p.x);
            if (p.x > 0.9f) fx -= EDGE_REPULSION * (p.x - 0.9f);
            if (p.y < 0.1f) fy += EDGE_REPULSION * (0.1f - p.y);
            if (p.y > 0.9f) fy -= EDGE_REPULSION * (p.y - 0.9f);
            
            // Add turbulence
            float time_factor = current_time * 0.001f * field_animation_speed;
            fx += sin(p.x * 10.0f + time_factor) * turbulence_strength * 0.1f;
            fy += cos(p.y * 8.0f + time_factor * 1.3f) * turbulence_strength * 0.1f;
            
            // Update velocity and position
            p.vx += fx * 0.01f;
            p.vy += fy * 0.01f;
            
            // Apply velocity damping
            p.vx *= 0.95f;
            p.vy *= 0.95f;
            
            // Update position
            p.x += p.vx * 0.02f;
            p.y += p.vy * 0.02f;
            
            // Clamp to bounds
            p.x = constrain(p.x, 0.0f, 1.0f);
            p.y = constrain(p.y, 0.0f, 1.0f);
            
            // Update energy based on velocity
            p.energy = sqrt(p.vx * p.vx + p.vy * p.vy) * 10.0f;
            p.energy = constrain(p.energy, 0.1f, 2.0f);
        }
    }
    
    void spawnParticles(uint32_t current_time) {
        if (current_time - last_spawn_time < spawn_interval) return;
        if (active_particle_count >= MAX_PARTICLES) return;
        
        // Find inactive particle slot
        for (uint8_t i = 0; i < MAX_PARTICLES; i++) {
            if (!particles[i].active) {
                PlasmaParticle& p = particles[i];
                
                // Spawn from edges with opposite charges
                bool from_edge1 = random(2) == 0;
                
                if (from_edge1) {
                    p.x = random(1000) / 1000.0f;  // Random position along edge
                    p.y = 0.05f;  // Near Edge 1
                    p.charge = charge_separation;  // Positive charge
                } else {
                    p.x = random(1000) / 1000.0f;
                    p.y = 0.95f;  // Near Edge 2  
                    p.charge = -charge_separation; // Negative charge
                }
                
                // Random initial velocity
                p.vx = (random(200) - 100) / 1000.0f;
                p.vy = (random(200) - 100) / 1000.0f;
                
                p.energy = 1.0f;
                p.hue = gHue + random(60);
                p.active = true;
                p.birth_time = current_time;
                
                active_particle_count++;
                last_spawn_time = current_time;
                break;
            }
        }
    }
    
    void calculateElectricField() {
        uint32_t current_time = millis();
        
        // Optimize: only recalculate field every 32ms
        if (current_time - last_field_calc < 32) return;
        last_field_calc = current_time;
        
        // Clear field map
        for (uint8_t x = 0; x < 32; x++) {
            for (uint8_t y = 0; y < 16; y++) {
                field_intensity_map[x][y] = 0.0f;
            }
        }
        
        // Calculate field at each point
        for (uint8_t x = 0; x < 32; x++) {
            for (uint8_t y = 0; y < 16; y++) {
                float fx = x / 32.0f;
                float fy = y / 16.0f;
                float field_strength = 0.0f;
                
                // Contribution from each particle
                for (uint8_t i = 0; i < MAX_PARTICLES; i++) {
                    if (!particles[i].active) continue;
                    
                    PlasmaParticle& p = particles[i];
                    float dx = fx - p.x;
                    float dy = fy - p.y;
                    float distance_sq = dx * dx + dy * dy + 0.01f;
                    
                    // Electric field magnitude (simplified)
                    float field_contrib = abs(p.charge * p.energy) / distance_sq;
                    field_strength += field_contrib;
                }
                
                // Apply decay
                field_strength *= exp(-field_decay_rate);
                field_intensity_map[x][y] = constrain(field_strength, 0.0f, 2.0f);
            }
        }
    }
    
    void renderFieldToLEDs() {
        // Render field to Edge 1 (Strip 1)
        for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            float position = (float)i / HardwareConfig::STRIP_LENGTH;
            
            // Sample field at multiple depths for layered effect
            float total_intensity = 0.0f;
            uint8_t average_hue = 0;
            
            for (uint8_t depth = 0; depth < 4; depth++) {
                float y_pos = depth / 4.0f;
                uint8_t map_x = constrain(position * 32, 0, 31);
                uint8_t map_y = constrain(y_pos * 16, 0, 15);
                
                float field_value = field_intensity_map[map_x][map_y];
                total_intensity += field_value * (1.0f - depth * 0.2f);  // Depth attenuation
            }
            
            total_intensity /= 4.0f;
            
            // Find nearest particle for color influence
            float nearest_dist = 1.0f;
            uint8_t nearest_hue = gHue;
            
            for (uint8_t p = 0; p < MAX_PARTICLES; p++) {
                if (!particles[p].active) continue;
                
                float dx = position - particles[p].x;
                float dy = 0.0f - particles[p].y;  // Edge 1 is at y=0
                float dist = sqrt(dx * dx + dy * dy);
                
                if (dist < nearest_dist) {
                    nearest_dist = dist;
                    nearest_hue = particles[p].hue;
                }
            }
            
            // Create plasma color
            uint8_t hue = nearest_hue + (position * 30);
            uint8_t saturation = 255 - (nearest_dist * 100);  // Less saturated farther from particles
            float intensity = total_intensity * 0.8f;
            
            CRGB color = getLightGuideColor(hue, intensity);
            color = CHSV(hue, saturation, color.r);  // Use HSV for better plasma colors
            
            setEdge1LED(i, color);
        }
        
        // Render field to Edge 2 (Strip 2) - similar but from opposite perspective
        for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            float position = (float)i / HardwareConfig::STRIP_LENGTH;
            float total_intensity = 0.0f;
            
            for (uint8_t depth = 0; depth < 4; depth++) {
                float y_pos = 1.0f - (depth / 4.0f);  // From opposite edge
                uint8_t map_x = constrain(position * 32, 0, 31);
                uint8_t map_y = constrain(y_pos * 16, 0, 15);
                
                float field_value = field_intensity_map[map_x][map_y];
                total_intensity += field_value * (1.0f - depth * 0.2f);
            }
            
            total_intensity /= 4.0f;
            
            // Find nearest particle for color
            float nearest_dist = 1.0f;
            uint8_t nearest_hue = gHue + 40;  // Hue offset for Edge 2
            
            for (uint8_t p = 0; p < MAX_PARTICLES; p++) {
                if (!particles[p].active) continue;
                
                float dx = position - particles[p].x;
                float dy = 1.0f - particles[p].y;  // Edge 2 is at y=1
                float dist = sqrt(dx * dx + dy * dy);
                
                if (dist < nearest_dist) {
                    nearest_dist = dist;
                    nearest_hue = particles[p].hue + 40;
                }
            }
            
            uint8_t hue = nearest_hue + (position * 30);
            uint8_t saturation = 255 - (nearest_dist * 100);
            float intensity = total_intensity * 0.8f;
            
            CRGB color = getLightGuideColor(hue, intensity);
            color = CHSV(hue, saturation, color.r);
            
            setEdge2LED(i, color);
        }
    }
    
    void renderElectrodes() {
        // Add electrode glow effects at the edges
        uint32_t time = millis();
        float electrode_pulse = sin(time * 0.003f) * 0.3f + 0.7f;
        
        // Edge 1 electrode effect (positive)
        for (uint16_t i = 0; i < 5; i++) {  // First 5 LEDs
            CRGB electrode_color = CHSV(0, 255, electrode_pulse * 255);  // Red glow
            setEdge1LED(i, blend(leds[i], electrode_color, 128));
        }
        
        for (uint16_t i = HardwareConfig::STRIP_LENGTH - 5; i < HardwareConfig::STRIP_LENGTH; i++) {  // Last 5 LEDs
            CRGB electrode_color = CHSV(0, 255, electrode_pulse * 255);
            setEdge1LED(i, blend(leds[i], electrode_color, 128));
        }
        
        // Edge 2 electrode effect (negative)
        for (uint16_t i = 0; i < 5; i++) {
            CRGB electrode_color = CHSV(160, 255, electrode_pulse * 255);  // Blue glow
            setEdge2LED(i, blend(leds[HardwareConfig::STRIP1_LED_COUNT + i], electrode_color, 128));
        }
        
        for (uint16_t i = HardwareConfig::STRIP_LENGTH - 5; i < HardwareConfig::STRIP_LENGTH; i++) {
            CRGB electrode_color = CHSV(160, 255, electrode_pulse * 255);
            setEdge2LED(i, blend(leds[HardwareConfig::STRIP1_LED_COUNT + i], electrode_color, 128));
        }
    }
    
public:
    // Parameter control methods
    void setFieldAnimationSpeed(float speed) {
        field_animation_speed = constrain(speed, 0.1f, 3.0f);
    }
    
    void setChargeSeparation(float separation) {
        charge_separation = constrain(separation, 0.1f, 2.0f);
    }
    
    void setTurbulenceStrength(float strength) {
        turbulence_strength = constrain(strength, 0.0f, 1.0f);
    }
    
    void setSpawnRate(float rate) {
        spawn_interval = constrain(1000.0f / rate, 100, 2000);  // Convert rate to interval
    }
    
    // Getters
    float getFieldAnimationSpeed() const { return field_animation_speed; }
    float getChargeSeparation() const { return charge_separation; }
    float getTurbulenceStrength() const { return turbulence_strength; }
    uint8_t getActiveParticleCount() const { return active_particle_count; }
};

#endif // LED_STRIPS_MODE && FEATURE_LIGHT_GUIDE_MODE

#endif // PLASMA_FIELD_EFFECT_H