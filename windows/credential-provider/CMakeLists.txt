add_library(PhoneUnlockProtoCP SHARED
    src/credential.cpp
    src/dll.cpp
    src/fields.cpp
    src/guid.cpp
    src/password_protection.cpp
    src/provider.cpp
    src/serialization.cpp
    src/service_listener.cpp
    src/tile_bitmap.cpp
    src/PhoneUnlockProtoCP.def
)

phoneunlock_add_version(PhoneUnlockProtoCP VFT_DLL PhoneUnlockProtoCP.dll "PhoneUnlock : tuile de connexion")
target_link_libraries(PhoneUnlockProtoCP PRIVATE PhoneUnlockShared bcrypt secur32 shlwapi ole32 gdi32 user32 advapi32 uuid)
