#ifndef VOLUMETRIC_DISPLAY_EFFECT_H
#define VOLUMETRIC_DISPLAY_EFFECT_H

#include "LightGuideBase.h"

#if LED_STRIPS_MODE && FEATURE_LIGHT_GUIDE_MODE

// 3D object structure for volumetric display
struct VolumetricObject {
    float x, y, z;           // Position in 3D space (0.0 - 1.0)
    float vx, vy, vz;        // Velocity components
    float size;              // Object size
    float intensity;         // Light intensity
    uint8_t hue;            // Object color hue
    uint8_t shape_type;     // 0=sphere, 1=cube, 2=cylinder, 3=torus
    bool active;            // Object active state
    uint32_t birth_time;    // Creation timestamp
    float rotation;         // Object rotation angle
    float rotation_speed;   // Rotation velocity
};

class VolumetricDisplayEffect : public LightGuideEffectBase {
private:
    // Volumetric parameters
    static constexpr uint8_t MAX_OBJECTS = 8;
    static constexpr float DEPTH_LAYERS = 10.0f;
    static constexpr float PERSPECTIVE_STRENGTH = 0.7f;
    static constexpr float FOG_DENSITY = 0.15f;
    
    // 3D objects
    VolumetricObject objects[MAX_OBJECTS];
    uint8_t active_object_count = 0;
    
    // Depth buffer for Z-sorting (reduced resolution for performance)
    struct DepthPixel {
        float depth;
        float intensity;
        uint8_t hue;
    };
    DepthPixel depth_buffer[80][20];  // 80x20 depth buffer
    
    // Animation parameters
    float camera_z = 0.5f;           // Camera Z position
    float fog_start = 0.3f;          // Fog start distance
    float fog_end = 1.0f;            // Fog end distance
    float object_spawn_rate = 0.02f; // Objects per frame
    float depth_exaggeration = 1.5f; // How much to exaggerate depth
    
    // Movement patterns
    uint8_t movement_pattern = 0;    // 0=float, 1=orbit, 2=spiral, 3=bounce
    float pattern_speed = 1.0f;
    
public:
    VolumetricDisplayEffect() : LightGuideEffectBase("Volumetric", 160, 18, 22) {
        // Initialize object system
        for (uint8_t i = 0; i < MAX_OBJECTS; i++) {
            objects[i].active = false;
        }
        
        // Clear depth buffer
        clearDepthBuffer();
        
        // Set cooperative sync mode for best depth effect
        sync_mode = LightGuideSyncMode::COOPERATIVE;
        
        // Spawn initial objects
        spawnInitialObjects();
    }
    
    void renderLightGuideEffect() override {
        // Update object animations
        updateObjects();
        
        // Manage object lifecycle
        manageObjectLifecycle();
        
        // Clear and render to depth buffer
        clearDepthBuffer();
        renderObjectsToDepthBuffer();
        
        // Convert depth buffer to LED output
        renderDepthBufferToLEDs();
        
        // Add atmospheric effects
        addAtmosphericEffects();
    }
    
private:
    void clearDepthBuffer() {
        for (uint8_t x = 0; x < 80; x++) {
            for (uint8_t y = 0; y < 20; y++) {
                depth_buffer[x][y].depth = 1.0f;      // Far depth
                depth_buffer[x][y].intensity = 0.0f;
                depth_buffer[x][y].hue = 0;
            }
        }
    }
    
