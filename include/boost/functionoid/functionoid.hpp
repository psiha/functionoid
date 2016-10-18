////////////////////////////////////////////////////////////////////////////////
///
/// Boost.Functionoid library
/// 
/// \file functionoid.hpp
/// ---------------------
///
///  Copyright (c) Domagoj Saric 2010 - 2016
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
#include <boost/core/ignore_unused.hpp>
#include <boost/core/typeinfo.hpp>
#include <boost/config.hpp>
#include <boost/function_equal.hpp>
#include <boost/throw_exception.hpp>

#include "detail/platform_specifics.hpp"
#include "policies.hpp"
#include "rtti.hpp"

#include <stdexcept>
#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>

#if defined(BOOST_MSVC)
#   pragma warning( push )
#   pragma warning( disable : 4127 ) // "conditional expression is constant"
#   pragma warning( disable : 4510 ) // "default constructor could not be generated" (boost::detail::function::vtable)
#   pragma warning( disable : 4512 ) // "assignment operator could not be generated" (boost::detail::function::vtable)
#   pragma warning( disable : 4610 ) // "class can never be instantiated - user defined constructor required" (boost::detail::function::vtable)
#   pragma warning( disable : 4793 ) // complaint about native code generation
#endif       
//------------------------------------------------------------------------------
namespace boost
{
//------------------------------------------------------------------------------
template <typename T> class reference_wrapper;
//------------------------------------------------------------------------------
namespace functionoid
{
//------------------------------------------------------------------------------

struct default_traits;

class typed_functor;

template <typename Signature, typename Traits = default_traits>
class callable;

namespace detail
{
    // http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0302r0.html Deprecating Allocator Support in std::function
    using dummy_allocator = std::allocator<void *>;

    // Basic buffer used to store small function objects in a functionoid. It is
    // a union containing function pointers, object pointers, and a structure
    // that resembles a bound member function pointer.
	union function_buffer_base
	{
		// For pointers to function objects
		void * __restrict obj_ptr;

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
	static_assert( offsetof( function_buffer_base, obj_ptr           ) == offsetof( function_buffer_base, func_ptr ), "" );
	static_assert( offsetof( function_buffer_base, bound_memfunc_ptr ) == offsetof( function_buffer_base, func_ptr ), "" );
	static_assert( offsetof( function_buffer_base::bound_memfunc_ptr_t, memfunc_ptr ) == 0, "" );

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
        !std::is_class      <T>::value &&
        !std::is_fundamental<T>::value &&
        ( sizeof( T ) == sizeof( void (*)() ) )
    #else
        false
    #endif
    >;

    template <typename F>
    struct get_function_tag
    {
	private:
		using ptr_or_obj_tag = typename std::conditional<(std::is_pointer<F>::value || std::is_function<F>::value),
									function_ptr_tag,
									function_obj_tag>::type;

		using ptr_or_obj_or_mem_tag = typename std::conditional<(std::is_member_pointer<F>::value),
									member_ptr_tag,
									ptr_or_obj_tag>::type;
	public:
		using type = ptr_or_obj_or_mem_tag;
    }; // get_function_tag

	template <typename F> struct get_function_tag<std  ::reference_wrapper<F>> { using type = function_obj_ref_tag; };
	template <typename F> struct get_function_tag<boost::reference_wrapper<F>> { using type = function_obj_ref_tag; };


    template <typename F, typename A>
    struct functor_and_allocator : F, A // enable EBO
    {
		functor_and_allocator( F const & f, A a ) : F( f ), A( a ) {}

		F       & functor  ()       { return *this; }
		F const & functor  () const { return *this; }
		A       & allocator()       { return *this; }
		A const & allocator() const { return *this; }
    }; // struct functor_and_allocator

    template <typename Functor, typename Buffer>
    struct functor_traits
    {
        static constexpr bool allowsPODOptimization =
                std::is_trivially_copy_constructible<Functor>::value &&
                std::is_trivially_destructible      <Functor>::value;

        static constexpr bool allowsSmallObjectOptimization =
				( sizeof ( Buffer ) >= sizeof ( Functor )      ) &&
				( alignof( Buffer ) %  alignof( Functor ) == 0 );

        static constexpr bool allowsPtrObjectOptimization =
				( sizeof ( void * ) >= sizeof ( Functor )      ) &&
				( alignof( void * ) %  alignof( Functor ) == 0 );

        static constexpr bool hasDefaultAlignement = alignof( Functor ) <= alignof( std::max_align_t );
    }; // struct functor_traits

    #if !defined( NDEBUG ) || defined( BOOST_ENABLE_ASSERT_HANDLER )
    template <typename T> void debug_clear( T & target ) { std::memset( std::addressof( target ), 0, sizeof( target ) ); }
    #else
    template <typename T> void debug_clear( T & ) {}
    #endif // _DEBUG

    /// Manager for trivial objects that fit into sizeof( void * ).
    struct manager_ptr
    {
        static auto functor_ptr( function_buffer_base       & buffer ) {                              return &buffer.obj_ptr; }
        static auto functor_ptr( function_buffer_base const & buffer ) { BF_ASSUME( buffer.obj_ptr ); return &buffer.obj_ptr; }

        template <typename Functor, typename Allocator>
        static void assign( Functor const functor, function_buffer_base & out_buffer, Allocator ) noexcept
        {
            static_assert( functor_traits<Functor, function_buffer_base>::allowsPtrObjectOptimization, "" );
            new ( functor_ptr( out_buffer ) ) Functor( functor );
        }

        static void BF_FASTCALL clone( function_buffer_base const & in_buffer, function_buffer_base & out_buffer ) noexcept
        {
		    //...zzz...even with __assume MSVC still generates branching code...
            //assign( *functor_ptr( in_buffer ), out_buffer, dummy_allocator() );
            out_buffer.obj_ptr = in_buffer.obj_ptr;
        }

        static void BF_FASTCALL move( function_buffer_base && in_buffer, function_buffer_base & out_buffer ) noexcept
        {
            clone( in_buffer, out_buffer );
            destroy( in_buffer );
        }

        static void BF_FASTCALL destroy( function_buffer_base & buffer ) noexcept
        {
            debug_clear( *functor_ptr( buffer ) );
        }
    }; // struct manager_ptr

    /// Manager for trivial objects that can live in a function_buffer.
	template <typename Buffer>
    struct manager_trivial_small
    {
        static void * functor_ptr( function_buffer_base & buffer ) { return &buffer; }

