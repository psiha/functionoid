// Traits::concurrent_reads: the vtable pointer doubles as the engagement flag,
// so empty()/operator bool() can be answered lock-free - and race-free against
// the thread doing the assigning - without the type growing an atomic member
// (which would also cost it the trivially-copyable guarantee).
// The publish-once/poll-until-engaged shape below is the one this exists for;
// run it under ThreadSanitizer for the part a single-threaded assertion cannot
// check.
#include <psi/functionoid/functionoid.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <thread>
#include <type_traits>

namespace {

namespace pf = psi::functionoid;

struct concurrent_traits : pf::default_traits
{
    static constexpr bool concurrent_reads = true;
};

// Fully trivial: the trivially-copyable guarantee must survive the opt-in.
struct trivial_concurrent_traits : concurrent_traits
{
    static constexpr auto copyable    = pf::support_level::trivial;
    static constexpr auto moveable    = pf::support_level::trivial;
    static constexpr auto destructor  = pf::support_level::trivial;
    static constexpr auto is_noexcept = true;
};

// Vtable-compatible with concurrent_traits but not the same Traits: exercises
// the cross-Traits assignment path (assign_functionoid_direct's reinterpreted
// vtable), which is what psi::sweater's generic backend does.
struct other_concurrent_traits : concurrent_traits
{
    static constexpr auto copyable = pf::support_level::nofail;
};

using plain_fn      = pf::callable<int(), pf::default_traits          >;
using concurrent_fn = pf::callable<int(), concurrent_traits           >;
using trivial_fn    = pf::callable<int(), trivial_concurrent_traits   >;

// The ordered accessors are available to every Traits - asking for an ordering
// IS the per-call opt-in. What concurrent_reads adds is that the type's own
// internal accesses become ordered too, so the guarantee holds by construction
// rather than by caller discipline.
template <typename Callable>
concept has_ordered_empty  = requires ( Callable const & c ) { c.empty( std::memory_order_acquire ); };
template <typename Callable>
concept has_ordered_assign = requires ( Callable & c ) { c.assign( +[]{ return 0; }, std::memory_order_release ); };

static_assert( has_ordered_empty <plain_fn     > );
static_assert( has_ordered_empty <concurrent_fn> );
static_assert( has_ordered_assign<plain_fn     > );
static_assert( has_ordered_assign<concurrent_fn> );

// ...but there is deliberately NO ordered clear: a release store orders the
// writes that precede it, and disengagement has none. See clear()'s comment.
template <typename Callable>
concept has_ordered_clear = requires ( Callable & c ) { c.clear( std::memory_order_release ); };

static_assert( !has_ordered_clear<plain_fn     > );
static_assert( !has_ordered_clear<concurrent_fn> );

// The whole point of publishing through the vtable pointer rather than an added
// atomic member: no size cost, and no loss of triviality.
static_assert( sizeof( concurrent_fn ) == sizeof( plain_fn ) );
static_assert( std::is_trivially_copyable_v<trivial_fn> );

} // namespace

TEST( ConcurrentReads, OrderedEmptyAgreesWithPlainEmpty )
{
    concurrent_fn f;
    EXPECT_TRUE( f.empty(                            ) );
    EXPECT_TRUE( f.empty( std::memory_order_acquire  ) );
    EXPECT_TRUE( f.empty( std::memory_order_relaxed  ) );

    f = [] { return 42; };
    EXPECT_FALSE( f.empty(                           ) );
    EXPECT_FALSE( f.empty( std::memory_order_acquire ) );
    EXPECT_EQ   ( f(), 42 );

    f.clear();
    EXPECT_TRUE( f.empty( std::memory_order_acquire ) );
}

// The cross-Traits assignment path publishes through the same accessor.
TEST( ConcurrentReads, CrossTraitsAssignmentPublishes )
{
    using other_fn = pf::callable<int(), other_concurrent_traits>;

    other_fn      source{ [] { return 7; } };
    concurrent_fn destination;
    ASSERT_TRUE( destination.empty( std::memory_order_acquire ) );

    destination = source;
    EXPECT_FALSE( destination.empty( std::memory_order_acquire ) );
    EXPECT_EQ   ( destination(), 7 );
}

