cmake_minimum_required(VERSION 3.24)
project(Iriseus VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Qt — ajuste Qt6_DIR para seu ambiente
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

set(SOURCES
    src/main.cpp
    src/app/Application.cpp
    src/ui/TrayIcon.cpp
    resources/resources.qrc
)

add_executable(Iriseus WIN32 ${SOURCES})

target_include_directories(Iriseus PRIVATE src)

target_link_libraries(Iriseus PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

# Manifesto Windows — DPI awareness, compatibilidade
set_target_properties(Iriseus PROPERTIES
    WIN32_EXECUTABLE TRUE
    VS_DPI_AWARE "PerMonitor"
)

# Instalar Qt DLLs junto ao executável (windeployqt)
find_program(WINDEPLOYQT windeployqt HINTS "${Qt6_DIR}/../../../bin")
if(WINDEPLOYQT)
    add_custom_command(TARGET Iriseus POST_BUILD
        COMMAND "${WINDEPLOYQT}"
            --no-translations
            --no-system-d3d-compiler
            --no-opengl-sw
            "$<TARGET_FILE:Iriseus>"
        COMMENT "Running windeployqt"
    )
endif()