        template <typename Functor, typename Allocator>
        static void assign( Functor const & functor, Buffer & out_buffer, Allocator )
        {
            static_assert
            (
				functor_traits<Functor, Buffer>::allowsPODOptimization &&
				functor_traits<Functor, Buffer>::allowsSmallObjectOptimization, ""
            );
            new ( functor_ptr( out_buffer ) ) Functor( functor );
        }

        static void BF_FASTCALL clone( function_buffer_base const & __restrict in_buffer, function_buffer_base & __restrict out_buffer ) noexcept
        {
            assign( Buffer::from_base( in_buffer ), Buffer::from_base( out_buffer ), dummy_allocator() );
        }

        static void BF_FASTCALL move( function_buffer_base && __restrict in_buffer, function_buffer_base & __restrict out_buffer ) noexcept
        {
            clone( in_buffer, out_buffer );
            destroy( in_buffer );
        }

        static void BF_FASTCALL destroy( function_buffer_base & buffer ) noexcept
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
        using trivial_allocator = typename Allocator:: template rebind<unsigned char>::other;

    public:
        static void       * functor_ptr( function_buffer_base       & buffer ) { BF_ASSUME( buffer.trivial_heap_obj.ptr ); return buffer.trivial_heap_obj.ptr; }
        static void const * functor_ptr( function_buffer_base const & buffer ) { return functor_ptr( const_cast<function_buffer_base &>( buffer ) ); }

        template <typename Functor>
        static void assign( Functor const & functor, function_buffer_base & out_buffer, Allocator const a )
        {
            BOOST_ASSERT( a == trivial_allocator() );
            ignore_unused( a );

            static_assert
            (
				functor_traits<Functor, function_buffer_base>::allowsPODOptimization &&
				functor_traits<Functor, function_buffer_base>::hasDefaultAlignement, ""
            );

            function_buffer_base in_buffer;
            in_buffer.trivial_heap_obj.ptr  = const_cast<Functor *>( &functor );
            in_buffer.trivial_heap_obj.size = sizeof( Functor );
            clone( in_buffer, out_buffer );
        }

        static void BF_FASTCALL clone( function_buffer_base const & __restrict in_buffer, function_buffer_base & __restrict out_buffer )
        {
            trivial_allocator a;
            auto const storage_size( in_buffer.trivial_heap_obj.size );
            out_buffer.trivial_heap_obj.ptr  = a.allocate( storage_size );
            out_buffer.trivial_heap_obj.size = storage_size;
            std::memcpy( functor_ptr( out_buffer ), functor_ptr( in_buffer ), storage_size );
        }

        static void BF_FASTCALL move( function_buffer_base && __restrict in_buffer, function_buffer_base & __restrict out_buffer ) noexcept
        {
            out_buffer.trivial_heap_obj = in_buffer.trivial_heap_obj;
            debug_clear( in_buffer.trivial_heap_obj );
        }

        static void BF_FASTCALL destroy( function_buffer_base & buffer ) noexcept
        {
            BOOST_ASSERT( buffer.trivial_heap_obj.ptr  );
            BOOST_ASSERT( buffer.trivial_heap_obj.size );

            trivial_allocator a;
            a.deallocate( static_cast<typename trivial_allocator::pointer>( functor_ptr( buffer ) ), buffer.trivial_heap_obj.size );
            debug_clear( buffer.trivial_heap_obj );
        }
    }; // struct manager_trivial_heap

    /// Manager for non-trivial objects that can live in a function_buffer.
    template <typename FunctorParam, typename Buffer>
    struct manager_small
    {
        using Functor = FunctorParam;

        static Functor       * functor_ptr( function_buffer_base       & buffer ) { return static_cast<Functor *>( manager_trivial_small<Buffer>::functor_ptr( buffer ) ); }
        static Functor const * functor_ptr( function_buffer_base const & buffer ) { return functor_ptr( const_cast<function_buffer_base &>( buffer ) ); }

        template <typename F, typename Allocator>
        static void assign( F && functor, Buffer & out_buffer, Allocator ) noexcept( noexcept( Functor( std::forward<F>( functor ) ) ) )
        {
            new ( functor_ptr( out_buffer ) ) Functor( std::forward<F>( functor ) );
        }

        static void BF_FASTCALL clone( function_buffer_base const & in_buffer, function_buffer_base & out_buffer ) noexcept( std::is_nothrow_copy_constructible<Functor>::value )
        {
            Functor const & in_functor( *functor_ptr( in_buffer ) );
            assign( Buffer::from_base( in_functor ), Buffer::from_base( out_buffer ), dummy_allocator() );
        }

        static void BF_FASTCALL move( function_buffer_base && __restrict in_buffer, function_buffer_base & __restrict out_buffer ) noexcept( std::is_nothrow_move_constructible<Functor>::value )
        {
            auto & __restrict in_functor( *functor_ptr( in_buffer ) );
            assign( std::move( in_functor ), Buffer::from_base( out_buffer ), dummy_allocator() );
            destroy( in_buffer );
        }

        static void BF_FASTCALL destroy( function_buffer_base & buffer ) noexcept ( std::is_nothrow_destructible<Functor>::value )
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
		using Functor           = FunctorParam  ;
		using OriginalAllocator = AllocatorParam;

        using functor_and_allocator_t =          functor_and_allocator<Functor, OriginalAllocator>                  ;
        using wrapper_allocator_t     = typename OriginalAllocator:: template rebind<functor_and_allocator_t>::other;
        using allocator_allocator_t   = typename OriginalAllocator:: template rebind<OriginalAllocator      >::other;

        static functor_and_allocator_t * functor_ptr( function_buffer_base & buffer )
        {
            return static_cast<functor_and_allocator_t *>
            (
                manager_trivial_heap<OriginalAllocator>::functor_ptr( buffer )
            );
        }

        static functor_and_allocator_t const * functor_ptr( function_buffer_base const & buffer )
        {
            return functor_ptr( const_cast<function_buffer_base &>( buffer ) );
        }

