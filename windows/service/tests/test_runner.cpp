#include "test_runner.h"

namespace test_runner
{
namespace
{
int failures = 0;
}

std::vector<TestCase>& Registry()
{
    static std::vector<TestCase> tests;
    return tests;
}

void Fail(const char* expression, const char* file, int line)
{
    ++failures;
    std::printf("  ECHEC %s (%s:%d)\n", expression, file, line);
}
}

int main()
{
    int failedTests = 0;
    for (const test_runner::TestCase& test : test_runner::Registry())
    {
        const int before = test_runner::failures;
        test.body();
        const bool passed = test_runner::failures == before;
        failedTests += passed ? 0 : 1;
        std::printf("%s %s\n", passed ? "OK   " : "ECHEC", test.name);
    }
    std::printf("%zu tests, %d en echec\n", test_runner::Registry().size(), failedTests);
    return failedTests == 0 ? 0 : 1;
}
