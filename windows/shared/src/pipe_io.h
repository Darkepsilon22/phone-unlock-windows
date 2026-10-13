#pragma once

#include "pipe_protocol.h"
#include <optional>

namespace pipe_protocol
{
bool WriteMessage(HANDLE pipe, MessageType type, const SecureBytes& payload, HANDLE stopEvent);

std::optional<Message> ReadMessage(HANDLE pipe, HANDLE stopEvent);

HANDLE ConnectToService(PCWSTR pipeName, HANDLE stopEvent, DWORD timeoutMilliseconds);
}