    void updateObjects() {
        uint32_t current_time = millis();
        float time_factor = current_time * 0.001f * pattern_speed;
        
        for (uint8_t i = 0; i < MAX_OBJECTS; i++) {
            if (!objects[i].active) continue;
            
            VolumetricObject& obj = objects[i];
            
            // Update rotation
            obj.rotation += obj.rotation_speed * 0.02f;
            
            // Apply movement pattern
            switch (movement_pattern) {
                case 0: // Float
                    obj.x += obj.vx * 0.01f;
                    obj.y += obj.vy * 0.01f;
                    obj.z += obj.vz * 0.005f;
                    break;
                    
                case 1: // Orbit
                    {
                        float orbit_radius = 0.3f;
                        float orbit_speed = time_factor * (i + 1) * 0.5f;
                        obj.x = 0.5f + cos(orbit_speed) * orbit_radius;
                        obj.y = 0.5f + sin(orbit_speed) * orbit_radius;
                        obj.z = 0.5f + sin(orbit_speed * 0.3f) * 0.2f;
                    }
                    break;
                    
                case 2: // Spiral
                    {
                        float spiral_time = time_factor + i * 0.5f;
                        obj.x = 0.5f + cos(spiral_time * 2.0f) * (0.4f - spiral_time * 0.05f);
                        obj.y = 0.5f + sin(spiral_time * 2.0f) * (0.4f - spiral_time * 0.05f);
                        obj.z = fmod(spiral_time * 0.1f, 1.0f);
                    }
                    break;
                    
                case 3: // Bounce
                    obj.x += obj.vx * 0.02f;
                    obj.y += obj.vy * 0.02f;
                    obj.z += obj.vz * 0.01f;
                    
                    // Bounce off boundaries
                    if (obj.x <= 0.0f || obj.x >= 1.0f) obj.vx = -obj.vx;
                    if (obj.y <= 0.0f || obj.y >= 1.0f) obj.vy = -obj.vy;
                    if (obj.z <= 0.0f || obj.z >= 1.0f) obj.vz = -obj.vz;
                    break;
            }
            
            // Keep objects in bounds
            obj.x = constrain(obj.x, 0.0f, 1.0f);
            obj.y = constrain(obj.y, 0.0f, 1.0f);
            obj.z = constrain(obj.z, 0.0f, 1.0f);
            
            // Update intensity based on depth and time
            float depth_factor = 1.0f - abs(obj.z - camera_z);
            float time_pulse = sin(time_factor * 2.0f + i) * 0.2f + 0.8f;
            obj.intensity = depth_factor * time_pulse;
        }
    }
    
    void manageObjectLifecycle() {
        // Spawn new objects occasionally
        if (random(1000) < object_spawn_rate * 1000 && active_object_count < MAX_OBJECTS) {
            spawnObject();
        }
        
        // Remove old objects
        uint32_t current_time = millis();
        for (uint8_t i = 0; i < MAX_OBJECTS; i++) {
            if (objects[i].active && (current_time - objects[i].birth_time > 15000)) {  // 15 second lifetime
                objects[i].active = false;
                active_object_count--;
            }
        }
    }
    
    void spawnObject() {
        // Find inactive object slot
        for (uint8_t i = 0; i < MAX_OBJECTS; i++) {
            if (!objects[i].active) {
                VolumetricObject& obj = objects[i];
                
                // Random initial position
                obj.x = random(1000) / 1000.0f;
                obj.y = random(1000) / 1000.0f;
                obj.z = random(1000) / 1000.0f;
                
                // Random velocity
                obj.vx = (random(200) - 100) / 1000.0f;
                obj.vy = (random(200) - 100) / 1000.0f;
                obj.vz = (random(200) - 100) / 1000.0f;
                
                // Random properties
                obj.size = 0.05f + (random(100) / 1000.0f);
                obj.intensity = 0.7f + (random(300) / 1000.0f);
                obj.hue = gHue + random(120);
                obj.shape_type = random(4);
                obj.rotation = 0.0f;
                obj.rotation_speed = (random(200) - 100) / 100.0f;
                
                obj.active = true;
                obj.birth_time = millis();
                active_object_count++;
                break;
            }
        }
    }
    
    void spawnInitialObjects() {
        // Spawn 3-4 initial objects
        for (uint8_t i = 0; i < 4; i++) {
            spawnObject();
        }
    }
    
    void renderObjectsToDepthBuffer() {
        for (uint8_t i = 0; i < MAX_OBJECTS; i++) {
            if (!objects[i].active) continue;
            
            VolumetricObject& obj = objects[i];
            renderObjectToDepthBuffer(obj);
        }
    }
    
