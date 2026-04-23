cmake_minimum_required(VERSION 3.16)

project(WundaVanguard VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick Qml QuickControls2 Network)

# Npcap Paths (User should adjust if needed)
# Default Npcap SDK location is often C:/Npcap-SDK
set(NPCAP_SDK_PATH "C:/Npcap-SDK" CACHED PATH "Path to Npcap SDK")
include_directories(${NPCAP_SDK_PATH}/Include)
link_directories(${NPCAP_SDK_PATH}/Lib/x64)

# WinDivert Paths
# We will assume WinDivert headers are in ./include and libs in ./lib
include_directories(include)
link_directories(lib)

set(PROJECT_SOURCES
    src/main.cpp
    src/NetworkVanguard.cpp
    src/ArpEngine.cpp
    src/ShaperEngine.cpp
    src/BackendBridge.cpp
    qml/qml.qrc
)

add_executable(WundaVanguard ${PROJECT_SOURCES})

target_link_libraries(WundaVanguard PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    Qt6::QuickControls2
    Qt6::Network
    wpcap
    Packet
    WinDivert
)

if(MSVC)
    target_compile_options(WundaVanguard PRIVATE /W4 /WX-)
endif()

# Deployment instructions for user
# Make sure wpcap.dll and WinDivert.dll are in the same directory as the executable.
