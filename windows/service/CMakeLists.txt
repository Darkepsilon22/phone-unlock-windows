find_package(unofficial-nayuki-qr-code-generator CONFIG REQUIRED)

add_library(PhoneUnlockServiceCore STATIC
    src/admin/admin_commands.cpp
    src/admin/qr_console.cpp
    src/crypto/crypto.cpp
    src/encoding/cbor.cpp
    src/encoding/text.cpp
    src/host/service_host.cpp
    src/ipc/authorization_broker.cpp
    src/ipc/pipe_server.cpp
    src/network/api.cpp
    src/network/api_errors.cpp
    src/network/http_server.cpp
    src/pairing/pairing_manager.cpp
    src/pairing/pairing_protocol.cpp
    src/pairing/pc_identity.cpp
    src/storage/file_io.cpp
    src/storage/pairing_store.cpp
    src/storage/secret_store.cpp
    src/support/worker_group.cpp
)

target_include_directories(PhoneUnlockServiceCore PUBLIC src)
target_compile_definitions(PhoneUnlockServiceCore PUBLIC WIN32_LEAN_AND_MEAN)
target_link_libraries(PhoneUnlockServiceCore PUBLIC
    PhoneUnlockShared
    unofficial::nayuki-qr-code-generator::nayuki-qr-code-generator
    advapi32 bcrypt crypt32 httpapi iphlpapi ws2_32 secur32
)

add_executable(PhoneUnlockService src/host/main.cpp)
target_link_libraries(PhoneUnlockService PRIVATE PhoneUnlockServiceCore)

add_executable(PhoneUnlockServiceTests
    tests/cbor_tests.cpp
    tests/crypto_tests.cpp
    tests/pairing_protocol_tests.cpp
    tests/test_runner.cpp
)
target_link_libraries(PhoneUnlockServiceTests PRIVATE PhoneUnlockServiceCore)
add_test(NAME PhoneUnlockServiceTests COMMAND PhoneUnlockServiceTests)
