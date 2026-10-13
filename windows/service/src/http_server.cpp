#include "http_server.h"
#include "log.h"
#include <array>

namespace
{
constexpr HTTPAPI_VERSION ApiVersion = HTTPAPI_VERSION_2;
constexpr size_t RequestBufferSize = 16 * 1024;
constexpr size_t MaxBodySize = 16 * 1024;
constexpr size_t ClientCertificateBufferSize = 8 * 1024;
constexpr size_t MaxConcurrentRequests = 8;
constexpr USHORT TooManyRequests = 503;
constexpr USHORT TooLarge = 413;
constexpr char ContentType[] = "application/cbor";

const char* ReasonFor(USHORT status)
{
    switch (status)
    {
    case 200:
        return "OK";
    case 204:
        return "No Content";
    case 400:
        return "Bad Request";
    case 403:
        return "Forbidden";
    case 404:
        return "Not Found";
    case 409:
        return "Conflict";
    case 410:
        return "Gone";
    case 413:
        return "Payload Too Large";
    case 429:
        return "Too Many Requests";
    default:
        return "Error";
    }
}

std::wstring FromCounted(PCWSTR text, USHORT lengthInBytes)
{
    return text ? std::wstring(text, lengthInBytes / sizeof(wchar_t)) : std::wstring();
}
}

HttpServer::HttpServer(std::wstring urlPrefix, HttpHandler handler)
    : urlPrefix_(std::move(urlPrefix)), handler_(std::move(handler))
{
}

HttpServer::~HttpServer()
{
    Close();
}

bool HttpServer::Run(HANDLE stopEvent)
{
    if (!Open())
    {
        Close();
        return false;
    }
    LogLine(L"https server listening");
    while (WaitForSingleObject(stopEvent, 0) != WAIT_OBJECT_0 && ReceiveNext(stopEvent))
    {
    }
    HttpShutdownRequestQueue(queue_);
    workers_.JoinAll();
    Close();
    LogLine(L"https server stopped");
    return true;
}

bool HttpServer::Open()
{
    ULONG result = HttpInitialize(ApiVersion, HTTP_INITIALIZE_SERVER, nullptr);
    initialized_ = result == NO_ERROR;
    if (result == NO_ERROR)
    {
        result = HttpCreateServerSession(ApiVersion, &session_, 0);
    }
    if (result == NO_ERROR)
    {
        result = HttpCreateUrlGroup(session_, &urlGroup_, 0);
    }
    if (result == NO_ERROR)
    {
        result = HttpCreateRequestQueue(ApiVersion, nullptr, nullptr, 0, &queue_);
    }
    if (result == NO_ERROR)
    {
        HTTP_BINDING_INFO binding = {};
        binding.Flags.Present = 1;
        binding.RequestQueueHandle = queue_;
        result = HttpSetUrlGroupProperty(urlGroup_, HttpServerBindingProperty, &binding, sizeof(binding));
    }
    if (result == NO_ERROR)
    {
        result = HttpAddUrlToUrlGroup(urlGroup_, urlPrefix_.c_str(), 0, 0);
    }
    if (result != NO_ERROR)
    {
        LogLine(L"https server setup failed error=%lu", result);
    }
    return result == NO_ERROR;
}

void HttpServer::Close()
{
    if (queue_)
    {
        HttpCloseRequestQueue(queue_);
        queue_ = nullptr;
    }
    if (urlGroup_)
    {
        HttpCloseUrlGroup(urlGroup_);
        urlGroup_ = 0;
    }
    if (session_)
    {
        HttpCloseServerSession(session_);
        session_ = 0;
    }
    if (initialized_)
    {
        HttpTerminate(HTTP_INITIALIZE_SERVER, nullptr);
        initialized_ = false;
    }
}

