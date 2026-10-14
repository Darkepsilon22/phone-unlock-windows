#include "unlock/unlock_requests.h"
#include "test_runner.h"
#include "test_support.h"

namespace
{
using namespace std::chrono_literals;
using test_support::GeneratedKey;

const std::wstring Alice = L"S-1-5-21-1-2-3-1001";
constexpr ULONGLONG RequestLifetime = 30'000;
constexpr ULONGLONG QuotaWindow = 60'000;

bool StopWaiting()
{
    return false;
}

std::string TextField(const cbor::Map& map, uint64_t key)
{
    const std::string* text = cbor::AsText(cbor::Find(map, key));
    return text ? *text : std::string();
}

class UnlockFixture
{
public:
    UnlockFixture()
        : tiles_(changes_),
          requests_(tiles_, cbor::Bytes(16, 3), "PC salon", [](const std::wstring&) { return std::string("hanjason"); },
              clock_.Function(), [] { return uint64_t{ 1'760'000'000 }; })
    {
        pairing_.pairingId = cbor::Bytes(16, 4);
        pairing_.accountSid = Alice;
        pairing_.phoneName = "Pixel 5";
        pairing_.authorizationSpki = phoneKey_.Spki();
    }

    uint64_t ShowTile(TileScenario scenario = TileScenario::Unlock)
    {
        return *tiles_.Register(scenario, { Alice });
    }

    HttpResponse Create()
    {
        const HttpResponse response = requests_.Create(pairing_);
        if (response.status == 200)
        {
            last_ = RequestFrom(response);
        }
        return response;
    }

    HttpResponse Prove(const unlock_protocol::AuthorizationRequest& request, bool tamper = false)
    {
        std::vector<uint8_t> signature = phoneKey_.Sign(unlock_protocol::SignedMessage(request));
        if (tamper)
        {
            signature[10] ^= 1;
        }
        return requests_.Prove(pairing_, request.requestId,
            cbor::Encode(cbor::Object({ { 1, cbor::ByteString(request.requestId) }, { 2, cbor::ByteString(signature) } })));
    }

    HttpResponse Prove(bool tamper = false)
    {
        return Prove(last_, tamper);
    }

    std::string ResultState(const unlock_protocol::AuthorizationRequest& request)
    {
        const HttpResponse response = requests_.Result(pairing_, request.requestId, 10ms);
        std::optional<cbor::Value> holder = cbor::Decode(response.body);
        return TextField(*cbor::AsMap(&*holder), 1);
    }

    static std::string ErrorCode(const HttpResponse& response)
    {
        std::optional<cbor::Value> holder = cbor::Decode(response.body);
        return TextField(*cbor::AsMap(&*holder), 1);
    }

    LogonTiles& Tiles()
    {
        return tiles_;
    }

    UnlockRequests& Requests()
    {
        return requests_;
    }

    test_support::FakeClock& Clock()
    {
        return clock_;
    }

    Pairing& PhonePairing()
    {
        return pairing_;
    }

    const unlock_protocol::AuthorizationRequest& Last() const
    {
        return last_;
    }

private:
    static unlock_protocol::AuthorizationRequest RequestFrom(const HttpResponse& response)
    {
        const std::optional<cbor::Value> holder = cbor::Decode(response.body);
        const cbor::Map& map = *cbor::AsMap(&*holder);
        unlock_protocol::AuthorizationRequest request;
        request.action = static_cast<unlock_protocol::Action>(*cbor::AsUInt(cbor::Find(map, 2)));
        request.requestId = *cbor::AsBytes(cbor::Find(map, 3));
        request.nonce = *cbor::AsBytes(cbor::Find(map, 4));
        request.pcId = *cbor::AsBytes(cbor::Find(map, 5));
        request.pairingId = *cbor::AsBytes(cbor::Find(map, 6));
        request.accountSid = TextField(map, 7);
        const cbor::Bytes& context = *cbor::AsBytes(cbor::Find(map, 8));
        std::copy(context.begin(), context.end(), request.context.begin());
        request.issuedAt = *cbor::AsUInt(cbor::Find(map, 9));
        request.expiresAt = *cbor::AsUInt(cbor::Find(map, 10));
        request.pcName = TextField(map, 11);
        request.accountName = TextField(map, 12);
        return request;
    }

    ChangeFeed changes_;
    LogonTiles tiles_;
    test_support::FakeClock clock_;
    UnlockRequests requests_;
    GeneratedKey phoneKey_;
    Pairing pairing_;
    unlock_protocol::AuthorizationRequest last_;
};

TEST(SignedRequestUnlocksTheShownTileOnce)
{
    UnlockFixture fixture;
    const uint64_t ticket = fixture.ShowTile();
    CHECK(fixture.Create().status == 200);
    CHECK(fixture.Last().action == unlock_protocol::Action::Unlock);
    CHECK(fixture.Last().context == fixture.Tiles().Find(Alice)->context);
    CHECK(fixture.Last().accountSid == "S-1-5-21-1-2-3-1001");
    CHECK(fixture.Last().expiresAt == fixture.Last().issuedAt + 30);
    CHECK(fixture.Last().pcName == "PC salon");
    CHECK(fixture.Last().accountName == "hanjason");

    CHECK(fixture.Prove().status == 200);
    CHECK(fixture.Tiles().WaitForAuthorization(ticket, StopWaiting) == Alice);
    CHECK(fixture.ResultState(fixture.Last()) == "consumed");

    fixture.Requests().ReportLogonResult(Alice, 0);
    CHECK(fixture.ResultState(fixture.Last()) == "logon_succeeded");
}

TEST(ReplayedProofIsRejected)
{
    UnlockFixture fixture;
    fixture.ShowTile();
    fixture.Create();
    CHECK(fixture.Prove().status == 200);
    CHECK(fixture.Prove().status == 410);
}

TEST(NoTileMeansWrongContext)
{
    UnlockFixture fixture;
    const HttpResponse response = fixture.Create();
    CHECK(response.status == 409);
    CHECK(UnlockFixture::ErrorCode(response) == "WRONG_CONTEXT");
}

TEST(LogonScreenRequestsAnInteractiveLogon)
{
    UnlockFixture fixture;
    fixture.ShowTile(TileScenario::Logon);
    fixture.Create();
    CHECK(fixture.Last().action == unlock_protocol::Action::InteractiveLogon);
}

TEST(ExpiredRequestIsRefusedWithoutAuthorizing)
{
    UnlockFixture fixture;
    const uint64_t ticket = fixture.ShowTile();
    fixture.Create();
    fixture.Clock().Advance(RequestLifetime);
    const HttpResponse response = fixture.Prove();
    CHECK(response.status == 410);
    CHECK(UnlockFixture::ErrorCode(response) == "EXPIRED");
    CHECK(!fixture.Tiles().WaitForAuthorization(ticket, StopWaiting).has_value());
}

TEST(RequestIsStillValidJustBeforeExpiry)
{
    UnlockFixture fixture;
    fixture.ShowTile();
    fixture.Create();
    fixture.Clock().Advance(RequestLifetime - 1);
    CHECK(fixture.Prove().status == 200);
}

TEST(NewLockScreenInvalidatesTheRequest)
{
    UnlockFixture fixture;
    const uint64_t first = fixture.ShowTile();
    fixture.Create();
    fixture.Tiles().Unregister(first);
    fixture.ShowTile();
    const HttpResponse response = fixture.Prove();
    CHECK(response.status == 409);
    CHECK(UnlockFixture::ErrorCode(response) == "WRONG_CONTEXT");
}

TEST(ForgedSignatureEndsTheRequest)
{
    UnlockFixture fixture;
    fixture.ShowTile();
    fixture.Create();
    const HttpResponse forged = fixture.Prove(true);
    CHECK(forged.status == 400);
    CHECK(UnlockFixture::ErrorCode(forged) == "INVALID_PROOF");
    CHECK(fixture.Prove().status == 410);
}

TEST(SignatureOverAlteredFieldsIsRejected)
{
    UnlockFixture fixture;
    fixture.ShowTile();
    fixture.Create();
    unlock_protocol::AuthorizationRequest altered = fixture.Last();
    altered.accountName = "autre";
    CHECK(fixture.Prove(altered).status == 400);
}

TEST(AnotherPairingCannotProveTheRequest)
{
    UnlockFixture fixture;
    fixture.ShowTile();
    fixture.Create();
    fixture.PhonePairing().pairingId = cbor::Bytes(16, 9);
    CHECK(fixture.Prove().status == 410);
}

TEST(NewRequestReplacesThePreviousOne)
{
    UnlockFixture fixture;
    fixture.ShowTile();
    fixture.Create();
    const unlock_protocol::AuthorizationRequest first = fixture.Last();
    fixture.Create();
    CHECK(fixture.Prove(first).status == 410);
    CHECK(fixture.Prove().status == 200);
}

TEST(ThreeRequestsPerMinuteAtMost)
{
    UnlockFixture fixture;
    fixture.ShowTile();
    CHECK(fixture.Create().status == 200);
    CHECK(fixture.Create().status == 200);
    CHECK(fixture.Create().status == 200);
    const HttpResponse limited = fixture.Create();
    CHECK(limited.status == 429);
    CHECK(UnlockFixture::ErrorCode(limited) == "RATE_LIMITED");
    fixture.Clock().Advance(QuotaWindow);
    CHECK(fixture.Create().status == 200);
}

TEST(WindowsRejectionIsReported)
{
    UnlockFixture fixture;
    fixture.ShowTile();
    fixture.Create();
    fixture.Prove();
    fixture.Requests().ReportLogonResult(Alice, static_cast<LONG>(0xC000006D));
    CHECK(fixture.ResultState(fixture.Last()) == "logon_rejected");
}

TEST(MalformedProofIsRejected)
{
    UnlockFixture fixture;
    fixture.ShowTile();
    fixture.Create();
    const cbor::Bytes& id = fixture.Last().requestId;
    CHECK(fixture.Requests().Prove(fixture.PhonePairing(), id, cbor::Bytes{ 0x01 }).status == 400);
    CHECK(fixture.Requests()
              .Prove(fixture.PhonePairing(), id,
                  cbor::Encode(cbor::Object({ { 1, cbor::ByteString(id) }, { 2, cbor::ByteString(cbor::Bytes(10, 0)) } })))
              .status == 400);
    CHECK(fixture.Requests()
              .Prove(fixture.PhonePairing(), cbor::Bytes(16, 0),
                  cbor::Encode(cbor::Object({ { 1, cbor::ByteString(id) }, { 2, cbor::ByteString(cbor::Bytes(64, 0)) } })))
              .status == 400);
}
}
