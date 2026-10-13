#include "pipe_protocol.h"

namespace pipe_protocol
{
PayloadWriter::PayloadWriter() : bytes_(MaxPayloadSize)
{
}

bool PayloadWriter::WriteInt32(int32_t value)
{
    return bytes_.Append(reinterpret_cast<const BYTE*>(&value), sizeof(value));
}

bool PayloadWriter::WriteString(const wchar_t* text, size_t length)
{
    if (length > MaxStringLength)
    {
        return false;
    }
    const auto prefix = static_cast<uint16_t>(length);
    return bytes_.Append(reinterpret_cast<const BYTE*>(&prefix), sizeof(prefix)) &&
        bytes_.Append(reinterpret_cast<const BYTE*>(text), length * sizeof(wchar_t));
}

bool PayloadWriter::WriteString(const std::wstring& text)
{
    return WriteString(text.c_str(), text.size());
}

bool PayloadWriter::WriteStringList(const std::vector<std::wstring>& texts)
{
    if (texts.size() > MaxListLength)
    {
        return false;
    }
    const auto count = static_cast<uint16_t>(texts.size());
    if (!bytes_.Append(reinterpret_cast<const BYTE*>(&count), sizeof(count)))
    {
        return false;
    }
    for (const std::wstring& text : texts)
    {
        if (!WriteString(text))
        {
            return false;
        }
    }
    return true;
}

const SecureBytes& PayloadWriter::Bytes() const
{
    return bytes_;
}

PayloadReader::PayloadReader(const SecureBytes& bytes) : bytes_(bytes)
{
}

bool PayloadReader::ReadInt32(int32_t* value)
{
    return Take(value, sizeof(*value));
}

bool PayloadReader::ReadString(std::wstring* text)
{
    uint16_t length = 0;
    if (!ReadLength(&length))
    {
        return false;
    }
    text->resize(length);
    return Take(text->data(), length * sizeof(wchar_t));
}

bool PayloadReader::ReadSecret(SecureString* secret)
{
    uint16_t length = 0;
    if (!ReadLength(&length) || length * sizeof(wchar_t) > bytes_.size() - position_)
    {
        return false;
    }
    *secret = SecureString(reinterpret_cast<const wchar_t*>(bytes_.data() + position_), length);
    position_ += length * sizeof(wchar_t);
    return true;
}

bool PayloadReader::ReadStringList(std::vector<std::wstring>* texts)
{
    uint16_t count = 0;
    if (!Take(&count, sizeof(count)) || count > MaxListLength)
    {
        return false;
    }
    texts->assign(count, std::wstring());
    for (std::wstring& text : *texts)
    {
        if (!ReadString(&text))
        {
            return false;
        }
    }
    return true;
}

bool PayloadReader::AtEnd() const
{
    return position_ == bytes_.size();
}

bool PayloadReader::ReadLength(uint16_t* length)
{
    return Take(length, sizeof(*length)) && *length <= MaxStringLength;
}

bool PayloadReader::Take(void* destination, size_t size)
{
    if (size > bytes_.size() - position_)
    {
        return false;
    }
    CopyMemory(destination, bytes_.data() + position_, size);
    position_ += size;
    return true;
}
}
