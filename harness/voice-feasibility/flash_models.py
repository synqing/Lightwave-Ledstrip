"""
PlatformIO extra script — adds srmodels.bin to the flash image list.
This ensures ESP-SR model data is flashed alongside firmware in a single
esptool invocation, using the correct flash parameters from the build.
"""
import os
Import("env")

# Locate srmodels.bin in the Arduino ESP32 framework libs
framework_libs = os.path.expanduser(
    "~/.platformio/packages/framework-arduinoespressif32-libs/esp32s3/esp_sr/srmodels.bin"
)

if os.path.isfile(framework_libs):
    env.Append(
        FLASH_EXTRA_IMAGES=[
            ("0xC10000", framework_libs)
        ]
    )
    print(f"[VOICE] Will flash srmodels.bin ({os.path.getsize(framework_libs)} bytes) at 0xC10000")
else:
    print(f"[VOICE] WARNING: srmodels.bin not found at {framework_libs}")
