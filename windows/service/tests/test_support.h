#pragma once

#include "crypto/crypto.h"
#include "encoding/cbor.h"
#include "support/clock.h"
#include <bcrypt.h>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace test_support
{
class GeneratedKey
{
public:
    GeneratedKey();
    ~GeneratedKey();

    GeneratedKey(const GeneratedKey&) = delete;
    GeneratedKey& operator=(const GeneratedKey&) = delete;

    const std::vector<uint8_t>& Spki() const;
    std::vector<uint8_t> Sign(std::span<const uint8_t> message) const;

private:
    BCRYPT_KEY_HANDLE handle_ = nullptr;
    std::vector<uint8_t> spki_;
};

class TemporaryDirectory
{
public:
    TemporaryDirectory();
    ~TemporaryDirectory();

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    const std::wstring& Path() const;

private:
    std::wstring path_;
};

class FakeClock
{
public:
    MonotonicClock Function();
    void Advance(ULONGLONG milliseconds);

private:
    ULONGLONG now_ = 1'000;
};

std::span<const uint8_t> AsBytes(std::string_view text);

std::optional<std::vector<uint8_t>> Base64UrlDecode(std::string_view text);
}
