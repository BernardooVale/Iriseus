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

set(FFMPEG_ROOT "C:/local/ffmpeg" CACHE PATH "FFmpeg root")
find_path(FFMPEG_INCLUDE_DIR libavcodec/avcodec.h PATHS "${FFMPEG_ROOT}/include")
find_library(FFMPEG_AVCODEC NAMES avcodec PATHS "${FFMPEG_ROOT}/lib")
find_library(FFMPEG_AVUTIL  NAMES avutil  PATHS "${FFMPEG_ROOT}/lib")
find_library(FFMPEG_SWSCALE NAMES swscale PATHS "${FFMPEG_ROOT}/lib")

# BaseClasses — DirectShow base
add_library(baseclasses STATIC
    third_party/softcam/src/baseclasses/amfilter.cpp
    third_party/softcam/src/baseclasses/amvideo.cpp
    third_party/softcam/src/baseclasses/combase.cpp
    third_party/softcam/src/baseclasses/dllentry.cpp
    third_party/softcam/src/baseclasses/dllsetup.cpp
    third_party/softcam/src/baseclasses/mtype.cpp
    third_party/softcam/src/baseclasses/source.cpp
    third_party/softcam/src/baseclasses/winutil.cpp
    third_party/softcam/src/baseclasses/wxdebug.cpp
    third_party/softcam/src/baseclasses/wxlist.cpp
    third_party/softcam/src/baseclasses/wxutil.cpp
)
target_include_directories(baseclasses PUBLIC
    third_party/softcam/src/baseclasses
)
target_compile_definitions(baseclasses PRIVATE
    UNICODE
    _UNICODE
)
target_compile_options(baseclasses PRIVATE 
    /Zc:strictStrings-
)
target_link_libraries(baseclasses PUBLIC
    strmiids winmm ole32 oleaut32 uuid
)

# SoftcamSender — apenas SenderAPI, sem DShowSoftcam
add_library(softcamsender STATIC
    third_party/softcam/src/softcamcore/FrameBuffer.cpp
    third_party/softcam/src/softcamcore/Misc.cpp
    third_party/softcam/src/softcamcore/SenderAPI.cpp
    third_party/softcam/src/softcamcore/Watchdog.cpp
)
target_include_directories(softcamsender PUBLIC
    third_party/softcam/src/softcamcore
    third_party/softcam/src/baseclasses
    third_party/softcam/src/softcam
    third_party/softcam/src
)

target_link_libraries(softcamsender PUBLIC
    baseclasses
    kernel32 user32
)

# Softcam DLL — registrada via regsvr32
add_library(softcam MODULE
    third_party/softcam/src/softcam/softcam.cpp
    third_party/softcam/src/softcam/softcam.def
    third_party/softcam/src/softcamcore/DShowSoftcam.cpp
)
target_include_directories(softcam PRIVATE
    third_party/softcam/src/softcam
    third_party/softcam/src/softcamcore
    third_party/softcam/src/baseclasses
    third_party/softcam/src
)
target_link_libraries(softcam PRIVATE
    softcamsender
    baseclasses
)
set_target_properties(softcam PROPERTIES SUFFIX ".dll")

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
    src/core/AdbManager.cpp
    src/core/StreamReceiver.cpp
    src/core/VideoDecoder.cpp
    src/core/VirtualCamera.cpp
    resources/resources.qrc
)

add_executable(Iriseus WIN32 ${SOURCES})

target_include_directories(Iriseus PRIVATE
    src
    ${Boost_INCLUDE_DIRS}
    ${mdns_SOURCE_DIR}
    ${LIBSODIUM_ROOT}/include
    ${FFMPEG_INCLUDE_DIR}
    third_party/softcam/src/softcam
    third_party/softcam/src/softcamcore
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
    ${FFMPEG_AVCODEC}
    ${FFMPEG_AVUTIL}
    ${FFMPEG_SWSCALE}
)

set_target_properties(Iriseus PROPERTIES
    WIN32_EXECUTABLE TRUE
    VS_DPI_AWARE "PerMonitor"
)

target_link_options(Iriseus PRIVATE /NODEFAULTLIB:LIBCMT)

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

add_custom_command(TARGET Iriseus POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:softcam>"
        "$<TARGET_FILE_DIR:Iriseus>/softcam.dll"
    COMMENT "Copying softcam.dll"
)

add_custom_command(TARGET Iriseus POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${FFMPEG_ROOT}/bin/avcodec-63.dll"
        "${FFMPEG_ROOT}/bin/avutil-61.dll"
        "${FFMPEG_ROOT}/bin/swscale-10.dll"
        "${FFMPEG_ROOT}/bin/swresample-7.dll"
        "$<TARGET_FILE_DIR:Iriseus>"
    COMMENT "Copying FFmpeg DLLs"
)