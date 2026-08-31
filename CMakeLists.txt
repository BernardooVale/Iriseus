cmake_minimum_required(VERSION 3.24)
set(FETCHCONTENT_BASE_DIR "C:/local/cmake_deps" CACHE PATH "FetchContent cache")
set(LIBSODIUM_ROOT "C:/local/libsodium" CACHE PATH "libsodium root")
project(Iriseus VERSION 0.1.0 LANGUAGES CXX)

add_compile_definitions(_WIN32_WINNT=0x0601)
add_compile_definitions(SODIUM_STATIC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(BOOST_ROOT "C:/local/boost_1_92_0" CACHE PATH "Boost root")
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
find_package(Boost 1.85 REQUIRED)

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

FetchContent_Declare(qrcodegen
    URL https://github.com/nayuki/QR-Code-generator/archive/refs/heads/master.zip
)
FetchContent_MakeAvailable(qrcodegen)

add_library(qrcodegen STATIC
    ${qrcodegen_SOURCE_DIR}/cpp/qrcodegen.cpp
)
target_include_directories(qrcodegen PUBLIC ${qrcodegen_SOURCE_DIR}/cpp)

set(LIBSODIUM_LIB "${LIBSODIUM_ROOT}/x64/Release/v143/static/libsodium.lib")

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

set(SOURCES
    src/main.cpp
    src/app/Application.cpp
    src/ui/TrayIcon.cpp
    src/core/WsServer.cpp
    src/core/WsSession.cpp
    src/core/MdnsService.cpp
    src/core/PairingManager.cpp
    src/core/PairingCrypto.cpp
    src/ui/PairingDialog.cpp
    resources/resources.qrc
)

add_executable(Iriseus WIN32 ${SOURCES})

target_include_directories(Iriseus PRIVATE
    src
    ${Boost_INCLUDE_DIRS}
    ${mdns_SOURCE_DIR}
    ${LIBSODIUM_ROOT}/include
)

target_link_libraries(Iriseus PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Boost::headers
    ws2_32
    wsock32
    iphlpapi
    nlohmann_json::nlohmann_json
    qrcodegen
    ${LIBSODIUM_LIB}
)

set_target_properties(Iriseus PROPERTIES
    WIN32_EXECUTABLE TRUE
    VS_DPI_AWARE "PerMonitor"
)

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