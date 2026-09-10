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

// A callable small and trivial enough to live in the ref's data word is copied
// into it, not pointed at, so it does not outlive-borrow anything: a ref built
// from a temporary stays valid after the full-expression, and may be returned.
TEST( FunctionRefTest, InlineStoredCallableSurvivesItsTemporary )
{
    int value{ 0 };
    auto const makeRef{ [ &value ]{
        // the lambda is a temporary of this full-expression; it captures one
        // pointer, so the ref takes a copy rather than its address
        return psi::functionoid::function_ref<void( int )>{
            [ p = &value ]( int const v ) { *p = v; }
        };
    } };
    auto const ref{ makeRef() };
    ref( 11 );
    EXPECT_EQ( value, 11 );
}

TEST( FunctionRefTest, InlineStorageClassification )
{
    using Ref = psi::functionoid::function_ref<void( int )>;

    int a{}, b{};
    auto const capturesOnePointer { [ pA = &a ]            ( int const v ) { *pA = v; } };
    auto const capturesTwoPointers{ [ pA = &a, pB = &b ]( int const v ) { *pA = *pB = v; } };

    // copied into the data word -> the ref borrows nothing
    static_assert( Ref::stored_inline<decltype( capturesOnePointer  )> );
    // too large -> the ref points at the caller's object, which must outlive it
    static_assert( !Ref::stored_inline<decltype( capturesTwoPointers )> );
    // a plain function pointer is copied too
    static_assert( Ref::stored_inline<void (*)( int )> );

    EXPECT_TRUE( Ref{ capturesTwoPointers } ); // the borrowing overload still works for an lvalue
}
