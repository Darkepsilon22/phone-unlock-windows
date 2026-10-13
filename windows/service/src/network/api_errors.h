#pragma once

#include "network/http_server.h"

namespace api_errors
{
constexpr USHORT Ok = 200;
constexpr USHORT BadRequest = 400;
constexpr USHORT Forbidden = 403;
constexpr USHORT NotFound = 404;
constexpr USHORT Conflict = 409;
constexpr USHORT Gone = 410;

HttpResponse Error(USHORT status, const char* code);

HttpResponse Body(const cbor::Value& value);
}
