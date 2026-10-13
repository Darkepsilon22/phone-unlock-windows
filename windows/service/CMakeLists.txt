add_executable(PhoneUnlockService
    src/admin_commands.cpp
    src/authorization_broker.cpp
    src/main.cpp
    src/pipe_server.cpp
    src/secret_store.cpp
    src/service_host.cpp
)

target_link_libraries(PhoneUnlockService PRIVATE PhoneUnlockShared advapi32 crypt32 secur32)
