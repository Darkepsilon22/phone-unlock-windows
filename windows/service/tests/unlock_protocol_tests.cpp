#include "unlock/unlock_protocol.h"
#include "crypto/crypto.h"
#include "encoding/text.h"
#include "test_runner.h"

namespace
{
unlock_protocol::AuthorizationRequest ReferenceRequest()
{
    unlock_protocol::AuthorizationRequest request;
    request.action = unlock_protocol::Action::Unlock;
    request.requestId = cbor::Bytes(16, 1);
    request.nonce = cbor::Bytes(32, 2);
    request.pcId = cbor::Bytes(16, 3);
    request.pairingId = cbor::Bytes(16, 4);
    request.accountSid = "S-1-5-21-1-2-3-1001";
    request.context.fill(5);
    request.issuedAt = 1'760'000'000;
    request.expiresAt = 1'760'000'030;
    request.pcName = "PC salon";
    request.accountName = "hanjason";
    return request;
}

std::wstring Sha256Hex(const cbor::Bytes& bytes)
{
    return HexFromBytes(*crypto::Sha256(bytes));
}

TEST(AuthorizationRequestMatchesReferenceVector)
{
    const cbor::Bytes encoded = cbor::Encode(unlock_protocol::Encode(ReferenceRequest()));
    CHECK(encoded.size() == 165);
    CHECK(Sha256Hex(encoded) == L"36381e39271f498e623589294100e739f1d0afb2d58342c76ebb817df25eae85");
}

TEST(SignedMessageMatchesReferenceVector)
{
    const cbor::Bytes message = unlock_protocol::SignedMessage(ReferenceRequest());
    CHECK(message.size() == 192);
    CHECK(Sha256Hex(message) == L"59bdd4587141fd4adfd9baf5d3dfd0d6139785f3680ed775c895da842e722a50");
}

TEST(SignedMessageDependsOnContextAndNonce)
{
    unlock_protocol::AuthorizationRequest other = ReferenceRequest();
    other.context[0] ^= 1;
    CHECK(unlock_protocol::SignedMessage(other) != unlock_protocol::SignedMessage(ReferenceRequest()));
    other = ReferenceRequest();
    other.nonce[0] ^= 1;
    CHECK(unlock_protocol::SignedMessage(other) != unlock_protocol::SignedMessage(ReferenceRequest()));
}
}
