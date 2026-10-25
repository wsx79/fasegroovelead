cmake_minimum_required(VERSION 3.22)

project(FaseGrooveLead VERSION 0.1.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Universal binary (Apple Silicon + Intel) on macOS
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
endif()
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

option(FGL_BUILD_PLUGIN "Build the JUCE plug-in (needs JUCE)" ON)
option(FGL_BUILD_TESTS  "Build the engine unit tests" ON)

#==============================================================================
# Lead engine (pure C++, no JUCE)
add_library(fgl_engine STATIC
    Source/engine/Lead.cpp
    Source/engine/Generators.cpp
    Source/engine/GrooveEngine.cpp)
target_include_directories(fgl_engine PUBLIC Source/engine)
set_target_properties(fgl_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(FGL_BUILD_TESTS)
    enable_testing()
    add_executable(fgl_engine_tests tests/EngineTests.cpp)
    target_include_directories(fgl_engine_tests PRIVATE ThirdParty/fasegroove-common)
    target_link_libraries(fgl_engine_tests PRIVATE fgl_engine)
    add_test(NAME engine COMMAND fgl_engine_tests)
endif()

#==============================================================================
# Plug-in
if(FGL_BUILD_PLUGIN)
    # Use a local JUCE checkout with -DJUCE_DIR=/path/to/JUCE, otherwise download it.
    set(JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout")

    if(JUCE_DIR)
        add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)
    else()
        include(FetchContent)
        FetchContent_Declare(JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG 8.0.9
            GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(JUCE)
    endif()

    set(FGL_FORMATS VST3 Standalone)
    if(APPLE)
        list(APPEND FGL_FORMATS AU)
    endif()

    juce_add_plugin(FaseGrooveLead
        PRODUCT_NAME "Fase Groove Lead"
        COMPANY_NAME "FS Pitch"
        BUNDLE_ID "com.fspitch.fasegroovelead"
        PLUGIN_MANUFACTURER_CODE FsPt
        PLUGIN_CODE Fgl1
        ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon_512.png"
        FORMATS ${FGL_FORMATS}
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT TRUE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES Instrument Generator
        AU_MAIN_TYPE kAudioUnitType_MusicDevice)

    juce_add_binary_data(FaseGrooveLeadData SOURCES
        Resources/fgl_icon.svg
        Resources/fs_pitch_logo.png
        Resources/Inter-Regular.otf
        Resources/Inter-SemiBold.otf)

    target_sources(FaseGrooveLead PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/Product.cpp
        # The family's shared module, vendored by fasegroove/scripts/sync.sh (do not edit here).
        ThirdParty/fasegroove-common/Branding.cpp
        ThirdParty/fasegroove-common/Localisation.cpp
        ThirdParty/fasegroove-common/Updater.cpp
        ThirdParty/fasegroove-common/EditorWidgets.cpp
        ThirdParty/fasegroove-common/UpdateOverlay.cpp)
    target_include_directories(FaseGrooveLead PUBLIC ThirdParty/fasegroove-common)

    target_compile_definitions(FaseGrooveLead PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

    target_link_libraries(FaseGrooveLead
        PUBLIC
            FaseGrooveLeadData
        PRIVATE
            fgl_engine
            juce::juce_audio_utils
            juce::juce_cryptography   # the updater checks downloads (SHA-256)
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    # Plug-in level tests (state, grid, MIDI export, transport); run by ctest in CI.
    if(FGL_BUILD_TESTS)
        juce_add_console_app(fgl_plugin_tests PRODUCT_NAME "Fase Groove Lead Tests")
        target_sources(fgl_plugin_tests PRIVATE tests/PluginTests.cpp)
        target_link_libraries(fgl_plugin_tests PRIVATE FaseGrooveLead juce::juce_audio_utils juce::juce_cryptography)
        add_test(NAME plugin COMMAND fgl_plugin_tests)
    endif()
endif()
