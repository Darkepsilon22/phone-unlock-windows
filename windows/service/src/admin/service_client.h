#pragma once

#include "pipe_protocol.h"
#include <optional>

namespace exit_code
{
constexpr int Success = 0;
constexpr int Failure = 1;
}

std::optional<pipe_protocol::Message> ExchangeWithService(pipe_protocol::MessageType type,
    const pipe_protocol::PayloadWriter& request);

bool ReadSuccess(pipe_protocol::PayloadReader& reply);

int SendAndReport(pipe_protocol::MessageType type, const pipe_protocol::PayloadWriter& request);
