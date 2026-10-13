#include "api_errors.h"

namespace api_errors
{
namespace
{
constexpr uint64_t ErrorCodeField = 1;
}

HttpResponse Error(USHORT status, const char* code)
{
    return { status, cbor::Encode(cbor::Object({ { ErrorCodeField, cbor::Text(code) } })) };
}

HttpResponse Body(const cbor::Value& value)
{
    return { Ok, cbor::Encode(value) };
}
}