bool HttpServer::ReceiveNext(HANDLE stopEvent)
{
    std::array<uint64_t, RequestBufferSize / sizeof(uint64_t)> buffer = {};
    auto* request = reinterpret_cast<HTTP_REQUEST*>(buffer.data());

    OVERLAPPED overlapped = {};
    overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!overlapped.hEvent)
    {
        return false;
    }

    HTTP_REQUEST_ID nullId;
    HTTP_SET_NULL_ID(&nullId);
    ULONG result = HttpReceiveHttpRequest(queue_, nullId, 0, request, static_cast<ULONG>(sizeof(buffer)), nullptr,
        &overlapped);
    DWORD received = 0;
    if (result == ERROR_IO_PENDING)
    {
        HANDLE handles[] = { overlapped.hEvent, stopEvent };
        if (WaitForMultipleObjects(2, handles, FALSE, INFINITE) != WAIT_OBJECT_0)
        {
            CancelIoEx(queue_, &overlapped);
            GetOverlappedResult(queue_, &overlapped, &received, TRUE);
            CloseHandle(overlapped.hEvent);
            return false;
        }
        result = GetOverlappedResult(queue_, &overlapped, &received, FALSE) ? NO_ERROR : GetLastError();
    }
    CloseHandle(overlapped.hEvent);

    if (result == ERROR_MORE_DATA)
    {
        Send(request->RequestId, { TooLarge, {} });
        return true;
    }
    if (result != NO_ERROR)
    {
        LogLine(L"https receive failed error=%lu", result);
        return result != ERROR_OPERATION_ABORTED && result != ERROR_INVALID_HANDLE;
    }

    HttpRequest copy;
    copy.verb = request->Verb;
    copy.path = FromCounted(request->CookedUrl.pAbsPath, request->CookedUrl.AbsPathLength);
    copy.query = FromCounted(request->CookedUrl.pQueryString, request->CookedUrl.QueryStringLength);
    const HTTP_REQUEST_ID requestId = request->RequestId;
    const HTTP_CONNECTION_ID connectionId = request->ConnectionId;
    const bool hasMoreBody = (request->Flags & HTTP_REQUEST_FLAG_MORE_ENTITY_BODY_EXISTS) != 0;

    if (workers_.ActiveCount() >= MaxConcurrentRequests)
    {
        Send(requestId, { TooManyRequests, {} });
        return true;
    }
    workers_.Start([this, requestId, connectionId, hasMoreBody, copy = std::move(copy)]() mutable {
        Handle(requestId, connectionId, std::move(copy), hasMoreBody);
    });
    return true;
}

void HttpServer::Handle(HTTP_REQUEST_ID requestId, HTTP_CONNECTION_ID connectionId, HttpRequest request,
    bool hasMoreBody)
{
    if (hasMoreBody && !ReadBody(requestId, &request.body))
    {
        Send(requestId, { TooLarge, {} });
        return;
    }
    request.clientFingerprint = ClientFingerprint(connectionId);
    Send(requestId, handler_(request));
}

bool HttpServer::ReadBody(HTTP_REQUEST_ID requestId, cbor::Bytes* body)
{
    std::array<uint8_t, 4096> chunk = {};
    for (;;)
    {
        ULONG read = 0;
        const ULONG result = HttpReceiveRequestEntityBody(queue_, requestId, 0, chunk.data(),
            static_cast<ULONG>(chunk.size()), &read, nullptr);
        if (result == ERROR_HANDLE_EOF)
        {
            return true;
        }
        if (result != NO_ERROR || body->size() + read > MaxBodySize)
        {
            return false;
        }
        body->insert(body->end(), chunk.begin(), chunk.begin() + read);
    }
}

std::optional<crypto::Digest> HttpServer::ClientFingerprint(HTTP_CONNECTION_ID connectionId)
{
    std::array<uint64_t, ClientCertificateBufferSize / sizeof(uint64_t)> buffer = {};
    auto* info = reinterpret_cast<HTTP_SSL_CLIENT_CERT_INFO*>(buffer.data());
    ULONG received = 0;
    const ULONG result = HttpReceiveClientCertificate(queue_, connectionId, 0, info,
        static_cast<ULONG>(sizeof(buffer)), &received, nullptr);
    if (result != NO_ERROR || !info->pCertEncoded || info->CertEncodedSize == 0)
    {
        return std::nullopt;
    }
    const std::optional<std::vector<uint8_t>> spki =
        crypto::CertificateSpki({ info->pCertEncoded, info->CertEncodedSize });
    return spki ? crypto::SpkiFingerprint(*spki) : std::nullopt;
}

void HttpServer::Send(HTTP_REQUEST_ID requestId, const HttpResponse& response)
{
    HTTP_RESPONSE http = {};
    http.StatusCode = response.status;
    http.pReason = ReasonFor(response.status);
    http.ReasonLength = static_cast<USHORT>(strlen(http.pReason));

    HTTP_DATA_CHUNK chunk = {};
    if (!response.body.empty())
    {
        auto& contentType = http.Headers.KnownHeaders[HttpHeaderContentType];
        contentType.pRawValue = ContentType;
        contentType.RawValueLength = static_cast<USHORT>(sizeof(ContentType) - 1);
        chunk.DataChunkType = HttpDataChunkFromMemory;
        chunk.FromMemory.pBuffer = const_cast<uint8_t*>(response.body.data());
        chunk.FromMemory.BufferLength = static_cast<ULONG>(response.body.size());
        http.EntityChunkCount = 1;
        http.pEntityChunks = &chunk;
    }

    ULONG sent = 0;
    HttpSendHttpResponse(queue_, requestId, 0, &http, nullptr, &sent, nullptr, 0, nullptr, nullptr);
}
