cmake_minimum_required(VERSION 3.24)
set(FETCHCONTENT_BASE_DIR "C:/local/cmake_deps" CACHE PATH "FetchContent cache")
project(Iriseus VERSION 0.1.0 LANGUAGES CXX)

add_compile_definitions(_WIN32_WINNT=0x0601)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Boost — ajuste o path se necessário
set(BOOST_ROOT "C:/local/boost_1_92_0" CACHE PATH "Boost root")
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
find_package(Boost 1.85 REQUIRED)

# nlohmann/json — header-only, baixado automaticamente
include(FetchContent)
FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

FetchContent_Declare(mdns
    URL https://github.com/mjansson/mdns/archive/refs/heads/master.zip
)
FetchContent_MakeAvailable(mdns)

# Qt — ajuste Qt6_DIR para seu ambiente
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

set(SOURCES
    src/main.cpp
    src/app/Application.cpp
    src/ui/TrayIcon.cpp
    src/core/WsServer.cpp
    src/core/WsSession.cpp
    src/core/MdnsService.cpp
    resources/resources.qrc
)

add_executable(Iriseus WIN32 ${SOURCES})

target_include_directories(Iriseus PRIVATE 
    src
    ${Boost_INCLUDE_DIRS}
    ${mdns_SOURCE_DIR}
)

target_link_libraries(Iriseus PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Boost::headers
    ws2_32      
    wsock32
    nlohmann_json::nlohmann_json
    iphlpapi 
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