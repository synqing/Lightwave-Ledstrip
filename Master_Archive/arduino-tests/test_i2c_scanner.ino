#include <Wire.h>

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  
  Serial.println("\n\nSimple I2C Scanner");
  Serial.println("==================");
  
  // Try different pin combinations
  int sdaPins[] = {7, 11, 21};  // GPIO7, GPIO11, GPIO21 (default)
  int sclPins[] = {8, 12, 22};  // GPIO8, GPIO12, GPIO22 (default)
  
  for (int i = 0; i < 3; i++) {
    Serial.printf("\n\nTrying SDA=%d, SCL=%d\n", sdaPins[i], sclPins[i]);
    
    Wire.end();
    delay(100);
    Wire.begin(sdaPins[i], sclPins[i], 100000);
    delay(100);
    
    Serial.println("Scanning...");
    int nDevices = 0;
    
    for (byte address = 1; address < 127; address++) {
      Wire.beginTransmission(address);
      byte error = Wire.endTransmission();
      
      if (error == 0) {
        Serial.printf("Device found at 0x%02X\n", address);
        nDevices++;
      }
    }
    
    if (nDevices == 0) {
      Serial.println("No I2C devices found");
    } else {
      Serial.printf("Found %d device(s)\n", nDevices);
    }
  }
  
  Serial.println("\n\nDone scanning all pin combinations");
}

void loop() {
  // Nothing
}