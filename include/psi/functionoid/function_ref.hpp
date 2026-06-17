#pragma once
////////////////////////////////////////////////////////////////////////////////
/// \file function_ref.hpp
/// Non-owning type-erased callable — Psi.Functionoid's `std::function_ref` slot.
///
/// \b Optimization: trivial callables that fit in a single pointer are stored
/// inline in the ref's data word (no indirection). Larger or non-trivial targets
/// are invoked through a pointer to the caller's object; lifetime stays external.
///
/// \b Exception tunneling: C APIs, V8, and other embedder callbacks require
/// `noexcept` function pointers and cannot propagate C++ exceptions. When the
/// source callable may throw, `make_c_callback` wraps it in
/// `exception_tunneling_callable`: the exported thunk is `noexcept`, catches
/// into `std::exception_ptr`, returns a default `R` (or `void`), and the caller
/// calls `check_failure()` after the foreign API returns to rethrow. See
/// `make_exception_tunneling_callable` below.
////////////////////////////////////////////////////////////////////////////////
#include <boost/assert.hpp>

#include <exception>
#include <type_traits>
#include <utility>
//------------------------------------------------------------------------------
namespace psi::functionoid {
//------------------------------------------------------------------------------

template <typename Sig>
class function_ref;

template <bool ne, typename R, typename... Args>
class [[ clang::trivial_abi ]] function_ref<R( Args... ) noexcept( ne )>
{
public:
    constexpr function_ref() = default;

    template <typename F>
    function_ref( F && callable [[ clang::lifetimebound ]] ) noexcept
    requires ( noexcept( callable( std::declval<Args>()... ) ) >= ne )
    {
        auto const cb{ make_c_callback( std::forward<F>( callable ) ) };
        data_     = cb.first;
        function_ = static_cast<decltype( function_ )>( cb.second );
    }

    template <typename... CallArgs>
    [[ gnu::always_inline ]]
    decltype( auto ) operator()( CallArgs &&... args ) const noexcept( ne )
    {
        BOOST_ASSERT_MSG( function_, "function_ref called with null function" );
        return function_( data_, std::forward<CallArgs>( args )... );
    }

    explicit operator bool() const noexcept { return function_ != nullptr; }

    /// Wraps a potentially-throwing callable for export through a `noexcept`
    /// C-style or embedder callback. Exceptions are captured in \c exception;
    /// call \c check_failure() after the foreign API returns to rethrow.
    template <typename Target>
    struct exception_tunneling_callable
    {
        mutable Target             target;
        mutable std::exception_ptr exception{};

        template <typename... CallArgs>
        [[ gnu::always_inline ]]
        decltype( auto ) operator()( CallArgs &&... args ) const noexcept
        {
            try { return target( std::forward<CallArgs>( args )... ); }
            catch ( ... ) {
                exception = std::current_exception();
                if constexpr ( !std::is_same_v<R, void> ) {
                    return R{};
                }
            }
        }

        void check_failure() const
        {
            if ( exception ) [[ unlikely ]] {
                std::rethrow_exception( exception );
            }
        }
    }; // struct exception_tunneling_callable

    template <typename F>
    static auto make_exception_tunneling_callable( F && f [[ clang::lifetimebound ]] ) noexcept
    {
        using Callable = std::remove_reference_t<F>;

        return exception_tunneling_callable<
            std::conditional_t<
                noexcept( auto{ std::forward<F>( f ) } ) && ( sizeof( f ) < 4 * sizeof( void * ) ),
                Callable, F
            >
        >{ std::forward<F>( f ) };
    }

    /// Builds the `(data, noexcept thunk)` pair stored in this ref. Trivial
    /// callables that fit in \c data_ are placement-new'd inline; otherwise the
    /// thunk dereferences a pointer to the caller's object. Throwing callables
    /// when \c ne is true are routed through \c exception_tunneling_callable.
    template <typename F>
    static auto make_c_callback( F && callable [[ clang::lifetimebound ]] ) noexcept
    {
        using Callable = std::remove_reference_t<F>;
        if constexpr ( noexcept( callable( std::declval<Args>()... ) ) >= ne ) {
            if constexpr ( std::is_trivially_copy_constructible_v<Callable> && ( sizeof( Callable ) <= sizeof( data_ ) ) ) {
                void * data;
                new ( &data ) Callable{ callable };
                return std::pair{
                    data,
                    []( void * f, Args... args ) noexcept( ne ) -> R {
                        return reinterpret_cast<Callable &>( f )( std::forward<Args>( args )... );
                    }
                };
            } else {
                return std::pair{
                    const_cast<void *>( static_cast<void const *>( std::addressof( callable ) ) ),
                    []( void * const pF, Args... args ) noexcept( ne ) -> R {
                        return ( *static_cast<Callable *>( pF ) )( std::forward<Args>( args )... );
                    }
                };
            }
        } else {
            return make_c_callback(
                make_exception_tunneling_callable( std::forward<F>( callable ) )
            );
        }
    }

private:
    R ( *function_ )( void *, Args... ) noexcept( ne ){};
    void * data_{};
}; // class function_ref

//------------------------------------------------------------------------------
} // namespace psi::functionoid
//------------------------------------------------------------------------------