		template <typename F>
        static void assign( F && functor, function_buffer_base & out_buffer, OriginalAllocator source_allocator )
        {
            using does_not_need_guard_t = std::integral_constant
            <
                bool,
				std::is_nothrow_copy_constructible<Functor          >::value &&
				std::is_nothrow_copy_constructible<OriginalAllocator>::value
            >;

            using guard_t = typename std::conditional
            <
				does_not_need_guard_t::value,
				functor_and_allocator_t *,
				std::unique_ptr<functor_and_allocator_t>
            >::type;
            assign_aux<guard_t>( std::forward<F>( functor ), out_buffer, source_allocator );
        }

        static void BF_FASTCALL clone( function_buffer_base const & __restrict in_buffer, function_buffer_base & __restrict out_buffer )
        {
            functor_and_allocator_t const & in_functor_and_allocator( *functor_ptr( in_buffer ) );
            assign( in_functor_and_allocator.functor(), out_buffer, in_functor_and_allocator.allocator() );
        }

        static void BF_FASTCALL move( function_buffer_base && __restrict in_buffer, function_buffer_base & __restrict out_buffer ) noexcept
        {
            manager_trivial_heap<OriginalAllocator>::move( std::move( in_buffer ), out_buffer );
        }

        static void BF_FASTCALL destroy( function_buffer_base & buffer ) noexcept( std::is_nothrow_destructible<Functor>::value )
        {
            functor_and_allocator_t & __restrict in_functor_and_allocator( *functor_ptr( buffer ) );

            OriginalAllocator     original_allocator ( in_functor_and_allocator.allocator() );
            allocator_allocator_t allocator_allocator( in_functor_and_allocator.allocator() );
            wrapper_allocator_t   full_allocator     ( in_functor_and_allocator.allocator() );

            original_allocator .destroy( original_allocator .address( in_functor_and_allocator.functor  () ) );
            allocator_allocator.destroy( allocator_allocator.address( in_functor_and_allocator.allocator() ) );

            full_allocator.deallocate( full_allocator.address( in_functor_and_allocator ), 1 );
            debug_clear( buffer );
        }

    private:
        template <typename Guard>
        static functor_and_allocator_t * release( Guard                   &       guard   ) { return guard.release(); }
        static functor_and_allocator_t * release( functor_and_allocator_t * const pointer ) { return pointer        ; }