    void renderObjectToDepthBuffer(VolumetricObject& obj) {
        // Project 3D object to 2D screen space
        float screen_x = obj.x;
        float screen_y = obj.y;
        
        // Apply perspective (closer objects appear larger)
        float perspective_factor = 1.0f / (obj.z * depth_exaggeration + 0.1f);
        float projected_size = obj.size * perspective_factor;
        
        // Calculate screen bounds
        int min_x = constrain((screen_x - projected_size) * 80, 0, 79);
        int max_x = constrain((screen_x + projected_size) * 80, 0, 79);
        int min_y = constrain((screen_y - projected_size) * 20, 0, 19);
        int max_y = constrain((screen_y + projected_size) * 20, 0, 19);
        
        // Render object shape
        for (int x = min_x; x <= max_x; x++) {
            for (int y = min_y; y <= max_y; y++) {
                float fx = x / 80.0f;
                float fy = y / 20.0f;
                
                // Calculate distance from object center
                float dx = fx - screen_x;
                float dy = fy - screen_y;
                float distance = sqrt(dx * dx + dy * dy);
                
                // Check if pixel is inside object
                bool inside = false;
                float intensity_factor = 1.0f;
                
                switch (obj.shape_type) {
                    case 0: // Sphere
                        inside = distance <= projected_size;
                        intensity_factor = 1.0f - (distance / projected_size);
                        break;
                        
                    case 1: // Cube
                        inside = (abs(dx) <= projected_size) && (abs(dy) <= projected_size);
                        intensity_factor = 1.0f - max(abs(dx), abs(dy)) / projected_size;
                        break;
                        
                    case 2: // Cylinder
                        inside = distance <= projected_size;
                        intensity_factor = 1.0f - (distance / projected_size);
                        // Add rotation effect
                        intensity_factor *= 0.7f + 0.3f * sin(obj.rotation + dx * 10.0f);
                        break;
                        
                    case 3: // Torus
                        {
                            float ring_radius = projected_size * 0.7f;
                            float tube_radius = projected_size * 0.3f;
                            float ring_distance = abs(distance - ring_radius);
                            inside = ring_distance <= tube_radius;
                            intensity_factor = 1.0f - (ring_distance / tube_radius);
                        }
                        break;
                }
                
                if (inside && obj.z < depth_buffer[x][y].depth) {
                    // Update depth buffer
                    depth_buffer[x][y].depth = obj.z;
                    depth_buffer[x][y].intensity = obj.intensity * intensity_factor;
                    depth_buffer[x][y].hue = obj.hue;
                }
            }
        }
    }
    
    void renderDepthBufferToLEDs() {
        // Render depth buffer to Edge 1 (Strip 1)
        for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            float position = (float)i / HardwareConfig::STRIP_LENGTH;
            float total_intensity = 0.0f;
            uint8_t blended_hue = gHue;
            float hue_weight = 0.0f;
            
            // Sample multiple depth layers for this LED position
            for (uint8_t depth_layer = 0; depth_layer < 5; depth_layer++) {
                float depth_y = depth_layer / 5.0f;
                uint8_t buf_x = constrain(position * 80, 0, 79);
                uint8_t buf_y = constrain(depth_y * 20, 0, 19);
                
                DepthPixel& pixel = depth_buffer[buf_x][buf_y];
                
                if (pixel.intensity > 0.0f) {
                    // Apply fog based on depth
                    float fog_factor = 1.0f;
                    if (pixel.depth > fog_start) {
                        fog_factor = 1.0f - ((pixel.depth - fog_start) / (fog_end - fog_start));
                        fog_factor = constrain(fog_factor, 0.0f, 1.0f);
                    }
                    
                    float layer_intensity = pixel.intensity * fog_factor * (1.0f - depth_layer * 0.15f);
                    total_intensity += layer_intensity;
                    
                    // Blend hue based on intensity
                    if (layer_intensity > hue_weight) {
                        blended_hue = pixel.hue;
                        hue_weight = layer_intensity;
                    }
                }
            }
            
            total_intensity = constrain(total_intensity, 0.0f, 1.0f);
            
            // Create final color
            CRGB color = getLightGuideColor(blended_hue, total_intensity);
            setEdge1LED(i, color);
        }
        
