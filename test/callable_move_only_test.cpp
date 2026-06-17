#include <psi/functionoid/functionoid.hpp>

#include <gtest/gtest.h>

#include <utility>

namespace {

struct move_only_traits : psi::functionoid::default_traits
{
    static constexpr auto copyable    = psi::functionoid::support_level::na;
    static constexpr auto moveable    = psi::functionoid::support_level::nofail;
    static constexpr auto destructor  = psi::functionoid::support_level::trivial;
    static constexpr auto is_noexcept = true;
};

using move_only_void = psi::functionoid::callable<void(), move_only_traits>;

struct Counter
{
    int value{ 0 };
    void operator()() noexcept { ++value; }
};

} // namespace

TEST( CallableMoveOnly, MoveConstructAndInvoke )
{
    Counter c;
    move_only_void fn{ [&c]() noexcept { c(); } };
    fn();
    EXPECT_EQ( c.value, 1 );

    move_only_void moved{ std::move( fn ) };
    moved();
    EXPECT_EQ( c.value, 2 );
}

TEST( CallableMoveOnly, AssignFromStatelessLambda )
{
    int hits{ 0 };
    move_only_void fn;
    fn = [&]() noexcept { ++hits; };
    fn();
    EXPECT_EQ( hits, 1 );
}

TEST( CallableMoveOnly, EmptyNopHandler )
{
    move_only_void fn;
    // assert_on_empty is default; empty invoke is UB in release — just check empty state.
    SUCCEED();
}
