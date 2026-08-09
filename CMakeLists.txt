cmake_minimum_required(VERSION 3.22)

project(CasioRZ1 VERSION 1.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment target" FORCE)
endif()

if(APPLE AND NOT DEFINED CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "macOS architectures" FORCE)
endif()

set(JUCE_DIR "" CACHE PATH "Path to the JUCE source tree")
if(NOT JUCE_DIR OR NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "Set JUCE_DIR to a JUCE source tree that contains CMakeLists.txt.")
endif()

add_subdirectory("${JUCE_DIR}" juce)

set(MAME_ROOT "${CMAKE_SOURCE_DIR}/mame-mac-master" CACHE PATH "Path to the MAME source tree")
set(MAME_LIB_DIR "${MAME_ROOT}/build/osx_clang/bin/x64/Release" CACHE PATH "Path to the compiled MAME static libraries")

set(MAME_INCLUDE_DIRS
    "${MAME_ROOT}/src"
    "${MAME_ROOT}/src/emu"
    "${MAME_ROOT}/src/osd"
    "${MAME_ROOT}/src/lib"
    "${MAME_ROOT}/src/lib/util"
    "${MAME_ROOT}/src/frontend/mame"
    "${MAME_ROOT}/src/devices"
    "${MAME_ROOT}/src/devices/machine"
    "${MAME_ROOT}/src/mame/ensoniq"
    "${MAME_ROOT}/src/mame/casio"
    "${MAME_ROOT}/3rdparty/bx/include"
    "${MAME_ROOT}/build/generated/mame/layout"
)

juce_add_plugin(CasioRZ1
    COMPANY_NAME "CasioMAME"
    PLUGIN_MANUFACTURER_CODE Crz1
    PLUGIN_CODE RZ1A
    PRODUCT_NAME "Casio RZ-1"
    FORMATS VST3 AU
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    # Register the AU as a MIDI-Controlled Effect (aumf) instead of a Music
    # Device: Logic Pro doesn't render aumu instruments that carry an audio
    # input bus (MAME never boots -> blank LCD), but aumf hosts audio inputs
    # as sidechain AND receives MIDI, which is exactly what the RZ-1 needs
    # (drums from MIDI + sampling from the input bus).
    AU_MAIN_TYPE kAudioUnitType_MusicEffect
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument
)

juce_generate_juce_header(CasioRZ1)

juce_add_binary_data(CasioRZ1Assets
    SOURCES
        Source/mame_plugins.zip
)

target_sources(CasioRZ1 PRIVATE
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/esqpanel.h
)

target_include_directories(CasioRZ1 PRIVATE
    "${CMAKE_SOURCE_DIR}/Source"
    ${MAME_INCLUDE_DIRS}
)

target_compile_definitions(CasioRZ1 PRIVATE
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(CasioRZ1 PRIVATE
    CasioRZ1Assets
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_plugin_client
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_midi_ci
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

file(GLOB MAME_ARCHIVES CONFIGURE_DEPENDS
    "${MAME_LIB_DIR}/*.a"
    "${MAME_LIB_DIR}/mame_mame/*.a"
)

target_link_libraries(CasioRZ1 PRIVATE ${MAME_ARCHIVES})

find_path(SDL2_INCLUDE_DIR SDL2/SDL.h PATHS /opt/homebrew/include REQUIRED NO_DEFAULT_PATH)
find_library(SDL2MAIN_LIBRARY SDL2main PATHS /opt/homebrew/lib REQUIRED NO_DEFAULT_PATH)
find_library(SDL2_LIBRARY SDL2 PATHS /opt/homebrew/lib REQUIRED NO_DEFAULT_PATH)

target_include_directories(CasioRZ1 PRIVATE ${SDL2_INCLUDE_DIR})
target_link_libraries(CasioRZ1 PRIVATE ${SDL2MAIN_LIBRARY} ${SDL2_LIBRARY}
    "-framework OpenGL"
    "-framework ApplicationServices")

set(MAME_PLUGINS_ZIP "${CMAKE_SOURCE_DIR}/Source/mame_plugins.zip")
set(MAME_PLUGINS_DIR "${CMAKE_BINARY_DIR}/CasioRZ1_artefacts/$<CONFIG>/VST3/Casio RZ-1.vst3/Contents/Resources/plugins")

if(EXISTS "${MAME_PLUGINS_ZIP}")
    add_custom_command(TARGET CasioRZ1_VST3 POST_BUILD
        COMMAND /bin/mkdir -p "${MAME_PLUGINS_DIR}"
        COMMAND /usr/bin/ditto -x -k "${MAME_PLUGINS_ZIP}" "${MAME_PLUGINS_DIR}"
        COMMAND /usr/bin/touch "${CMAKE_BINARY_DIR}/CasioRZ1_artefacts/$<CONFIG>/VST3/Casio RZ-1.vst3/Contents/Resources/.mame_plugins_installed"
        COMMENT "Installing MAME plugins into the VST3 bundle"
        VERBATIM
    )
endif()

if(APPLE)
    target_link_options(CasioRZ1 PRIVATE
        "-Wl,-rpath,@loader_path/../Frameworks"
        "-Wl,-rpath,/Library/Frameworks"
    )

    # Install MAME plugins into the AU bundle BEFORE codesigning, then
    # ad-hoc codesign so Logic can load it locally (auval requires the
    # completed bundle to be sealed).
    set(AU_BUNDLE_DIR "${CMAKE_BINARY_DIR}/CasioRZ1_artefacts/$<CONFIG>/AU/Casio RZ-1.component")
    set(AU_PLUGINS_DIR "${AU_BUNDLE_DIR}/Contents/Resources/plugins")
    set(AU_MARKER_FILE "${CMAKE_BINARY_DIR}/CasioRZ1_artefacts/$<CONFIG>/AU/Casio RZ-1.component/Contents/Resources/.mame_plugins_installed")

    if(EXISTS "${MAME_PLUGINS_ZIP}")
        add_custom_command(TARGET CasioRZ1_AU POST_BUILD
            COMMAND /bin/mkdir -p "${AU_PLUGINS_DIR}"
            COMMAND /usr/bin/ditto -x -k "${MAME_PLUGINS_ZIP}" "${AU_PLUGINS_DIR}"
            COMMAND /usr/bin/touch "${AU_MARKER_FILE}"
            COMMAND /usr/bin/codesign --force --deep --sign - "${AU_BUNDLE_DIR}"
            COMMENT "Installing MAME plugins into the AU bundle and ad-hoc codesigning"
            VERBATIM
        )
    else()
        add_custom_command(TARGET CasioRZ1_AU POST_BUILD
            COMMAND /usr/bin/codesign --force --deep --sign - "${AU_BUNDLE_DIR}"
            COMMENT "Ad-hoc codesigning the AU bundle"
            VERBATIM
        )
    endif()
endif()
