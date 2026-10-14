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

TEST(PairingTheSamePhoneAgainReplacesThePreviousPairing)
{
    test_support::TemporaryDirectory directory;
    Pairing first = SamplePairing();
    Pairing second = SamplePairing();
    second.pairingId = cbor::Bytes(16, 0xCD);
    second.createdAt = first.createdAt + 60;
    {
        PairingStore store(directory.Path());
        CHECK(SUCCEEDED(store.Save(first)));
        CHECK(SUCCEEDED(store.Save(second)));
        CHECK(store.Count() == 1);
        CHECK(store.FindByTransport(second.transportFingerprint)->pairingId == second.pairingId);
    }
    PairingStore reloaded(directory.Path());
    CHECK(reloaded.Count() == 1);
    CHECK(reloaded.FindByTransport(second.transportFingerprint)->pairingId == second.pairingId);
}

TEST(AnotherPhoneKeepsItsOwnPairing)
{
    test_support::TemporaryDirectory directory;
    PairingStore store(directory.Path());
    Pairing other = SamplePairing();
    other.pairingId = cbor::Bytes(16, 0xCD);
    other.transportFingerprint.fill(0x43);
    CHECK(SUCCEEDED(store.Save(SamplePairing())));
    CHECK(SUCCEEDED(store.Save(other)));
    CHECK(store.Count() == 2);
}

TEST(RevokedPairingIsGoneEvenAfterRestart)
{
    test_support::TemporaryDirectory directory;
    const Pairing pairing = SamplePairing();
    {
        PairingStore store(directory.Path());
        CHECK(SUCCEEDED(store.Save(pairing)));
        CHECK(SUCCEEDED(store.Remove(pairing.pairingId)));
        CHECK(!store.FindByTransport(pairing.transportFingerprint).has_value());
        CHECK(!store.FindByAccount(pairing.accountSid).has_value());
        CHECK(store.All().empty());
    }
    PairingStore reloaded(directory.Path());
    CHECK(reloaded.Count() == 0);
}

TEST(RevokingAnUnknownPairingFails)
{
    test_support::TemporaryDirectory directory;
    PairingStore store(directory.Path());
    CHECK(store.Remove(cbor::Bytes(16, 0x01)) == HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
}
}
