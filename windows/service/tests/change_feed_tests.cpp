#include "support/change_feed.h"
#include "test_runner.h"
#include <thread>

namespace
{
using namespace std::chrono_literals;

TEST(ChangeFeedAnswersAtOnceWhenVersionIsOld)
{
    ChangeFeed feed;
    const uint64_t first = feed.Version();
    feed.Notify();
    CHECK(feed.WaitForChange(first, 10s) == first + 1);
}

TEST(ChangeFeedTimesOutWithoutChange)
{
    ChangeFeed feed;
    const uint64_t current = feed.Version();
    CHECK(feed.WaitForChange(current, 10ms) == current);
}

TEST(ChangeFeedWakesWaitersOnNotify)
{
    ChangeFeed feed;
    const uint64_t current = feed.Version();
    std::thread notifier([&feed] {
        std::this_thread::sleep_for(20ms);
        feed.Notify();
    });
    CHECK(feed.WaitForChange(current, 10s) == current + 1);
    notifier.join();
}
}
