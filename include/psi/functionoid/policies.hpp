////////////////////////////////////////////////////////////////////////////////
///
/// Psi.Functionoid library
/// 
/// \file policies.hpp
/// ------------------
///
///  Copyright (c) Domagoj Saric 2010 - 2017
///
///  Use, modification and distribution is subject to the Boost Software
///  License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt)
///
/// For more information, see http://www.boost.org
///
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <boost/assert.hpp>
#include <boost/config_ex.hpp>
#include <boost/throw_exception.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
//------------------------------------------------------------------------------
namespace psi::functionoid
{
//------------------------------------------------------------------------------

/// The bad_function_call exception class is thrown when an empty
/// Psi.Functionoid callable is invoked that uses the throw_on_empty empty
/// handler.
class bad_function_call : public std::runtime_error
{
public:
    bad_function_call() : std::runtime_error( "call to empty Psi.Functionoid callable" ) {}
};

class throw_on_empty
{
private:
    static void BOOST_NORETURN throw_bad_call()
    {
        boost::throw_exception( bad_function_call() );
    }

public:
    template <class result_type>
    static result_type handle_empty_invoke() { throw_bad_call(); }
}; // class throw_on_empty

////////////////////////////////////////////////////////////////////////////////
#ifdef BOOST_MSVC
/// \note In optimized builds with enabled asserts MSVC detects that the assert
/// handler never returns and so it issues this warning about the "return{};"
/// statement.
///                                           (02.01.2018.) (Domagoj Saric)
#pragma warning( push )
#pragma warning( disable : 4702 ) // unreachable code
#endif // BOOST_MSVC
struct assert_on_empty { template <class result_type> static result_type handle_empty_invoke() noexcept { handle_empty_invoke<void>(); return{}; } };
template <> inline void assert_on_empty::handle_empty_invoke<void>() noexcept { BOOST_ASSERT_MSG( false, "Call to empty functionoid!" ); }
#ifdef BOOST_MSVC
#pragma warning( pop )
#endif // BOOST_MSVC

////////////////////////////////////////////////////////////////////////////////
struct nop_on_empty { template <class result_type> static result_type handle_empty_invoke() noexcept { return {}; } };
template <> inline void nop_on_empty::handle_empty_invoke<void>() noexcept {}

///   How an assignment publishes the new target: unset - the default -
/// defers to Traits::concurrent_reads; set, it is the per-call opt-in and the
/// publishing store is made atomically with the given ordering even for Traits
/// that did not opt in.
///   Use it when a callable type is overwhelmingly single-threaded but ONE
/// site needs the publish-once pattern: the trait is per-type and would tax
/// every other use (and, because it makes the invoke path atomic too, it costs
/// far more than the store - see concurrent_reads). This is the same trade
/// std::atomic_ref itself offers, and it carries the same caller obligation:
/// while an ordered access is in flight, every conflicting access to that
/// object must also be ordered. Prefer the trait when in doubt - it makes that
/// hold by construction rather than by discipline.
using publish_order = std::optional<std::memory_order>;

enum struct support_level : std::uint8_t
{
    na        = false,
    supported = true,
    nofail,
    trivial
};

template <support_level Level>
using support_level_t = std::integral_constant<support_level, Level>;

struct std_traits
{
    static constexpr auto copyable             = support_level::supported;
    static constexpr auto moveable             = support_level::supported;
    static constexpr auto destructor           = support_level::nofail;
    static constexpr auto is_noexcept          = false;
    static constexpr auto rtti                 = true;
    static constexpr auto dll_safe_empty_check = true;

    /// Opt-in for callables whose engagement is probed - empty() /
    /// operator bool() - from a thread that may concurrently race the one
    /// assigning to them (the build-once / publish-once pattern: one thread
    /// assigns the target, others poll until they observe it). It makes every
    /// access to the internal vtable pointer atomic (std::atomic_ref, so the
    /// member stays a plain pointer and the type stays trivially copyable when
    /// its Traits say so) and adds the empty( std::memory_order ) overload.
    ///   Off by default, and the cost is larger than "one extra store": an
    /// atomic access is not elided even when the compiler can prove the object
    /// never escapes the function. Neither clang 22 nor gcc 16 promotes such an
    /// object to registers - the atomics pin it to the stack and block the
    /// devirtualization that normally follows. Measured, x86-64 -O3 -DNDEBUG,
    /// both compilers in agreement: a purely local callable constructed from a
    /// lambda and invoked once folds to 2 instructions without the opt-in and
    /// to ~40 with it. (The as-if rule would permit the elision - an object no
    /// other thread can reference cannot observe the difference - but neither
    /// compiler implements it.)
    ///   For what this is actually for, a shared long-lived callable whose
    /// address escapes anyway, there is no such scalar replacement to lose and
    /// the cost is only the atomic accesses themselves: ~+15% instructions on
    /// an assign-and-publish. Ordinary single-threaded use should still never
    /// pay it, hence the opt-in.
    ///   Note that this orders the vtable pointer only. It publishes the
    /// target and makes 'is it engaged' race-free; it does not make assignment
    /// itself, or a concurrent invocation of a target being reassigned, safe.
    ///   What makes the ordered read useful is a property of the *writer*, not
    /// of the type: every assignment path here writes the target buffer first
    /// and the vtable pointer last, so that one store is a genuine publication
    /// point and an acquire load of it orders everything before it. A reader
    /// that observes an engaged callable and then invokes it is safe only for
    /// as long as no one reassigns or clears it - clear() and a subsequent
    /// assign() destroy the target the reader is about to call. This is a
    /// publish-once mechanism; it does not make the callable's whole lifetime
    /// concurrent.
    static constexpr bool concurrent_reads     = false;

    static constexpr std::uint8_t sbo_size      = 4 * sizeof( void * );
    static constexpr std::uint8_t sbo_alignment = alignof( std::max_align_t );

    using empty_handler = throw_on_empty;

    template <typename T>
    using allocator = std::allocator<T>;
}; // struct std_traits

struct default_traits : std_traits
{
    static constexpr auto moveable             = support_level::nofail;
    static constexpr auto rtti                 = false;
    static constexpr auto dll_safe_empty_check = false;

    using empty_handler = assert_on_empty;

    /// Optional hook for decorating vtable invoke/manager function pointers with
    /// compiler attributes (`gnu::pure`, `clang::preserve_most`, …). Set
    /// `PSI_FUNCTIONOID_DETAIL_INVOKE_FN_ATTR` / `MANAGER_FN_ATTR` before
    /// including functionoid headers, or specialize the constexpr flags below
    /// for documentation / static introspection (`detail::vtable_attr_meta`).
    struct vtable_fn_attrs
    {
        static constexpr bool invoke_pure    = false;
        static constexpr bool manager_nofail = false;
    };
}; // struct default_traits

//------------------------------------------------------------------------------
} // namespace psi::functionoid
//------------------------------------------------------------------------------
