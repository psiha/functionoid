////////////////////////////////////////////////////////////////////////////////
///
/// Psi.Functionoid library
///
/// \file functionoid.hpp
/// ---------------------
///
///  Copyright (c) Domagoj Saric 2010 - 2020
///
///  Use, modification and distribution is subject to the Boost Software
///  License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt)
///
/// For more information, see http://www.boost.org
///
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "detail/callable_base.hpp"
#include "functionoid_fwd.hpp"
#include "policies.hpp"

#include <boost/assert.hpp>

#include <cstdint>
#include <type_traits>
//------------------------------------------------------------------------------
namespace psi::functionoid
{
//------------------------------------------------------------------------------

template<typename ReturnType, typename ... Arguments, typename Traits>
class callable<ReturnType(Arguments ...), Traits>
	: public detail::callable_base<Traits>
{
private:
    using function_base = detail::callable_base<Traits>;
	using buffer        = typename function_base::buffer;

public: // Public typedefs/introspection section.
	using result_type = ReturnType;

    using signature_type = result_type ( Arguments... );

	using empty_handler = typename Traits::empty_handler;

	static constexpr std::uint8_t arity = sizeof...( Arguments );

    template <typename FunctionObject>
    static bool constexpr requires_allocation = !detail::functor_traits<FunctionObject, buffer>::allowsSmallObjectOptimization;

private: // Private implementation types.
    // We need a specific thin wrapper around the base empty handler that will
    // just consume all the parameters. This way the base empty handler can have
    // one plain simple operator(). As part of anti-code-bloat measures,
    // my_empty_handler is used only when really necessary (with the invoker),
    // otherwise the base_empty_handler type is used.
    struct my_empty_handler : empty_handler
    {
        result_type operator()( Arguments... ) const noexcept( noexcept( empty_handler:: template handle_empty_invoke<result_type>() ) )
        {
            return empty_handler:: template handle_empty_invoke<result_type>();
        }
    };

    using base_vtable = typename detail::callable_base<Traits>::vtable;
    using vtable_type = detail::vtable<detail::invoker<Traits::is_noexcept, ReturnType, Arguments...>, Traits>;

    /// \note
    ///   Simply default-constructing callable_base and then performing proper
    /// initialization in the body of the derived class constructor has
    /// unfortunate efficiency implications because it creates unnecessary
    /// EH states (=unnecessary bloat) in case of non-trivial
    /// (i.e. fallible/throwable) constructors of derived classes (when
    /// constructing from/with complex function objects).
    ///   In such cases the compiler has to generate EH code to call the
    /// (non-trivial) callable_base destructor if the derived-class constructor
    /// fails after callable_base is already constructed. This is completely
    /// redundant because initially callable_base is/was always initialized with
    /// the empty handler for which no destruction is necessary but the compiler
    /// does not see this due to the indirect vtable call.
    ///   Because of the above issue, the helper ne_eh_state_* functionality is
    /// used as a quick-hack to actually construct the callable_base object
    /// before the callable_base constructor is called.
    /// The entire object is also cleared beforehand in debugging builds to
    /// allow checking that the vtable and/or the function_buffer are not used
    /// before being initialized.
    ///   Using a plain callable<> member function that does the construction
    /// and then simply calling the empty callable_base default constructor
    /// was crashing (i.e. producing bad codegen) in release builds with GCC 6
    /// and Clang 3.9 (when used with libstdc++ as opposed to libc++; why that
    /// makes any difference in this case is one huge ?...) - the fix was to
    /// move the construction code into the callable_base constructor with a
    /// 'constructor' functor (that can call callable internals required for
    /// proper construction).
    ///                                       (24.11.2017.) (Domagoj Saric)
    /// \todo Devise a cleaner way to deal with all of this (maybe move/add more
    /// template methods to function_base so that it can call assign methods
    /// from its template constructors thereby moving all construction code
    /// there).
    ///                                       (02.11.2010.) (Domagoj Saric)
    struct no_eh_state_constructor
    {
        template <typename F, typename Allocator>
        base_vtable const & operator()( function_base & base, F && f, Allocator const a ) const noexcept( std::is_nothrow_constructible_v<std::remove_reference_t<F>, F> )
        {
            detail::debug_clear( base );
            static_cast<callable &>( base ).do_assign<true>( std::forward<F>( f ), a );
            return static_cast<callable &>( base ).vtable();
        }

        template <typename F>
        base_vtable const & operator()( function_base & base, F && f ) const noexcept( std::is_nothrow_constructible_v<std::remove_reference_t<F>, F> )
        {
		    using NakedFunctionObj = std::remove_const_t<std::remove_reference_t<F>>;
		    return (*this)( base, std::forward<F>( f ), typename Traits:: template allocator<NakedFunctionObj>() );
        }
    }; // struct no_eh_state_constructor

    using no_eh_state_construction_trick_tag = typename function_base::no_eh_state_construction_trick_tag;

public: // Public function interface.
    callable() noexcept : function_base( empty_handler_vtable(), empty_handler{} ) {}

    // Constrained (rather than SFINAE'd through a defaulted dummy parameter)
    // so it does not hide the copy/move constructors for a `callable &` - the
    // original reason this needed excluding at all.
    template <typename Functor>
        requires ( !std::is_same_v<std::decay_t<Functor>, callable> )
    callable( Functor && f ) noexcept( std::is_nothrow_constructible_v<std::decay_t<Functor>, Functor> /*...mrmlj...&& !is_heap_allocated*/ )
        : function_base( no_eh_state_construction_trick_tag{}, no_eh_state_constructor{}, std::forward<Functor>( f ) ) {}

    template <typename Functor, typename Allocator>
    callable( Functor && f, Allocator const a ) noexcept( std::is_nothrow_constructible_v<std::decay_t<Functor>, Functor> /*...mrmlj...&& !is_heap_allocated*/ )
        : function_base( no_eh_state_construction_trick_tag{}, no_eh_state_constructor{}, std::forward<Functor>( f ), a ) {}

    callable( signature_type * const plain_function_pointer ) noexcept
        : function_base( no_eh_state_construction_trick_tag{}, no_eh_state_constructor{}, plain_function_pointer ) {}

    // Traits-declared trivial copyability/moveability/destructibility is made
    // real - the corresponding special member is defaulted (and therefore
    // actually trivial, memberwise vtable pointer + buffer copy, exactly what
    // the trivial vtable clone/move entries do) so std::is_trivially_* report
    // the truth. Note: a trivial move is a copy - the source is left intact
    // (as for any trivially copyable type) rather than emptied.
    // A copyable = na instantiation is now properly uncopyable to the type
    // system (deleted) instead of static_assert-ing on use.
    // (The constraints are spelled mutually exclusive rather than relying on
    // the more-constrained-wins tie-breaker: GCC and Clang disagree on it for
    // special members.)
    callable( callable const &   ) requires ( Traits::copyable == support_level::trivial ) = default;
    callable( callable const &   ) requires ( Traits::copyable == support_level::na      ) = delete;
    callable( callable const & f ) noexcept( Traits::copyable >= support_level::nofail )
        requires ( Traits::copyable != support_level::trivial && Traits::copyable != support_level::na )
        : function_base( static_cast<function_base const &>( f ), empty_handler_vtable() ) {}

	callable( callable &&   ) noexcept requires ( Traits::moveable == support_level::trivial ) = default;
	callable( callable && f ) noexcept( Traits::moveable >= support_level::nofail )
		requires ( Traits::moveable != support_level::trivial )
		: function_base( static_cast<function_base &&>( f ), empty_handler_vtable() ) {}

    template <typename ... CallArguments>
	result_type operator()( CallArguments &&... args ) const noexcept( Traits::is_noexcept )
	{
        return vtable().invoke( this->functor(), std::forward< CallArguments >( args )... );
	}

    // Defaulted (trivial) assignment additionally requires a trivial
    // destructor: it overwrites the previous target without destroying it.
    static bool constexpr trivially_copy_assignable{ Traits::copyable == support_level::trivial && Traits::destructor == support_level::trivial };
    static bool constexpr trivially_move_assignable{ Traits::moveable == support_level::trivial && Traits::destructor == support_level::trivial };
    callable & operator=( callable const &   )           requires trivially_copy_assignable = default;
    callable & operator=( callable const &   )           requires ( Traits::copyable == support_level::na ) = delete;
    callable & operator=( callable const  & f ) noexcept( Traits::copyable >= support_level::nofail )
        requires ( !trivially_copy_assignable && Traits::copyable != support_level::na ) { this->assign( f ); return *this; }
    callable & operator=( callable &&   )      noexcept  requires trivially_move_assignable = default;
    callable & operator=( callable       && f ) noexcept( Traits::moveable >= support_level::nofail )
        requires ( !trivially_move_assignable ) { static_assert( Traits::moveable > support_level::na, "This callable instantiation is not moveable." ); this->assign( std::move( f ) ); return *this; }
    callable & operator=( signature_type * const plain_function_pointer ) noexcept { this->assign( plain_function_pointer ); return *this; }
    template <typename F>
    callable & operator=( F && f ) noexcept { this->assign( std::forward<F>( f ) ); return *this; }

    template <typename F>
    void assign( F && f                    ) { this->do_assign<false>( std::forward<F>( f )    ); }

    template <typename F, typename Allocator>
    void assign( F && f, Allocator const a ) { this->do_assign<false>( std::forward<F>( f ), a ); }

    void assign( std::nullptr_t ) noexcept { clear(); }

    /// Clear out a target (replace it with an empty handler), if there is one.
    void clear() { function_base:: template clear<false, empty_handler>( empty_handler_vtable() ); }

    /// Determine if the function is empty (i.e. has an empty target).
    bool empty() const noexcept { return function_base::empty( &empty_handler_vtable() ); }

    ///   Same question, asked from a thread that may be racing the one which
    /// assigns the target - available only for Traits::concurrent_reads (see
    /// policies.hpp for what the opt-in costs and, more importantly, what it
    /// does and does not make safe).
    ///   memory_order_acquire is the ordering that makes the answer usable:
    /// a false result then also orders the target itself, so reading it
    /// afterwards - invoking it, target()-ing it - is race-free with respect to
    /// the assignment that published it. memory_order_relaxed answers the
    /// question alone and orders nothing else.
    bool empty( std::memory_order const order ) const noexcept
        requires ( Traits::concurrent_reads )
    {
        return function_base::empty( &empty_handler_vtable(), order );
    }

    void swap( callable & other ) noexcept
    {
        static_assert( sizeof( callable ) == sizeof( function_base ), "Internal inconsistency" );
        return function_base:: template swap<empty_handler>( other, empty_handler_vtable() );
    }

    explicit operator bool() const noexcept { return !this->empty(); }

private:
    static auto const & empty_handler_vtable() noexcept { return vtable_for_functor<std::allocator<empty_handler>, empty_handler>( my_empty_handler() ); }

    auto const & vtable() const noexcept { return reinterpret_cast<vtable_type const &>( function_base::get_vtable() ); }

    //  The source is itself a functionoid callable (of the same or a
	// compatible-other Traits): assignment reuses the source's own vtable (see
	// assign_functionoid_direct) so no vtable may be *derived* for it here -
	// deriving one would needlessly instantiate the functor_manager machinery
	// for the callable type (whose constraints need not hold on this path,
	// e.g. a non-trivially-destructible source under destructor=trivial
	// Traits) and the derived vtable would be wrong anyway: it would describe
	// a callable wrapping the source instead of aliasing the source's target.
	//  The source's *current* vtable is also precisely the value assign()'s
	// is_a_callable overload assumes it is given (its same-Traits
	// BOOST_ASSUME( &functor_vtable == f.p_vtable_ )).
    template <typename Allocator, typename ActualFunctor, typename StoredFunctor>
        requires function_base::template is_a_callable<StoredFunctor>
    static vtable_type const & vtable_for_functor( StoredFunctor const & functor )
    {
        static_assert( std::is_base_of_v<std::remove_reference_t<StoredFunctor>, std::remove_reference_t<ActualFunctor>> );
        return reinterpret_cast<vtable_type const &>( functor.get_vtable() );
    }

    // Target contracts, as constraints rather than assertions in the body: the
    // instantiation is rejected at the call that violates it, and the failing
    // atomic constraint is named. (The empty-handler invariant below stays a
    // static_assert on purpose - it is an internal consistency check on this
    // library's own types, not a contract on the caller's target.)
    template <typename Allocator, typename ActualFunctor, typename StoredFunctor>
        requires
        (
            !function_base::template is_a_callable<StoredFunctor> &&
            ( std::is_copy_constructible_v<StoredFunctor> || Traits::copyable == support_level::na ) &&
            ( std::is_nothrow_copy_constructible_v<StoredFunctor> || Traits::copyable == support_level::na || Traits::copyable == support_level::supported )
        )
    static vtable_type const & vtable_for_functor( StoredFunctor const & /*functor*/ )
    {
        using namespace detail;

        // For the empty handler we use the manager for the base_empty_handler not
        // my_empty_handler (anti-code-bloat) because they only differ in the
        // operator() member function which is irrelevant for/not used by the
        // manager.
        static bool constexpr is_empty_handler{ std::is_same_v<ActualFunctor, empty_handler> };
        using manager_type = functor_manager
        <
            std::conditional_t
            <
                is_empty_handler,
                ActualFunctor,
                StoredFunctor
            >,
            Allocator,
            buffer
        >;

        static_assert
        (
            std::is_same_v<ActualFunctor, empty_handler>
                ==
            std::is_same_v<StoredFunctor, my_empty_handler>
        );

        using invoker_type = invoker<Traits::is_noexcept, ReturnType, Arguments...>;

        // Note: it is extremely important that this initialization uses
        // static initialization. Otherwise, we will have a race
        // condition here in multi-threaded code or inefficient thread-safe
		// initialization. See
        // http://thread.gmane.org/gmane.comp.lib.boost.devel/164902.
        static constexpr
		detail::vtable<invoker_type, Traits> const the_vtable
        {
            static_cast<manager_type  const *>( nullptr ),
            static_cast<ActualFunctor const *>( nullptr ),
            static_cast<StoredFunctor const *>( nullptr ),
            is_empty_handler
        };
        return the_vtable;
    } // vtable_for_functor()

    // ...direct actually means whether to skip pre-destruction (when not
    // assigning but constructing) so it should probably be renamed to
    // pre_destroy or the whole thing solved in some smarter way...
    template <bool direct, typename F, typename Allocator>
    void do_assign( F && f, Allocator const a )
    {
        using tag = typename detail::get_function_tag<F>::type;
        dispatch_assign<direct>( std::forward<F>( f ), a, tag{} );
    }

    template <bool direct, typename F>
    void do_assign( F && f )
    {
        using functor_type = std::remove_const_t<std::remove_reference_t<F>>;
        using allocator    = typename Traits:: template allocator<functor_type>;
        do_assign<direct>( std::forward<F>( f ), allocator{} );
    }

    template <bool direct, typename F, typename Allocator>
    void dispatch_assign( F && f   , Allocator const a, detail::function_obj_tag     ) { do_assign<direct>( std::forward<F>( f ), std::forward<F>( f ), a ); }
    // Explicit support for member function objects, so we invoke through
    // mem_fn() but retain the right target_type() values.
    template <bool direct, typename F, typename Allocator>
    void dispatch_assign( F const f, Allocator const a, detail::member_ptr_tag       ) { do_assign<direct                  >( f      , mem_fn( f ), a ); }
    template <bool direct, typename F, typename Allocator>
    void dispatch_assign( F const f, Allocator const a, detail::function_obj_ref_tag ) { do_assign<direct, typename F::type>( f.get(),         f  , a ); }
    template <bool direct, typename F, typename Allocator>
    void dispatch_assign( F       f, Allocator const a, detail::function_ptr_tag     )
    {
        // Plain function pointers need special care because when assigned
        // using the syntax without the ampersand they wreck havoc with certain
        // compilers, causing either compilation failures or broken runtime
        // behaviour, e.g. not invoking the assigned target with GCC 4.0.1 or
        // causing access-violation crashes with MSVC (tested 8 and 10).
        using non_const_function_pointer_t = std::add_pointer_t<std::remove_const_t<std::remove_pointer_t<F>>>;
        do_assign<direct, non_const_function_pointer_t, non_const_function_pointer_t>( f, std::move( f ), a );
    }

    template <bool direct, typename ActualFunctor, typename StoredFunctor, typename ActualFunctorAllocator>
    void do_assign( ActualFunctor const &, StoredFunctor && stored_functor, ActualFunctorAllocator const a )
    {
		using NakedStoredFunctor     = std::remove_const_t<std::remove_reference_t<StoredFunctor>>;
        using StoredFunctorAllocator = typename std::allocator_traits<ActualFunctorAllocator>::template rebind_alloc<NakedStoredFunctor>;
        function_base:: template assign<direct, empty_handler>
        (
            std::forward<StoredFunctor>( stored_functor ),
            vtable_for_functor<StoredFunctorAllocator, ActualFunctor>( stored_functor ),
            empty_handler_vtable(),
            StoredFunctorAllocator( a )
        );
    }
}; // class callable


// Poison comparisons between callable objects of the same type.
template <typename Signature, typename Traits> void operator==( callable<Signature, Traits> const &, callable<Signature, Traits> const & );
template <typename Signature, typename Traits> void operator!=( callable<Signature, Traits> const &, callable<Signature, Traits> const & );

//------------------------------------------------------------------------------
} // namespace psi::functionoid
//------------------------------------------------------------------------------
