find_package(unofficial-nayuki-qr-code-generator CONFIG REQUIRED)

option(PHONEUNLOCK_TEST_COMMANDS "Inclut la commande de test authorize qui simule le telephone" OFF)

add_library(PhoneUnlockServiceCore STATIC
    src/admin/account_password.cpp
    src/admin/admin_commands.cpp
    src/admin/console.cpp
    src/admin/current_user.cpp
    src/admin/local_endpoints.cpp
    src/admin/pair_command.cpp
    src/admin/qr_console.cpp
    src/admin/service_client.cpp
    src/crypto/crypto.cpp
    src/encoding/cbor.cpp
    src/encoding/text.cpp
    src/host/service_host.cpp
    src/ipc/admin_requests.cpp
    src/ipc/logon_tiles.cpp
    src/ipc/logon_ui_session.cpp
    src/ipc/pipe_server.cpp
    src/network/api.cpp
    src/network/api_errors.cpp
    src/network/http_server.cpp
    src/network/state_endpoint.cpp
    src/pairing/pairing_manager.cpp
    src/pairing/pairing_protocol.cpp
    src/pairing/pc_identity.cpp
    src/session/pc_state.cpp
    src/session/session_monitor.cpp
    src/storage/file_io.cpp
    src/storage/pairing_store.cpp
    src/storage/secret_store.cpp
    src/support/change_feed.cpp
    src/support/worker_group.cpp
)

target_include_directories(PhoneUnlockServiceCore PUBLIC src)
target_compile_definitions(PhoneUnlockServiceCore PUBLIC
    WIN32_LEAN_AND_MEAN
    $<$<BOOL:${PHONEUNLOCK_TEST_COMMANDS}>:PHONEUNLOCK_TEST_COMMANDS>
)
target_link_libraries(PhoneUnlockServiceCore PUBLIC
    PhoneUnlockShared
    unofficial::nayuki-qr-code-generator::nayuki-qr-code-generator
    advapi32 bcrypt crypt32 httpapi iphlpapi netapi32 ws2_32 secur32 wtsapi32
)

add_executable(PhoneUnlockService src/host/main.cpp)
target_link_libraries(PhoneUnlockService PRIVATE PhoneUnlockServiceCore)

add_executable(PhoneUnlockServiceTests
    tests/cbor_tests.cpp
    tests/change_feed_tests.cpp
    tests/crypto_tests.cpp
    tests/logon_tiles_tests.cpp
    tests/pairing_manager_tests.cpp
    tests/pairing_protocol_tests.cpp
    tests/pairing_store_tests.cpp
    tests/pc_state_tests.cpp
    tests/state_endpoint_tests.cpp
    tests/test_runner.cpp
    tests/test_support.cpp
)
target_link_libraries(PhoneUnlockServiceTests PRIVATE PhoneUnlockServiceCore)
add_test(NAME PhoneUnlockServiceTests COMMAND PhoneUnlockServiceTests)
