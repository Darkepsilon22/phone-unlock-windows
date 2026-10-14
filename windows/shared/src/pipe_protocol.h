#pragma once

#include "secure_vector.h"
#include <cstdint>
#include <string>
#include <vector>

namespace pipe_protocol
{
constexpr wchar_t LogonUiPipeName[] = L"\\\\.\\pipe\\PhoneUnlock.LogonUi";
constexpr wchar_t AdminPipeName[] = L"\\\\.\\pipe\\PhoneUnlock.Admin";
constexpr wchar_t ServiceName[] = L"PhoneUnlockService";
constexpr uint32_t MaxPayloadSize = 4096;
constexpr uint16_t MaxStringLength = 256;
constexpr uint16_t MaxListLength = 32;

enum class MessageType : uint16_t
{
    Wait = 1,
    Ready = 2,
    Result = 3,
    Provision = 4,
    Authorize = 5,
    Ack = 6,
    StartPairing = 7,
    PairingStatus = 8,
    ConfirmPairing = 9,
    RequestUnlock = 10,
    UnlockCode = 11,
    ListPhones = 12,
    RevokePhone = 13,
    Status = 14,
    ClearHistory = 15,
    CancelUnlock = 16,
};

constexpr MessageType LastMessageType = MessageType::CancelUnlock;
constexpr size_t ContextIdSize = 16;

enum class TileRequestOutcome : int32_t
{
    Created = 0,
    NoPhone = 1,
    WrongContext = 2,
    RateLimited = 3,
};

enum class TileScenario : int32_t
{
    Logon = 1,
    Unlock = 2,
};

struct Message
{
    MessageType type = MessageType::Ack;
    SecureBytes payload;
};

class PayloadWriter
{
public:
    PayloadWriter();

    bool WriteInt32(int32_t value);
    bool WriteString(const wchar_t* text, size_t length);
    bool WriteString(const std::wstring& text);
    bool WriteStringList(const std::vector<std::wstring>& texts);
    bool WriteBytes(const uint8_t* bytes, size_t length);

    const SecureBytes& Bytes() const;

private:
    SecureBytes bytes_;
};

class PayloadReader
{
public:
    explicit PayloadReader(const SecureBytes& bytes);

    bool ReadInt32(int32_t* value);
    bool ReadString(std::wstring* text);
    bool ReadSecret(SecureString* secret);
    bool ReadStringList(std::vector<std::wstring>* texts);
    bool ReadBytes(std::vector<uint8_t>* bytes);
    bool AtEnd() const;

private:
    bool ReadLength(uint16_t* length);
    bool Take(void* destination, size_t size);

    const SecureBytes& bytes_;
    size_t position_ = 0;
};
}
