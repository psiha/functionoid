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

// Implementation note:
//   BOOST_FUNCTION_TARGET_FIX is still required by tests.
//                                            (03.11.2010.) (Domagoj Saric)
#if defined(BOOST_MSVC) && BOOST_MSVC <= 1300 || defined(__ICL) && __ICL <= 600 || defined(__MWERKS__) && __MWERKS__ < 0x2406 && !defined(BOOST_STRICT_CONFIG)
#  define BOOST_FUNCTION_TARGET_FIX(x) x
#else
#  define BOOST_FUNCTION_TARGET_FIX(x)
#endif // not MSVC
//------------------------------------------------------------------------------
namespace boost
{
//------------------------------------------------------------------------------

template <typename T> class reference_wrapper;

namespace functionoid
{
//------------------------------------------------------------------------------

struct default_traits;


template <typename Signature, typename Traits = default_traits>
class callable;

namespace detail
{
    using dummy_allocator = std::allocator<void *>;

    struct thiscall_optimization_available_helper
    {
		using free_function  = void (                                        *)();
		using bound_function = void (thiscall_optimization_available_helper::*)();

        static constexpr bool value = ( sizeof( thiscall_optimization_available_helper::free_function ) == sizeof( thiscall_optimization_available_helper::bound_function ) );
    };
	using thiscall_optimization_available = std::bool_constant<thiscall_optimization_available_helper::value>;

    // A buffer used to store small function objects in
    // boost::function. It is a union containing function pointers,
    // object pointers, and a structure that resembles a bound
    // member function pointer.

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
			std::size_t       size;   // in number of bytes
		} trivial_heap_obj;

		// For function pointers of all kinds
		void(*func_ptr)();

		// For bound member pointers
		struct bound_memfunc_ptr_t
		{
			class X;
			void  (X::* memfunc_ptr)();
			void* obj_ptr;
		} bound_memfunc_ptr;
	}; // function_buffer_base

	template <std::uint8_t Size, std::uint8_t Alignment>
	union alignas( Alignment ) function_buffer
	{
		function_buffer_base base;
		char bytes[Size];

			      operator function_buffer_base       && () &&    noexcept { return std::move( base ); }
		          operator function_buffer_base       &  ()       noexcept { return base; }
		constexpr operator function_buffer_base const &  () const noexcept { return base; }
	};

	// Check that all function_buffer "access points" are actually at the same
	// address/offset.
	static_assert( offsetof( function_buffer_base, obj_ptr           ) == offsetof( function_buffer_base, func_ptr ), "" );
	static_assert( offsetof( function_buffer_base, bound_memfunc_ptr ) == offsetof( function_buffer_base, func_ptr ), "" );
	static_assert( offsetof( function_buffer_base::bound_memfunc_ptr_t, memfunc_ptr ) == 0, "" );

    // A simple wrapper to allow deriving and a thiscall invoker.
	template <std::uint8_t Size, std::uint8_t Alignment>
    struct function_buffer_holder
	{
		function_buffer<Size, Alignment> buffer;
	};

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
    struct is_msvc_exception_specified_function_pointer
        :
        public std::bool_constant
        <
        #ifdef _MSC_VER
            !std::is_class      <T>::value &&
            !std::is_fundamental<T>::value &&
            ( sizeof( T ) == sizeof( void (*) (void) ) )
        #else
            false
        #endif
        >
    {};

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

	template <typename F>
	struct get_function_tag<std::reference_wrapper<F>> { using type = function_obj_ref_tag; };
	template <typename F>
	struct get_function_tag<boost::reference_wrapper<F>> { using type = function_obj_ref_tag; };


    template <typename F, typename A>
    struct functor_and_allocator : F, A // enable EBO
    {
		functor_and_allocator( F const & f, A a ) : F( f ), A( a ) {}

		F       & functor  ()       { return *this; }
		F const & functor  () const { return *this; }
		A       & allocator()       { return *this; }
		A const & allocator() const { return *this; }
    };

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
    public:
        static void       *       * functor_ptr( function_buffer_base       & buffer ) {                              return &buffer.obj_ptr; }
        static void const * const * functor_ptr( function_buffer_base const & buffer ) { BF_ASSUME( buffer.obj_ptr ); return &buffer.obj_ptr; }

