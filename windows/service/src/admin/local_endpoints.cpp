#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include "admin/local_endpoints.h"

namespace
{
constexpr ULONG AdapterBufferSize = 32 * 1024;
constexpr ULONG AdapterFlags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;

bool IsUsable(const IP_ADAPTER_ADDRESSES& adapter)
{
    return adapter.OperStatus == IfOperStatusUp && adapter.IfType != IF_TYPE_SOFTWARE_LOOPBACK;
}
}

std::vector<std::wstring> LocalIpv4Endpoints(unsigned short port)
{
    std::vector<std::wstring> endpoints;
    std::vector<BYTE> buffer(AdapterBufferSize);
    ULONG size = AdapterBufferSize;
    auto* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
    if (GetAdaptersAddresses(AF_INET, AdapterFlags, nullptr, adapters, &size) != NO_ERROR)
    {
        return endpoints;
    }

    const std::wstring suffix = L":" + std::to_wstring(port);
    for (auto* adapter = adapters; adapter; adapter = adapter->Next)
    {
        if (!IsUsable(*adapter))
        {
            continue;
        }
        for (auto* address = adapter->FirstUnicastAddress; address; address = address->Next)
        {
            wchar_t text[INET_ADDRSTRLEN] = {};
            const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(address->Address.lpSockaddr);
            if (InetNtopW(AF_INET, &ipv4->sin_addr, text, INET_ADDRSTRLEN))
            {
                endpoints.push_back(text + suffix);
            }
        }
    }
    return endpoints;
}
