////////////////////////////////////////////////////////////////////////////////
///
/// Boost.Functionoid library
///
/// \file callable_base.hpp
/// -----------------------
///
///  Copyright (c) Domagoj Saric 2010 - 2021
///
///  Use, modification and distribution is subject to the Boost Software
///  License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt)
///
/// For more information, see http://www.boost.org
///
////////////////////////////////////////////////////////////////////////////////
#ifndef callable_base_hpp__2785487C_DC49_4E29_96D0_E0D00C85E40A
#define callable_base_hpp__2785487C_DC49_4E29_96D0_E0D00C85E40A
#pragma once
//------------------------------------------------------------------------------
#include <boost/functionoid/policies.hpp>
#include <boost/functionoid/rtti.hpp>

#include <boost/assert.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/core/typeinfo.hpp>
#include <boost/config.hpp>
#include <boost/function_equal.hpp>

#include <cstdint>
#include <cstring>
#include <memory>
#include <type_traits>
//------------------------------------------------------------------------------
namespace boost
{
//------------------------------------------------------------------------------
template <typename T> class reference_wrapper;
//------------------------------------------------------------------------------
namespace functionoid
{
//------------------------------------------------------------------------------
namespace detail
{
//------------------------------------------------------------------------------

#ifdef __clang__
#   define BOOST_AUX_NO_SANITIZE __attribute__(( no_sanitize( "function" ) ))
#else
#   define BOOST_AUX_NO_SANITIZE
#endif

// http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0302r0.html Deprecating Allocator Support in std::function
using dummy_allocator = std::allocator<void *>;

// Basic buffer used to store small function objects in a functionoid. It is
// a union containing function pointers, object pointers, and a structure
// that resembles a bound member function pointer.
union function_buffer_base
{
	// For pointers to function objects
	void * obj_ptr;

	//  For 'trivial' function objects (that can be managed without type
	// information) that must be allocated on the heap (we must only save
	// the object's size information for the clone operation).
	struct trivial_heap_obj_t
	{
		void * __restrict ptr;
		std::size_t       size; // in number of bytes
	} trivial_heap_obj;

	// For function pointers of all kinds
	void (*func_ptr)();

	// For bound member pointers
	struct bound_memfunc_ptr_t
	{
		class X;
		void (X::* memfunc_ptr)();
		X * obj_ptr;
	} bound_memfunc_ptr;
}; // union function_buffer_base

template <std::uint8_t Size, std::uint8_t Alignment>
union alignas( Alignment ) function_buffer
{
	function_buffer_base base;
	char bytes[ Size ];

			  operator function_buffer_base       && () &&    noexcept { return std::move( base ); }
		      operator function_buffer_base       &  ()       noexcept { return base; }
	constexpr operator function_buffer_base const &  () const noexcept { return base; }

    static function_buffer       & from_base( function_buffer_base       & base ) noexcept { return reinterpret_cast<function_buffer       &>( base ); }
    static function_buffer const & from_base( function_buffer_base const & base ) noexcept { return reinterpret_cast<function_buffer const &>( base ); }
}; // union function_buffer

// Check that all function_buffer "access points" are actually at the same
// address/offset.
static_assert( offsetof( function_buffer_base, obj_ptr           ) == offsetof( function_buffer_base, func_ptr ) );
static_assert( offsetof( function_buffer_base, bound_memfunc_ptr ) == offsetof( function_buffer_base, func_ptr ) );
static_assert( offsetof( function_buffer_base::bound_memfunc_ptr_t, memfunc_ptr ) == 0 );

// Tags used to decide between different types of functions
struct function_ptr_tag     {};
struct function_obj_tag     {};
struct member_ptr_tag       {};
struct function_obj_ref_tag {};

// When functions and function pointers are decorated with exception
// specifications MSVC mangles their type (almost) beyond recognition.
// Even MSVC supplied type traits is_pointer, is_member_pointer and
// is_function no longer recognize them. This tester is a workaround that
// seems to work well enough for now.
template <typename T>
using is_msvc_exception_specified_function_pointer = std::integral_constant
<
    bool,
#ifdef _MSC_VER
    !std::is_class_v      <T> &&
    !std::is_fundamental_v<T> &&
    ( sizeof( T ) == sizeof( void (*)() ) )
#else
    false
#endif
>;

template <typename F>
struct get_function_tag
{
private:
	using ptr_or_obj_tag = std::conditional_t<(std::is_pointer_v<F> || std::is_function_v<F>),
								function_ptr_tag,
								function_obj_tag>;

	using ptr_or_obj_or_mem_tag = std::conditional_t<std::is_member_pointer_v<F>,
								member_ptr_tag,
								ptr_or_obj_tag>;
public:
	using type = ptr_or_obj_or_mem_tag;
}; // get_function_tag

template <typename F> struct get_function_tag<std  ::reference_wrapper<F>> { using type = function_obj_ref_tag; };
template <typename F> struct get_function_tag<boost::reference_wrapper<F>> { using type = function_obj_ref_tag; };


template <typename F, typename A>
struct functor_and_allocator : F, A // enable EBO
{
    template <typename U>
	functor_and_allocator( U && f, A a ) : F{ std::forward<U>( f ) }, A{ a } {}

	F       & functor  ()       { return *this; }
	F const & functor  () const { return *this; }
	A       & allocator()       { return *this; }
	A const & allocator() const { return *this; }
}; // struct functor_and_allocator

template <typename Functor, typename Buffer>
struct functor_traits
{
    static constexpr bool allowsPODOptimization =
            std::is_trivially_copy_constructible_v<Functor> &&
            std::is_trivially_destructible_v      <Functor>;

    static constexpr bool allowsSmallObjectOptimization =
			( sizeof ( Buffer ) >= sizeof ( Functor )      ) &&
			( alignof( Buffer ) %  alignof( Functor ) == 0 );

    static constexpr bool allowsPtrObjectOptimization =
			( sizeof ( void * ) >= sizeof ( Functor )      ) &&
			( alignof( void * ) %  alignof( Functor ) == 0 );

    static constexpr bool hasDefaultAlignement = ( alignof( Functor ) <= alignof( std::max_align_t ) );
}; // struct functor_traits

#if !defined( NDEBUG ) || defined( BOOST_ENABLE_ASSERT_HANDLER )
/// \note The cast to void is a workaround to silence the Clang warning with
/// polymorhpic Ts that we are stomping over a vtable.
///                                           (13.03.2017.) (Domagoj Saric)
template <typename T> void debug_clear( T & target ) { std::memset( static_cast<void *>( std::addressof( target ) ), -1, sizeof( target ) ); }
auto const invalid_ptr( reinterpret_cast<void const *>( static_cast<std::ptrdiff_t>( -1 ) ) );
#else
template <typename T> void debug_clear( T & ) {}
#endif // _DEBUG

/// Manager for trivial objects that fit into sizeof( void * ).
struct manager_ptr
{
    static bool constexpr trivial_destroy = true;

    static auto functor_ptr( function_buffer_base       & buffer ) {                                 return &buffer.obj_ptr; }
    static auto functor_ptr( function_buffer_base const & buffer ) { BOOST_ASSUME( buffer.obj_ptr ); return &buffer.obj_ptr; }