        template <typename Guard, typename F>
        static void assign_aux( F && functor, function_buffer_base & out_buffer, OriginalAllocator source_allocator )
        {
            wrapper_allocator_t   full_allocator     ( source_allocator );
            allocator_allocator_t allocator_allocator( source_allocator );

            Guard                     p_placeholder          ( full_allocator.allocate( 1 ) );
            Functor           * const p_functor_placeholder  ( &*p_placeholder );
            OriginalAllocator * const p_allocator_placeholder( &*p_placeholder );

            source_allocator   .construct( p_functor_placeholder  , std::forward<F>( functor ) );
            allocator_allocator.construct( p_allocator_placeholder, source_allocator );

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
        using type = typename std::conditional
		<
			//...zzz...is_stateless<Allocator>,
			std::is_empty<Allocator>::value,
			manager_trivial_heap<         Allocator>,
			manager_generic     <Functor, Allocator>
		>::type;
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

    /// \note MSVC (14 update 3 and 14.1 preview 5) ICE on function pointers
    /// with conditional noexcept specifiers in a template context.
    /// https://connect.microsoft.com/VisualStudio/feedback/details/3105692/ice-w-noexcept-function-pointer-in-a-template-context
    /// Additionally this compiler generates bad binaries if function references
    /// are used (instead of const pointers) by generating/storing 'null
    /// references'.
    ///                                       (14.10.2016.) (Domagoj Saric)
#if BOOST_WORKAROUND( BOOST_MSVC, BOOST_TESTED_AT( 1900 ) )
    #define BOOST_AUX_NOEXCEPT_PTR( condition )
#else
    #define BOOST_AUX_NOEXCEPT_PTR( condition ) noexcept( condition )
#endif // MSVC workaround

    template <bool is_noexcept, typename ReturnType, typename ... InvokerArguments>
    struct invoker
    {
        template <typename Manager, typename StoredFunctor> constexpr invoker( Manager const *, StoredFunctor const * ) noexcept : invoke( &invoke_impl<Manager, StoredFunctor> ) {}
        ReturnType (BF_FASTCALL * const invoke)( function_buffer_base & __restrict buffer, InvokerArguments... args ) BOOST_AUX_NOEXCEPT_PTR( is_noexcept );

        /// \note Defined here instead of within the callable template because
        /// of MSVC14 deficiencies with noexcept( expr ) function pointers (to
        /// enable the specialization workaround).
        ///                                   (17.10.2016.) (Domagoj Saric)
        template <typename FunctionObjManager, typename FunctionObj>
		/// \note Argument order optimized for a pass-in-reg calling convention.
		///                                   (17.10.2016.) (Domagoj Saric)
		static ReturnType BF_FASTCALL invoke_impl( detail::function_buffer_base & buffer, InvokerArguments... args ) BOOST_AUX_NOEXCEPT_PTR( is_noexcept ) // MSVC14u3 and Xcode8 AppleClang barf @ ( noexcept( std::declval<FunctionObj>( args... ) ) )
		{
			// We provide the invoker with a manager with a minimum amount of
			// type information (because it already knows the stored function
			// object it works with, it only needs to get its address from a
			// function_buffer object). Because of this we must cast the pointer
			// returned by FunctionObjManager::functor_ptr() because it can be
			// a plain void * in case of the trivial managers. In case of the
			// trivial ptr manager it is even a void * * so a double static_cast
			// (or a reinterpret_cast) is necessary.
			auto & __restrict functionObject
			(
				*static_cast<FunctionObj *>
				(
					static_cast<void *>
					(
						FunctionObjManager::functor_ptr( buffer )
					)
				)
			);
            static_assert( noexcept( functionObject( args... ) ) >= is_noexcept, "Trying to assign a not-noexcept function object to a noexcept functionoid." );
			return functionObject( args... );
		}
    }; // invoker

    template <support_level Level>
    struct destroyer
    {
        template <typename Manager> constexpr destroyer( Manager const * ) noexcept : destroy( &Manager::destroy ) {}
        void (BF_FASTCALL * const destroy)( function_buffer_base & __restrict buffer ) BOOST_AUX_NOEXCEPT_PTR( Level >= support_level::nofail );
    };
    template <>
    struct destroyer<support_level::trivial>
    {
        constexpr destroyer( void const * ) noexcept {}
        static void BF_FASTCALL destroy( function_buffer_base & __restrict buffer ) noexcept { debug_clear( buffer ); }
    };
    template <>
    struct destroyer<support_level::na> { constexpr destroyer( void const * ) noexcept {} };

    template <support_level Level>
    struct cloner
    {
        template <typename Manager> constexpr cloner( Manager const * ) noexcept : clone( &Manager::clone ) {}
        void (BF_FASTCALL * const clone)( function_buffer_base const &  __restrict in_buffer, function_buffer_base & __restrict out_buffer ) BOOST_AUX_NOEXCEPT_PTR( Level >= support_level::nofail );
    };
    template <>
    struct cloner<support_level::trivial>
    {
        constexpr cloner( void const * ) noexcept {}
        static void BF_FASTCALL clone( function_buffer_base const & __restrict in_buffer, function_buffer_base & __restrict out_buffer ) noexcept { out_buffer = in_buffer; }
    };
    template <>
    struct cloner<support_level::na> { constexpr cloner( void const * ) noexcept {} };

    template <support_level Level>
    struct mover
    {
        template <typename Manager> constexpr mover( Manager const * ) noexcept : move( &Manager::move ) {}
        void (BF_FASTCALL * const move)( function_buffer_base && __restrict in_buffer, function_buffer_base & __restrict out_buffer ) BOOST_AUX_NOEXCEPT_PTR( Level >= support_level::nofail );
    };
    template <>
    struct mover<support_level::trivial>
    {
        constexpr mover( void const * ) noexcept {}
        static void BF_FASTCALL move( function_buffer_base && __restrict in_buffer, function_buffer_base & __restrict out_buffer ) noexcept { cloner<support_level::trivial>::clone( in_buffer, out_buffer ); }
    };
    template <>
    struct mover<support_level::na> { constexpr mover( void const * ) noexcept {} };

    template <typename ActualFunctor, typename StoredFunctor, typename FunctorManager> class functor_type_info;
    template <bool enabled>
    struct reflector
    {
        template <typename Manager, typename ActualFunctor, typename StoredFunctor>
        constexpr reflector( std::tuple<Manager, ActualFunctor, StoredFunctor> const * ) noexcept : get_typed_functor( &functor_type_info<ActualFunctor, StoredFunctor, Manager>::get_typed_functor ) {}
        typed_functor (BF_FASTCALL * const get_typed_functor)( function_buffer_base const & ) noexcept;
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

    /// \note See the above note for BOOST_AUX_NOEXCEPT_PTR.
    ///                                       (14.10.2016.) (Domagoj Saric)
#if BOOST_WORKAROUND( BOOST_MSVC, BOOST_TESTED_AT( 1900 ) )
    template <typename ReturnType, typename ... InvokerArguments>
    struct invoker<true, ReturnType, InvokerArguments...>
    {
        template <typename Manager, typename StoredFunctor> constexpr invoker( Manager const *, StoredFunctor const * ) noexcept : invoke( &invoke_impl<Manager, StoredFunctor> ) {}
        ReturnType (BF_FASTCALL * const invoke)( function_buffer_base & buffer, InvokerArguments... args ) noexcept;

        template <typename FunctionObjManager, typename FunctionObj>
		static ReturnType BF_FASTCALL invoke_impl( detail::function_buffer_base & buffer, InvokerArguments... args ) noexcept
		{
			auto & __restrict functionObject( *static_cast<FunctionObj *>( static_cast<void *>( FunctionObjManager::functor_ptr( buffer ) ) ) );
			return functionObject( args... );
		}
    };
    template <>
    struct destroyer<support_level::nofail>
    {
        template <typename Manager> constexpr destroyer( Manager const * ) noexcept : destroy( &Manager::destroy ) {}
        void (BF_FASTCALL * const destroy)( function_buffer_base &  __restrict buffer ) noexcept;
    };
    template <>
    struct cloner<support_level::nofail>
    {
        template <typename Manager> constexpr cloner( Manager const * ) noexcept : clone( &Manager::clone ) {}
        void (BF_FASTCALL * const clone)( function_buffer_base &  __restrict buffer ) noexcept;
    };
    template <>
    struct mover<support_level::nofail>
    {
        template <typename Manager> constexpr mover( Manager const * ) noexcept : move( &Manager::move ) {}
        void (BF_FASTCALL * const move)( function_buffer_base && __restrict in_buffer, function_buffer_base & __restrict out_buffer ) noexcept;
    };
#endif // MSVC workaround
    #undef BOOST_AUX_NOEXCEPT_PTR

    template <typename Invoker, typename Traits>
    // Implementation note:
    //   To test whether a functionoid instance is empty a simple check whether
    // the current vtable pointer points to the vtable for the current empty
    // handler vtable is not good enough for applications that use DLLs
    // (or equivalents) and pass boost::function<> instances across shared
    // module boundaries. In such circumstances one can create an empty
    // boost::function<> instance in module A, where it will get initialised
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
    struct vtable
        :
        Invoker,
        destroyer    <Traits::destructor          >,
        mover        <Traits::moveable            >,
        cloner       <Traits::copyable            >,
        reflector    <Traits::rtti                >,
        empty_checker<Traits::dll_safe_empty_check>
    {
        template <typename ActualFunctor, typename StoredFunctor, typename Manager>
        constexpr vtable( Manager const * const manager_type, ActualFunctor const * const actual_functor_type, StoredFunctor const * const stored_functor_type, bool const is_empty_handler ) noexcept
            :
            Invoker                                    ( manager_type, stored_functor_type ),
            destroyer    <Traits::destructor          >( manager_type ),
            mover        <Traits::moveable            >( manager_type ),
            cloner       <Traits::copyable            >( manager_type ),
            reflector    <Traits::rtti                >( static_cast<std::tuple<Manager, ActualFunctor, StoredFunctor> const *>( nullptr ) ),
		    empty_checker<Traits::dll_safe_empty_check>( is_empty_handler )
        {}

        bool is_empty_handler_vtable( void const * const p_empty_handler_vtable ) const noexcept { return empty_checker<Traits::dll_safe_empty_check>::is_empty_handler_vtable( this, p_empty_handler_vtable ); }

        // Implementation note:
        //  The "generic typed/void() invoker pointer is also stored here so
        // that it can (more easily) be placed at the beginning of the vtable so
        // that a vtable pointer would actually point directly to it (thus
        // avoiding pointer offset calculation on invocation).
        //  This also gives a unique/non-template vtable that can be held by
        // function_base entirely but it also opens a window for erroneous vtable
        // copying/assignment between different boost::function<> instantiations.
        // A typed wrapper should therefor be added to the boost::function<>
        // class to catch such errors at compile-time.
        //                                      (xx.xx.2009.) (Domagoj Saric)
        using base_vtable = vtable<invoker<true, void>, Traits>;
        operator base_vtable const & () const noexcept { return reinterpret_cast<base_vtable const &>( *this ); }
    }; // struct vtable

#if BOOST_WORKAROUND( BOOST_MSVC, BOOST_TESTED_AT( 1900 ) )
    template <class Invoker, class Manager, class ActualFunctor, class StoredFunctor, class IsEmptyHandler, typename Traits>
    struct vtable_holder
    {
        static constexpr Invoker       const * invoker_type        = nullptr;
        static constexpr Manager       const * manager_type        = nullptr;
        static constexpr ActualFunctor const * actual_functor_type = nullptr;
        static constexpr StoredFunctor const * stored_functor_type = nullptr;
        static constexpr vtable<Invoker, Traits> const stored_vtable { manager_type, actual_functor_type, stored_functor_type, IsEmptyHandler::value };
    };
#endif // MSVC workaround

    template <typename T>
    T get_default_value( std::false_type /*not a reference type*/ ) { return {}; }

    template <>
    inline void get_default_value<void>( std::false_type /*not a reference type*/ ) {}

    template <typename T>
    T get_default_value( std::true_type /*a reference type*/ )
    {
        using actual_type_t = typename std::remove_reference<T>::type;
        static T invalid_reference( *static_cast<actual_type_t *>( 0 ) );
        return invalid_reference;
    }

	////////////////////////////////////////////////////////////////////////////
    struct callable_tag {};

	template <typename Traits>
	class function_base : public callable_tag
	{
    public:
        /// Retrieve the type of the stored function object.
        core::typeinfo const & target_type() const
        {
            return get_vtable().get_typed_functor( this->functor_ ).functor_type_info();
        }

        template <typename Functor> Functor       * target()       noexcept { return get_vtable().get_typed_functor( this->functor_ ). template target<Functor>(); }
        template <typename Functor> Functor const * target() const noexcept { return const_cast<function_base &>( *this ).target<Functor const>(); }

        template <typename F>
        bool contains( F & f ) const noexcept
        {
            auto const p_f( target<F>() );
            return p_f && function_equal( *p_f, f );
        }

    protected:
        using buffer = function_buffer<Traits::sbo_size, Traits::sbo_alignment>;
		using vtable = vtable<invoker<true, void>, Traits>;

	private: // Private helper guard classes.
		// This needs to be a template only to support stateful empty handlers.
		template <class EmptyHandler>
		class cleaner
		{
		public:
			cleaner( function_base & function, vtable const & empty_handler_vtable )
				:
				p_function_          ( &function            ),
				empty_handler_vtable_( empty_handler_vtable )
			{}
			cleaner(cleaner const &) = delete;
			~cleaner() { conditional_clear( p_function_ != 0 ); }

			void cancel() { BOOST_ASSERT( p_function_ ); p_function_ = 0; }

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
						empty_handler_traits::allowsSmallObjectOptimization, ""
					);
					empty_handler_manager::assign( EmptyHandler(), p_function_->functor_, std::allocator<EmptyHandler>() );
					p_function_->p_vtable_ = &empty_handler_vtable_;
				}
			}

		private:
			function_base       * p_function_;
			vtable        const & empty_handler_vtable_;
		}; // class cleaner

