#pragma once

#include "cbor.h"
#include "crypto.h"
#include "worker_group.h"
#include <winsock2.h>
#include <windows.h>
#include <http.h>
#include <functional>
#include <optional>
#include <string>

struct HttpRequest
{
    HTTP_VERB verb = HttpVerbUnknown;
    std::wstring path;
    std::wstring query;
    cbor::Bytes body;
    std::optional<crypto::Digest> clientFingerprint;
};

struct HttpResponse
{
    USHORT status = 200;
    cbor::Bytes body;
};

using HttpHandler = std::function<HttpResponse(const HttpRequest&)>;

class HttpServer
{
public:
    HttpServer(std::wstring urlPrefix, HttpHandler handler);
    ~HttpServer();

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    bool Run(HANDLE stopEvent);

private:
    bool Open();
    void Close();
    bool ReceiveNext(HANDLE stopEvent);
    void Handle(HTTP_REQUEST_ID requestId, HTTP_CONNECTION_ID connectionId, HttpRequest request, bool hasMoreBody);
    bool ReadBody(HTTP_REQUEST_ID requestId, cbor::Bytes* body);
    std::optional<crypto::Digest> ClientFingerprint(HTTP_CONNECTION_ID connectionId);
    void Send(HTTP_REQUEST_ID requestId, const HttpResponse& response);

    std::wstring urlPrefix_;
    HttpHandler handler_;
    HTTP_SERVER_SESSION_ID session_ = 0;
    HTTP_URL_GROUP_ID urlGroup_ = 0;
    HANDLE queue_ = nullptr;
    bool initialized_ = false;
    WorkerGroup workers_;
};
