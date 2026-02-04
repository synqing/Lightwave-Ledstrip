# PSRAM on ESP32-S3 FH4R2 (e.g. Waveshare ESP32-S3-Zero)

Boards with **ESP32-S3FH4R2** (4MB flash, 2MB PSRAM) can still show ~38KB free heap and fail to start **AudioActor** / **WiFi** if PSRAM is not used for the heap.

## Cause

With **Arduino** framework on PlatformIO **espressif32@6.9.0**, the base board is `esp32-s3-devkitc-1` (no PSRAM). Overrides (`board_build.psram_type = opi`, `board_build.arduino.memory_type = qio_opi`) enable PSRAM at boot, but **CONFIG_SPIRAM_USE_MALLOC** (use PSRAM for `malloc()` / heap) is controlled by **sdkconfig**. The Arduino build in this setup does **not** apply `board_build.esp-idf.sdkconfig_defaults` or project `sdkconfig.defaults`, so the heap stays internal-only.

## What we have in the repo

- **sdkconfig_fh4r2.defaults** – `CONFIG_SPIRAM_SUPPORT=y`, `CONFIG_SPIRAM_USE_MALLOC=y`, `CONFIG_SPIRAM_CACHE_WORKAROUND=y`
- **sdkconfig.defaults** and **sdkconfig.defaults.esp32s3** – same content for project/target defaults
- **platformio.ini** `[env:esp32dev_FH4R2]`: `board_build.esp-idf.sdkconfig_defaults = sdkconfig_fh4r2.defaults` (used when the build actually merges sdkconfig defaults; with Arduino it currently is not)

## Options

1. **Use a newer platform with an N4R2 board**  
   e.g. `platform = espressif32` (latest) and `board = esp32-s3-devkitc1-n4r2`.  
   That board definition enables PSRAM and heap correctly, but the latest platform uses Arduino-ESP32 3.3.x, which has **breaking API changes** (e.g. `Network.h`), so the project would need code updates.

2. **Run menuconfig once (if available)**  
   If your PlatformIO/Arduino build exposes a menuconfig target, run it for the FH4R2 env, then:  
   **Component config → ESP PSRAM** (or “Support for external memory”) → enable **“Allow external memory as an additional heap”** (CONFIG_SPIRAM_USE_MALLOC).  
   Save and exit, then do a clean build. This only works if the Arduino build actually uses the generated sdkconfig.

3. **Pre-build script**  
   An `extra_script` could copy `sdkconfig_fh4r2.defaults` into the path where the Arduino-ESP32 build reads sdkconfig defaults (if that path is known and stable).

4. **Upgrade and adapt**  
   Move to the latest `espressif32` platform and `esp32-s3-devkitc1-n4r2`, then fix WiFi/Network API usage for Arduino-ESP32 3.3.x.

## Boot log check

After a fix, serial should show:

- **Boot memory: heap XXXXX bytes free** with a **large** value (e.g. 300KB+), and  
- **Boot memory: PSRAM XXXXX bytes free**

If you only see ~38KB heap and no PSRAM line, PSRAM is still not in the heap.