	protected:
		function_base() { debug_clear( *this ); }
		function_base( function_base const & other, vtable const & empty_handler_vtable )
		{
			debug_clear( *this );
			assign_functionoid_direct( other, empty_handler_vtable );
		}

		function_base( function_base && other, vtable const & empty_handler_vtable ) noexcept
		{
			debug_clear( *this );
			assign_functionoid_direct( std::move( other ), empty_handler_vtable );
		}

		template <class EmptyHandler>
		function_base( vtable const & empty_handler_vtable, EmptyHandler )
		{
			debug_clear( *this );
			this->clear<true, EmptyHandler>( empty_handler_vtable );
		}

		// See the note for the no_eh_state_construction_trick() helper in
		// function_template.hpp to see the purpose of this constructor.
		function_base( vtable const & vtable ) { BF_ASSUME( &vtable == p_vtable_ ); }

		~function_base() { destroy(); }

		template <class EmptyHandler>
		void swap( function_base & other, vtable const & empty_handler_vtable );

	protected:
        bool empty( void const * const p_empty_handler_vtable ) const noexcept { return get_vtable().is_empty_handler_vtable( p_empty_handler_vtable ); }

		vtable const & get_vtable() const noexcept { BF_ASSUME( p_vtable_ ); return *p_vtable_; }

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
			FunctionObj  && f,
			vtable const &  functor_vtable,
			vtable const &  empty_handler_vtable,
			Allocator,
			std::true_type /*assignment of an instance of the same callable instantiation*/
		)
		{
			BOOST_ASSERT( &functor_vtable == f.p_vtable_ );
			ignore_unused( functor_vtable );
			if ( direct )
			{
				BOOST_ASSERT( &static_cast<function_base const &>( f ) != this );
				BOOST_ASSERT( this->p_vtable_ == &empty_handler_vtable );
				assign_functionoid_direct( std::forward<FunctionObj>( f ), empty_handler_vtable );
			}
			else if ( &static_cast<function_base const &>( f ) != this )
			{
				assign_functionoid_guarded<EmptyHandler>( std::forward<FunctionObj>( f ), empty_handler_vtable );
			}
		}

		// General actual assignment.
		template <bool direct, typename EmptyHandler, typename FunctionObj, typename Allocator>
		void assign
		(
			FunctionObj  && f,
			vtable const &  functor_vtable,
			vtable const &  empty_handler_vtable,
			Allocator,
			std::false_type /*generic assign*/
		);

		template <typename EmptyHandler, typename F, typename Allocator>
		void actual_assign
		(
			F            &&       f,
			vtable const &        functor_vtable,
			vtable const &        /*empty_handler_vtable*/,
			Allocator       const a,
			std::true_type /*can use direct assign*/
		) noexcept
		{
			using functor_manager = functor_manager<F, Allocator, buffer>;
			this->destroy();
			functor_manager::assign(std::forward<F>(f), this->functor_, a);
			this->p_vtable_ = &functor_vtable;
		}

