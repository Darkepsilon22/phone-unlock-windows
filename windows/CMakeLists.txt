cmake_minimum_required(VERSION 3.20)
project(PhoneUnlockWindows VERSION 0.1.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(PHONEUNLOCK_TEST_COMMANDS "Inclut la commande de test authorize qui simule le telephone" OFF)
set(PHONEUNLOCK_VERSION ${PROJECT_VERSION})
if(PHONEUNLOCK_TEST_COMMANDS)
    string(APPEND PHONEUNLOCK_VERSION "-test")
endif()

add_compile_definitions(UNICODE _UNICODE NOMINMAX)
add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/utf-8;/permissive-;/sdl;/guard:cf;/EHsc>")
add_link_options(/guard:cf /DYNAMICBASE /NXCOMPAT)

function(phoneunlock_add_version target file_type file_name description)
    set(PHONEUNLOCK_FILE_TYPE ${file_type})
    set(PHONEUNLOCK_FILE_NAME ${file_name})
    set(PHONEUNLOCK_FILE_DESCRIPTION ${description})
    configure_file(${PROJECT_SOURCE_DIR}/shared/version.rc.in ${CMAKE_CURRENT_BINARY_DIR}/${target}_version.rc @ONLY)
    target_sources(${target} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/${target}_version.rc)
endfunction()

enable_testing()

add_subdirectory(shared)
add_subdirectory(credential-provider)
add_subdirectory(service)
