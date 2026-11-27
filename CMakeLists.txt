cmake_minimum_required(VERSION 3.20)
project(cruelfury VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 REQUIRED COMPONENTS Core Gui Qml Quick Multimedia)

add_executable(cruelfury
    src/main.cpp
    src/SynthAudio.h
    src/SynthAudio.cpp
    src/TextureAtlas.h
    src/TextureAtlas.cpp
    src/GameSim.h
    src/GameSim.cpp
    src/GameItem.h
    src/GameItem.cpp
)

target_compile_options(cruelfury PRIVATE -O3 -Wall -Wextra -Wno-unused-parameter)

target_link_libraries(cruelfury PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Qml
    Qt5::Quick
    Qt5::Multimedia
    GLESv2
)