    template <typename Functor, typename Allocator>
    static void assign( Functor const functor, function_buffer_base & out_buffer, Allocator ) noexcept
    {
        static_assert( functor_traits<Functor, function_buffer_base>::allowsPtrObjectOptimization );
#   ifdef _MSC_VER
        // MSVC14u3 still generates a branch w/o this (GCC issues a warning that it knows that &out_buffer cannot be null so we have to ifdef guard this).
        BOOST_ASSUME( &out_buffer );
#   endif // _MSC_VER
        new ( functor_ptr( out_buffer ) ) Functor( functor );
    }

    static void BOOST_CC_FASTCALL clone( function_buffer_base const & in_buffer, function_buffer_base & out_buffer ) noexcept
    {
		//...zzz...even with __assume MSVC still generates branching code...
        //assign( *functor_ptr( in_buffer ), out_buffer, dummy_allocator() );
        out_buffer.obj_ptr = in_buffer.obj_ptr;
    }

    static void BOOST_CC_FASTCALL move( function_buffer_base && in_buffer, function_buffer_base & out_buffer ) noexcept
    {
        clone( in_buffer, out_buffer );
        destroy( in_buffer );
    }

    static void BOOST_CC_FASTCALL destroy( function_buffer_base & buffer ) noexcept
    {
        debug_clear( *functor_ptr( buffer ) );
    }
}; // struct manager_ptr

/// Manager for trivial objects that can live in a function_buffer.
template <typename Buffer>
struct manager_trivial_small
{
    static bool constexpr trivial_destroy = true;

    static void * functor_ptr( function_buffer_base & buffer ) { return &buffer; }

    template <typename Functor, typename Allocator>
    static void assign( Functor const & functor, Buffer & out_buffer, Allocator ) noexcept
    {
        static_assert
        (
			functor_traits<Functor, Buffer>::allowsPODOptimization &&
			functor_traits<Functor, Buffer>::allowsSmallObjectOptimization
        );
#   ifdef _MSC_VER
        // MSVC14u3 still generates a branch w/o this (GCC issues a warning that it knows that &out_buffer cannot be null so we have to ifdef guard this).
        BOOST_ASSUME( &out_buffer );
#   endif // _MSC_VER
        new ( functor_ptr( out_buffer ) ) Functor( functor );
    }

    static void BOOST_CC_FASTCALL clone( function_buffer_base const & __restrict in_buffer, function_buffer_base & __restrict out_buffer ) noexcept
    {
        assign( Buffer::from_base( in_buffer ), Buffer::from_base( out_buffer ), dummy_allocator{} );
    }

    static void BOOST_CC_FASTCALL move( function_buffer_base && __restrict in_buffer, function_buffer_base & __restrict out_buffer ) noexcept
    {
        clone( in_buffer, out_buffer );
        destroy( in_buffer );
    }

    static void BOOST_CC_FASTCALL destroy( function_buffer_base & buffer ) noexcept
    {
        debug_clear( Buffer::from_base( buffer ) );
    }
}; // struct manager_trivial_small

/// Manager for trivial objects that cannot live/fit in a function_buffer
/// and are used with 'empty' allocators.
template <typename Allocator>
struct manager_trivial_heap
{
private:
    using trivial_allocator = typename std::allocator_traits<Allocator>::template rebind_alloc<unsigned char>;

public:
    static bool constexpr trivial_destroy = false;

    static void       * functor_ptr( function_buffer_base       & buffer ) { BOOST_ASSUME( buffer.trivial_heap_obj.ptr ); return buffer.trivial_heap_obj.ptr; }
    static void const * functor_ptr( function_buffer_base const & buffer ) { return functor_ptr( const_cast<function_buffer_base &>( buffer ) ); }

    template <typename Functor>
    static void assign( Functor const & functor, function_buffer_base & out_buffer, [[ maybe_unused ]] Allocator const a )
    {
        static_assert
        (
			functor_traits<Functor, function_buffer_base>::allowsPODOptimization &&
			functor_traits<Functor, function_buffer_base>::hasDefaultAlignement
        );

        BOOST_ASSERT( a == trivial_allocator() );

        function_buffer_base in_buffer;
        in_buffer.trivial_heap_obj.ptr  = const_cast<Functor *>( &functor );
        in_buffer.trivial_heap_obj.size = sizeof( Functor );
        clone( in_buffer, out_buffer );
    }

    static void BOOST_CC_FASTCALL clone( function_buffer_base const & __restrict in_buffer, function_buffer_base & __restrict out_buffer )
    {
        BOOST_ASSERT( ( out_buffer.trivial_heap_obj.ptr  == 0 ) || ( out_buffer.trivial_heap_obj.ptr  == reinterpret_cast<void const *>( -1 ) ) );
        BOOST_ASSERT( ( out_buffer.trivial_heap_obj.size == 0 ) || ( out_buffer.trivial_heap_obj.size == static_cast     <std::size_t >( -1 ) ) );

        auto const storage_size( in_buffer.trivial_heap_obj.size );
        out_buffer.trivial_heap_obj.ptr  = trivial_allocator{}.allocate( storage_size );
        out_buffer.trivial_heap_obj.size = storage_size;
        std::memcpy( functor_ptr( out_buffer ), functor_ptr( in_buffer ), storage_size );
    }

    static void BOOST_CC_FASTCALL move( function_buffer_base && __restrict in_buffer, function_buffer_base & __restrict out_buffer ) noexcept
    {
        out_buffer.trivial_heap_obj = in_buffer.trivial_heap_obj;
        debug_clear( in_buffer.trivial_heap_obj );
    }

    static void BOOST_CC_FASTCALL destroy( function_buffer_base & buffer ) noexcept
    {
        BOOST_ASSERT( buffer.trivial_heap_obj.ptr  );
        BOOST_ASSERT( buffer.trivial_heap_obj.size );

        trivial_allocator a;
        a.deallocate( static_cast<typename std::allocator_traits<trivial_allocator>::pointer>( functor_ptr( buffer ) ), buffer.trivial_heap_obj.size );
        debug_clear( buffer.trivial_heap_obj );
    }
}; // struct manager_trivial_heap

/// Manager for non-trivial objects that can live in a function_buffer.
template <typename FunctorParam, typename Buffer>
struct manager_small
{
    using Functor = FunctorParam;

    static bool constexpr trivial_destroy = std::is_trivially_destructible_v<Functor>;

    static Functor       * functor_ptr( function_buffer_base       & buffer ) { return static_cast<Functor *>( manager_trivial_small<Buffer>::functor_ptr( buffer ) ); }
    static Functor const * functor_ptr( function_buffer_base const & buffer ) { return functor_ptr( const_cast<function_buffer_base &>( buffer ) ); }

    template <typename F, typename Allocator>
    static void assign( F && functor, Buffer & out_buffer, Allocator ) noexcept( noexcept( Functor( std::forward<F>( functor ) ) ) )
    {
#   ifdef _MSC_VER
        // MSVC14u3 still generates a branch w/o this (GCC issues a warning that it knows that &out_buffer cannot be null so we have to ifdef guard this).
        BOOST_ASSUME( &out_buffer );
#   endif // _MSC_VER
        new ( functor_ptr( out_buffer ) ) Functor( std::forward<F>( functor ) );
    }

