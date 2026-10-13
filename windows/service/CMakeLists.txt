find_package(unofficial-nayuki-qr-code-generator CONFIG REQUIRED)

add_library(PhoneUnlockServiceCore STATIC
    src/admin_commands.cpp
    src/api.cpp
    src/api_errors.cpp
    src/authorization_broker.cpp
    src/cbor.cpp
    src/crypto.cpp
    src/file_io.cpp
    src/http_server.cpp
    src/pairing_manager.cpp
    src/pairing_protocol.cpp
    src/pairing_store.cpp
    src/pc_identity.cpp
    src/pipe_server.cpp
    src/qr_console.cpp
    src/secret_store.cpp
    src/service_host.cpp
    src/text.cpp
    src/worker_group.cpp
)

target_include_directories(PhoneUnlockServiceCore PUBLIC src)
target_compile_definitions(PhoneUnlockServiceCore PUBLIC WIN32_LEAN_AND_MEAN)
target_link_libraries(PhoneUnlockServiceCore PUBLIC
    PhoneUnlockShared
    unofficial::nayuki-qr-code-generator::nayuki-qr-code-generator
    advapi32 bcrypt crypt32 httpapi iphlpapi ws2_32 secur32
)

add_executable(PhoneUnlockService src/main.cpp)
target_link_libraries(PhoneUnlockService PRIVATE PhoneUnlockServiceCore)

add_executable(PhoneUnlockServiceTests
    tests/cbor_tests.cpp
    tests/crypto_tests.cpp
    tests/pairing_protocol_tests.cpp
    tests/test_runner.cpp
)
target_link_libraries(PhoneUnlockServiceTests PRIVATE PhoneUnlockServiceCore)
add_test(NAME PhoneUnlockServiceTests COMMAND PhoneUnlockServiceTests)
