cmake_minimum_required(VERSION 3.20)
project(PhoneUnlockProtoCP LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_library(PhoneUnlockProtoCP SHARED
    src/credential.cpp
    src/dll.cpp
    src/fields.cpp
    src/guid.cpp
    src/log.cpp
    src/provider.cpp
    src/secure_string.cpp
    src/serialization.cpp
    src/settings.cpp
    src/tile_bitmap.cpp
    src/PhoneUnlockProtoCP.def
)

target_compile_definitions(PhoneUnlockProtoCP PRIVATE UNICODE _UNICODE NOMINMAX)
target_compile_options(PhoneUnlockProtoCP PRIVATE /W4 /WX /utf-8 /permissive- /sdl /guard:cf)
target_link_options(PhoneUnlockProtoCP PRIVATE /guard:cf /DYNAMICBASE /NXCOMPAT)
target_link_libraries(PhoneUnlockProtoCP PRIVATE secur32 credui shlwapi ole32 gdi32 user32 advapi32 uuid)
