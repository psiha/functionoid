#include <psi/functionoid/functionoid.hpp>

#include <gtest/gtest.h>

#include <utility>

namespace {

struct move_only_noexcept_traits : psi::functionoid::default_traits
{
    static constexpr auto copyable    = psi::functionoid::support_level::na;
    static constexpr auto moveable    = psi::functionoid::support_level::nofail;
    static constexpr auto destructor  = psi::functionoid::support_level::trivial;
    static constexpr auto is_noexcept = true;
};

using work_t = psi::functionoid::callable<void(), move_only_noexcept_traits>;

} // namespace

TEST( FunctionoidSmoke, MoveOnlyNoexceptCallable )
{
    int count{ 0 };
    work_t fn{ [&] { ++count; } };
    fn();
    EXPECT_EQ( count, 1 );

    work_t moved{ std::move( fn ) };
    moved();
    EXPECT_EQ( count, 2 );
}

TEST( FunctionoidSmoke, SboStatelessLambda )
{
    work_t fn{ []() noexcept {} };
    EXPECT_FALSE( work_t::requires_allocation<decltype( []() noexcept {} )> );
    fn();
}
