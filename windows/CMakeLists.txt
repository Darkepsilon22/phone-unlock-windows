cmake_minimum_required(VERSION 3.20)
project(PhoneUnlockWindows LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_compile_definitions(UNICODE _UNICODE NOMINMAX)
add_compile_options(/W4 /WX /utf-8 /permissive- /sdl /guard:cf /EHsc)
add_link_options(/guard:cf /DYNAMICBASE /NXCOMPAT)

add_subdirectory(shared)
add_subdirectory(credential-provider)
add_subdirectory(service)
