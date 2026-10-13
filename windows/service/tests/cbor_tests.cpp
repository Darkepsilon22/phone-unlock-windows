#include "encoding/cbor.h"
#include "test_runner.h"

namespace
{
cbor::Bytes Hex(std::initializer_list<int> bytes)
{
    cbor::Bytes out;
    for (int byte : bytes)
    {
        out.push_back(static_cast<uint8_t>(byte));
    }
    return out;
}

TEST(EncodesShortestIntegerHeads)
{
    CHECK(cbor::Encode(cbor::UInt(23)) == Hex({ 0x17 }));
    CHECK(cbor::Encode(cbor::UInt(24)) == Hex({ 0x18, 0x18 }));
    CHECK(cbor::Encode(cbor::UInt(256)) == Hex({ 0x19, 0x01, 0x00 }));
    CHECK(cbor::Encode(cbor::UInt(65536)) == Hex({ 0x1A, 0x00, 0x01, 0x00, 0x00 }));
}

TEST(SortsMapKeys)
{
    const cbor::Value map = cbor::Object({ { 10, cbor::UInt(1) }, { 2, cbor::Boolean(true) } });
    CHECK(cbor::Encode(map) == Hex({ 0xA2, 0x02, 0xF5, 0x0A, 0x01 }));
}

TEST(RoundTripsNestedValues)
{
    const cbor::Value original = cbor::Object({
        { 1, cbor::UInt(1) },
        { 2, cbor::ByteString({ 1, 2, 3 }) },
        { 3, cbor::Text("PC de test") },
        { 4, cbor::List({ cbor::Text("10.0.2.15"), cbor::UInt(47863) }) },
    });
    const cbor::Bytes encoded = cbor::Encode(original);
    const std::optional<cbor::Value> decoded = cbor::Decode(encoded);
    CHECK(decoded.has_value());
    CHECK(cbor::Encode(*decoded) == encoded);
    CHECK(*cbor::AsText(cbor::Find(*cbor::AsMap(&*decoded), 3)) == "PC de test");
}

TEST(RejectsNonShortestHeads)
{
    CHECK(!cbor::Decode(Hex({ 0x18, 0x17 })).has_value());
    CHECK(!cbor::Decode(Hex({ 0x19, 0x00, 0xFF })).has_value());
}

TEST(RejectsUnsortedOrDuplicateKeys)
{
    CHECK(!cbor::Decode(Hex({ 0xA2, 0x02, 0x01, 0x01, 0x01 })).has_value());
    CHECK(!cbor::Decode(Hex({ 0xA2, 0x01, 0x01, 0x01, 0x02 })).has_value());
}

TEST(RejectsIndefiniteLengthsAndTrailingBytes)
{
    CHECK(!cbor::Decode(Hex({ 0x5F, 0x41, 0x00, 0xFF })).has_value());
    CHECK(!cbor::Decode(Hex({ 0x01, 0x02 })).has_value());
}

TEST(RejectsTruncatedInput)
{
    CHECK(!cbor::Decode(Hex({ 0x43, 0x01, 0x02 })).has_value());
    CHECK(!cbor::Decode(Hex({ 0xA1, 0x01 })).has_value());
}

TEST(RejectsTextKeysAndNegativeIntegers)
{
    CHECK(!cbor::Decode(Hex({ 0xA1, 0x61, 0x61, 0x01 })).has_value());
    CHECK(!cbor::Decode(Hex({ 0x20 })).has_value());
}
}