    static void BOOST_CC_FASTCALL clone( function_buffer_base const & in_buffer, function_buffer_base & out_buffer ) noexcept( std::is_nothrow_copy_constructible_v<Functor> )
    {
        auto const & __restrict in_functor( *functor_ptr( in_buffer ) );
        assign( in_functor, Buffer::from_base( out_buffer ), dummy_allocator() );
    }

    static void BOOST_CC_FASTCALL move( function_buffer_base && __restrict in_buffer, function_buffer_base & __restrict out_buffer ) noexcept( std::is_nothrow_move_constructible_v<Functor> )
    {
        auto & __restrict in_functor( *functor_ptr( in_buffer ) );
        assign( std::move( in_functor ), Buffer::from_base( out_buffer ), dummy_allocator{} );
        destroy( in_buffer );
    }

    static void BOOST_CC_FASTCALL destroy( function_buffer_base & buffer ) noexcept ( std::is_nothrow_destructible_v<Functor> )
    {
        auto & __restrict functor( *functor_ptr( buffer ) );
        functor.~Functor();
        debug_clear( functor );
    }
}; // struct manager_small

///  Fully generic manager for non-trivial objects that cannot live/fit in
/// a function_buffer.
template <typename FunctorParam, typename AllocatorParam>
struct manager_generic
{
public:
    static bool constexpr trivial_destroy = false;

	using Functor           = FunctorParam  ;
	using OriginalAllocator = AllocatorParam;

    using functor_and_allocator_t = functor_and_allocator<Functor, OriginalAllocator>                                                ;
    using wrapper_allocator_t     = typename std::allocator_traits<OriginalAllocator>::template rebind_alloc<functor_and_allocator_t>;
    using allocator_allocator_t   = typename std::allocator_traits<OriginalAllocator>::template rebind_alloc<OriginalAllocator      >;

    static functor_and_allocator_t * functor_ptr( function_buffer_base & buffer ) noexcept
    {
        return static_cast<functor_and_allocator_t *>
        (
            manager_trivial_heap<OriginalAllocator>::functor_ptr( buffer )
        );
    }

    static functor_and_allocator_t const * functor_ptr( function_buffer_base const & buffer ) noexcept
    {
        return functor_ptr( const_cast<function_buffer_base &>( buffer ) );
    }

	template <typename F>
    static void assign( F && functor, function_buffer_base & out_buffer, OriginalAllocator source_allocator )
    {
        auto constexpr does_not_need_guard
        {
            std::is_nothrow_copy_constructible_v<Functor          > &&
            std::is_nothrow_copy_constructible_v<OriginalAllocator>
        };

        using guard_t = std::conditional_t
        <
            does_not_need_guard,
			functor_and_allocator_t *,
			std::unique_ptr<functor_and_allocator_t>
        >;
        assign_aux<guard_t>( std::forward<F>( functor ), out_buffer, source_allocator );
    }
#if BOOST_MSVC // Bogus heap-overflow failure w/ VS 16.10 in implicit memcpy of OriginalAllocator{ in_functor_and_allocator.allocator() }
    __declspec( no_sanitize_address )
#endif // BOOST_MSVC
    static void BOOST_CC_FASTCALL clone( function_buffer_base const & __restrict in_buffer, function_buffer_base & __restrict out_buffer )
    {
        functor_and_allocator_t const & in_functor_and_allocator{ *functor_ptr( in_buffer ) };
        assign( in_functor_and_allocator.functor(), out_buffer, in_functor_and_allocator.allocator() );
    }

    static void BOOST_CC_FASTCALL move( function_buffer_base && __restrict in_buffer, function_buffer_base & __restrict out_buffer ) noexcept
    {
        manager_trivial_heap<OriginalAllocator>::move( std::move( in_buffer ), out_buffer );
    }

    static void BOOST_CC_FASTCALL destroy( function_buffer_base & buffer ) noexcept( std::is_nothrow_destructible_v<Functor> )
    {
        functor_and_allocator_t & __restrict in_functor_and_allocator( *functor_ptr( buffer ) );

        OriginalAllocator     original_allocator ( in_functor_and_allocator.allocator() );
        allocator_allocator_t allocator_allocator( in_functor_and_allocator.allocator() );
        wrapper_allocator_t   full_allocator     ( in_functor_and_allocator.allocator() );

        std::allocator_traits<OriginalAllocator    >::destroy( original_allocator , std::addressof( in_functor_and_allocator.functor  () ) );
        std::allocator_traits<allocator_allocator_t>::destroy( allocator_allocator, std::addressof( in_functor_and_allocator.allocator() ) );

        full_allocator.deallocate( std::addressof( in_functor_and_allocator ), 1 );
        debug_clear( buffer );
    }

private:
    template <typename Guard>
    static functor_and_allocator_t * release( Guard                   &       guard   ) { return guard.release(); }
    static functor_and_allocator_t * release( functor_and_allocator_t * const pointer ) { return pointer        ; }

    template <typename Guard, typename F>
    static void assign_aux( F && functor, function_buffer_base & out_buffer, OriginalAllocator source_allocator )
    {
        wrapper_allocator_t   full_allocator     { source_allocator };
        allocator_allocator_t allocator_allocator{ source_allocator };

        Guard                                    p_placeholder          { full_allocator.allocate( 1 ) };
        std::remove_reference_t<Functor> * const p_functor_placeholder  { &*p_placeholder };
        OriginalAllocator                * const p_allocator_placeholder{ &*p_placeholder };

        std::allocator_traits<OriginalAllocator    >::construct( source_allocator   , p_functor_placeholder  , std::forward<F>( functor ) );
        std::allocator_traits<allocator_allocator_t>::construct( allocator_allocator, p_allocator_placeholder, source_allocator           );

        //...zzz...functor_ptr( out_buffer ) = release( p_placeholder );
        out_buffer.trivial_heap_obj.ptr = release( p_placeholder );
    }
}; // struct manager_generic

// Helper metafunction for retrieving an appropriate functor manager.
template
<
	typename Functor,
	typename Allocator,
	typename Buffer,
	bool POD,
	bool smallObj,
	bool ptrSmall,
	bool defaultAligned
>
struct functor_manager_aux
{
    using type = manager_generic<Functor, Allocator>;
};

template <typename Functor, typename Allocator, typename Buffer>
struct functor_manager_aux<Functor, Allocator, Buffer, true, false, false, true>
{
    using type = std::conditional_t
	<
		//...zzz...is_stateless<Allocator>,
		std::is_empty_v<Allocator>,
		manager_trivial_heap<         Allocator>,
		manager_generic     <Functor, Allocator>
	>;
};

template <typename Functor, typename Allocator, typename Buffer, bool defaultAligned>
struct functor_manager_aux<Functor, Allocator, Buffer, true, true, false, defaultAligned>
{
    using type = manager_trivial_small<Buffer>;
};

template <typename Functor, typename Allocator, typename Buffer, bool smallObj, bool defaultAligned>
struct functor_manager_aux<Functor, Allocator, Buffer, true, smallObj, true, defaultAligned>
{
    using type = manager_ptr;
};

template <typename Functor, typename Allocator, typename Buffer, bool ptrSmall, bool defaultAligned>
struct functor_manager_aux<Functor, Allocator, Buffer, false, true, ptrSmall, defaultAligned>
{
    using type = manager_small<Functor, Buffer>;
};

/// Metafunction for retrieving an appropriate functor manager with
/// minimal type information.
template <typename StoredFunctor, typename Allocator, typename Buffer>
using functor_manager = typename functor_manager_aux
<
    StoredFunctor,
    Allocator,
	Buffer,
    functor_traits<StoredFunctor, Buffer>::allowsPODOptimization,
    functor_traits<StoredFunctor, Buffer>::allowsSmallObjectOptimization,
    functor_traits<StoredFunctor, Buffer>::allowsPtrObjectOptimization,
    functor_traits<StoredFunctor, Buffer>::hasDefaultAlignement
>::type;

/// \note MSVC (14.1u5 : 16.6+) ICEs on function pointers with conditional noexcept
/// specifiers in a template context.
/// https://connect.microsoft.com/VisualStudio/feedback/details/3105692/ice-w-noexcept-function-pointer-in-a-template-context
/// Additionally this compiler generates bad binaries if function references
/// are used (instead of const pointers) by generating/storing 'null
/// references'.
///                                       (14.10.2016.) (Domagoj Saric)
/// \note Clang struggles...
/// https://github.com/emscripten-core/emscripten/issues/12639#issuecomment-719235628
///                                       (30.11.2020.) (Domagoj Saric)
#if BOOST_WORKAROUND( BOOST_MSVC, BOOST_TESTED_AT( 1900 ) ) || ( defined( __clang__ ) && ( __clang_major__ <= 7 || __clang_major__ >= 12 ) )
#define BOOST_AUX_NOEXCEPT_PTR( condition )
#else
#define BOOST_AUX_NOEXCEPT_PTR( condition ) noexcept( condition )
#endif // MSVC workaround

template <bool is_noexcept, typename ReturnType, typename ... InvokerArguments>
struct invoker
{
    template <typename Manager, typename StoredFunctor> constexpr invoker( Manager const *, StoredFunctor const * ) noexcept : invoke( &invoke_impl<Manager, StoredFunctor> ) {}
    ReturnType (BOOST_CC_FASTCALL * const invoke)( function_buffer_base & buffer, InvokerArguments... args ) BOOST_AUX_NOEXCEPT_PTR( is_noexcept );

