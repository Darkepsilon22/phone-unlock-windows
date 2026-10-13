#include "pipe_io.h"

namespace pipe_protocol
{
namespace
{
#pragma pack(push, 1)
struct Header
{
    uint32_t size;
    uint16_t type;
    uint16_t reserved;
};
#pragma pack(pop)

constexpr DWORD ConnectAttemptDelay = 200;

class OverlappedEvent
{
public:
    OverlappedEvent() : event_(CreateEventW(nullptr, TRUE, FALSE, nullptr))
    {
    }

    OverlappedEvent(const OverlappedEvent&) = delete;
    OverlappedEvent& operator=(const OverlappedEvent&) = delete;

    ~OverlappedEvent()
    {
        if (event_)
        {
            CloseHandle(event_);
        }
    }

    HANDLE get() const
    {
        return event_;
    }

private:
    HANDLE event_;
};

bool WaitForCompletion(HANDLE pipe, OVERLAPPED* overlapped, HANDLE stopEvent, DWORD* transferred)
{
    HANDLE handles[] = { overlapped->hEvent, stopEvent };
    const DWORD count = stopEvent ? 2 : 1;
    if (WaitForMultipleObjects(count, handles, FALSE, INFINITE) != WAIT_OBJECT_0)
    {
        CancelIoEx(pipe, overlapped);
        GetOverlappedResult(pipe, overlapped, transferred, TRUE);
        return false;
    }
    return GetOverlappedResult(pipe, overlapped, transferred, FALSE) != FALSE;
}

bool Transfer(HANDLE pipe, BYTE* buffer, DWORD size, bool writing, HANDLE stopEvent)
{
    OverlappedEvent completion;
    if (!completion.get())
    {
        return false;
    }

    DWORD done = 0;
    while (done < size)
    {
        OVERLAPPED overlapped = {};
        overlapped.hEvent = completion.get();
        ResetEvent(completion.get());

        DWORD transferred = 0;
        const BOOL started = writing
            ? WriteFile(pipe, buffer + done, size - done, &transferred, &overlapped)
            : ReadFile(pipe, buffer + done, size - done, &transferred, &overlapped);
        if (!started && GetLastError() != ERROR_IO_PENDING)
        {
            return false;
        }
        if (!WaitForCompletion(pipe, &overlapped, stopEvent, &transferred) || transferred == 0)
        {
            return false;
        }
        done += transferred;
    }
    return true;
}

bool IsKnownType(uint16_t type)
{
    return type >= static_cast<uint16_t>(MessageType::Wait) && type <= static_cast<uint16_t>(LastMessageType);
}
}

bool WriteMessage(HANDLE pipe, MessageType type, const SecureBytes& payload, HANDLE stopEvent)
{
    if (payload.size() > MaxPayloadSize)
    {
        return false;
    }

    SecureBytes frame(sizeof(Header) + payload.size());
    const Header header = { static_cast<uint32_t>(payload.size()), static_cast<uint16_t>(type), 0 };
    frame.Append(reinterpret_cast<const BYTE*>(&header), sizeof(header));
    frame.Append(payload.data(), payload.size());
    return Transfer(pipe, frame.data(), static_cast<DWORD>(frame.size()), true, stopEvent);
}

std::optional<Message> ReadMessage(HANDLE pipe, HANDLE stopEvent)
{
    Header header = {};
    if (!Transfer(pipe, reinterpret_cast<BYTE*>(&header), sizeof(header), false, stopEvent) ||
        header.size > MaxPayloadSize || !IsKnownType(header.type))
    {
        return std::nullopt;
    }

    Message message;
    message.type = static_cast<MessageType>(header.type);
    message.payload = SecureBytes(header.size);
    message.payload.ResizeWithinCapacity(header.size);
    if (header.size > 0 && !Transfer(pipe, message.payload.data(), header.size, false, stopEvent))
    {
        return std::nullopt;
    }
    return message;
}

HANDLE ConnectToService(PCWSTR pipeName, HANDLE stopEvent, DWORD timeoutMilliseconds)
{
    const ULONGLONG deadline = GetTickCount64() + timeoutMilliseconds;
    for (;;)
    {
        HANDLE pipe = CreateFileW(pipeName, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr);
        if (pipe != INVALID_HANDLE_VALUE || GetLastError() == ERROR_ACCESS_DENIED)
        {
            return pipe;
        }
        if (GetTickCount64() >= deadline)
        {
            return INVALID_HANDLE_VALUE;
        }
        if (stopEvent && WaitForSingleObject(stopEvent, ConnectAttemptDelay) == WAIT_OBJECT_0)
        {
            return INVALID_HANDLE_VALUE;
        }
        if (!stopEvent)
        {
            Sleep(ConnectAttemptDelay);
        }
    }
}
}
