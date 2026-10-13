#pragma once

#include <cstdio>
#include <functional>
#include <vector>

namespace test_runner
{
struct TestCase
{
    const char* name;
    std::function<void()> body;
};

std::vector<TestCase>& Registry();
void Fail(const char* expression, const char* file, int line);

struct Registration
{
    Registration(const char* name, std::function<void()> body)
    {
        Registry().push_back({ name, std::move(body) });
    }
};
}

#define TEST(name)                                                                  \
    void name();                                                                    \
    const test_runner::Registration name##Registration(#name, name);                \
    void name()

#define CHECK(expression)                                                           \
    do                                                                              \
    {                                                                               \
        if (!(expression))                                                          \
        {                                                                           \
            test_runner::Fail(#expression, __FILE__, __LINE__);                     \
        }                                                                           \
    } while (false)
