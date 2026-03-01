cmake_minimum_required(VERSION 3.15)

# ---- macOS: Deployment Target + Universal Binary (MUST be BEFORE project()) ----
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build Universal Binary" FORCE)

project(FUNKY_MOOSE_AMP VERSION 1.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ------------------------------
# Release presets (zero-drama)
# ------------------------------
option(FUNKY_RELEASE "Enable release presets (fast + clean log)" ON)
option(FUNKY_CLEAN_LOG "Suppress known JUCE deprecation warnings in release builds" ON)

# Silence JUCE warning triggered by -ffast-math (Clang only)
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wno-nan-infinity-disabled)
endif()

# --- JUCE ---
# Usage example:
# cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="/Users/uwearthurfelchle/Developer/JUCE/install"
# Alternatively:
# cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DJUCE_DIR="/path/to/JUCE/install/lib/cmake/JUCE-8.0.10"
find_package(JUCE CONFIG REQUIRED)

# ---- Icon Setup ----
if (APPLE)
    set(FUNKY_ICON_ARG ICON_BIG "Source/Assets/FunkyMoose.icns")
else()
    set(FUNKY_ICON_ARG "")
endif()

# ---- Plugin (VST3 + AU + Standalone) ----
juce_add_plugin(FUNKY_MOOSE_AMP
    COMPANY_NAME "Funky Moose Audio"
    PRODUCT_NAME "Funky Moose Amp"
    VERSION "1.0.1"
    BUNDLE_ID "at.uwefelchle.funkymooseamp"

    PLUGIN_MANUFACTURER_CODE FMSA
    PLUGIN_CODE FMAP

    COMPANY_COPYRIGHT "Copyright (c) 2026 Uwe Arthur Felchle"
    COMPANY_WEBSITE "www.funkymoose.com"
    COMPANY_EMAIL "info@funkymoose.com"

    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE

    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Funky Moose Amp needs access to your audio input so it can process live audio."

    ${FUNKY_ICON_ARG}

    COPY_PLUGIN_AFTER_BUILD TRUE
    FORMATS VST3 AU Standalone
)

# ---- Sources ----
target_sources(FUNKY_MOOSE_AMP PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h

    Source/ui/FunkyMooseLookAndFeel.cpp
    Source/ui/FunkyMooseLookAndFeel.h
    Source/ui/ElchComponent.cpp
    Source/ui/ElchComponent.h

    Source/dsp/AudioFifo.h
    Source/dsp/YinPitch.h
    Source/dsp/TunerMath.h
    Source/ui/TunerComponent.cpp
    Source/ui/TunerComponent.h

    Source/PluginEntry.cpp
)

# ---- BinaryData (Assets) ----
juce_add_binary_data(FUNKY_MOOSE_AMPData SOURCES
    Source/Assets/panel_base.png
    Source/Assets/panel_wear.png
    Source/Assets/metal_tile.png
    Source/Assets/knob_spec.png
    Source/Assets/knob_scratches.png
    Source/Assets/knob_big.png
    Source/Assets/knob_small.png
    Source/Assets/FunkyMoose.icns
    Source/Assets/elch_new.png
    Source/Assets/elch_vintage.png
    Source/Assets/Frame.png
    Source/Assets/FrameOverlay.png
)

target_link_libraries(FUNKY_MOOSE_AMP PRIVATE
    FUNKY_MOOSE_AMPData
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_audio_processors
    juce::juce_audio_utils
)

# VST3 replace-vst2 guard
target_compile_definitions(FUNKY_MOOSE_AMP PRIVATE JUCE_VST3_CAN_REPLACE_VST2=0)
if (TARGET FUNKY_MOOSE_AMP_VST3)
    target_compile_definitions(FUNKY_MOOSE_AMP_VST3 PRIVATE JUCE_VST3_CAN_REPLACE_VST2=0)
endif()

# ---- Standalone: macOS bundle properties ----
if (APPLE AND TARGET FUNKY_MOOSE_AMP_Standalone)
    set_target_properties(FUNKY_MOOSE_AMP_Standalone PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Funky Moose Amp"
        MACOSX_BUNDLE_GUI_IDENTIFIER "at.uwefelchle.funkymooseamp"
    )

    # Force the real ELCH icon into the bundle after build
    # (JUCE/Xcode currently produce an 8-byte dummy Icon.icns)
    add_custom_command(TARGET FUNKY_MOOSE_AMP_Standalone POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/Assets/FunkyMoose.icns"
        "$<TARGET_FILE_DIR:FUNKY_MOOSE_AMP_Standalone>/../Resources/Icon.icns"
        COMMAND /usr/bin/touch "$<TARGET_FILE_DIR:FUNKY_MOOSE_AMP_Standalone>/../.."
        COMMENT "Enforcing Elch icon over dummy Icon.icns and refreshing bundle..."
    )
endif()

# ---- Optimizations ----
if (FUNKY_RELEASE)
    if ((CMAKE_CXX_COMPILER_ID MATCHES "Clang") OR (CMAKE_CXX_COMPILER_ID MATCHES "GNU"))
        target_compile_options(FUNKY_MOOSE_AMP PRIVATE
            $<$<CONFIG:Release>:-O3>
            # $<$<CONFIG:Release>:-ffast-math>
        )

        # Clean Release log (optional): JUCE 8 deprecation warnings etc.
        if (FUNKY_CLEAN_LOG)
            target_compile_options(FUNKY_MOOSE_AMP PRIVATE
                $<$<CONFIG:Release>:-Wno-deprecated-declarations>
                $<$<CONFIG:Release>:-Wno-deprecated-anon-enum-enum-conversion>
            )
        endif()
    endif()
endif()

# ---- System-wide Plugin Install (macOS) ----
# Usage: sudo cmake --install build
# VST3 -> /Library/Audio/Plug-Ins/VST3/
# AU   -> /Library/Audio/Plug-Ins/Components/
if (APPLE)
    set(SYSTEM_VST3_DIR "/Library/Audio/Plug-Ins/VST3")
    set(SYSTEM_AU_DIR   "/Library/Audio/Plug-Ins/Components")

    if (TARGET FUNKY_MOOSE_AMP_VST3)
        install(
            DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/FUNKY_MOOSE_AMP_artefacts/Release/VST3/Funky Moose Amp.vst3"
            DESTINATION "${SYSTEM_VST3_DIR}"
            USE_SOURCE_PERMISSIONS
        )
    endif()

    if (TARGET FUNKY_MOOSE_AMP_AU)
        install(
            DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/FUNKY_MOOSE_AMP_artefacts/Release/AU/Funky Moose Amp.component"
            DESTINATION "${SYSTEM_AU_DIR}"
            USE_SOURCE_PERMISSIONS
        )
    endif()
endif()
