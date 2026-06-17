#include <psi/functionoid/functionoid.hpp>

#include <gtest/gtest.h>

namespace {

int g_sum{ 0 };

} // namespace

TEST( CallableInvoke, StatelessLambda )
{
    psi::functionoid::callable<int()> fn{ []() noexcept -> int { return 42; } };
    EXPECT_EQ( fn(), 42 );
}

TEST( CallableInvoke, CapturingLambda )
{
    psi::functionoid::callable<void()> fn{ [&]() noexcept { g_sum += 10; } };
    fn();
    EXPECT_EQ( g_sum, 10 );
}

TEST( CallableInvoke, Reassign )
{
    psi::functionoid::callable<int()> fn{ []() noexcept -> int { return 1; } };
    fn = []() noexcept -> int { return 2; };
    EXPECT_EQ( fn(), 2 );
}
