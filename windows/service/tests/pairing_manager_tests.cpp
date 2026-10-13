#include "pairing/pairing_manager.h"
#include "test_runner.h"
#include "test_support.h"

namespace
{
using test_support::GeneratedKey;

const std::wstring AccountSid = L"S-1-5-21-1-2-3-1001";
constexpr char QrPrefix[] = "phoneunlock:1:";
constexpr uint64_t QrTokenField = 4;
constexpr ULONGLONG SessionLifetime = 120'000;
constexpr int MaxAttempts = 3;

enum Status : USHORT
{
    Ok = 200,
    BadRequest = 400,
    Forbidden = 403,
    Conflict = 409,
    Gone = 410,
};

class PairingFixture
{
public:
    PairingFixture()
        : store_(directory_.Path()),
          identity_{ cbor::Bytes(16, 0x07), DigestFilledWith(0x01), "PC-TEST" },
          manager_(identity_, store_, clock_.Function()),
          phoneTransport_(DigestFilledWith(0x09))
    {
    }

    void Start()
    {
        const std::optional<std::string> qr = manager_.Start(AccountSid, { { "10.0.0.2", 47863 } });
        token_ = TokenFrom(*qr);
    }

    HttpResponse SendPairRequest(const cbor::Bytes& token, const crypto::Digest& transport)
    {
        HttpRequest request;
        request.clientFingerprint = transport;
        request.body = cbor::Encode(cbor::Object({
            { 1, cbor::ByteString(token) },
            { 2, cbor::ByteString(authorizationKey_.Spki()) },
            { 3, cbor::Text("Pixel 5") },
        }));
        return manager_.HandlePairRequest(request);
    }

    HttpResponse SendPairRequest()
    {
        return SendPairRequest(token_, phoneTransport_);
    }

    HttpResponse SendConfirmation(const crypto::Digest& transport, bool tamperSignature = false)
    {
        std::vector<uint8_t> signature = authorizationKey_.Sign(pairing_protocol::ConfirmationMessage(Transcript()));
        if (tamperSignature)
        {
            signature[0] ^= 0x01;
        }
        HttpRequest request;
        request.clientFingerprint = transport;
        request.body = cbor::Encode(cbor::Object({
            { 1, cbor::ByteString(pairingId_) },
            { 2, cbor::ByteString(signature) },
        }));
        return manager_.HandlePairConfirm(request);
    }

    HttpResponse SendConfirmation()
    {
        return SendConfirmation(phoneTransport_);
    }

    void RememberPairingId(const HttpResponse& challenge)
    {
        const std::optional<cbor::Value> decoded = cbor::Decode(challenge.body);
        pairingId_ = *cbor::AsBytes(cbor::Find(*cbor::AsMap(&*decoded), 1));
    }

    crypto::Digest Transcript() const
    {
        const crypto::Digest authorization = *crypto::Sha256(authorizationKey_.Spki());
        return *pairing_protocol::Transcript(identity_.pcId, identity_.fingerprint, phoneTransport_, authorization,
            token_, pairingId_);
    }

    PairingManager& Manager()
    {
        return manager_;
    }

    PairingStore& Store()
    {
        return store_;
    }

    test_support::FakeClock& Clock()
    {
        return clock_;
    }

    const cbor::Bytes& Token() const
    {
        return token_;
    }

    const crypto::Digest& PhoneTransport() const
    {
        return phoneTransport_;
    }

    static crypto::Digest DigestFilledWith(uint8_t value)
    {
        crypto::Digest digest;
        digest.fill(value);
        return digest;
    }

private:
    static cbor::Bytes TokenFrom(const std::string& qr)
    {
        const std::optional<std::vector<uint8_t>> encoded =
            test_support::Base64UrlDecode(std::string_view(qr).substr(sizeof(QrPrefix) - 1));
        const std::optional<cbor::Value> decoded = cbor::Decode(*encoded);
        return *cbor::AsBytes(cbor::Find(*cbor::AsMap(&*decoded), QrTokenField));
    }