        template <typename Functor, typename Allocator>
        static void assign( Functor const functor, function_buffer_base & out_buffer, Allocator )
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
    public:
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

        static void BF_FASTCALL clone( Buffer const & __restrict in_buffer, Buffer & __restrict out_buffer ) noexcept
        {
            assign( in_buffer, out_buffer, dummy_allocator() );
        }

        static void BF_FASTCALL move( Buffer && __restrict in_buffer, Buffer & __restrict out_buffer ) noexcept
        {
            clone( in_buffer, out_buffer );
            destroy( in_buffer );
        }

        static void BF_FASTCALL destroy( Buffer & buffer ) noexcept
        {
            debug_clear( buffer );
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
            boost::ignore_unused( a );

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
    public:
        using Functor = FunctorParam;
v
        static Functor       * functor_ptr( function_buffer_base       & buffer ) { return static_cast<Functor *>( manager_trivial_small<Buffer>::functor_ptr( buffer ) ); }
        static Functor const * functor_ptr( function_buffer_base const & buffer ) { return functor_ptr( const_cast<function_buffer_base &>( buffer ) ); }

        template <typename F, typename Allocator>
        static void assign( F && functor, Buffer & out_buffer, Allocator )
        {
            new ( functor_ptr( out_buffer ) ) Functor( std::forward<F>( functor ) );
        }

        static void BF_FASTCALL clone( Buffer const & in_buffer, Buffer & out_buffer )
        {
            Functor const & in_functor( *functor_ptr( in_buffer ) );
            assign( in_functor, out_buffer, dummy_allocator() );
        }

        static void BF_FASTCALL move( Buffer && __restrict in_buffer, Buffer & __restrict out_buffer ) noexcept
        {
            auto & __restrict in_functor( *functor_ptr( in_buffer ) );
            assign( std::move( in_functor ), out_buffer, dummy_allocator() );
            destroy( in_buffer );
        }

        static void BF_FASTCALL destroy( Buffer & buffer ) noexcept
        {
            functor_ptr( buffer )->~Functor();
            debug_clear( *functor_ptr( buffer ) );
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
            using does_not_need_guard_t = std::bool_constant
            <
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

        static void BF_FASTCALL move( function_buffer_base && in_buffer, function_buffer_base & out_buffer ) noexcept
        {
            manager_trivial_heap<OriginalAllocator>::move( std::move( in_buffer ), out_buffer );
        }

        static void BF_FASTCALL destroy( function_buffer_base & buffer ) noexcept
        {
            functor_and_allocator_t & in_functor_and_allocator( *functor_ptr( buffer ) );

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

		static constexpr auto clone_ptr_aux( std::true_type  ) { return &clone; }
		static constexpr auto clone_ptr_aux( std::false_type ) { return nullptr; }
    }; // struct manager_generic

	// Wrapper that adapts the function signatures for the vtable (for
	// managers that unconditionally use function_buffer_base).
	template <typename BaseManager, typename Buffer>
	struct base_buffer_manager_adapter : BaseManager
	{
		static void clone  ( Buffer const &  __restrict in_buffer, Buffer & __restrict out_buffer )          { BaseManager::clone(            in_buffer  , out_buffer ); }
		static void move   ( Buffer       && __restrict in_buffer, Buffer & __restrict out_buffer ) noexcept { BaseManager::move ( std::move( in_buffer ), out_buffer ); }
		static void destroy( Buffer       &  __restrict buffer                                    ) noexcept { BaseManager::destroy( buffer ); }
	};

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
    struct functor_manager
    {
        using type = base_buffer_manager_adapter<manager_generic<Functor, Allocator>, Buffer>;
    };

    template <typename Functor, typename Allocator, typename Buffer>
    struct functor_manager<Functor, Allocator, Buffer, true, false, false, true>
    {
        using type = base_buffer_manager_adapter
		<
			typename std::conditional
			<
				//...zzz...is_stateless<Allocator>,
				std::is_empty<Allocator>::value,
				manager_trivial_heap<         Allocator>,
				manager_generic     <Functor, Allocator>
			>::type,
			Buffer
		>;
    };

    template <typename Functor, typename Allocator, typename Buffer, bool defaultAligned>
    struct functor_manager<Functor, Allocator, Buffer, true, true, false, defaultAligned>
    {
        using type = manager_trivial_small<Buffer>;
    };

    template <typename Functor, typename Allocator, typename Buffer, bool smallObj, bool defaultAligned>
    struct functor_manager<Functor, Allocator, Buffer, true, smallObj, true, defaultAligned>
    {
        using type = base_buffer_manager_adapter<manager_ptr, Buffer>;
    };

    template <typename Functor, typename Allocator, typename Buffer, bool ptrSmall, bool defaultAligned>
    struct functor_manager<Functor, Allocator, Buffer, false, true, ptrSmall, defaultAligned>
    {
        using type = manager_small<Functor, Buffer>;
    };

    /// Metafunction for retrieving an appropriate functor manager with
    /// minimal type information.
    template<typename StoredFunctor, typename Allocator, typename Buffer>
    struct get_functor_manager
    {
        using type = typename functor_manager
        <
            StoredFunctor,
            Allocator,
			Buffer,
            functor_traits<StoredFunctor, Buffer>::allowsPODOptimization,
            functor_traits<StoredFunctor, Buffer>::allowsSmallObjectOptimization,
            functor_traits<StoredFunctor, Buffer>::allowsPtrObjectOptimization,
            functor_traits<StoredFunctor, Buffer>::hasDefaultAlignement
        >::type;
    };
#if 0
    /// Metafunction for retrieving an appropriate fully typed functor manager.
    template<typename ActualFunctor, typename StoredFunctor, typename Allocator, typename Buffer>
    struct get_typed_functor_manager
    {
        using type = typed_manager
        <
            typename get_functor_manager<StoredFunctor, Allocator, Buffer>::type,
            ActualFunctor,
            StoredFunctor
        >;
    };
#endif

    // Implementation note:
    //  The "generic typed/void-void invoker pointer is also stored here so
    // that it can (more easily) be placed at the beginning of the vtable so
    // that a vtable pointer would actually point directly to it (thus
    // avoiding pointer offset calculation on invocation).
    //  This also gives a unique/non-template vtable that can be held by
    // function_base entirely but it also opens a window for erroneous vtable
    // copying/assignment between different boost::function<> instantiations.
    // A typed wrapper should therefor be added to the boost::function<>
    // class to catch such errors at compile-time.
    //                                      (xx.xx.2009.) (Domagoj Saric)
    template <typename Buffer>
    // Implementation note:
    //   To test whether a boost::function<> instance is empty a simple check
    // whether the current vtable pointer points to the vtable for the current
    // empty handler vtable is not good enough for applications that use DLLs
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
    {
		using this_call_invoker_placeholder_type = void (Buffer::*)();
		using free_call_invoker_placeholder_type = void (        *)();
		using invoker_placeholder_type = typename std::conditional
		<
			thiscall_optimization_available::value,
			this_call_invoker_placeholder_type,
			free_call_invoker_placeholder_type
		>::type;

		template<typename TargetInvokerType>
		TargetInvokerType const & invoker() const { return reinterpret_cast<TargetInvokerType const &>( void_invoker ); }

        //void BF_NOALIAS        clone  ( function_buffer const & in_buffer, function_buffer & out_buffer ) const { do_clone( in_buffer, out_buffer ); }
        //void BF_NOALIAS        move   ( function_buffer       & in_buffer, function_buffer & out_buffer ) const { do_move ( in_buffer, out_buffer ); }
        //void BF_NOTHROWNOALIAS destroy( function_buffer       & buffer                                  ) const { do_destroy( buffer );              }

		invoker_placeholder_type const void_invoker;

		void (BF_FASTCALL * const clone  )( Buffer const &  __restrict in_buffer, Buffer & __restrict out_buffer )         ;
		void (BF_FASTCALL &       move   )( Buffer       && __restrict in_buffer, Buffer & __restrict out_buffer ) noexcept;
            //typed_functor ( BF_FASTCALL &  get_typed_functor )( function_buffer const & ) noexcept;
		void (BF_FASTCALL &       destroy)( Buffer       &  __restrict buffer                                    ) noexcept;

		bool const is_empty_handler_vtable;
    }; // struct vtable

    //template <support_level Level>
    //void (BF_FASTCALL & clone)( Buffer const &  __restrict in_buffer, Buffer & __restrict out_buffer ) noexcept( Level >= support_level::nofail );

    template <support_level Level, typename Buffer>
    struct cloner
    {
        void (BF_FASTCALL & clone)( Buffer const &  __restrict in_buffer, Buffer & __restrict out_buffer ) noexcept( Level >= support_level::nofail );
    };
    template <typename Buffer>
    struct cloner<support_level::trivial, Buffer>
    {
        static void BF_FASTCALL clone( Buffer const &  __restrict in_buffer, Buffer & __restrict out_buffer ) noexcept { out_buffer = in_buffer; }
    };

    template <support_level Level, typename Buffer>
    struct mover
    {
        void (BF_FASTCALL & move)( Buffer && __restrict in_buffer, Buffer & __restrict out_buffer ) noexcept( Level >= support_level::nofail );
    };
    template <typename Buffer>
    struct mover<support_level::trivial, Buffer>
    {
        static void BF_FASTCALL move( Buffer && __restrict in_buffer, Buffer & __restrict out_buffer ) noexcept { cloner<support_level::trivial, Buffer>::clone( in_buffer, out_buffer ); }
    };

    template <support_level Level, typename Buffer>
    struct destroyer
    {
        void ( BF_FASTCALL & destroy)( Buffer &  __restrict buffer ) noexcept( Level >= support_level::nofail );
    };
    template <typename Buffer>
    struct destroyer<support_level::trivial, Buffer>
    {
        static void BF_FASTCALL destroy( Buffer & __restrict buffer ) noexcept { debug_clear( buffer ); }
    };

    template <class Invoker, class Manager, class IsEmptyHandler, typename Buffer, typename Copyable>
    struct vtable_holder
    {
		using vtable = vtable<Buffer>;

        static typename vtable::this_call_invoker_placeholder_type get_invoker_pointer( std::true_type /*this call*/ )
        {
            return reinterpret_cast<vtable::this_call_invoker_placeholder_type>( &Invoker::bound_invoke );
        }

        static typename vtable::free_call_invoker_placeholder_type get_invoker_pointer( std::false_type /*free call*/ )
        {
            return reinterpret_cast<vtable::free_call_invoker_placeholder_type>( &Invoker::free_invoke );
        }

        static constexpr auto invoker( support_level_t<support_level::nofail> ) { return &Manager::clone; }
		static constexpr auto invoker( std::false_type ) { return nullptr; }

        static constexpr auto clone( support_level_t<support_level::na       > ) { return struct {}; }
        static constexpr auto clone( support_level_t<support_level::supported> ) { return Manager::clone; }
        static constexpr auto clone( support_level_t<support_level::nofail   > ) { return Manager::clone_noexcept; }
        static constexpr auto clone( support_level_t<support_level::trivial  > ) { return nullptr; }

        //std::tuple<invoker, copier, mover, destructor, rtti, empty_flag> vtable;

        static vtable const stored_vtable;
    };

    // Note: it is extremely important that this initialization uses
    // static initialization. Otherwise, we will have a race
    // condition here in multi-threaded code. See
    // http://thread.gmane.org/gmane.comp.lib.boost.devel/164902.
	template <class Invoker, class Manager, class IsEmptyHandler, typename Buffer, typename Copyable>
    vtable<Buffer> const vtable_holder<Invoker, Manager, IsEmptyHandler, Buffer, Copyable>::stored_vtable =
    {
        get_invoker_pointer( thiscall_optimization_available() ),
        clone_ptr( Copyable() ),
        Manager::move,
        Manager::destroy,
		Manager::get_typed_functor,
		IsEmptyHandler::value
    };

    template <typename T>
    T get_default_value( std::false_type /*not a reference type*/ ) { return {}; }

    template <>
    inline void get_default_value<void>( std::false_type /*not a reference type*/ ) {}

    template <typename T>
    T get_default_value( std::true_type /*a reference type*/ )
    {
        typedef typename std::remove_reference<T>::type actual_type_t;
        static T invalid_reference( *static_cast<actual_type_t *>( 0 ) );
        return invalid_reference;
    }

	///////////////////////////////////////////////////////////////////////
	template <typename Traits>
	class function_base
	{
	private: // Private helper guard classes.
		// ...(definition) to be moved out of body

		// ...if the is_stateless<EmptyHandler> requirement sticks this will not need
		// to be a template...
		template <class EmptyHandler>
		class cleaner
		{
		public:
			cleaner( function_base & function, vtable const & empty_handler_vtable )
				:
				pFunction_           ( &function            ),
				empty_handler_vtable_( empty_handler_vtable )
			{}
			cleaner(cleaner const &) = delete;
			~cleaner() { conditional_clear( pFunction_ != 0 ); }

			void cancel() { BOOST_ASSERT( pFunction_ ); pFunction_ = 0; }

		private:
			void conditional_clear( bool const clear )
			{
				using namespace detail;
				if ( BOOST_UNLIKELY( clear ) )
				{
					BOOST_ASSERT( pFunction_ );
					using empty_handler_traits  =          functor_traits     <EmptyHandler                          , Buffer>;
					using empty_handler_manager = typename get_functor_manager<EmptyHandler, fallocator<EmptyHandler>, Buffer>::type;
					//...zzz..remove completely or replace with a simple is_stateless<>?
					static_assert
					(
						empty_handler_traits::allowsPODOptimization &&
						empty_handler_traits::allowsSmallObjectOptimization, ""
					);
					empty_handler_manager::assign( EmptyHandler(), pFunction_->functor_, fallocator<EmptyHandler>() );
					pFunction_->p_vtable_ = &empty_handler_vtable_;
				}
			}

		private:
			function_base       * pFunction_;
			vtable        const & empty_handler_vtable_;
		}; // class cleaner

	protected:
		using Buffer = function_buffer<Traits::SboSize, Traits::Alignment>;
		using vtable = vtable<Buffer>;

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

	public:
		/// Determine if the function is empty (i.e. has an empty target).
		bool empty() const { return get_vtable().is_empty_handler_vtable; }


	protected:
		vtable const & get_vtable() const { BF_ASSUME( p_vtable_ ); return *p_vtable_; }

		Buffer & functor() const { return functor_; }

		template <bool direct, class EmptyHandler>
		void clear( vtable const & empty_handler_vtable ) noexcept
		{
			// If we hold to the is_stateless<EmptyHandler> requirement a full assign
			// is not necessary here but a simple
			// this->p_vtable_ = &empty_handler_vtable...
			EmptyHandler /*const*/ emptyHandler;
			assign<direct, EmptyHandler>
			(
				emptyHandler,
				empty_handler_vtable,
				empty_handler_vtable, 
				fallocator<EmptyHandler>(),
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
			boost::ignore_unused( functor_vtable );
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
			using functor_manager = typename get_functor_manager<F, Allocator, Buffer>::type;
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
			using functor_manager = typename get_functor_manager<F, Allocator, Buffer>::type;
			function_base tmp(empty_handler_vtable, EmptyHandler());
			functor_manager::assign(std::forward<F>(f), tmp.functor_, a);
			tmp.p_vtable_ = &functor_vtable;
			this->swap<EmptyHandler>(tmp, empty_handler_vtable);
		}

	private: // Assignment from another functionoid helpers.
		void assign_functionoid_direct( function_base const & source, vtable const & /*empty_handler_vtable*/ ) noexcept
		{
			BOOST_ASSERT_MSG( source.get_vtable().clone != nullptr, "Should not get here: functionoid instance not copyable!" );
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
		mutable Buffer         functor_ ;
	}; // class function_base

	template <typename Traits>
	class function_base<Traits>::safe_mover_base
	{
	protected:
		using functor = typename function_base<Traits>::Buffer;
		using vtable = typename function_base<Traits>::vtable;

	protected:
		safe_mover_base(safe_mover_base const&) = delete;
		~safe_mover_base() = default;

	public:
		safe_mover_base( function_base & functionToGuard, function_base & emptyFunctionToMoveTo )
			:
			pFunctionToRestoreTo  ( &functionToGuard                   ),
			emptyFunctionToMoveTo_( emptyFunctionToMoveTo              ),
			empty_handler_vtable_ ( emptyFunctionToMoveTo.get_vtable() )
		{
			BOOST_ASSERT( emptyFunctionToMoveTo.p_vtable_ == &empty_handler_vtable_ );
			move( functionToGuard, emptyFunctionToMoveTo, empty_handler_vtable_ );
		}

	public:
		void cancel() { BOOST_ASSERT( pFunctionToRestoreTo ); pFunctionToRestoreTo = 0; }

		static void move( function_base & source, function_base & destination, vtable const & empty_handler_vtable )
		{
			source.get_vtable().move( std::move( source.functor_ ), destination.functor_ );
			destination.p_vtable_ = source.p_vtable_;
			source     .p_vtable_ = &empty_handler_vtable;
		}

	protected:
		function_base * __restrict pFunctionToRestoreTo  ;
		function_base & __restrict emptyFunctionToMoveTo_;
		vtable const  & __restrict empty_handler_vtable_ ;
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
			if ( pFunctionToRestoreTo )
			{
				cleaner<EmptyHandler> guard( *pFunctionToRestoreTo, empty_handler_vtable_ );
				move( emptyFunctionToMoveTo_, *pFunctionToRestoreTo, empty_handler_vtable_ );
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
	}

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
        //   We save/assign a reference to a boost::function even if it is empty
        // and let the referenced function handle a possible empty invocation.
        //                                    (28.10.2010.) (Domagoj Saric)
        return std::is_base_of<function_base, FunctionObj>::value
            ? false
            : has_empty_target( f.get(), function_obj_tag() );
    }

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
            // function_template.hpp as to why a null vtable is allowed and expected
            // here.
            //                                    (02.11.2010.) (Domagoj Saric)
			BOOST_ASSERT( this->p_vtable_ == nullptr );
			using functor_manager = typename get_functor_manager<F, Allocator, Buffer>::type;
			functor_manager::assign( std::forward<F>( f ), this->functor_, a );
			this->p_vtable_ = &functor_vtable;
		}
		else
		{
			/// \todo This can/should be rewritten because the
			/// small-object-optimization condition is too strict, even heap
			/// allocated targets can be assigned directly because they have a
			/// nothrow swap operation.
	        ///                                   (28.10.2010.) (Domagoj Saric)
			using has_no_fail_assignement_t = std::bool_constant
			<
				functor_traits<F, Buffer>::allowsSmallObjectOptimization &&
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
	} // void function_base::assign(...)
} // namespace Detail


///////////////////////////////////////////////////////////////////////////
template<typename ReturnType, typename ... Arguments, typename Traits>
class callable<ReturnType(Arguments ...), Traits>
	: public detail::function_base<Traits>
{
private:
	using Buffer = typename detail::function_base<Traits>::Buffer;

	template <typename FunctionObj, typename FunctionObjManager>
	struct function_obj_invoker : detail::function_buffer_holder<Traits::SboSize, Traits::Alignment>
	{
		//   The buffer argument comes last so that the stack layout in the
		// invoker would be as similar as possible to the one expected by the
		// target (with the assumption of a cdecl-like right-to-left argument
		// order).
		static ReturnType free_invoke( Arguments... args, detail::function_buffer_base & buffer )
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
			return functionObject( args... );
		}

		BOOST_FORCEINLINE ReturnType bound_invoke( Arguments... args ) { return free_invoke( args..., buffer ); }
	};

public: // Public typedefs/introspection section.
	using result_type = ReturnType;

    using signature_type = result_type ( Arguments... );

	using empty_handler = typename Traits::EmptyHandler;

	static constexpr std::uint8_t arity = sizeof...( Arguments );

private: // Private implementation types.
    //  We need a specific thin wrapper around the base empty handler that will
    // just consume all the parameters. This way the base empty handler can have
    // one plain simple operator(). As part of anti-code-bloat measures,
    // my_empty_handler is used only when really necessary (with the invoker),
    // otherwise the base_empty_handler type is used.
    struct my_empty_handler : empty_handler
    {
        result_type operator()( Arguments... ) const
        {
            return empty_handler:: template handle_empty_invoke<result_type>();
        }
    };

    using vtable_type = typename detail::function_base<Traits>::vtable;

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
        : function_base( static_cast<function_base const &>( f ), empty_handler_vtable() ) { static_assert( Traits::CopyConstructible, "This callable instantiation is not copyable." ); }

	callable( callable && f ) noexcept
		: function_base( static_cast<function_base&&>( f ), empty_handler_vtable() ) {}

	result_type operator()(Arguments...args) const
	{
		return do_invoke(args ..., detail::thiscall_optimization_available());
	}

    /// Clear out a target (replace it with an empty handler), if there is one.
    void clear() { function_base::clear<false, empty_handler>( empty_handler_vtable() ); }

    template <typename F>
    void assign( F && f                    ) { this->do_assign<false>( std::forward<F>( f )    ); }

    template <typename F, typename Allocator>
    void assign( F && f, Allocator const a ) { this->do_assign<false>( std::forward<F>( f ), a ); }

    callable & operator=( callable const & f ) { { static_assert( Traits::CopyConstructible, "This callable instantiation is not copyable." ); } this->assign( f ); return *this; }
	callable & operator=( callable && f ) noexcept { this->assign( std::move( f ) ); return *this; }
    callable & operator=( signature_type * const plain_function_pointer ) noexcept { this->assign( plain_function_pointer ); return *this; }

    void swap( callable & other ) noexcept
    {
        static_assert( sizeof( callable ) == sizeof( function_base ), "" );
        return function_base::swap<empty_handler>( other, empty_handler_vtable() );
    }

    explicit operator bool() const noexcept { return !this->empty(); }

private:
    static vtable_type const & empty_handler_vtable() { return vtable_for_functor<detail::fallocator<empty_handler>, empty_handler>( my_empty_handler() ); }

    BOOST_FORCEINLINE
    result_type do_invoke( Arguments... args, std::true_type /*this call*/ ) const
    {
        using invoker_type = result_type (Buffer::*)(Arguments...);
        return (functor().*(get_vtable(). template invoker<invoker_type>()))(args...);
    }

    BOOST_FORCEINLINE
    result_type do_invoke( Arguments... args, std::false_type /*free call*/ ) const
    {
        using invoker_type = result_type (*)(Arguments..., Buffer &);
        return get_vtable(). template invoker<invoker_type>()( args..., functor() );
    }

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

      // A minimally typed manager is used for the invoker (anti-code-bloat).
      using invoker_manager_type = typename get_functor_manager<StoredFunctor, Allocator, Buffer>::type;

      // For the empty handler we use the manager for the base_empty_handler not
      // my_empty_handler (anti-code-bloat) because they only differ in the
      // operator() member function which is irrelevant for/not used by the
      // manager.
      using manager_type = typename get_functor_manager // get_typed_functor_manager
              <
                ActualFunctor,
                typename std::conditional
                <
                  std::is_same<ActualFunctor, empty_handler>::value,
                  ActualFunctor,
                  StoredFunctor
                >::type,
                Allocator,
				Buffer
              >::type;

	  using invoker_type = function_obj_invoker<StoredFunctor, invoker_manager_type>;

      static_assert
      (
        std::is_same<ActualFunctor, empty_handler>::value
            ==
        std::is_same<StoredFunctor, my_empty_handler  >::value, ""
      );
      using is_empty_handler = std::is_same<ActualFunctor, empty_handler>;
	  using is_copyable = std::bool_constant<Traits::CopyConstructible>;
      return vtable_holder<invoker_type, manager_type, is_empty_handler, Buffer, is_copyable>::stored_vtable;
    } // vtable_for_functor_aux()

    template <typename Allocator, typename ActualFunctor, typename StoredFunctor>
    static vtable_type const & vtable_for_functor( StoredFunctor const & functor )
    {
        return vtable_for_functor_aux<Allocator, ActualFunctor>
        (
            std::is_base_of<callable, StoredFunctor>(),
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
    void do_assign( F && f ) { do_assign<direct>( std::forward<F>( f ), Traits:: template Allocator<F>() ); }

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
        function_base::assign<direct, empty_handler>
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
		static_assert( !Traits::CopyConstructible || std::is_copy_constructible<FunctionObj>::value, "This callable instantiation requires copyable function objects." );

        detail::debug_clear( *this );
        do_assign<true>( std::forward<FunctionObj>( f ), a );
        return function_base::get_vtable();
    }

    template <typename FunctionObj>
    vtable_type const & no_eh_state_construction_trick( FunctionObj && f )
    {
		using NakedFunctionObj = typename std::remove_const<typename std::remove_reference<FunctionObj>::type>::type;
		return no_eh_state_construction_trick( std::forward<FunctionObj>( f ), Traits:: template Allocator<NakedFunctionObj>() );
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