    /// \note Defined here instead of within the callable template because
    /// of MSVC deficiencies with noexcept( expr ) function pointers (to
    /// enable the specialization workaround).
    ///                                   (17.10.2016.) (Domagoj Saric)
    template <typename FunctionObjManager, typename FunctionObj>
	/// \note Argument order optimized for a pass-in-reg calling convention.
	///                                   (17.10.2016.) (Domagoj Saric)
    /// \todo For arguments that cannot be passed in registers (eg. ineligible
    /// PODs or non-trivially copyable or moveable types) it would be more
    /// efficient to pass those by reference (i.e. do a type transformation
    /// on the InvokerArguments pack) in case the function_object() call
    /// cannot be inlined (to avoid one extra copy). Handling this is
    /// nontrivial so for now the user can handle/workaround this easily by
    /// using callables with signatures that do not specify argument types
    /// that cannot be passed in registers in case these will be used with
    /// noninlineable function objects.
    ///                                   (07.07.2020.) (Domagoj Saric)
	static ReturnType BOOST_CC_FASTCALL invoke_impl( function_buffer_base & buffer, InvokerArguments... args ) BOOST_AUX_NOEXCEPT_PTR( is_noexcept )
	{
		// We provide the invoker with a manager with a minimum amount of
		// type information (because it already knows the stored function
		// object it works with, it only needs to get its address from a
		// function_buffer object). Because of this we must cast the pointer
		// returned by FunctionObjManager::functor_ptr() because it can be
		// a plain void * in case of the trivial managers. In case of the
		// trivial ptr manager it is even a void * * so a double static_cast
		// (or a reinterpret_cast) is necessary.
        auto & __restrict function_object
		(
			*static_cast<FunctionObj *>
			(
				static_cast<void *>
				(
					FunctionObjManager::functor_ptr( buffer )
				)
			)
		);
        static_assert( noexcept( function_object( std::forward<InvokerArguments>( args )... ) ) >= is_noexcept, "Trying to assign a not-noexcept function object to a noexcept functionoid." );
		return function_object( std::forward<InvokerArguments>( args )... );
	}
}; // invoker

template <support_level Level>
struct destroyer
{
    template <typename Manager> constexpr destroyer( Manager const * ) noexcept : destroy( &Manager::destroy ) {}
    void (BOOST_CC_FASTCALL * const destroy)( function_buffer_base & __restrict buffer ) BOOST_AUX_NOEXCEPT_PTR( Level >= support_level::nofail );
};
template <>
struct destroyer<support_level::trivial>
{
    constexpr destroyer( void const * ) noexcept {}
    static void BOOST_CC_FASTCALL destroy( function_buffer_base & __restrict buffer ) noexcept { debug_clear( buffer ); }
};
template <>
struct destroyer<support_level::na> { constexpr destroyer( void const * ) noexcept {} };

template <support_level Level>
struct cloner
{
    template <typename Manager> constexpr cloner( Manager const * ) noexcept : clone( &Manager::clone ) {}
    void (BOOST_CC_FASTCALL * const clone)( function_buffer_base const &  __restrict in_buffer, function_buffer_base & __restrict out_buffer ) BOOST_AUX_NOEXCEPT_PTR( Level >= support_level::nofail );
};
template <>
struct cloner<support_level::trivial>
{
    constexpr cloner( void const * ) noexcept {}
    template <typename Buffer>
    static void BOOST_CC_FASTCALL clone( Buffer const & __restrict in_buffer, Buffer & __restrict out_buffer ) noexcept { out_buffer = in_buffer; }
};
template <>
struct cloner<support_level::na> { constexpr cloner( void const * ) noexcept {} };

template <support_level Level>
struct mover
{
    template <typename Manager> constexpr mover( Manager const * ) noexcept : move( &Manager::move ) {}
    void (BOOST_CC_FASTCALL * const move)( function_buffer_base && __restrict in_buffer, function_buffer_base & __restrict out_buffer ) BOOST_AUX_NOEXCEPT_PTR( Level >= support_level::nofail );
};
template <>
struct mover<support_level::trivial>
{
    constexpr mover( void const * ) noexcept {}
    template <typename Buffer>
    static void BOOST_CC_FASTCALL move( Buffer && __restrict in_buffer, Buffer & __restrict out_buffer ) noexcept { cloner<support_level::trivial>::clone( in_buffer, out_buffer ); }
};
template <>
struct mover<support_level::na> { constexpr mover( void const * ) noexcept {} };

template <typename ActualFunctor, typename StoredFunctor, typename FunctorManager>
class functor_type_info;

template <bool enabled>
struct reflector
{
    template <typename Manager, typename ActualFunctor, typename StoredFunctor>
    constexpr reflector( std::tuple<Manager, ActualFunctor, StoredFunctor> const * ) noexcept : get_typed_functor( &functor_type_info<ActualFunctor, StoredFunctor, Manager>::get_typed_functor ) {}
    typed_functor (BOOST_CC_FASTCALL * const get_typed_functor)( function_buffer_base const & ) noexcept;
};
template <>
struct reflector<false> { constexpr reflector( void const * ) noexcept {} };

template <bool safe>
struct empty_checker
{
    constexpr empty_checker( bool const is_empty_handler ) noexcept : is_empty( is_empty_handler ) {}
    bool is_empty_handler_vtable( void const * /*const p_current_vtable*/, void const * /*const p_empty_vtable*/ ) const noexcept { return is_empty; }
    bool const is_empty;
};
template <>
struct empty_checker<false>
{
    constexpr empty_checker( bool /*const is_empty_handler*/ ) noexcept {}
    static bool is_empty_handler_vtable( void const * const p_current_vtable, void const * const p_empty_vtable ) noexcept
    {
        return p_current_vtable == p_empty_vtable;
    }
};


#if BOOST_WORKAROUND( BOOST_MSVC, BOOST_TESTED_AT( 1903 ) ) || defined( __clang__ /*NDK r21 TODO test more versions*/ )
/// \note Clang seems to uncoditionally 'see' invoke_impl (in the main template) as not noexcept and then errors at the
///  assignment.
///  See the above note for BOOST_AUX_NOEXCEPT_PTR for MSVC.
///                                       (14.10.2016.) (Domagoj Saric)
template <typename ReturnType, typename ... InvokerArguments>
struct invoker<true, ReturnType, InvokerArguments...>
{
    template <typename Manager, typename StoredFunctor> constexpr invoker( Manager const *, StoredFunctor const * ) noexcept : invoke( &invoke_impl<Manager, StoredFunctor> ) {}
    ReturnType (BOOST_CC_FASTCALL * const invoke)( function_buffer_base & buffer, InvokerArguments... args ) noexcept;