		template <typename EmptyHandler, typename F, typename Allocator>
		void actual_assign
		(
			F            &&       f,
			vtable const &        functor_vtable,
			vtable const &        empty_handler_vtable,
			Allocator       const a,
			std::false_type /*must use safe assignment*/
		)
		{
			// This most generic case needs to be reworked [currently does redundant
			// copying (through the vtable function pointers) and does not use all
			// the type information it could...]...
			using functor_manager = functor_manager<F, Allocator, buffer>;
			function_base tmp( empty_handler_vtable, EmptyHandler() );
			functor_manager::assign( std::forward<F>( f ), tmp.functor_, a );
			tmp.p_vtable_ = &functor_vtable;
			this->swap<EmptyHandler>( tmp, empty_handler_vtable );
		}

	private: // Assignment from another functionoid helpers.
		void assign_functionoid_direct( function_base const & source, vtable const & /*empty_handler_vtable*/ ) noexcept
		{
			source.get_vtable().clone( source.functor_, this->functor_ );
			p_vtable_ = &source.get_vtable();
		}

		void assign_functionoid_direct( function_base && source, vtable const & empty_handler_vtable ) noexcept
		{
			source.get_vtable().move( std::move( source.functor_ ), this->functor_ );
			this ->p_vtable_ = &source.get_vtable();
			source.p_vtable_ = &empty_handler_vtable;
		}

		template <typename EmptyHandler, typename FunctionBaseRef>
		void assign_functionoid_guarded( FunctionBaseRef && source, vtable const & empty_handler_vtable )
		{
			this->destroy();
			cleaner<EmptyHandler> guard( *this, empty_handler_vtable );
			assign_functionoid_direct( std::forward<FunctionBaseRef>( source ), empty_handler_vtable );
			guard.cancel();
		}

		void destroy() noexcept { get_vtable().destroy( this->functor_ ); }

	private:
		class safe_mover_base;
		template <class EmptyHandler> class safe_mover;

				vtable const * p_vtable_;
		mutable buffer         functor_ ;
	}; // class function_base

	template <typename T>
    BOOST_FORCEINLINE bool has_empty_target( T * const funcPtr, function_ptr_tag ) { return funcPtr == 0; }

    template <typename T>
    BOOST_FORCEINLINE bool has_empty_target_aux( T * const funcPtr, member_ptr_tag ) { return has_empty_target<T>( funcPtr, function_ptr_tag() ); }

	template <class Traits>
    BOOST_FORCEINLINE bool has_empty_target_aux( function_base<Traits> const * const f, function_obj_tag ) { BF_ASSUME( f != nullptr ); return f->empty(); }

    // Some compilers seem unable to inline even trivial vararg functions
    // (e.g. MSVC).
    //inline bool has_empty_target_aux(...)
    BOOST_FORCEINLINE bool has_empty_target_aux( void const * /*f*/, function_obj_tag ) { return false; }

    template <typename T>
    BOOST_FORCEINLINE bool has_empty_target( T const & f, function_obj_tag ) { return has_empty_target_aux( std::addressof( f ), function_obj_tag() ); }

    template <class FunctionObj>
    BOOST_FORCEINLINE bool has_empty_target( std::reference_wrapper<FunctionObj> const & f, function_obj_ref_tag )
    {
        // Implementation note:
        // We save/assign a reference to a boost::function even if it is
        // empty and let the referenced function handle a possible empty
        // invocation.
        //                                    (28.10.2010.) (Domagoj Saric)
        return std::is_base_of<callable_tag, FunctionObj>::value
            ? false
            : has_empty_target( f.get(), function_obj_tag() );
    }

    template <class FunctionObj>
    BOOST_FORCEINLINE bool has_empty_target( boost::reference_wrapper<FunctionObj> const & f, function_obj_ref_tag )
    {
        return has_empty_target( std::cref( f.get() ), function_obj_ref_tag() );
    }

	template <typename Traits>
	class function_base<Traits>::safe_mover_base
	{
	protected:
		using functor = typename function_base<Traits>::buffer;
		using vtable  = typename function_base<Traits>::vtable;

	protected:
		safe_mover_base(safe_mover_base const&) = delete;
		~safe_mover_base() = default;

	public:
		safe_mover_base( function_base & functionToGuard, function_base & empty_function_to_move_to )
			:
			p_function_to_restore_to_ ( &functionToGuard                       ),
			empty_function_to_move_to_( empty_function_to_move_to              ),
			empty_handler_vtable_     ( empty_function_to_move_to.get_vtable() )
		{
			BOOST_ASSERT( empty_function_to_move_to_.p_vtable_ == &empty_handler_vtable_ );
			move( functionToGuard, empty_function_to_move_to_, empty_handler_vtable_ );
		}

	public:
		void cancel() { BOOST_ASSERT( p_function_to_restore_to_ ); p_function_to_restore_to_ = 0; }

		static void move( function_base & source, function_base & destination, vtable const & empty_handler_vtable )
		{
			source.get_vtable().move( std::move( source.functor_ ), destination.functor_ );
			destination.p_vtable_ = source.p_vtable_;
			source     .p_vtable_ = &empty_handler_vtable;
		}

	protected:
		function_base * __restrict p_function_to_restore_to_ ;
		function_base & __restrict empty_function_to_move_to_;
		vtable const  & __restrict empty_handler_vtable_     ;
	}; // safe_mover_base

	// ...if the is_stateless<EmptyHandler> requirement sticks this will not need
	// to be a template...
	template <class Traits>
	template <class EmptyHandler>
	class function_base<Traits>::safe_mover : public safe_mover_base
	{
	public:
		using safe_mover_base::safe_mover_base;
		~safe_mover()
		{
			if ( this->p_function_to_restore_to_ )
			{
				cleaner<EmptyHandler> guard( *this->p_function_to_restore_to_, this->empty_handler_vtable_ );
				move( this->empty_function_to_move_to_, *this->p_function_to_restore_to_, this->empty_handler_vtable_ );
				guard.cancel();
			}
		}
	}; // class safe_mover

