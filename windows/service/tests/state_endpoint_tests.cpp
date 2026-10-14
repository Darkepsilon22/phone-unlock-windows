#include "network/state_endpoint.h"
#include "test_runner.h"
#include "test_support.h"

namespace
{
using namespace std::chrono_literals;

const std::wstring Alice = L"S-1-5-21-1-2-3-1001";

struct Fixture
{
    test_support::TemporaryDirectory directory;
    PairingStore pairings{ directory.Path() };
    ChangeFeed changes;
    Diagnostics diagnostics;
    std::wstring lastSid;
    StateEndpoint endpoint{ pairings, changes, diagnostics,
        [this](const std::wstring& sid) {
            lastSid = sid;
            return AccountView{ SessionState::Locked, true };
        },
        "PC salon", 10ms };

    Fixture()
    {
        Pairing pairing;
        pairing.pairingId = cbor::Bytes(16, 1);
        pairing.accountSid = Alice;
        pairing.phoneName = "Pixel 5";
        pairing.transportFingerprint.fill(0x42);
        pairing.authorizationSpki = { 1, 2, 3 };
        pairings.Save(pairing);
    }

    HttpRequest Request(std::wstring query, uint8_t fingerprintByte = 0x42) const
    {
        HttpRequest request;
        request.verb = HttpVerbGET;
        request.path = L"/v1/state";
        request.query = std::move(query);
        crypto::Digest fingerprint;
        fingerprint.fill(fingerprintByte);
        request.clientFingerprint = fingerprint;
        return request;
    }
};

const cbor::Map& BodyOf(const HttpResponse& response, std::optional<cbor::Value>& holder)
{
    holder = cbor::Decode(response.body);
    return *cbor::AsMap(&*holder);
}

TEST(StateDescribesThePairedAccount)
{
    Fixture fixture;
    const HttpResponse response = fixture.endpoint.Handle(fixture.Request(L""));
    std::optional<cbor::Value> holder;
    const cbor::Map& body = BodyOf(response, holder);
    CHECK(response.status == 200);
    CHECK(fixture.lastSid == Alice);
    CHECK(cbor::AsUInt(cbor::Find(body, 1)) == fixture.changes.Version());
    CHECK(*cbor::AsText(cbor::Find(body, 2)) == "locked");
    CHECK(cbor::AsBool(cbor::Find(body, 3)) == true);
    CHECK(*cbor::AsText(cbor::Find(body, 4)) == "PC salon");
}

TEST(StateWaitsForAChangeThenAnswersTheSameVersion)
{
    Fixture fixture;
    const uint64_t current = fixture.changes.Version();
    const HttpResponse response =
        fixture.endpoint.Handle(fixture.Request(L"?since=" + std::to_wstring(current)));
    std::optional<cbor::Value> holder;
    CHECK(cbor::AsUInt(cbor::Find(BodyOf(response, holder), 1)) == current);
}

TEST(StateRefusesUnknownPhones)
{
    Fixture fixture;
    CHECK(!fixture.diagnostics.LastError());
    CHECK(fixture.endpoint.Handle(fixture.Request(L"", 0x07)).status == 403);
    CHECK(fixture.diagnostics.LastError()->code == "REVOKED");
    HttpRequest anonymous = fixture.Request(L"");
    anonymous.clientFingerprint.reset();
    CHECK(fixture.endpoint.Handle(anonymous).status == 403);
}

TEST(StateRefusesMalformedQueries)
{
    Fixture fixture;
    CHECK(fixture.endpoint.Handle(fixture.Request(L"?since=")).status == 400);
    CHECK(fixture.endpoint.Handle(fixture.Request(L"?since=12a")).status == 400);
    CHECK(fixture.endpoint.Handle(fixture.Request(L"?other=1")).status == 400);
    CHECK(fixture.endpoint.Handle(fixture.Request(L"?since=99999999999999999999999")).status == 400);
}

TEST(RevokedPhoneIsRefusedAtOnce)
{
    Fixture fixture;
    CHECK(fixture.endpoint.Handle(fixture.Request(L"")).status == 200);
    CHECK(SUCCEEDED(fixture.pairings.Remove(cbor::Bytes(16, 1))));
    const HttpResponse response = fixture.endpoint.Handle(fixture.Request(L""));
    CHECK(response.status == 403);
    std::optional<cbor::Value> holder = cbor::Decode(response.body);
    CHECK(*cbor::AsText(cbor::Find(*cbor::AsMap(&*holder), 1)) == "REVOKED");
}
}
