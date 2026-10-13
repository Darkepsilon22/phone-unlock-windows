#include "encoding/cbor.h"
#include <algorithm>

namespace cbor
{
namespace
{
enum MajorType : uint8_t
{
    UnsignedInteger = 0,
    ByteStringType = 2,
    TextStringType = 3,
    ArrayType = 4,
    MapType = 5,
    SimpleType = 7,
};

constexpr uint8_t FalseValue = 20;
constexpr uint8_t TrueValue = 21;
constexpr uint8_t OneByteLength = 24;
constexpr uint8_t TwoByteLength = 25;
constexpr uint8_t FourByteLength = 26;
constexpr uint8_t EightByteLength = 27;
constexpr int MaxDepth = 8;
constexpr uint64_t MaxItems = 64;
constexpr int BitsPerByte = 8;

void WriteHead(Bytes& out, MajorType major, uint64_t argument)
{
    const auto type = static_cast<uint8_t>(major << 5);
    if (argument < OneByteLength)
    {
        out.push_back(static_cast<uint8_t>(type | argument));
        return;
    }

    int width = 8;
    uint8_t additional = EightByteLength;
    if (argument <= UINT8_MAX)
    {
        width = 1;
        additional = OneByteLength;
    }
    else if (argument <= UINT16_MAX)
    {
        width = 2;
        additional = TwoByteLength;
    }
    else if (argument <= UINT32_MAX)
    {
        width = 4;
        additional = FourByteLength;
    }
    out.push_back(static_cast<uint8_t>(type | additional));
    for (int shift = (width - 1) * BitsPerByte; shift >= 0; shift -= BitsPerByte)
    {
        out.push_back(static_cast<uint8_t>(argument >> shift));
    }
}

void EncodeInto(Bytes& out, const Value& value);

struct EncodeVisitor
{
    Bytes& out;

    void operator()(uint64_t number) const
    {
        WriteHead(out, UnsignedInteger, number);
    }

    void operator()(bool flag) const
    {
        out.push_back(static_cast<uint8_t>((SimpleType << 5) | (flag ? TrueValue : FalseValue)));
    }

    void operator()(const Bytes& bytes) const
    {
        WriteHead(out, ByteStringType, bytes.size());
        out.insert(out.end(), bytes.begin(), bytes.end());
    }

    void operator()(const std::string& text) const
    {
        WriteHead(out, TextStringType, text.size());
        out.insert(out.end(), text.begin(), text.end());
    }

    void operator()(const Array& items) const
    {
        WriteHead(out, ArrayType, items.size());
        for (const Value& item : items)
        {
            EncodeInto(out, item);
        }
    }

    void operator()(const Map& entries) const
    {
        Map sorted = entries;
        std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        WriteHead(out, MapType, sorted.size());
        for (const auto& [key, item] : sorted)
        {
            WriteHead(out, UnsignedInteger, key);
            EncodeInto(out, item);
        }
    }
};

void EncodeInto(Bytes& out, const Value& value)
{
    std::visit(EncodeVisitor{ out }, value.data);
}

class Decoder
{
public:
    explicit Decoder(std::span<const uint8_t> input) : input_(input)
    {
    }

    std::optional<Value> ReadValue(int depth)
    {
        if (depth > MaxDepth)
        {
            return std::nullopt;
        }
        uint8_t major = 0;
        uint64_t argument = 0;
        if (!ReadHead(&major, &argument))
        {
            return std::nullopt;
        }

        switch (major)
        {
        case UnsignedInteger:
            return UInt(argument);
        case ByteStringType:
            return ReadBytes(argument);
        case TextStringType:
            return ReadText(argument);
        case ArrayType:
            return ReadArray(argument, depth);
        case MapType:
            return ReadMap(argument, depth);
        case SimpleType:
            return ReadSimple(argument);
        default:
            return std::nullopt;
        }
    }

    bool AtEnd() const
    {
        return position_ == input_.size();
    }

private:
    bool ReadHead(uint8_t* major, uint64_t* argument)
    {
        if (position_ >= input_.size())
        {
            return false;
        }
        const uint8_t initial = input_[position_++];
        *major = static_cast<uint8_t>(initial >> 5);
        const uint8_t additional = initial & 0x1F;

        if (*major == SimpleType)
        {
            *argument = additional;
            return additional == FalseValue || additional == TrueValue;
        }
        if (additional < OneByteLength)
        {
            *argument = additional;
            return true;
        }

        int width = 0;
        uint64_t minimum = 0;
        switch (additional)
        {
        case OneByteLength:
            width = 1;
            minimum = OneByteLength;
            break;
        case TwoByteLength:
            width = 2;
            minimum = static_cast<uint64_t>(UINT8_MAX) + 1;
            break;
        case FourByteLength:
            width = 4;
            minimum = static_cast<uint64_t>(UINT16_MAX) + 1;
            break;
        case EightByteLength:
            width = 8;
            minimum = static_cast<uint64_t>(UINT32_MAX) + 1;
            break;
        default:
            return false;
        }
        if (input_.size() - position_ < static_cast<size_t>(width))
        {
            return false;
        }
        uint64_t value = 0;
        for (int index = 0; index < width; ++index)
        {
            value = (value << BitsPerByte) | input_[position_++];
        }
        *argument = value;
        return value >= minimum;
    }

