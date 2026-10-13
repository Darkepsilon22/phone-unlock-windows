#include "ipc/authorization_broker.h"
#include "test_runner.h"
#include "test_support.h"

namespace
{
const std::wstring Alice = L"S-1-5-21-1-2-3-1001";
const std::wstring Bob = L"S-1-5-21-1-2-3-1002";
constexpr ULONGLONG AuthorizationLifetime = 30'000;

bool StopWaiting()
{
    return false;
}

TEST(AuthorizationIsConsumedOnlyOnce)
{
    test_support::FakeClock clock;
    AuthorizationBroker broker(clock.Function());
    broker.Authorize(Alice);
    CHECK(broker.WaitForAuthorization({ Alice }, StopWaiting) == Alice);
    CHECK(!broker.WaitForAuthorization({ Alice }, StopWaiting).has_value());
}

TEST(AuthorizationIsReservedToItsAccount)
{
    test_support::FakeClock clock;
    AuthorizationBroker broker(clock.Function());
    broker.Authorize(Alice);
    CHECK(!broker.WaitForAuthorization({ Bob }, StopWaiting).has_value());
    CHECK(broker.WaitForAuthorization({ Bob, Alice }, StopWaiting) == Alice);
}

TEST(AuthorizationExpiresAfterThirtySeconds)
{
    test_support::FakeClock clock;
    AuthorizationBroker broker(clock.Function());
    broker.Authorize(Alice);
    clock.Advance(AuthorizationLifetime);
    CHECK(!broker.WaitForAuthorization({ Alice }, StopWaiting).has_value());
}

TEST(AuthorizationIsStillValidJustBeforeExpiry)
{
    test_support::FakeClock clock;
    AuthorizationBroker broker(clock.Function());
    broker.Authorize(Alice);
    clock.Advance(AuthorizationLifetime - 1);
    CHECK(broker.WaitForAuthorization({ Alice }, StopWaiting) == Alice);
}
}
