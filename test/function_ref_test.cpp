#include <psi/functionoid/function_ref.hpp>

#include <gtest/gtest.h>

namespace {

int g_value{ 0 };

} // namespace

TEST( FunctionRefTest, InvokesBoundLambda )
{
    psi::functionoid::function_ref<void()> ref{ [] { g_value = 42; } };
    ASSERT_TRUE( ref );
    ref();
    EXPECT_EQ( g_value, 42 );
}

TEST( FunctionRefTest, NoexceptIntReturn )
{
    psi::functionoid::function_ref<int() noexcept> ref{ []() noexcept -> int { return 7; } };
    EXPECT_EQ( ref(), 7 );
}

TEST( FunctionRefTest, EmptyIsFalse )
{
    psi::functionoid::function_ref<void()> ref;
    EXPECT_FALSE( ref );
}

TEST( FunctionRefTest, BoostAliasMatchesPsi )
{
    psi::functionoid::function_ref<void()> ref{ [] { ++g_value; } };
    ref();
    EXPECT_EQ( g_value, 43 );
}
