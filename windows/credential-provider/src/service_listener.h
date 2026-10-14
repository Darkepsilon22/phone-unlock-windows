#pragma once

#include "pipe_protocol.h"
#include "secure_vector.h"
#include <windows.h>
#include <array>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

class ServiceListener
{
public:
    using ReadyHandler = std::function<void(const std::wstring& sid, SecureString password)>;

    static constexpr wchar_t UnavailableStatus[] = L"Service PhoneUnlock injoignable.";

    ServiceListener(pipe_protocol::TileScenario scenario, std::vector<std::wstring> sids, ReadyHandler onReady);
    ~ServiceListener();

    ServiceListener(const ServiceListener&) = delete;
    ServiceListener& operator=(const ServiceListener&) = delete;

    void ReportResult(const std::wstring& sid, LONG status, LONG subStatus);
    std::wstring RequestUnlock(const std::wstring& sid);

    const std::vector<std::wstring>& Sids() const;

private:
    struct PendingResult
    {
        std::wstring sid;
        LONG status;
        LONG subStatus;
    };

    void Run();
    HANDLE Connect();
    bool IsGenuineService(HANDLE pipe) const;
    void Converse(HANDLE pipe);
    bool SendResultWhenReady(HANDLE pipe);

    pipe_protocol::TileScenario scenario_;
    std::array<uint8_t, pipe_protocol::ContextIdSize> context_;
    std::vector<std::wstring> sids_;
    ReadyHandler onReady_;
    HANDLE stopEvent_;
    HANDLE resultEvent_;
    std::mutex mutex_;
    std::optional<PendingResult> result_;
    std::thread worker_;
};