    test_support::TemporaryDirectory directory_;
    test_support::FakeClock clock_;
    PairingStore store_;
    PcIdentity identity_;
    PairingManager manager_;
    GeneratedKey authorizationKey_;
    crypto::Digest phoneTransport_;
    cbor::Bytes token_;
    cbor::Bytes pairingId_;
};

TEST(CompletesPairingWhenBothSidesConfirm)
{
    PairingFixture fixture;
    fixture.Start();
    const HttpResponse challenge = fixture.SendPairRequest();
    CHECK(challenge.status == Ok);
    fixture.RememberPairingId(challenge);

    const PairingStatus awaiting = fixture.Manager().Status();
    CHECK(awaiting.stage == PairingStage::AwaitingConfirmation);
    CHECK(awaiting.code == pairing_protocol::ComparisonCode(fixture.Transcript()));
    CHECK(awaiting.phoneName == L"Pixel 5");

    CHECK(fixture.Manager().ConfirmOnPc(true));
    CHECK(fixture.SendConfirmation().status == Ok);
    CHECK(fixture.Manager().Status().stage == PairingStage::Completed);
    CHECK(fixture.Store().FindByTransport(fixture.PhoneTransport()).has_value());
}

TEST(RejectsWrongTokenAndFailsAfterThreeAttempts)
{
    PairingFixture fixture;
    fixture.Start();
    cbor::Bytes wrongToken = fixture.Token();
    wrongToken[0] ^= 0xFF;
    for (int attempt = 0; attempt < MaxAttempts; ++attempt)
    {
        CHECK(fixture.SendPairRequest(wrongToken, fixture.PhoneTransport()).status == Forbidden);
    }
    CHECK(fixture.Manager().Status().stage == PairingStage::Failed);
    CHECK(fixture.SendPairRequest().status == Gone);
}

TEST(TokenCannotBeReused)
{
    PairingFixture fixture;
    fixture.Start();
    CHECK(fixture.SendPairRequest().status == Ok);
    CHECK(fixture.SendPairRequest().status == Gone);
}

TEST(PhoneCannotConfirmBeforePc)
{
    PairingFixture fixture;
    fixture.Start();
    fixture.RememberPairingId(fixture.SendPairRequest());
    CHECK(fixture.SendConfirmation().status == Conflict);
    CHECK(fixture.Manager().Status().stage == PairingStage::AwaitingConfirmation);
}

TEST(RefusalOnPcFailsPairing)
{
    PairingFixture fixture;
    fixture.Start();
    fixture.RememberPairingId(fixture.SendPairRequest());
    CHECK(fixture.Manager().ConfirmOnPc(false));
    CHECK(fixture.Manager().Status().stage == PairingStage::Failed);
    CHECK(fixture.SendConfirmation().status == Gone);
}

TEST(RejectsTamperedConfirmationSignature)
{
    PairingFixture fixture;
    fixture.Start();
    fixture.RememberPairingId(fixture.SendPairRequest());
    fixture.Manager().ConfirmOnPc(true);
    CHECK(fixture.SendConfirmation(fixture.PhoneTransport(), true).status == BadRequest);
    CHECK(fixture.Manager().Status().stage == PairingStage::Failed);
    CHECK(!fixture.Store().FindByTransport(fixture.PhoneTransport()).has_value());
}

TEST(RejectsConfirmationFromAnotherPhone)
{
    PairingFixture fixture;
    fixture.Start();
    fixture.RememberPairingId(fixture.SendPairRequest());
    fixture.Manager().ConfirmOnPc(true);
    CHECK(fixture.SendConfirmation(PairingFixture::DigestFilledWith(0x66)).status == Gone);
    CHECK(fixture.Manager().Status().stage == PairingStage::PcConfirmed);
}

TEST(SessionExpiresAfterTwoMinutes)
{
    PairingFixture fixture;
    fixture.Start();
    fixture.Clock().Advance(SessionLifetime);
    CHECK(fixture.SendPairRequest().status == Gone);
    CHECK(fixture.Manager().Status().stage == PairingStage::Failed);
}

TEST(RejectsMalformedOrUnauthenticatedRequests)
{
    PairingFixture fixture;
    fixture.Start();

    HttpRequest garbage;
    garbage.clientFingerprint = fixture.PhoneTransport();
    garbage.body = { 0xFF, 0x00 };
    CHECK(fixture.Manager().HandlePairRequest(garbage).status == BadRequest);

    HttpRequest withoutCertificate;
    withoutCertificate.body = cbor::Encode(cbor::Object({ { 1, cbor::ByteString(fixture.Token()) } }));
    CHECK(fixture.Manager().HandlePairRequest(withoutCertificate).status == BadRequest);

    HttpRequest unknownField;
    unknownField.clientFingerprint = fixture.PhoneTransport();
    unknownField.body = cbor::Encode(cbor::Object({ { 9, cbor::UInt(1) } }));
    CHECK(fixture.Manager().HandlePairRequest(unknownField).status == BadRequest);

    CHECK(fixture.Manager().Status().stage == PairingStage::WaitingForPhone);
}
}