    template <typename FunctionObjManager, typename FunctionObj>
	static ReturnType BOOST_CC_FASTCALL invoke_impl( detail::function_buffer_base & buffer, InvokerArguments... args ) noexcept
	{
		auto & __restrict function_object( *static_cast<FunctionObj *>( static_cast<void *>( FunctionObjManager::functor_ptr( buffer ) ) ) );
		return function_object( std::forward<InvokerArguments>( args )... );
	}
};
template <>
struct destroyer<support_level::nofail>
{
    template <typename Manager> constexpr destroyer( Manager const * ) noexcept : destroy( &Manager::destroy ) {}
    void (BOOST_CC_FASTCALL * const destroy)( function_buffer_base &  __restrict buffer ) BOOST_AUX_NOEXCEPT_PTR( true ); // more MSVC noexcept( <expr> ) brainfarts;
};
template <>
struct cloner<support_level::nofail>
{
    template <typename Manager> constexpr cloner( Manager const * ) noexcept : clone( &Manager::clone ) {}
    void (BOOST_CC_FASTCALL * const clone)( function_buffer_base const &  __restrict in_buffer, function_buffer_base & __restrict out_buffer ) noexcept;
};
template <>
struct mover<support_level::nofail>
{
    void (BOOST_CC_FASTCALL * const move)( function_buffer_base && __restrict in_buffer, function_buffer_base & __restrict out_buffer ) BOOST_AUX_NOEXCEPT_PTR( true ); // more MSVC noexcept( <expr> ) brainfarts
    template <typename Manager> constexpr mover( Manager const * ) noexcept : move( &Manager::move )
    {
        static_assert( noexcept( Manager::move( std::declval<function_buffer_base &&>(), std::declval<function_buffer_base &>() ) ) );
    }
};
#undef BOOST_AUX_NOEXCEPT_PTR
#endif // conditional noexcept mess workarounds


// Implementation note:
//  A "generic typed/void() invoker pointer is also stored in the vtable so
// that it can (more easily) be placed at the beginning of the vtable so
// that a vtable pointer would actually point directly to it (thus
// avoiding pointer offset calculation on invocation).
//  This also gives a unique/non-template vtable that can be held by
// callable_base entirely but it also opens a window for erroneous vtable
// copying/assignment between different callable<> instantiations.
// A typed wrapper should therefor be added to the callable<>
// class to catch such errors at compile-time.
//                                      (xx.xx.2009.) (Domagoj Saric)

template <typename Invoker, typename Traits>
struct
#ifdef _MSC_VER
__declspec( empty_bases )
#endif // MSVC 16.6 still does not have 'proper'/'automatic' EBO https://developercommunity.visualstudio.com/content/problem/561677/empty-base-class-optimization-causes-first-derived.html
vtable
    :
    // update compatible_vtables() if you change this
    Invoker,
    destroyer    <Traits::destructor          >,
    mover        <Traits::moveable            >,
    cloner       <Traits::copyable            >,
    reflector    <Traits::rtti                >,
    empty_checker<Traits::dll_safe_empty_check>
{
    template <typename ActualFunctor, typename StoredFunctor, typename Manager>
    constexpr vtable( Manager const * const manager_type, ActualFunctor const *, StoredFunctor const * const stored_functor_type, bool const is_empty_handler ) noexcept
        :
        Invoker                                    ( manager_type, stored_functor_type ),
        destroyer    <Traits::destructor          >( manager_type ),
        mover        <Traits::moveable            >( manager_type ),
        cloner       <Traits::copyable            >( manager_type ),
        reflector    <Traits::rtti                >( static_cast<std::tuple<Manager, ActualFunctor, StoredFunctor> const *>( nullptr ) ),
		empty_checker<Traits::dll_safe_empty_check>( is_empty_handler )
    {}

    // Implementation note:
    //   To test whether a functionoid instance is empty a simple check whether
    // the current vtable pointer points to the vtable for the current empty
    // handler vtable is not good enough for applications that use DLLs
    // (or equivalents) and pass callable<> instances across shared
    // module boundaries. In such circumstances one can create an empty
    // callable<> instance in module A, where it will get initialised
    // with a vtable pointer pointing to the empty handler vtable stored in
    // that module, pass it on to module B which will then query it whether
    // it is empty at which point the function<> instance will incorrectly
    // return false because it will compare its vtable pointer with the
    // address of the empty handler vtable for module B. This comparison will
    // obviously result in not-equal yielding the incorrect result.
    //   Because of the above, 'is empty' information is additionally stored
    // in the vtable. For this an additional bool data member is added. The
    // alternative would be to mangle/'tag' the vtable pointer but this would
    // actually add much more overhead (in both size and speed) because of
    // the inline-replicated demangling code. Mangling only a single, least
    // often used function pointer from the vtable is also no-good because it
    // would require functions-are-at-least-even-aligned assumption to hold
    // which need not be the case.
    //                                      (01.11.2010.) (Domagoj Saric)
    bool is_empty_handler_vtable( void const * const p_empty_handler_vtable ) const noexcept { return empty_checker<Traits::dll_safe_empty_check>::is_empty_handler_vtable( this, p_empty_handler_vtable ); }

    using base_vtable = vtable<invoker<true, void>, Traits>;
    operator base_vtable const & () const noexcept { return reinterpret_cast<base_vtable const &>( *this ); }
}; // struct vtable

template <typename Traits>
using base_vtable = vtable<invoker<true, void>, Traits>;

template <typename T>
T get_default_value( std::false_type /*not a reference type*/ ) { return {}; }

template <>
inline void get_default_value<void>( std::false_type /*not a reference type*/ ) {}

template <typename T>
T get_default_value( std::true_type /*a reference type*/ )
{
    using actual_type_t = std::remove_reference_t<T>;
    static T invalid_reference( *static_cast<actual_type_t *>( 0 ) );
    return invalid_reference;
}

////////////////////////////////////////////////////////////////////////////
struct callable_tag {};

#ifdef BOOST_MSVC
#    pragma warning( push )
#    pragma warning( disable : 4324 ) // Structure was padded due to alignment specifier.
#endif // BOOST_MSVC

template <typename Traits>
class callable_base : public callable_tag
{
public:
    /// Retrieve the type of the stored function object.
    core::typeinfo const & target_type() const
    {
        return get_vtable().get_typed_functor( this->functor_ ).functor_type_info();
    }

