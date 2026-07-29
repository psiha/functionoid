// Cross-Traits nested-callable assignment: a callable used as the Functor of
// another callable with different (but vtable-compatible) Traits must alias
// the source's target (reuse its vtable, assign_functionoid_direct) rather
// than wrap the source as an ordinary function object - and must compile even
// when the destination's Traits (e.g. destructor = trivial) would reject the
// source callable type going through the functor_manager machinery.
// Mirrors psi::sweater's generic backend usage (its clang-22/gcc-15 breakage).
#include <psi/functionoid/functionoid.hpp>

#include <gtest/gtest.h>

#include <utility>

namespace {

namespace pf = psi::functionoid;

struct worker_traits : pf::default_traits
{
    static constexpr auto copyable    = pf::support_level::na     ;
    static constexpr auto moveable    = pf::support_level::nofail ;
    static constexpr auto destructor  = pf::support_level::trivial;
    static constexpr auto is_noexcept = true;
    static constexpr auto rtti        = false;
};

struct template_traits : worker_traits
{
    static constexpr auto copyable = pf::support_level::trivial;
    static constexpr auto moveable = pf::support_level::nofail ;
};

using work_t          = pf::callable<void(), worker_traits  >;
using work_template_t = pf::callable<void(), template_traits>;

} // namespace

TEST( NestedCallable, CrossTraitsCopyConstruction )
{
    int count{ 0 };
    work_template_t const work_template{ [&]() noexcept { ++count; } };

    work_t chunk1{ work_template };
    work_t chunk2{ work_template };
    chunk1();
    chunk2();
    EXPECT_EQ( count, 2 );

    // source must remain intact & invocable after being copied from
    work_template();
    EXPECT_EQ( count, 3 );
}

TEST( NestedCallable, CrossTraitsAssignment )
{
    int count{ 0 };
    work_template_t const work_template{ [&]() noexcept { ++count; } };

    work_t chunk{ []() noexcept {} };
    chunk = work_template;
    chunk();
    EXPECT_EQ( count, 1 );
}

TEST( NestedCallable, SameTraitsCopyAndAssignmentStillCorrect )
{
    // The same-Traits case flows through the same is_a_callable assign branch
    // (whose BOOST_ASSUME on the passed vtable made a wrong fix livelock
    // silently) - pin its observable behaviour here.
    int count{ 0 };
    work_template_t a{ [&]() noexcept { ++count; } };
    work_template_t b{ a };
    b = a;
    a();
    b();
    EXPECT_EQ( count, 2 );

    work_t c{ [&]() noexcept { count += 10; } };
    work_t d{ std::move( c ) };
    work_t e{ []() noexcept {} };
    e = std::move( d );
    e();
    EXPECT_EQ( count, 12 );
}

TEST( NestedCallable, CrossTraitsMove )
{
    int count{ 0 };
    work_template_t work_template{ [&]() noexcept { ++count; } };

    work_t chunk{ std::move( work_template ) };
    chunk();
    EXPECT_EQ( count, 1 );
}

namespace {

struct fully_trivial_traits : pf::default_traits
{
    static constexpr auto copyable    = pf::support_level::trivial;
    static constexpr auto moveable    = pf::support_level::trivial;
    static constexpr auto destructor  = pf::support_level::trivial;
    static constexpr auto is_noexcept = true;
    static constexpr auto rtti        = false;
};

using trivial_work_t = pf::callable<void(), fully_trivial_traits>;

// Traits-declared triviality is real, not just a no-op vtable dispatch.
static_assert(  std::is_trivially_destructible_v     <trivial_work_t> );
static_assert(  std::is_trivially_copy_constructible_v<trivial_work_t> );
static_assert(  std::is_trivially_move_constructible_v<trivial_work_t> );
static_assert(  std::is_trivially_copy_assignable_v  <trivial_work_t> );
static_assert(  std::is_trivially_move_assignable_v  <trivial_work_t> );
static_assert(  std::is_trivially_copyable_v         <trivial_work_t> );

// destructor = trivial alone must make the type trivially destructible.
static_assert(  std::is_trivially_destructible_v<work_t> );
// copyable = na is now visible to the type system instead of a static_assert.
static_assert( !std::is_copy_constructible_v<work_t> );
static_assert( !std::is_copy_assignable_v   <work_t> );

} // namespace

TEST( TrivialSpecialMembers, TriviallyCopiedCallableInvokes )
{
    int count{ 0 };
    int * const p_count{ &count };
    trivial_work_t const fn{ [=]() noexcept { ++*p_count; } };

    trivial_work_t copy{ fn }; // trivial copy - aliases fn's target
    copy();
    fn  ();
    EXPECT_EQ( count, 2 );

    trivial_work_t assigned{ []() noexcept {} };
    assigned = fn;
    assigned();
    EXPECT_EQ( count, 3 );

    trivial_work_t moved{ std::move( copy ) };
    moved();
    EXPECT_EQ( count, 4 );
    copy(); // a trivial move is a copy - the source stays intact
    EXPECT_EQ( count, 5 );
}