	template <class Traits>
	template <class EmptyHandler>
	void function_base<Traits>::swap( function_base & other, vtable const & empty_handler_vtable )
	{
		if ( &other == this )
			return;

		function_base tmp( empty_handler_vtable, EmptyHandler() );

		safe_mover<EmptyHandler> my_restorer   ( *this, tmp   );
		safe_mover<EmptyHandler> other_restorer( other, *this );

		safe_mover_base::move( tmp, other, empty_handler_vtable );

		my_restorer   .cancel();
		other_restorer.cancel();
    } // void function_base::swap()

	template <typename Traits>
	template <bool direct, typename EmptyHandler, typename F, typename Allocator>
	void function_base<Traits>::assign
	(
		F            &&       f,
		vtable const &        functor_vtable,
		vtable const &        empty_handler_vtable,
		Allocator       const a,
		std::false_type /*generic assign*/
	)
	{
		using namespace detail;

		using tag = typename get_function_tag<F>::type;
		if ( has_empty_target( f, tag() ) )
			this->clear<direct, EmptyHandler>( empty_handler_vtable );
		else
		if ( direct )
		{
            // Implementation note:
            //   See the note for the no_eh_state_construction_trick() helper in
            // function_template.hpp as to why a null vtable is allowed and
            // expected here.
            //                                (02.11.2010.) (Domagoj Saric)
			BOOST_ASSERT( this->p_vtable_ == nullptr );
			using functor_manager = functor_manager<F, Allocator, buffer>;
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
				(
					std::is_nothrow_copy_constructible<F>::value ||
					std::is_trivially_copy_constructible<F>::value
				)
			>;

			actual_assign<EmptyHandler>
			(
				std::forward<F>( f ),
				functor_vtable,
				empty_handler_vtable,
				a,
				has_no_fail_assignement_t()
			);
		}
	} // void function_base::assign()
} // namespace Detail


///////////////////////////////////////////////////////////////////////////
template<typename ReturnType, typename ... Arguments, typename Traits>
class callable<ReturnType(Arguments ...), Traits>
	: public detail::function_base<Traits>
{
private:
    using function_base = detail::function_base<Traits>;
	using buffer        = typename function_base::buffer;

public: // Public typedefs/introspection section.
	using result_type = ReturnType;

    using signature_type = result_type ( Arguments... );

	using empty_handler = typename Traits::empty_handler;

	static constexpr std::uint8_t arity = sizeof...( Arguments );

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

    using base_vtable = typename detail::function_base<Traits>::vtable;
    using vtable_type = detail::vtable<detail::invoker<Traits::is_noexcept, ReturnType, Arguments...>, Traits>;

public: // Public function interface.

    callable() noexcept : function_base( empty_handler_vtable(), empty_handler() ) {}

    template <typename Functor>
    callable( Functor && f )
        : function_base( no_eh_state_construction_trick( std::forward<Functor>( f ) ) ) {}

    template <typename Functor, typename Allocator>
    callable( Functor && f, Allocator const a )
        : function_base( no_eh_state_construction_trick( std::forward<Functor>( f ), a ) ) {}

    callable( signature_type * const plain_function_pointer ) noexcept
        : function_base( no_eh_state_construction_trick( plain_function_pointer ) ) {}

    callable( callable const & f )
        : function_base( static_cast<function_base const &>( f ), empty_handler_vtable() ) { static_assert( Traits::copyable > support_level::na, "This callable instantiation is not copyable." ); }

	callable( callable && f ) noexcept
		: function_base( static_cast<function_base&&>( f ), empty_handler_vtable() ) {}

	result_type BF_FASTCALL operator()( Arguments... args ) const noexcept( Traits::is_noexcept )
	{
        return vtable().invoke( this->functor(), args... );
	}

    callable & operator=( callable const & f ) { { static_assert( Traits::copyable, "This callable instantiation is not copyable." ); } this->assign( f ); return *this; }
	callable & operator=( callable && f ) noexcept { this->assign( std::move( f ) ); return *this; }
    callable & operator=( signature_type * const plain_function_pointer ) noexcept { this->assign( plain_function_pointer ); return *this; }
    template <typename F>
    callable & operator=( F && f ) noexcept { this->assign( std::forward<F>( f ) ); return *this; }

    template <typename F>
    void assign( F && f                    ) { this->do_assign<false>( std::forward<F>( f )    ); }

    template <typename F, typename Allocator>
    void assign( F && f, Allocator const a ) { this->do_assign<false>( std::forward<F>( f ), a ); }

    /// Clear out a target (replace it with an empty handler), if there is one.
    void clear() { function_base:: template clear<false, empty_handler>( empty_handler_vtable() ); }

    /// Determine if the function is empty (i.e. has an empty target).
    bool empty() const noexcept { return function_base::empty( &empty_handler_vtable() ); }

    void swap( callable & other ) noexcept
    {
        static_assert( sizeof( callable ) == sizeof( function_base ), "Internal inconsistency" );
        return function_base:: template swap<empty_handler>( other, empty_handler_vtable() );
    }

    explicit operator bool() const noexcept { return !this->empty(); }

private:
    static auto const & empty_handler_vtable() noexcept { return vtable_for_functor<std::allocator<empty_handler>, empty_handler>( my_empty_handler() ); }

    auto const & vtable() const noexcept { return reinterpret_cast<vtable_type const &>( function_base::get_vtable() ); }

    //  This overload should not actually be for a 'complete' callable as it is enough
	// for the signature template parameter to be the same (and therefor the vtable is the same, with
	// a possible exception being the case of an empty source as empty handler vtables depend on the
	// policy as well as the signature).
    template <typename Allocator, typename ActualFunctor>
    static vtable_type const & vtable_for_functor_aux( std::true_type /*is a callable*/, callable const & functor )
    {
        static_assert( std::is_base_of<callable, typename std::remove_reference<ActualFunctor>::type>::value, "" );
        return functor.get_vtable();
    }

    template <typename Allocator, typename ActualFunctor, typename StoredFunctor>
    static vtable_type const & vtable_for_functor_aux( std::false_type /*is not a callable*/, StoredFunctor const & /*functor*/ )
    {
        using namespace detail;

        // For the empty handler we use the manager for the base_empty_handler not
        // my_empty_handler (anti-code-bloat) because they only differ in the
        // operator() member function which is irrelevant for/not used by the
        // manager.
        using is_empty_handler = std::is_same<ActualFunctor, empty_handler>;
        using manager_type = functor_manager
        <
            typename std::conditional
            <
                is_empty_handler::value,
                ActualFunctor,
                StoredFunctor
            >::type,
            Allocator,
            buffer
        >;

        // A minimally typed manager is used for the invoker (anti-code-bloat).
        using invoker_manager_type = functor_manager<StoredFunctor, Allocator, buffer>;

        static_assert
        (
            std::is_same<ActualFunctor, empty_handler>::value
                ==
            std::is_same<StoredFunctor, my_empty_handler>::value, ""
        );

        using invoker_type = invoker<Traits::is_noexcept, ReturnType, Arguments...>;

#if BOOST_WORKAROUND( BOOST_MSVC, BOOST_TESTED_AT( 1900 ) )
        return vtable_holder<invoker_type, manager_type, ActualFunctor, StoredFunctor, is_empty_handler, Traits>::stored_vtable;
#else
        // http://stackoverflow.com/questions/24398102/constexpr-and-initialization-of-a-static-const-void-pointer-with-reinterpret-cas
        // 
        // Note: it is extremely important that this initialization uses
        // static initialization. Otherwise, we will have a race
        // condition here in multi-threaded code. See
        // http://thread.gmane.org/gmane.comp.lib.boost.devel/164902.
        static constexpr detail::vtable<invoker_type, Traits> const the_vtable
        (
            static_cast<manager_type  const *>( nullptr ),
            static_cast<ActualFunctor const *>( nullptr ),
            static_cast<StoredFunctor const *>( nullptr ),
            is_empty_handler::value
        );
        return the_vtable;
#endif
    } // vtable_for_functor_aux()

    template <typename Allocator, typename ActualFunctor, typename StoredFunctor>
    static vtable_type const & vtable_for_functor( StoredFunctor const & functor )
    {
        return vtable_for_functor_aux<Allocator, ActualFunctor>
        (
            std::is_base_of<function_base, StoredFunctor>(),
            functor
        );
    }

    // ...direct actually means whether to skip pre-destruction (when not
    // assigning but constructing) so it should probably be renamed to
    // pre_destroy or the whole thing solved in some smarter way...
    template <bool direct, typename F, typename Allocator>
    void do_assign( F && f, Allocator const a )
    {
        using tag = typename detail::get_function_tag<F>::type;
        dispatch_assign<direct>( std::forward<F>( f ), a, tag() );
    }

    template <bool direct, typename F>
    void do_assign( F && f )
    {
        using functor_type = typename std::remove_reference<F>::type;
    #ifdef __clang__ // Xcode8 Apple Clang dubious compiler error workaround.
        struct allocator : Traits:: template allocator<functor_type> { using Traits:: template allocator<functor_type>::allocator; };
    #else
        using allocator = Traits:: template allocator<functor_type>;
    #endif // __clang__
        do_assign<direct>( std::forward<F>( f ), allocator() );
    }

    template <bool direct, typename F, typename Allocator>
    void dispatch_assign( F && f, Allocator const a, detail::function_obj_tag ) { do_assign<direct>( std::forward<F>( f ), std::forward<F>( f ), a ); }
    // Explicit support for member function objects, so we invoke through
    // mem_fn() but retain the right target_type() values.
    template <bool direct, typename F, typename Allocator>
    void dispatch_assign( F const f, Allocator const a, detail::member_ptr_tag       ) { do_assign<direct                  >( f      , mem_fn( f ), a ); }
    template <bool direct, typename F, typename Allocator>
    void dispatch_assign( F const f, Allocator const a, detail::function_obj_ref_tag ) { do_assign<direct, typename F::type>( f.get(),         f  , a ); }
    template <bool direct, typename F, typename Allocator>
    void dispatch_assign( F const f, Allocator const a, detail::function_ptr_tag     )
    {
        //   Plain function pointers need special care because when assigned
        // using the syntax without the ampersand they wreck havoc with certain
        // compilers, causing either compilation failures or broken runtime
        // behaviour, e.g. not invoking the assigned target with GCC 4.0.1 or
        // causing access-violation crashes with MSVC (tested 8 and 10).
        using non_const_function_pointer_t = typename std::add_pointer<typename std::remove_const<typename std::remove_pointer<F>::type>::type>::type;
        do_assign<direct, non_const_function_pointer_t, non_const_function_pointer_t>( f, f, a );
    }

    template <bool direct, typename ActualFunctor, typename StoredFunctor, typename ActualFunctorAllocator>
    void do_assign( ActualFunctor const &, StoredFunctor && stored_functor, ActualFunctorAllocator const a )
    {
		using NakedStoredFunctor = typename std::remove_const<typename std::remove_reference<StoredFunctor>::type>::type;
        using StoredFunctorAllocator = typename ActualFunctorAllocator:: template rebind<NakedStoredFunctor>::other;
        function_base:: template assign<direct, empty_handler>
        (
            std::forward<StoredFunctor>( stored_functor ),
            vtable_for_functor<StoredFunctorAllocator, ActualFunctor>( stored_functor ),
            empty_handler_vtable(),
            StoredFunctorAllocator( a ),
            std::is_base_of<callable, NakedStoredFunctor>() /*are we assigning another callable?*/
        );
    }

    template <typename FunctionObj, typename Allocator>
    vtable_type const & no_eh_state_construction_trick( FunctionObj && f, Allocator const a )
    {
		static_assert( Traits::copyable != support_level::na || std::is_copy_constructible<FunctionObj>::value, "This callable instantiation requires copyable function objects." );

        detail::debug_clear( *this );
        do_assign<true>( std::forward<FunctionObj>( f ), a );
        return function_base::get_vtable();
    }

    template <typename FunctionObj>
    vtable_type const & no_eh_state_construction_trick( FunctionObj && f )
    {
		using NakedFunctionObj = typename std::remove_const<typename std::remove_reference<FunctionObj>::type>::type;
		return no_eh_state_construction_trick( std::forward<FunctionObj>( f ), Traits:: template allocator<NakedFunctionObj>() );
    }
}; // class callable


template <typename Signature>
void swap(callable<Signature> & f1, callable<Signature>& f2) { f1.swap(f2); }

// Poison comparisons between callable objects of the same type.
template <typename Signature>
void operator==(callable<Signature> const&, callable<Signature> const&);
template <typename Signature>
void operator!=(callable<Signature> const&, callable<Signature> const&);

//------------------------------------------------------------------------------
} // namespace functionoid
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------