    template <typename Functor> Functor const * target() const noexcept { return const_cast<callable_base &>( *this ).target<Functor const>(); }
    template <typename Functor> Functor       * target()       noexcept
    BOOST_AUX_NO_SANITIZE
    {
        static_assert( Traits::rtti, "RTTI not enabled for this callable" );
        return get_vtable().get_typed_functor( this->functor_ ). template target<Functor>();
    }

    template <typename Functor>
    Functor & target_as() noexcept // unchecked version of target
    {
        static_assert( sizeof( Functor ) <= sizeof( this->functor_ ) );
        return reinterpret_cast< Functor & >( this->functor_ );
    }
    template <typename Functor>
    Functor const & target_as() const noexcept { return const_cast<callable_base &>( *this ).target_as<Functor const>(); }

    template <typename F>
    bool contains( F & f ) const noexcept
    {
        auto const p_f( target<F>() );
        return p_f && function_equal( *p_f, f );
    }

protected:
    using vtable = base_vtable<Traits>;
    using buffer = function_buffer<Traits::sbo_size, Traits::sbo_alignment>;

private: // Private helper guard classes.
	// This needs to be a template only to support stateful empty handlers.
	template <class EmptyHandler>
	class cleaner
	{
	public:
		cleaner( callable_base & function, vtable const & empty_handler_vtable )
			:
			p_function_          ( &function            ),
			empty_handler_vtable_( empty_handler_vtable )
		{}
		cleaner(cleaner const &) = delete;
		~cleaner() { conditional_clear( p_function_ != nullptr ); }

		void cancel() { BOOST_ASSERT( p_function_ ); p_function_ = nullptr; }

	private:
		void conditional_clear( bool const clear )
		{
			using namespace detail;
			if ( BOOST_UNLIKELY( clear ) )
			{
				BOOST_ASSERT( p_function_ );
				using empty_handler_traits  = functor_traits <EmptyHandler                              , buffer>;
				using empty_handler_manager = functor_manager<EmptyHandler, std::allocator<EmptyHandler>, buffer>;
				//...zzz..remove completely or replace with a simple is_stateless<>?
				static_assert
				(
					empty_handler_traits::allowsPODOptimization &&
					empty_handler_traits::allowsSmallObjectOptimization
				);
				empty_handler_manager::assign( EmptyHandler(), p_function_->functor_, std::allocator<EmptyHandler>() );
				p_function_->p_vtable_ = &empty_handler_vtable_;
			}
		}

	private:
		callable_base       * p_function_;
		vtable        const & empty_handler_vtable_;
	}; // class cleaner

protected:
	callable_base() noexcept { debug_clear( *this ); }
    callable_base( callable_base const & other, vtable const & empty_handler_vtable )
	{
		debug_clear( *this );
		assign_functionoid_direct( other, empty_handler_vtable );
	}

	callable_base( callable_base && other, vtable const & empty_handler_vtable ) noexcept
	{
		debug_clear( *this );
		assign_functionoid_direct( std::move( other ), empty_handler_vtable );
	}

	template <class EmptyHandler>
	callable_base( vtable const & empty_handler_vtable, EmptyHandler ) noexcept
	{
		debug_clear( *this );
		this->clear<true, EmptyHandler>( empty_handler_vtable );
	}

    struct no_eh_state_construction_trick_tag {};
    template <typename Constructor, typename ... Args>
    callable_base( no_eh_state_construction_trick_tag, Constructor const constructor, Args && ... args ) noexcept( noexcept( constructor( std::declval<callable_base &>(), std::forward<Args>( args )... ) ) )
    {
        auto const & vtable( constructor( *this, std::forward<Args>( args )... ) );
        BOOST_ASSUME( p_vtable_ == &vtable );
    }

	~callable_base() noexcept { destroy(); }

	template <class EmptyHandler>
	void swap( callable_base & other, vtable const & empty_handler_vtable ) noexcept;

protected:
    bool empty( void const * const p_empty_handler_vtable ) const noexcept { return get_vtable().is_empty_handler_vtable( p_empty_handler_vtable ); }

    /// \todo Add atomic vtable accessors that would enable lock-free operation
    /// for basic functionality (such as empty(), clear() and operator()()) w/o
    /// requiring an additional std::atomic<bool> is_my_functionoid_set-like
    /// variable.
    /// Making the vtable pointer a std::atomic<vtable const *> is not an
    /// option currently because even with std::memory_order_relaxed access the
    /// variable is accessed 'like a volatile' which produces bad codegen (e.g.
    /// it is reread from memory for every access).
    /// Atomic operations on non-atomic data:
    /// http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2014/n4013.html
    /// Making std::function safe for concurrency:
    /// http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/n4348.html
    /// Atomic Smart Pointers:
    /// http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2014/n4058.pdf
    /// Overloaded and qualified std::function:
    /// (for 'automatic' atomic vtable access through volatile member function
    /// overloads)
    /// http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/p0045r0.pdf
    ///                                       (08.11.2016.) (Domagoj Saric)
	auto const & get_vtable() const noexcept { BOOST_ASSUME( p_vtable_ ); return *p_vtable_; }

	buffer & functor() const noexcept { return functor_; }

	template <bool direct, class EmptyHandler>
	void clear( vtable const & empty_handler_vtable ) noexcept
	{
		// For stateless empty handlers a full assign is not necessary here
		// but a simple this->p_vtable_ = &empty_handler_vtable...
		EmptyHandler /*const*/ emptyHandler;
		assign<direct, EmptyHandler>
		(
			emptyHandler,
			empty_handler_vtable,
			empty_handler_vtable,
			std::allocator<EmptyHandler>(),
			std::false_type()
		);
	}

	// Assignment from another functionoid.
	template <bool direct, typename EmptyHandler, typename FunctionObj, typename Allocator>
	void assign
	(
		FunctionObj       && f,
        [[ maybe_unused ]]
		vtable      const &  functor_vtable,
		vtable      const &  empty_handler_vtable,
		Allocator,
		std::true_type // assignment of an instance of a callable (with possibly different traits)
	)
	{
        auto const same_traits{ std::is_convertible_v<std::decay_t<FunctionObj> *, callable_base const *> };
        if constexpr ( same_traits )
        {
		    BOOST_ASSUME( &functor_vtable == f.p_vtable_ );
        }
		if constexpr ( direct )
		{
		    BOOST_ASSUME( &f != static_cast<callable_tag const *>( this ) );
			BOOST_ASSERT
            (
                ( this->p_vtable_ == &empty_handler_vtable ) ||
                // just being constructed/inside a no_eh_state_construction_trick constructor in a debug build:
                ( this->p_vtable_ == invalid_ptr )
            );
			assign_functionoid_direct( std::forward<FunctionObj>( f ), empty_handler_vtable );
		}
		else
		{
            // guard against self-assignment where pre-destruction could 'bug up' the process
            if constexpr ( same_traits && ( Traits::destructor != support_level::trivial ) )
                if ( BOOST_UNLIKELY( &f == this ) )
                    return;
			assign_functionoid_guarded<EmptyHandler>( std::forward<FunctionObj>( f ), empty_handler_vtable );
		}
	}

