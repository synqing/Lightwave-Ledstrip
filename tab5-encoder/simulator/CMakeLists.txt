cmake_minimum_required(VERSION 3.10)
project(Tab5EncoderSimulator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find SDL2
find_package(SDL2 REQUIRED)
find_package(Threads REQUIRED)

# Include directories
include_directories(
    ${SDL2_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/widgets
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/hal
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/parameters
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/config
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/presets
    ${CMAKE_CURRENT_SOURCE_DIR}/m5gfx_mock
)

# Source files - UI components
set(UI_SOURCES
    ../src/ui/DisplayUI.cpp
    ../src/ui/widgets/GaugeWidget.cpp
    ../src/ui/widgets/UIHeader.cpp
    # PresetSlotWidget is header-only (implementation in .h file)
    ../src/ui/LoadingScreen.cpp
    ../src/ui/LedFeedback.cpp
)

# Source files - Mock implementations
set(MOCK_SOURCES
    m5gfx_mock/M5GFX_Mock.cpp
    ../src/hal/SimHal.cpp
)

# Source files - Supporting code (minimal implementations for simulator)
set(SUPPORT_SOURCES
    # ParameterMap::getParameterMax is stubbed in main_sim.cpp
    # PresetManager not needed for basic UI rendering
)

# Main simulator executable
add_executable(tab5_ui_sim
    main_sim.cpp
    ${UI_SOURCES}
    ${MOCK_SOURCES}
    ${SUPPORT_SOURCES}
)

# Link libraries
target_link_libraries(tab5_ui_sim
    ${SDL2_LIBRARIES}
    Threads::Threads
)

# Compiler definitions
target_compile_definitions(tab5_ui_sim PRIVATE
    SIMULATOR_BUILD=1
    ENABLE_UI_DIAGNOSTICS=1
)

# Compiler flags
target_compile_options(tab5_ui_sim PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-unused-variable
)

# macOS-specific: Fix SDL2 framework path
if(APPLE)
    find_library(SDL2_FRAMEWORK SDL2)
    if(SDL2_FRAMEWORK)
        target_link_libraries(tab5_ui_sim ${SDL2_FRAMEWORK})
    endif()
endif()