    std::optional<Value> ReadBytes(uint64_t length)
    {
        if (length > input_.size() - position_)
        {
            return std::nullopt;
        }
        Bytes bytes(input_.begin() + position_, input_.begin() + position_ + length);
        position_ += length;
        return ByteString(std::move(bytes));
    }

    std::optional<Value> ReadText(uint64_t length)
    {
        if (length > input_.size() - position_)
        {
            return std::nullopt;
        }
        std::string text(reinterpret_cast<const char*>(input_.data() + position_), length);
        position_ += length;
        return Text(std::move(text));
    }

    std::optional<Value> ReadArray(uint64_t count, int depth)
    {
        if (count > MaxItems)
        {
            return std::nullopt;
        }
        Array items;
        for (uint64_t index = 0; index < count; ++index)
        {
            std::optional<Value> item = ReadValue(depth + 1);
            if (!item)
            {
                return std::nullopt;
            }
            items.push_back(std::move(*item));
        }
        return List(std::move(items));
    }

    std::optional<Value> ReadMap(uint64_t count, int depth)
    {
        if (count > MaxItems)
        {
            return std::nullopt;
        }
        Map entries;
        for (uint64_t index = 0; index < count; ++index)
        {
            uint8_t keyMajor = 0;
            uint64_t key = 0;
            if (!ReadHead(&keyMajor, &key) || keyMajor != UnsignedInteger ||
                (!entries.empty() && key <= entries.back().first))
            {
                return std::nullopt;
            }
            std::optional<Value> item = ReadValue(depth + 1);
            if (!item)
            {
                return std::nullopt;
            }
            entries.emplace_back(key, std::move(*item));
        }
        return Object(std::move(entries));
    }

    static std::optional<Value> ReadSimple(uint64_t argument)
    {
        return Boolean(argument == TrueValue);
    }

    std::span<const uint8_t> input_;
    size_t position_ = 0;
};
}

Value UInt(uint64_t value)
{
    return Value{ value };
}

Value Boolean(bool value)
{
    return Value{ value };
}

Value ByteString(Bytes value)
{
    return Value{ std::move(value) };
}

Value Text(std::string value)
{
    return Value{ std::move(value) };
}

Value List(Array items)
{
    return Value{ std::move(items) };
}

Value Object(Map entries)
{
    return Value{ std::move(entries) };
}

Bytes Encode(const Value& value)
{
    Bytes out;
    EncodeInto(out, value);
    return out;
}

std::optional<Value> Decode(std::span<const uint8_t> encoded)
{
    Decoder decoder(encoded);
    std::optional<Value> value = decoder.ReadValue(0);
    if (!value || !decoder.AtEnd())
    {
        return std::nullopt;
    }
    return value;
}

const Value* Find(const Map& map, uint64_t key)
{
    const auto match = std::find_if(map.begin(), map.end(), [key](const auto& entry) { return entry.first == key; });
    return match == map.end() ? nullptr : &match->second;
}

const Map* AsMap(const Value* value)
{
    return value ? std::get_if<Map>(&value->data) : nullptr;
}

const Array* AsArray(const Value* value)
{
    return value ? std::get_if<Array>(&value->data) : nullptr;
}

std::optional<uint64_t> AsUInt(const Value* value)
{
    const auto* number = value ? std::get_if<uint64_t>(&value->data) : nullptr;
    return number ? std::optional<uint64_t>(*number) : std::nullopt;
}

std::optional<bool> AsBool(const Value* value)
{
    const auto* flag = value ? std::get_if<bool>(&value->data) : nullptr;
    return flag ? std::optional<bool>(*flag) : std::nullopt;
}

const Bytes* AsBytes(const Value* value)
{
    return value ? std::get_if<Bytes>(&value->data) : nullptr;
}

const std::string* AsText(const Value* value)
{
    return value ? std::get_if<std::string>(&value->data) : nullptr;
}
}