	// General actual assignment.
	template <bool direct, typename EmptyHandler, typename FunctionObj, typename Allocator>
	void assign
	(
		FunctionObj       && f,
		vtable      const &  functor_vtable,
		vtable      const &  empty_handler_vtable,
		Allocator,
		std::false_type /*generic assign*/
	);

	template <typename EmptyHandler, typename F, typename Allocator>
	void actual_assign
	(
		F               &&      f,
		vtable    const &       functor_vtable,
		vtable    const &     /*empty_handler_vtable*/,
		Allocator         const a,
		std::true_type /*can use direct assign*/
	) noexcept
	{
		using functor_manager = functor_manager<F, Allocator, buffer>;
		this->destroy();
		functor_manager::assign( std::forward<F>( f ), this->functor_, a );
		this->p_vtable_ = &functor_vtable;
	}

	template <typename EmptyHandler, typename F, typename Allocator>
	void actual_assign
	(
		F               &&       f,
		vtable    const &        functor_vtable,
		vtable    const &        empty_handler_vtable,
		Allocator          const a,
		std::false_type /*must use safe assignment*/
	)
	{
		// This most generic case needs to be reworked [currently does redundant
		// copying (through the vtable function pointers) and does not use all
		// the type information it could...]...
		using functor_manager = functor_manager<std::remove_reference_t<F>, Allocator, buffer>;
		callable_base tmp( empty_handler_vtable, EmptyHandler() );
		functor_manager::assign( std::forward<F>( f ), tmp.functor_, a );
		tmp.p_vtable_ = &functor_vtable;
		this->swap<EmptyHandler>( tmp, empty_handler_vtable );
	}

private: // Assignment from another functionoid helpers.
	void assign_functionoid_direct( callable_base const & source, vtable const & /*empty_handler_vtable*/ ) noexcept( Traits::copyable >= support_level::nofail )
    BOOST_AUX_NO_SANITIZE
	{
        static_assert( Traits::copyable != support_level::na, "Callable not copyable" );
		source.get_vtable().clone( source.functor_, this->functor_ );
		p_vtable_ = &source.get_vtable();
	}

	void assign_functionoid_direct( callable_base && source, vtable const & empty_handler_vtable ) noexcept( ( Traits::moveable >= support_level::nofail ) || ( Traits::moveable == support_level::na && Traits::copyable >= support_level::nofail ) )
	{
        source.move_to( *this, std::integral_constant<bool, Traits::moveable != support_level::na>{} );
		this ->p_vtable_ = &source.get_vtable();
		source.p_vtable_ = &empty_handler_vtable;
	}

    static constexpr bool compatible_vtable_function_entry( support_level const me, support_level const other ) noexcept
    {
        using level = support_level;

        bool compatible{ me == other };
        if ( !compatible )
        {
            switch ( me )
            {
                case level::supported: compatible = ( other != level::na      ) && ( other != level::trivial ); break; // trivial does not use/provide a pointer
                case level::na       : compatible = ( other == level::trivial );                                break; // trivial also does not use/'allocate' a function pointer
                // silence warnings
                case level::nofail   : [[ fallthrough ]]; //compatible = ( other == level::trivial ); break; // trivial does not use/provide a pointer
                case level::trivial  : break;
            }
        }
        return compatible;
    }

    template <typename OtherTraits>
    static constexpr bool compatible_vtables() noexcept
    {
        // vtable pointer order
        // invoker
        // destroyer
        // mover
        // cloner
        // reflector
        // empty_checker

        auto const destroy_matches{ compatible_vtable_function_entry( Traits::destructor, OtherTraits::destructor ) };
        auto          move_matches{ compatible_vtable_function_entry( Traits::moveable  , OtherTraits::moveable   ) };
        auto          copy_matches{ compatible_vtable_function_entry( Traits::copyable  , OtherTraits::copyable   ) };

        if ( !copy_matches && !move_matches )
        {
            auto const swappable
            {
                compatible_vtable_function_entry( Traits::moveable, OtherTraits::copyable ) &&
                compatible_vtable_function_entry( Traits::copyable, OtherTraits::moveable )
            };

            copy_matches = move_matches = swappable;
        }

        return destroy_matches && move_matches && copy_matches &&
            ( OtherTraits::is_noexcept >= Traits::is_noexcept ) &&
            ( OtherTraits::rtti == Traits::rtti || ( !Traits::rtti && !Traits::dll_safe_empty_check ) ) && // second part -> means the rest of the vtable is ignored/not used by this callable
            ( OtherTraits::dll_safe_empty_check >= Traits::dll_safe_empty_check );
    }

    template <typename OtherTraits>
    void assign_functionoid_direct( callable_base<OtherTraits> const & source, vtable const & /*empty_handler_vtable*/ ) noexcept( OtherTraits::copyable >= support_level::nofail )
    BOOST_AUX_NO_SANITIZE
	{
        static_assert( compatible_vtables<OtherTraits>() );
        static_assert( Traits::sbo_size      >= OtherTraits::sbo_size      );
        static_assert( Traits::sbo_alignment >= OtherTraits::sbo_alignment );

        static_assert( ( OtherTraits::copyable != support_level::na ) || ( OtherTraits::moveable == support_level::trivial ), "Not copyable" );

        if constexpr ( OtherTraits::copyable != support_level::na )
		    source.get_vtable().clone( source.functor_, reinterpret_cast< typename callable_base<OtherTraits>::buffer & >( this->functor_ ) );
        else
        if constexpr ( OtherTraits::moveable == support_level::trivial )
            source.get_vtable().move( source.functor_, reinterpret_cast< typename callable_base<OtherTraits>::buffer & >( this->functor_ ) );

        auto & source_vtable{ source.get_vtable() } ;
        static_assert( sizeof( *p_vtable_ ) == sizeof( source_vtable ) );
		p_vtable_ = reinterpret_cast<vtable const *>( &source_vtable );
	}

	template <typename EmptyHandler, typename FunctionBaseRef>
	void assign_functionoid_guarded( FunctionBaseRef && source, vtable const & empty_handler_vtable )
	{
		destroy();
		cleaner<EmptyHandler> guard( *this, empty_handler_vtable );
		assign_functionoid_direct( std::forward<FunctionBaseRef>( source ), empty_handler_vtable );
		guard.cancel();
	}

    // It is safe to unconditionally call destroy on an empty callable as the
    // empty handler's vtable will correctly handle it.
	void destroy() noexcept BOOST_AUX_NO_SANITIZE { get_vtable().destroy( this->functor_ ); }

