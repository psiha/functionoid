////////////////////////////////////////////////////////////////////////////////
///
/// Psi.Functionoid library
///
/// \file vtable_attrs.hpp
/// ----------------------
///
/// Optional compiler attributes for vtable function pointers (invoke + manager
/// slots). Override the macros below before including functionoid.hpp, or
/// from a traits-specific header pulled in by the embedding project.
///
/// Example (hot read-only invoke path):
///
///   #define PSI_FUNCTIONOID_DETAIL_INVOKE_FN_ATTR [[gnu::pure]]
///   #include <psi/functionoid/functionoid.hpp>
///
///   struct hot_traits : psi::functionoid::default_traits {
///       struct vtable_fn_attrs {
///           static constexpr bool invoke_pure = true;
///       };
///   };
///
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifndef PSI_FUNCTIONOID_DETAIL_INVOKE_FN_ATTR
#define PSI_FUNCTIONOID_DETAIL_INVOKE_FN_ATTR
#endif

#ifndef PSI_FUNCTIONOID_DETAIL_MANAGER_FN_ATTR
#define PSI_FUNCTIONOID_DETAIL_MANAGER_FN_ATTR
#endif

//------------------------------------------------------------------------------
namespace psi::functionoid
{
//------------------------------------------------------------------------------
namespace detail
{
//------------------------------------------------------------------------------

/// Traits hook: `Traits::vtable_fn_attrs` may set `invoke_pure` /
/// `manager_nofail` for documentation and static introspection. Attribute
/// application itself is via the macros above (C++ cannot attach arbitrary
/// attribute tokens from a traits template today).
template <typename Traits>
struct vtable_attr_meta
{
    using attrs = typename Traits::vtable_fn_attrs;

    static constexpr bool invoke_pure   = attrs::invoke_pure;
    static constexpr bool manager_nofail = attrs::manager_nofail;
};

//------------------------------------------------------------------------------
} // namespace detail
//------------------------------------------------------------------------------
} // namespace psi::functionoid
//------------------------------------------------------------------------------