TEST( ConcurrentReads, OrderedEmptyTracksEveryEngagingOperation )
{
    concurrent_fn f;

    f.assign( [] { return 1; } );
    EXPECT_FALSE( f.empty( std::memory_order_acquire ) );

    concurrent_fn const copy{ f };
    EXPECT_FALSE( copy.empty( std::memory_order_acquire ) );

    concurrent_fn moved{ std::move( f ) };
    EXPECT_FALSE( moved.empty( std::memory_order_acquire ) );

    concurrent_fn empty_target;
    moved.swap( empty_target );
    EXPECT_TRUE ( moved       .empty( std::memory_order_acquire ) );
    EXPECT_FALSE( empty_target.empty( std::memory_order_acquire ) );

    empty_target.assign( nullptr );
    EXPECT_TRUE( empty_target.empty( std::memory_order_acquire ) );
}

// Publish-once: one thread assigns, others poll until they observe engagement
// and then invoke. The acquire load is what makes reading the target - which
// the assigning thread wrote before publishing the vtable pointer - not a race.
TEST( ConcurrentReads, PublishOnceIsObservedWithItsTarget )
{
    static constexpr auto readers{ 4 };

    for ( auto round{ 0 }; round < 64; ++round )
    {
        // Heap-allocated so that the target's own storage is fresh each round
        // (a stack slot would be reused and could hide a missing edge).
        auto const p_function{ std::make_unique<concurrent_fn>() };
        auto     & function  { *p_function };

        std::atomic<bool> go{ false };
        std::atomic<int > observed{ 0 };

        std::vector<std::jthread> pollers;
        for ( auto reader{ 0 }; reader < readers; ++reader )
        {
            pollers.emplace_back( [&]
            {
                go.wait( false, std::memory_order_acquire );
                while ( function.empty( std::memory_order_acquire ) ) { std::this_thread::yield(); }
                observed.fetch_add( function(), std::memory_order_relaxed );
            } );
        }

        std::jthread writer{ [&]
        {
            go.store( true, std::memory_order_release );
            go.notify_all();
            function = [ round ] { return round; };
        } };

        pollers.clear(); // join
        writer .join ();

        EXPECT_EQ( observed.load( std::memory_order_relaxed ), round * readers );
    }
}

// ---------------------------------------------------------------------------
// Per-call opt-in: the same publish-once discipline on Traits that did NOT set
// concurrent_reads. The point is that a type used single-threaded everywhere
// else pays nothing - only this site is ordered.
// ---------------------------------------------------------------------------

TEST( PerCallPublishOrder, OrderedEmptyAgreesWithPlainEmptyOnPlainTraits )
{
    plain_fn f;
    EXPECT_TRUE( f.empty(                           ) );
    EXPECT_TRUE( f.empty( std::memory_order_acquire ) );

    f.assign( +[]{ return 5; }, std::memory_order_release );
    EXPECT_FALSE( f.empty(                           ) );
    EXPECT_FALSE( f.empty( std::memory_order_acquire ) );
    EXPECT_EQ   ( f(), 5 );

    f.clear();
    EXPECT_TRUE( f.empty( std::memory_order_acquire ) );
}

// The opt-in must not cost the type anything it did not already pay.
static_assert( sizeof( plain_fn ) == sizeof( concurrent_fn ) );

// Publish-once on plain Traits: identical to the concurrent_traits case above,
// but the ordering comes from the two annotated calls rather than the type.
TEST( PerCallPublishOrder, PublishOnceIsObservedWithItsTargetOnPlainTraits )
{
    static constexpr auto readers{ 4 };

    for ( auto round{ 0 }; round < 64; ++round )
    {
        auto const p_function{ std::make_unique<plain_fn>() };
        auto     & function  { *p_function };

        std::atomic<bool> go{ false };
        std::atomic<int > observed{ 0 };

        std::vector<std::jthread> pollers;
        for ( auto reader{ 0 }; reader < readers; ++reader )
        {
            pollers.emplace_back( [&]
            {
                go.wait( false, std::memory_order_acquire );
                while ( function.empty( std::memory_order_acquire ) ) { std::this_thread::yield(); }
                observed.fetch_add( function(), std::memory_order_relaxed );
            } );
        }

        std::jthread writer{ [&]
        {
            go.store( true, std::memory_order_release );
            go.notify_all();
            function.assign( [ round ] { return round; }, std::memory_order_release );
        } };

        pollers.clear(); // join
        writer .join ();

        EXPECT_EQ( observed.load( std::memory_order_relaxed ), round * readers );
    }
}