    void move_to( callable_base & destination, std::true_type  /*    has move*/ ) const noexcept( Traits::moveable >= support_level::nofail )
    BOOST_AUX_NO_SANITIZE
    {
        get_vtable().move ( std::move( this->functor_ ), destination.functor_ );
    }
    void move_to( callable_base & destination, std::false_type /*not has move*/ ) const noexcept( Traits::copyable >= support_level::nofail )
    BOOST_AUX_NO_SANITIZE
    {
        get_vtable().clone( std::move( this->functor_ ), destination.functor_ );
    }

private: template <typename OtherTraits> friend class callable_base;
	class safe_mover_base;
	template <class EmptyHandler> class safe_mover;

			vtable const * __restrict p_vtable_;
	mutable buffer                    functor_ ;
}; // class callable_base

#ifdef BOOST_MSVC
#    pragma warning( pop )
#endif // BOOST_MSVC

template <typename T>
BOOST_FORCEINLINE bool has_empty_target( T * const funcPtr, function_ptr_tag ) noexcept { return funcPtr == 0; }

template <typename T>
BOOST_FORCEINLINE bool has_empty_target_aux( T * const funcPtr, member_ptr_tag ) noexcept { return has_empty_target<T>( funcPtr, function_ptr_tag{} ); }

template <typename F>
BOOST_FORCEINLINE bool has_empty_target_aux( F const * const f, function_obj_tag ) noexcept
{
    // https://stackoverflow.com/questions/16893992/check-if-type-can-be-explicitly-converted
    if constexpr ( std::is_constructible_v<bool, F> )
        return !static_cast<bool>( *f );
    else
        return false;
}

template <typename T>
BOOST_FORCEINLINE bool has_empty_target( T const & f, function_obj_tag ) noexcept { return has_empty_target_aux( std::addressof( f ), function_obj_tag{} ); }

template <class FunctionObj>
BOOST_FORCEINLINE bool has_empty_target( std::reference_wrapper<FunctionObj> const & f, function_obj_ref_tag ) noexcept
{
    // Implementation note:
    // We save/assign a reference to a functionoid even if it is empty and let
    // the referenced functionoid handle a possible empty invocation.
    //                                        (28.10.2010.) (Domagoj Saric)
    return std::is_base_of_v<callable_tag, FunctionObj>
        ? false
        : has_empty_target( f.get(), function_obj_tag{} );
}

template <class FunctionObj>
BOOST_FORCEINLINE bool has_empty_target( boost::reference_wrapper<FunctionObj> const & f, function_obj_ref_tag ) noexcept
{
    return has_empty_target( std::cref( f.get() ), function_obj_ref_tag{} );
}

template <typename Traits>
class callable_base<Traits>::safe_mover_base
{
protected:
	using functor = typename callable_base<Traits>::buffer;
	using vtable  = typename callable_base<Traits>::vtable;

protected:
	safe_mover_base( safe_mover_base const & ) = delete;
   ~safe_mover_base() = default;

public:
	safe_mover_base( callable_base & function_to_guard, callable_base & empty_function_to_move_to ) noexcept
		:
		p_function_to_restore_to_ { &function_to_guard                     },
		empty_function_to_move_to_{ empty_function_to_move_to              },
		empty_handler_vtable_     { empty_function_to_move_to.get_vtable() }
	{
		BOOST_ASSERT( empty_function_to_move_to_.p_vtable_ == &empty_handler_vtable_ );
		move( function_to_guard, empty_function_to_move_to_, empty_handler_vtable_ );
	}

public:
	void cancel() noexcept { BOOST_ASSERT( p_function_to_restore_to_ ); p_function_to_restore_to_ = 0; }

	static void move( callable_base & source, callable_base & destination, vtable const & empty_handler_vtable ) noexcept
	{
        source.move_to( destination, std::integral_constant<bool, Traits::moveable != support_level::na>{} );
		destination.p_vtable_ = source.p_vtable_;
		source     .p_vtable_ = &empty_handler_vtable;
	}

protected:
	callable_base       * __restrict p_function_to_restore_to_ ;
	callable_base       & __restrict empty_function_to_move_to_;
	vtable        const & __restrict empty_handler_vtable_     ;
}; // safe_mover_base

// ...if the is_stateless<EmptyHandler> requirement sticks this will not need
// to be a template...
template <class Traits>
template <class EmptyHandler>
class callable_base<Traits>::safe_mover : public safe_mover_base
{
public:
	using safe_mover_base::safe_mover_base;
	~safe_mover()
	{
		if ( this->p_function_to_restore_to_ )
		{
			cleaner<EmptyHandler> guard( *this->p_function_to_restore_to_, this->empty_handler_vtable_ );
			this->move( this->empty_function_to_move_to_, *this->p_function_to_restore_to_, this->empty_handler_vtable_ );
			guard.cancel();
		}
	}
}; // class safe_mover

template <class Traits>
template <class EmptyHandler>
void callable_base<Traits>::swap( callable_base & other, vtable const & empty_handler_vtable ) noexcept
{
	if ( &other == this )
		return;

	callable_base tmp( empty_handler_vtable, EmptyHandler() );

	safe_mover<EmptyHandler> my_restorer   ( *this, tmp   );
	safe_mover<EmptyHandler> other_restorer( other, *this );

	safe_mover_base::move( tmp, other, empty_handler_vtable );

	my_restorer   .cancel();
	other_restorer.cancel();
} // void callable_base::swap()

template <typename Traits>
template <bool direct, typename EmptyHandler, typename F, typename Allocator>
void callable_base<Traits>::assign
(
	F               &&       f,
	vtable    const &        functor_vtable,
	vtable    const &        empty_handler_vtable,
	Allocator          const a,
	std::false_type /*generic assign*/
)
{
	using namespace detail;

	using tag = typename get_function_tag<F>::type;
	if ( has_empty_target( f, tag{} ) )
		this->clear<direct, EmptyHandler>( empty_handler_vtable );
	else
	if constexpr ( direct )
	{
        // Implementation note:
        //   See the note for the no_eh_state_constructor helper in
        // functionoid.hpp as to why a null vtable is allowed and expected
        // here.
        //                                    (02.11.2010.) (Domagoj Saric)
        BOOST_ASSERT( this->p_vtable_ == &empty_handler_vtable || /*just being constructed/inside a no_eh_state_construction_trick constructor in a debug build:*/ this->p_vtable_ == invalid_ptr );
		using functor_manager = functor_manager<std::remove_reference_t<F>, Allocator, buffer>;
		functor_manager::assign( std::forward<F>( f ), this->functor_, a );
		this->p_vtable_ = &functor_vtable;
	}
	else
	{
		/// \todo This can/should be rewritten because the
		/// small-object-optimization condition is too strict, even heap
		/// allocated targets can be assigned directly because they have a
		/// nothrow swap operation.
	    ///                               (28.10.2010.) (Domagoj Saric)
		using has_no_fail_assignement_t = std::integral_constant
        <
            bool,
			functor_traits<F, buffer>::allowsSmallObjectOptimization &&
            std::is_nothrow_assignable_v<std::remove_reference_t<F>, F>
		>;

		actual_assign<EmptyHandler>
		(
			std::forward<F>( f ),
			functor_vtable,
			empty_handler_vtable,
			a,
            has_no_fail_assignement_t{}
		);
	}
} // void callable_base::assign()

//------------------------------------------------------------------------------
} // namespace detail
//------------------------------------------------------------------------------
} // namespace functionoid
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // callable_base_hpp
