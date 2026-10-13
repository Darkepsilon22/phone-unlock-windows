#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace cbor
{
struct Value;

using Bytes = std::vector<uint8_t>;
using Array = std::vector<Value>;
using Map = std::vector<std::pair<uint64_t, Value>>;

struct Value
{
    std::variant<uint64_t, bool, Bytes, std::string, Array, Map> data;
};

Value UInt(uint64_t value);
Value Boolean(bool value);
Value ByteString(Bytes value);
Value Text(std::string value);
Value List(Array items);
Value Object(Map entries);

Bytes Encode(const Value& value);

std::optional<Value> Decode(std::span<const uint8_t> encoded);

const Value* Find(const Map& map, uint64_t key);
const Map* AsMap(const Value* value);
const Array* AsArray(const Value* value);
std::optional<uint64_t> AsUInt(const Value* value);
std::optional<bool> AsBool(const Value* value);
const Bytes* AsBytes(const Value* value);
const std::string* AsText(const Value* value);
}
