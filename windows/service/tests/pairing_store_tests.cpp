#include "storage/pairing_store.h"
#include "test_runner.h"
#include "test_support.h"

namespace
{
Pairing SamplePairing()
{
    Pairing pairing;
    pairing.pairingId = cbor::Bytes(16, 0xAB);
    pairing.accountSid = L"S-1-5-21-1-2-3-1001";
    pairing.phoneName = "Pixel 5";
    pairing.transportFingerprint.fill(0x42);
    pairing.authorizationSpki = { 1, 2, 3 };
    pairing.createdAt = 1'760'000'000;
    return pairing;
}

TEST(SavedPairingIsFoundByTransportFingerprint)
{
    test_support::TemporaryDirectory directory;
    PairingStore store(directory.Path());
    const Pairing pairing = SamplePairing();
    CHECK(SUCCEEDED(store.Save(pairing)));
    CHECK(store.FindByTransport(pairing.transportFingerprint).has_value());

    crypto::Digest unknown = {};
    CHECK(!store.FindByTransport(unknown).has_value());
}

TEST(PairingsSurviveRestart)
{
    test_support::TemporaryDirectory directory;
    const Pairing pairing = SamplePairing();
    {
        PairingStore store(directory.Path());
        CHECK(SUCCEEDED(store.Save(pairing)));
    }
    PairingStore reloaded(directory.Path());
    const std::optional<Pairing> found = reloaded.FindByTransport(pairing.transportFingerprint);
    CHECK(reloaded.Count() == 1);
    CHECK(found.has_value());
    CHECK(found->accountSid == pairing.accountSid);
    CHECK(found->phoneName == pairing.phoneName);
    CHECK(found->authorizationSpki == pairing.authorizationSpki);
    CHECK(found->createdAt == pairing.createdAt);
}
}
