add_library(PhoneUnlockProtoCP SHARED
    src/credential.cpp
    src/dll.cpp
    src/fields.cpp
    src/guid.cpp
    src/provider.cpp
    src/secure_string.cpp
    src/serialization.cpp
    src/service_listener.cpp
    src/settings.cpp
    src/tile_bitmap.cpp
    src/PhoneUnlockProtoCP.def
)

target_link_libraries(PhoneUnlockProtoCP PRIVATE PhoneUnlockShared secur32 credui shlwapi ole32 gdi32 user32 advapi32 uuid)