        // Render to Edge 2 (Strip 2) with different perspective
        for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            float position = (float)i / HardwareConfig::STRIP_LENGTH;
            float total_intensity = 0.0f;
            uint8_t blended_hue = gHue + 30;  // Slight hue shift for Edge 2
            float hue_weight = 0.0f;
            
            // Sample from opposite perspective
            for (uint8_t depth_layer = 0; depth_layer < 5; depth_layer++) {
                float depth_y = 1.0f - (depth_layer / 5.0f);  // Opposite perspective
                uint8_t buf_x = constrain(position * 80, 0, 79);
                uint8_t buf_y = constrain(depth_y * 20, 0, 19);
                
                DepthPixel& pixel = depth_buffer[buf_x][buf_y];
                
                if (pixel.intensity > 0.0f) {
                    float fog_factor = 1.0f;
                    if (pixel.depth > fog_start) {
                        fog_factor = 1.0f - ((pixel.depth - fog_start) / (fog_end - fog_start));
                        fog_factor = constrain(fog_factor, 0.0f, 1.0f);
                    }
                    
                    float layer_intensity = pixel.intensity * fog_factor * (1.0f - depth_layer * 0.15f);
                    total_intensity += layer_intensity;
                    
                    if (layer_intensity > hue_weight) {
                        blended_hue = pixel.hue + 30;
                        hue_weight = layer_intensity;
                    }
                }
            }
            
            total_intensity = constrain(total_intensity, 0.0f, 1.0f);
            
            CRGB color = getLightGuideColor(blended_hue, total_intensity);
            setEdge2LED(i, color);
        }
    }
    
    void addAtmosphericEffects() {
        // Add subtle fog/haze effects for depth enhancement
        uint32_t time = millis();
        float fog_animation = sin(time * 0.002f) * 0.1f + 0.1f;
        
        // Add fog to both edges
        for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            float position = (float)i / HardwareConfig::STRIP_LENGTH;
            
            // Create fog pattern
            float fog_intensity = fog_animation * sin(position * LightGuide::PI_F * 3.0f + time * 0.001f);
            fog_intensity = abs(fog_intensity) * FOG_DENSITY;
            
            if (fog_intensity > 0.0f) {
                CRGB fog_color = CHSV(gHue + 60, 100, fog_intensity * 255);
                
                strip1[i] = blend(strip1[i], fog_color, 64);
                strip2[i] = blend(strip2[i], fog_color, 64);
            }
        }
    }
    
public:
    // Parameter control methods
    void setMovementPattern(uint8_t pattern) {
        movement_pattern = constrain(pattern, 0, 3);
    }
    
    void setPatternSpeed(float speed) {
        pattern_speed = constrain(speed, 0.1f, 3.0f);
    }
    
    void setCameraZ(float z) {
        camera_z = constrain(z, 0.0f, 1.0f);
    }
    
    void setDepthExaggeration(float exag) {
        depth_exaggeration = constrain(exag, 0.5f, 3.0f);
    }
    
    void setFogDensity(float start, float end) {
        fog_start = constrain(start, 0.0f, 1.0f);
        fog_end = constrain(end, fog_start, 1.0f);
    }
    
    void setObjectSpawnRate(float rate) {
        object_spawn_rate = constrain(rate, 0.001f, 0.1f);
    }
    
    // Getters
    uint8_t getMovementPattern() const { return movement_pattern; }
    float getPatternSpeed() const { return pattern_speed; }
    float getCameraZ() const { return camera_z; }
    float getDepthExaggeration() const { return depth_exaggeration; }
    uint8_t getActiveObjectCount() const { return active_object_count; }
};

#endif // LED_STRIPS_MODE && FEATURE_LIGHT_GUIDE_MODE

#endif // VOLUMETRIC_DISPLAY_EFFECT_H