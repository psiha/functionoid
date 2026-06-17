// Compile-time test: vtable attribute macros + traits metadata hook.
#if defined( __GNUC__ ) && !defined( __clang__ )
#    define PSI_FUNCTIONOID_DETAIL_INVOKE_FN_ATTR __attribute__( ( pure ) )
#elif defined( __clang__ )
#    define PSI_FUNCTIONOID_DETAIL_INVOKE_FN_ATTR [[gnu::pure]]
#endif

#include <psi/functionoid/detail/vtable_attrs.hpp>
#include <psi/functionoid/functionoid.hpp>

#include <gtest/gtest.h>

namespace {

struct attributed_traits : psi::functionoid::default_traits
{
    struct vtable_fn_attrs
    {
        static constexpr bool invoke_pure    = true;
        static constexpr bool manager_nofail = false;
    };
};

} // namespace

TEST( VtableAttrs, MetaFlags )
{
    static_assert( psi::functionoid::detail::vtable_attr_meta<attributed_traits>::invoke_pure );
    static_assert( !psi::functionoid::detail::vtable_attr_meta<attributed_traits>::manager_nofail );
    static_assert( !psi::functionoid::detail::vtable_attr_meta<psi::functionoid::default_traits>::invoke_pure );
}

TEST( VtableAttrs, CallableStillInvokes )
{
    psi::functionoid::callable<int(), attributed_traits> fn{ []() noexcept -> int { return 7; } };
    EXPECT_EQ( fn(), 7 );
}
