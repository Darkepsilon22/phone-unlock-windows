add_library(PhoneUnlockShared STATIC
    src/log.cpp
    src/pipe_io.cpp
    src/pipe_protocol.cpp
)

target_include_directories(PhoneUnlockShared PUBLIC src)
