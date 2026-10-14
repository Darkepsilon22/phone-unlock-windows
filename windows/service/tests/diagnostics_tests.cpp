#include "support/diagnostics.h"
#include "test_runner.h"

namespace
{
constexpr LONG WrongPassword = static_cast<LONG>(0xC000006D);

struct Fixture
{
    uint64_t now = 1'760'000'000;
    Diagnostics diagnostics{ [this] { return now; } };
};

TEST(DiagnosticsStartEmpty)
{
    Fixture fixture;
    CHECK(!fixture.diagnostics.LastUnlock());
    CHECK(!fixture.diagnostics.LastError());
}

TEST(SuccessfulUnlockIsNotAnError)
{
    Fixture fixture;
    fixture.diagnostics.RecordUnlock("Pixel 5", 0);
    CHECK(fixture.diagnostics.LastUnlock()->unixSeconds == 1'760'000'000);
    CHECK(fixture.diagnostics.LastUnlock()->phoneName == "Pixel 5");
    CHECK(fixture.diagnostics.LastUnlock()->status == 0);
    CHECK(!fixture.diagnostics.LastError());
}

TEST(WindowsRejectionIsAlsoTheLastError)
{
    Fixture fixture;
    fixture.diagnostics.RecordUnlock("Pixel 5", WrongPassword);
    CHECK(fixture.diagnostics.LastUnlock()->status == WrongPassword);
    CHECK(fixture.diagnostics.LastError()->code == "WINDOWS_REJECTED");
    CHECK(fixture.diagnostics.LastError()->phoneName == "Pixel 5");
}

TEST(LatestErrorReplacesThePreviousOne)
{
    Fixture fixture;
    fixture.diagnostics.RecordError("Pixel 5", "EXPIRED");
    fixture.now += 60;
    fixture.diagnostics.RecordError({}, "REVOKED");
    CHECK(fixture.diagnostics.LastError()->code == "REVOKED");
    CHECK(fixture.diagnostics.LastError()->phoneName.empty());
    CHECK(fixture.diagnostics.LastError()->unixSeconds == 1'760'000'060);
}

TEST(SuccessDoesNotEraseAnEarlierError)
{
    Fixture fixture;
    fixture.diagnostics.RecordError("Pixel 5", "INVALID_PROOF");
    fixture.diagnostics.RecordUnlock("Pixel 5", 0);
    CHECK(fixture.diagnostics.LastError()->code == "INVALID_PROOF");
}
}
