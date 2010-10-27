// Boost.Function library

//  Copyright Douglas Gregor 2001-2006
//  Copyright Emil Dotchevski 2007
//  Use, modification and distribution is subject to the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

// For more information, see http://www.boost.org

#ifndef BOOST_FUNCTION_BASE_HEADER
#define BOOST_FUNCTION_BASE_HEADER

#include "detail/platform_specifics.hpp"
#include "detail/safe_bool.hpp"

#include <stdexcept>
#include <stddef.h>
#include <string>
#include <memory>
#include <new>
#ifndef BOOST_FUNCTION_NO_RTTI
    #include <boost/detail/sp_typeinfo.hpp>
#endif // BOOST_FUNCTION_NO_RTTI
#include <boost/config.hpp>
#include <boost/assert.hpp>
#include <boost/integer.hpp>

#include <boost/aligned_storage.hpp>
#include <boost/concept_check.hpp>
#include <boost/noncopyable.hpp>
#include <boost/type_traits/add_const.hpp>
#include <boost/type_traits/add_reference.hpp>
#include <boost/type_traits/has_nothrow_copy.hpp>
#include <boost/type_traits/has_trivial_copy.hpp>
#include <boost/type_traits/has_trivial_destructor.hpp>
#include <boost/type_traits/is_base_of.hpp>
#include <boost/type_traits/is_class.hpp>
#include <boost/type_traits/is_const.hpp>
#include <boost/type_traits/is_function.hpp>
#include <boost/type_traits/is_fundamental.hpp>
#include <boost/type_traits/is_integral.hpp>
#include <boost/type_traits/is_member_pointer.hpp>
#include <boost/type_traits/is_object.hpp>
#include <boost/type_traits/is_stateless.hpp>
#include <boost/type_traits/is_volatile.hpp>
#include <boost/type_traits/composite_traits.hpp>
#include <boost/type_traits/ice.hpp>
#include <boost/type_traits/remove_const.hpp>
#include <boost/type_traits/remove_pointer.hpp>
#include <boost/ref.hpp>
#include <boost/static_assert.hpp>
#include <boost/mpl/at.hpp>
#include <boost/mpl/eval_if.hpp>
#include <boost/mpl/identity.hpp>
#include <boost/mpl/if.hpp>
#include <boost/detail/workaround.hpp>
#include <boost/type_traits/alignment_of.hpp>
#ifndef BOOST_NO_SFINAE
#  include "boost/utility/enable_if.hpp"
#else
#  include "boost/mpl/bool.hpp"
#endif
#include <boost/function_equal.hpp>
#include <boost/function/function_fwd.hpp>

#if defined(BOOST_MSVC)
#   pragma warning( push )
#   pragma warning( disable : 4127 ) // "conditional expression is constant"
#   pragma warning( disable : 4510 ) // "default constructor could not be generated" (boost::detail::function::vtable)
#   pragma warning( disable : 4512 ) // "assignment operator could not be generated" (boost::detail::function::vtable)
#   pragma warning( disable : 4610 ) // "class can never be instantiated - user defined constructor required" (boost::detail::function::vtable)
#   pragma warning( disable : 4793 ) // complaint about native code generation
#endif       

#ifndef BOOST_NO_TYPEID
// Define BOOST_FUNCTION_STD_NS to the namespace that contains type_info.
#if defined( BOOST_NO_STD_TYPEINFO ) || ( defined( BOOST_MSVC ) && !_HAS_EXCEPTIONS )
// Embedded VC++ does not have type_info in namespace std
#  define BOOST_FUNCTION_STD_NS
#else
#  define BOOST_FUNCTION_STD_NS std
#endif
#endif // BOOST_NO_TYPEID


// Borrowed from Boost.Python library: determines the cases where we
// need to use std::type_info::name to compare instead of operator==.
#if defined( BOOST_NO_TYPEID )
#  define BOOST_FUNCTION_COMPARE_TYPE_ID(X,Y) ((X)==(Y))
#elif (defined(__GNUC__) && __GNUC__ >= 3) \
 || defined(_AIX) \
 || (   defined(__sgi) && defined(__host_mips))
#  include <cstring>
#  define BOOST_FUNCTION_COMPARE_TYPE_ID(X,Y) \
     (std::strcmp((X).name(),(Y).name()) == 0)
# else
#  define BOOST_FUNCTION_COMPARE_TYPE_ID(X,Y) ((X)==(Y))
#endif


#if defined(BOOST_MSVC) && BOOST_MSVC <= 1300 || defined(__ICL) && __ICL <= 600 || defined(__MWERKS__) && __MWERKS__ < 0x2406 && !defined(BOOST_STRICT_CONFIG)
#  define BOOST_FUNCTION_TARGET_FIX(x) x
#else
#  define BOOST_FUNCTION_TARGET_FIX(x)
#endif // not MSVC

#if !BOOST_WORKAROUND(__BORLANDC__, < 0x5A0)
#  define BOOST_FUNCTION_ENABLE_IF_NOT_INTEGRAL(Functor,Type)              \
      typename ::boost::enable_if_c<(::boost::type_traits::ice_not<          \
                            (::boost::is_integral<Functor>::value)>::value), \
                           Type>::type
#else
// BCC doesn't recognize this depends on a template argument and complains
// about the use of 'typename'
#  define BOOST_FUNCTION_ENABLE_IF_NOT_INTEGRAL(Functor,Type)     \
      ::boost::enable_if_c<(::boost::type_traits::ice_not<          \
                   (::boost::is_integral<Functor>::value)>::value), \
                       Type>::type
#endif


#if defined( __clang__ ) || ( defined( __GNUC__ ) && ( ( ( __GNUC__ * 10 ) + __GNUC_MINOR__ ) < 45 ) )
    #define BOOST_FUNCTION_CLANG_AND_OLD_GCC_BROKEN_STATIC_ASSERT BOOST_ASSERT
#else
    #define BOOST_FUNCTION_CLANG_AND_OLD_GCC_BROKEN_STATIC_ASSERT BOOST_STATIC_ASSERT
#endif // __clang__

#if defined( BOOST_MSVC ) || defined( __clang__ ) || ( defined( __GNUC__ ) && ( ( ( __GNUC__ * 10 ) + __GNUC_MINOR__ ) >= 45 ) )
    #define BF_VT_REF   &
    #define BF_VT_DEREF *
    #define BF_FASTCALL_WORKAROUND BF_FASTCALL
#else
    #define BF_VT_REF   * const
    #define BF_VT_DEREF
    #define BF_FASTCALL_WORKAROUND // GCC 4.2.1 just doesn't seem happy with decorated function pointers...
#endif


namespace boost {
  namespace detail {
    namespace function {

        template <typename T>
        class fallocator
        {
        public:
            typedef T value_type;

            typedef value_type       * pointer        ;
            typedef value_type       & reference      ;
            typedef value_type const * const_pointer  ;
            typedef value_type const & const_reference;

            typedef std::size_t    size_type      ;
            typedef std::ptrdiff_t difference_type;

            template <class Other>
            struct rebind { typedef fallocator<Other> other; };

            fallocator() {}

            template <class Other>
            fallocator( fallocator<Other> const & ) {}

            static pointer       address( reference       value ) { return addressof( value ); }
            static const_pointer address( const_reference value ) { return addressof( value ); }

            template <class Other>
            fallocator<Other> & operator=( fallocator<Other> const & ) { return *this; }

            pointer allocate  ( size_type const count, void const * /*p_hint*/ ) { return allocate( count ); }
            pointer allocate  ( size_type const count                          ) { return ::operator new( count * sizeof( T ) ); }
            void    deallocate( pointer   const ptr  , size_type const /*count*/ ) { ::operator delete( ptr ); }

            void construct( pointer const ptr, T const & source ) { new ( ptr ) T( source ); }
            void destroy  ( pointer const ptr                   ) { ptr->~T(); }

            static size_type max_size() { return std::numeric_limits<size_type>::max() / sizeof( T ); }
        };

        typedef fallocator<void *> dummy_allocator;

        struct thiscall_optimization_available_helper
        {
            static void free_function () {}
                   void bound_function() {}

            BOOST_STATIC_CONSTANT( bool, value = sizeof( &thiscall_optimization_available_helper::free_function ) == sizeof( &thiscall_optimization_available_helper::bound_function ) );
        };

        typedef mpl::bool_
        <
            //...zzz...MSVC10 does not like this...
            //sizeof( &thiscall_optimization_available_helper::free_function  )
            //    ==
            //sizeof( &thiscall_optimization_available_helper::bound_function )
            thiscall_optimization_available_helper::value
        > thiscall_optimization_available;

    #ifndef BOOST_FUNCTION_NO_RTTI
      // For transferring stored function object type information back to the
      // interface side.
      class typed_functor
      #ifndef __GNUC__ //...zzz...GCC and Clang have serious RVO problems...
          : noncopyable
      #endif // __GNUC__
      {
      public:
          template <typename Functor>
          typed_functor( Functor & functor )
              :
              pFunctor          ( addressof      ( functor )  ),
              type_id           ( BOOST_SP_TYPEID( Functor )  ),
              const_qualified   ( is_const   <Functor>::value ),
              volatile_qualified( is_volatile<Functor>::value )
          {
              BOOST_ASSERT( pFunctor );
          }

          detail::sp_typeinfo const & functor_type_info() const { return type_id; }

          template <typename Functor>
          Functor * target()
          {
              return static_cast<Functor *>
                     (
                         get_functor_if_types_match
                         (
                             BOOST_SP_TYPEID( Functor ),
                             is_const   <Functor>::value,
                             is_volatile<Functor>::value
                         )
                     );
          }

      private:
          void * get_functor_if_types_match
          (
            detail::sp_typeinfo const & other,
            bool const other_const_qualified,
            bool const other_volatile_qualified
          )
          {
              // Check whether we have the same type. We can add
              // cv-qualifiers, but we can't take them away.
              bool const types_match
              (
                BOOST_FUNCTION_COMPARE_TYPE_ID( type_id, other )
                    &&
                ( !const_qualified    || other_const_qualified    )
                    &&
                ( !volatile_qualified || other_volatile_qualified )
              );
              return types_match ? const_cast<void *>( pFunctor ) : 0;
          }

      private:
          void                const * const pFunctor;
          detail::sp_typeinfo const &       type_id ;
          // Whether the type is const-qualified.
          bool const const_qualified;
          // Whether the type is volatile-qualified.
          bool const volatile_qualified;
      };
    #endif // BOOST_FUNCTION_NO_RTTI

      /**
       * A buffer used to store small function objects in
       * boost::function. It is a union containing function pointers,
       * object pointers, and a structure that resembles a bound
       * member function pointer.
       */
      #ifdef BOOST_MSVC
        // http://msdn.microsoft.com/en-us/library/5ft82fed(VS.80).aspx (ad unions)
        #define BF_AUX_RESTRICT __restrict
      #else
        #define BF_AUX_RESTRICT
      #endif
      union function_buffer
      {
        // For pointers to function objects
        void * BF_AUX_RESTRICT obj_ptr;

        //  For 'trivial' function objects (that can be managed without type
        // information) that must be allocated on the heap (we must only save
        // the object's size information for the clone operation).
        struct trivial_heap_obj_t
        {
            void        * ptr;
            std::size_t   size; // in number of allocation atoms
        } trivial_heap_obj;

        // For function pointers of all kinds
        void (* BF_AUX_RESTRICT func_ptr)();

        // For bound member pointers
        struct bound_memfunc_ptr_t {
          class X;
          void (X::* BF_AUX_RESTRICT memfunc_ptr)(int);
          void * obj_ptr;
        } bound_memfunc_ptr;

        // To relax aliasing constraints
        char data;
      };
      #undef BF_AUX_RESTRICT

      // A simple wrapper to allow deriving and a thiscall invoker.
      struct function_buffer_holder { function_buffer buffer; };

      // Check that all function_buffer "access points" are actually at the same
      // address/offset.
      BOOST_STATIC_ASSERT( offsetof( function_buffer, obj_ptr           ) == offsetof( function_buffer, func_ptr ) );
      BOOST_STATIC_ASSERT( offsetof( function_buffer, bound_memfunc_ptr ) == offsetof( function_buffer, func_ptr ) );
      BOOST_STATIC_ASSERT( offsetof( function_buffer::bound_memfunc_ptr_t, memfunc_ptr ) == 0 );

      template
      <
          typename throw_test_signature,
          throw_test_signature * no_throw_test,
          throw_test_signature * throw_test
      >
      struct is_nothrow_helper
      {
          static bool const is_nothrow;
      };

      // is_nothrow_helper static member initialization
      template <typename throw_test_signature, throw_test_signature * no_throw_test, throw_test_signature * throw_test>
      bool const is_nothrow_helper
                <
                    throw_test_signature,
                    no_throw_test,
                    throw_test
                >::is_nothrow = ( no_throw_test == throw_test );

      /**
       * The unusable class is a placeholder for unused function arguments
       * It is also completely unusable except that it constructible from
       * anything. This helps compilers without partial specialization to
       * handle Boost.Function objects returning void.
       */
      struct unusable
      {
        unusable() {}
        template<typename T> unusable(const T&) {}
      };

      /* Determine the return type. This supports compilers that do not support
       * void returns or partial specialization by silently changing the return
       * type to "unusable".
       */
      template<typename T> struct function_return_type       { typedef T        type; };
      template<>           struct function_return_type<void> { typedef unusable type; };

      // Tags used to decide between different types of functions
      struct function_ptr_tag {};
      struct function_obj_tag {};
      struct member_ptr_tag {};
      struct function_obj_ref_tag {};

      // When functions and function pointers are decorated with exception
      // specifications MSVC mangles their type (almost) beyond recognition.
      // Even MSVC supplied type traits is_pointer, is_member_pointer and
      // is_function no longer recognize them. This tester is a workaround that
      // seems to work well enough for now.
      template <typename T>
      struct is_msvc_exception_specified_function_pointer
          :
          public mpl::bool_
          <
            #ifdef _MSC_VER
              !is_class      <T>::value &&
              !is_fundamental<T>::value &&
              ( sizeof( T ) == sizeof( void (*) (void) ) )
            #else
              false
            #endif
          >
      {};

      template<typename F>
      class get_function_tag
      {
        typedef typename mpl::if_c<(is_pointer<F>::value || is_msvc_exception_specified_function_pointer<F>::value),
                                   function_ptr_tag,
                                   function_obj_tag>::type ptr_or_obj_tag;

        typedef typename mpl::if_c<(is_member_pointer<F>::value),
                                   member_ptr_tag,
                                   ptr_or_obj_tag>::type ptr_or_obj_or_mem_tag;

        typedef typename mpl::if_c<(is_reference_wrapper<F>::value),
                                   function_obj_ref_tag,
                                   ptr_or_obj_or_mem_tag>::type or_ref_tag;

      public:
        typedef or_ref_tag type;
      };


      template <typename F, typename A>
      struct functor_and_allocator : public F, public A
      {
        functor_and_allocator( F const & f, A a ) : F( f ), A( a ) {}

        F & functor  () { return *this; }
        A & allocator() { return *this; }
      };


      typedef boost::aligned_storage<sizeof( void * ) * 2, sizeof( void * ) * 2>::type trivial_heap_storage_atom;

      template<typename Functor>
      struct functor_traits
      {
          BOOST_STATIC_CONSTANT
          (bool,
          allowsPODOptimization =
              is_msvc_exception_specified_function_pointer<Functor>::value ||
              (
                  has_trivial_copy_constructor<Functor>::value &&
                  has_trivial_destructor      <Functor>::value
              ));

          BOOST_STATIC_CONSTANT
          (bool,
          allowsSmallObjectOptimization =
            is_msvc_exception_specified_function_pointer<Functor>::value                   ||
            (
              ( sizeof( Functor ) <= sizeof( function_buffer ) )                           &&
              ( alignment_of<function_buffer>::value % alignment_of<Functor>::value == 0 ) &&
                allowsPODOptimization
            ));

          BOOST_STATIC_CONSTANT
          (bool,
          allowsPtrObjectOptimization =
            is_msvc_exception_specified_function_pointer<Functor>::value          ||
            (
              ( sizeof( Functor ) <= sizeof( void * )                             &&
              ( alignment_of<void *>::value % alignment_of<Functor>::value == 0 ) &&
                allowsPODOptimization)
            ));

          BOOST_STATIC_CONSTANT
          (bool,
          hasDefaultAlignement =
            alignment_of<Functor>::value == alignment_of<trivial_heap_storage_atom>::value);
      };


      ///  A helper class that can construct a typed_functor object from a
      /// function_buffer instance for given template parameters.
      template
      <
        typename ActualFunctor,
        typename StoredFunctor,
        class FunctorManager
      >
      class functor_type_info
      {
      #ifndef BOOST_FUNCTION_NO_RTTI
      private:
          static ActualFunctor * actual_functor_ptr( StoredFunctor * pStoredFunctor, mpl::true_ /*is_ref_wrapper*/, mpl::false_ /*is_member_pointer*/ )
          {
              // needed when StoredFunctor is a static reference
              ignore_unused_variable_warning( pStoredFunctor );
              return pStoredFunctor->get_pointer();
          }

          static ActualFunctor * actual_functor_ptr( StoredFunctor * pStoredFunctor, mpl::false_ /*is_ref_wrapper*/, mpl::true_ /*is_member_pointer*/ )
          {
              BOOST_STATIC_ASSERT( sizeof( StoredFunctor ) == sizeof( ActualFunctor ) );
              return static_cast<ActualFunctor *>( static_cast<void *>( pStoredFunctor ) );
          }

          static ActualFunctor * actual_functor_ptr( StoredFunctor * pStoredFunctor, mpl::false_ /*is_ref_wrapper*/, mpl::false_ /*is_member_pointer*/ )
          {
              BOOST_STATIC_ASSERT
              ((
                  ( is_same<typename remove_cv<ActualFunctor>::type, StoredFunctor>::value ) ||
                  (
                    is_msvc_exception_specified_function_pointer<ActualFunctor>::value &&
                    is_msvc_exception_specified_function_pointer<StoredFunctor>::value &&
                    sizeof( ActualFunctor ) == sizeof( StoredFunctor )
                  )
              ));
              return pStoredFunctor;
          }
      public:
          // See the related comment in the vtable struct definition.
          #ifdef __GNUC__
            static typed_functor             get_typed_functor( function_buffer const & buffer )
          #else
            static typed_functor BF_FASTCALL get_typed_functor( function_buffer const & buffer )
          #endif
          {
              StoredFunctor * const pStoredFunctor( FunctorManager::functor_ptr( const_cast<function_buffer &>( buffer ) ) );
              ActualFunctor * const pActualFunctor( actual_functor_ptr( pStoredFunctor, is_reference_wrapper<StoredFunctor>(), is_member_pointer<ActualFunctor>() ) );
              return typed_functor( *pActualFunctor );
          }
      #endif // BOOST_FUNCTION_NO_RTTI
      };

      // A helper wrapper class that adds type information functionality (e.g.
      // for non typed/trivial managers).
      template
      <
          class FunctorManager,
          typename ActualFunctor,
          typename StoredFunctor = typename FunctorManager::Functor
      >
      struct typed_manager
          :
          public FunctorManager,
          public functor_type_info<ActualFunctor, StoredFunctor, typed_manager<FunctorManager, ActualFunctor, StoredFunctor> >
      {
          typedef StoredFunctor Functor;
          static StoredFunctor * functor_ptr( function_buffer & buffer ) { return static_cast<StoredFunctor *>( static_cast<void *>( FunctorManager::functor_ptr( buffer ) ) ); }
      };

      /// Manager for trivial objects that fit into sizeof( void * ).
      struct manager_ptr
      {
      public:
          static void       *       * functor_ptr( function_buffer       & buffer ) { return &buffer.obj_ptr; }
          static void const * const * functor_ptr( function_buffer const & buffer ) { BF_ASSUME( buffer.obj_ptr ); return functor_ptr( const_cast<function_buffer &>( buffer ) ); }

          template <typename Functor, typename Allocator>
          static void assign( Functor const & functor, function_buffer & out_buffer, Allocator )
          {
              BOOST_STATIC_ASSERT( functor_traits<Functor>::allowsPtrObjectOptimization );
              new ( functor_ptr( out_buffer ) ) Functor( functor );
          }

          static void BF_FASTCALL_WORKAROUND clone( function_buffer const & in_buffer, function_buffer & out_buffer )
          {
              return assign( *functor_ptr( in_buffer ), out_buffer, dummy_allocator() );
          }

          static void BF_FASTCALL_WORKAROUND move( function_buffer & in_buffer, function_buffer & out_buffer )
          {
              clone( in_buffer, out_buffer );
              destroy( in_buffer );
          }

          static void BF_FASTCALL_WORKAROUND destroy( function_buffer & /*buffer*/ )
          {
              //*functor_ptr( buffer ) = 0;
          }
      };

      /// Manager for trivial objects that can live in a function_buffer.
      struct manager_trivial_small
      {
      public:
          static void * functor_ptr( function_buffer & buffer ) { return &buffer; }

          template <typename Functor, typename Allocator>
          static void assign( Functor const & functor, function_buffer & out_buffer, Allocator )
          {
              BOOST_FUNCTION_CLANG_AND_OLD_GCC_BROKEN_STATIC_ASSERT
              (
                functor_traits<Functor>::allowsPODOptimization &&
                functor_traits<Functor>::allowsSmallObjectOptimization
              );
              new ( functor_ptr( out_buffer ) ) Functor( functor );
          }

          static void BF_FASTCALL_WORKAROUND clone( function_buffer const & in_buffer, function_buffer & out_buffer )
          {
              return assign( in_buffer, out_buffer, dummy_allocator() );
          }

          static void BF_FASTCALL_WORKAROUND move( function_buffer & in_buffer, function_buffer & out_buffer )
          {
              clone( in_buffer, out_buffer );
              destroy( in_buffer );
          }

          static void BF_FASTCALL_WORKAROUND destroy( function_buffer & /*buffer*/ )
          {
              //std::memset( &buffer, 0, sizeof( buffer ) );
          }
      };

      /// Manager for trivial objects that cannot live/fit in a function_buffer
      /// and are used with 'empty' allocators.
      template <typename Allocator>
      struct manager_trivial_heap
      {
      public:
          typedef typename Allocator:: BOOST_NESTED_TEMPLATE rebind<unsigned char>::other trivial_allocator;

      public:
          static void * & functor_ptr( function_buffer       & buffer ) { return buffer.trivial_heap_obj.ptr; }
          static void *   functor_ptr( function_buffer const & buffer ) { BF_ASSUME( buffer.trivial_heap_obj.ptr ); return functor_ptr( const_cast<function_buffer &>( buffer ) ); }

          template <typename Functor>
          static void assign( Functor const & functor, function_buffer & out_buffer, Allocator const a )
          {
              BOOST_ASSERT( a == trivial_allocator() );
              ignore_unused_variable_warning( a );

              BOOST_FUNCTION_CLANG_AND_OLD_GCC_BROKEN_STATIC_ASSERT
              (
                functor_traits<Functor>::allowsPODOptimization &&
                functor_traits<Functor>::hasDefaultAlignement
              );

              function_buffer in_buffer;
              in_buffer.trivial_heap_obj.ptr  = const_cast<Functor *>( &functor );
              in_buffer.trivial_heap_obj.size = sizeof( Functor );
              return clone( in_buffer, out_buffer );
          }

          static void BF_FASTCALL_WORKAROUND clone( function_buffer const & in_buffer, function_buffer & out_buffer )
          {
              trivial_allocator a;
              std::size_t const storage_size( in_buffer.trivial_heap_obj.size );
              out_buffer.trivial_heap_obj.ptr  = a.allocate( storage_size );
              out_buffer.trivial_heap_obj.size = storage_size;
              std::memcpy( functor_ptr( out_buffer ), functor_ptr( in_buffer ), storage_size );
          }

          static void BF_FASTCALL_WORKAROUND move( function_buffer & in_buffer, function_buffer & out_buffer )
          {
              out_buffer.trivial_heap_obj = in_buffer.trivial_heap_obj;
              //in_buffer.trivial_heap_obj.ptr = 0;
          }

          static void BF_FASTCALL_WORKAROUND destroy( function_buffer & buffer )
          {
              trivial_allocator a;
              a.deallocate( functor_ptr( buffer ) );
              //functor_ptr( buffer ) = 0;
          }
      };

      /// Manager for non-trivial objects that can live in a function_buffer.
      template <typename FunctorParam>
      struct manager_small
      {
      public:
          typedef FunctorParam Functor;

          static Functor       * functor_ptr( function_buffer       & buffer ) { return static_cast<Functor *>( manager_trivial_small::functor_ptr( buffer ) ); }
          static Functor const * functor_ptr( function_buffer const & buffer ) { return functor_ptr( const_cast<function_buffer &>( buffer ) ); }

          static void assign( Functor const & functor, function_buffer & out_buffer )
          {
              new ( functor_ptr( out_buffer ) ) Functor( functor );
          }

          static void BF_FASTCALL_WORKAROUND clone( function_buffer const & in_buffer, function_buffer & out_buffer )
          {
              Functor const & in_functor( *functor_ptr( in_buffer ) );
              return assign( in_functor, out_buffer );
          }

          static void BF_FASTCALL_WORKAROUND move( function_buffer & in_buffer, function_buffer & out_buffer )
          {
              // ...use swap here?
              clone( in_buffer, out_buffer );
              destroy( in_buffer );
          }

          static void BF_FASTCALL_WORKAROUND destroy( function_buffer & buffer )
          {
              functor_ptr( buffer )->~Functor();
          }
      };

      ///  Fully generic manager for non-trivial objects that cannot live/fit in
      /// a function_buffer.
      template <typename FunctorParam, typename AllocatorParam>
      struct manager_generic
      {
      public:
          typedef FunctorParam   Functor          ;
          typedef AllocatorParam OriginalAllocator;

          typedef          functor_and_allocator<Functor, OriginalAllocator>                              functor_and_allocator_t;
          typedef typename OriginalAllocator:: BOOST_NESTED_TEMPLATE rebind<functor_and_allocator>::other wrapper_allocator_t    ;
          typedef typename OriginalAllocator:: BOOST_NESTED_TEMPLATE rebind<OriginalAllocator    >::other allocator_allocator_t  ;

          static functor_and_allocator_t * & functor_ptr( function_buffer & buffer )
          {
              return *static_cast<functor_and_allocator_t * *>
              (
                  static_cast<void *>
                  (
                      &manager_trivial_heap<OriginalAllocator>::functor_ptr( buffer )
                  )
              );
          }

          static functor_and_allocator_t * functor_ptr( function_buffer const & buffer )
          {
              return functor_ptr( const_cast<function_buffer &>( buffer ) );
          }

          static void assign( Functor const & functor, function_buffer & out_buffer, OriginalAllocator source_allocator )
          {
              typedef ::boost::type_traits::ice_and
                        <
                            ::boost::has_nothrow_copy<Functor          >::value,
                            ::boost::has_nothrow_copy<OriginalAllocator>::value
                        > does_not_need_guard_t;

              typedef typename mpl::if_
              <
                does_not_need_guard_t,
                functor_and_allocator_t *,
                std::auto_ptr<functor_and_allocator_t>
              >::type guard_t;
              assign_aux<guard_t>( functor, out_buffer, source_allocator );
          }

          static void clone( function_buffer const & in_buffer, function_buffer & out_buffer )
          {
              functor_and_allocator_t const & in_functor_and_allocator( *functor_ptr( in_buffer ) );
              return assign( in_functor_and_allocator.functor(), out_buffer, in_functor_and_allocator.allocator() );
          }

          static void move( function_buffer & in_buffer, function_buffer & out_buffer )
          {
              manager_trivial_heap<OriginalAllocator>::move( in_buffer, out_buffer );
          }

          static void destroy( function_buffer & buffer )
          {
              functor_and_allocator_t & in_functor_and_allocator( *functor_ptr( buffer ) );

              OriginalAllocator     original_allocator ( in_functor_and_allocator.allocator() );
              allocator_allocator_t allocator_allocator( in_functor_and_allocator.allocator() );
              wrapper_allocator_t   full_allocator     ( in_functor_and_allocator.allocator() );

              original_allocator .destroy( &in_functor_and_allocator.functor  () );
              allocator_allocator.destroy( &in_functor_and_allocator.allocator() );

              full_allocator.deallocate( &in_functor_and_allocator );
              //functor_ptr( buffer ) = 0;
          }

        private:
          template <typename Guard>
          static functor_and_allocator_t * release( Guard                   &       guard   ) { return guard.release(); }
          static functor_and_allocator_t * release( functor_and_allocator_t * const pointer ) { return pointer        ; }

          template <typename Guard>
          static void assign_aux( Functor const & functor, function_buffer & out_buffer, OriginalAllocator source_allocator )
          {
              wrapper_allocator_t   full_allocator     ( source_allocator );
              allocator_allocator_t allocator_allocator( source_allocator );

              Guard                         p_placeholder          ( full_allocator.allocate( 1 ) );
              Functor               * const p_functor_placeholder  ( p_placeholder                );
              OriginalAllocator     * const p_allocator_placeholder( p_placeholder                );

              source_allocator   .construct( p_functor_placeholder  , functor          );
              allocator_allocator.construct( p_allocator_placeholder, source_allocator );

              functor_ptr( out_buffer ) = release( p_placeholder );
          }
      };

      /// Helper metafunction for retrieving an appropriate functor manager.
      template
      <
        typename Functor,
        typename Allocator,
        bool POD,
        bool smallObj,
        bool ptrSmall,
        bool defaultAligned
      >
      struct functor_manager
      {
          typedef manager_generic<Functor, Allocator> type;
      };

      template <typename Functor, typename Allocator>
      struct functor_manager<Functor, Allocator, true, false, false, true>
      {
          typedef manager_trivial_heap<Allocator> type;
      };

      template <typename Functor, typename Allocator, bool defaultAligned>
      struct functor_manager<Functor, Allocator, true, true, false, defaultAligned>
      {
          typedef manager_trivial_small type;
      };

      template <typename Functor, typename Allocator, bool smallObj, bool defaultAligned>
      struct functor_manager<Functor, Allocator, true, smallObj, true, defaultAligned>
      {
          typedef manager_ptr type;
      };

      template <typename Functor, typename Allocator, bool ptrSmall, bool defaultAligned>
      struct functor_manager<Functor, Allocator, false, true, ptrSmall, defaultAligned>
      {
          typedef manager_small<Functor> type;
      };

      /// Metafunction for retrieving an appropriate functor manager with
      /// minimal type information.
      template<typename StoredFunctor, typename Allocator>
      struct get_functor_manager
      {
          typedef typename functor_manager
                    <
                        StoredFunctor,
                        Allocator,
                        functor_traits<StoredFunctor>::allowsPODOptimization,
                        functor_traits<StoredFunctor>::allowsSmallObjectOptimization,
                        functor_traits<StoredFunctor>::allowsPtrObjectOptimization,
                        functor_traits<StoredFunctor>::hasDefaultAlignement
                    >::type type;
      };

      /// Metafunction for retrieving an appropriate fully typed functor manager.
      template<typename ActualFunctor, typename StoredFunctor, typename Allocator>
      struct get_typed_functor_manager
      {
          typedef typed_manager
                  <
                    typename get_functor_manager<StoredFunctor, Allocator>::type,
                    ActualFunctor,
                    StoredFunctor
                  > type;
      };

      // A type that is only used for comparisons against zero
      struct useless_clear_type {};

#ifdef BOOST_NO_SFINAE
      // These routines perform comparisons between a Boost.Function
      // object and an arbitrary function object (when the last
      // parameter is mpl::bool_<false>) or against zero (when the
      // last parameter is mpl::bool_<true>). They are only necessary
      // for compilers that don't support SFINAE.
      template<typename Function, typename Functor>
        bool
        compare_equal(const Function& f, const Functor&, int, mpl::bool_<true>)
        { return f.empty(); }

      template<typename Function, typename Functor>
        bool
        compare_not_equal(const Function& f, const Functor&, int,
                          mpl::bool_<true>)
        { return !f.empty(); }

      template<typename Function, typename Functor>
        bool
        compare_equal(const Function& f, const Functor& g, long,
                      mpl::bool_<false>)
        {
          if (const Functor* fp = f.template target<Functor>())
            return function_equal(*fp, g);
          else return false;
        }

      template<typename Function, typename Functor>
        bool
        compare_equal(const Function& f, const reference_wrapper<Functor>& g,
                      int, mpl::bool_<false>)
        {
          if (const Functor* fp = f.template target<Functor>())
            return fp == g.get_pointer();
          else return false;
        }

      template<typename Function, typename Functor>
        bool
        compare_not_equal(const Function& f, const Functor& g, long,
                          mpl::bool_<false>)
        {
          if (const Functor* fp = f.template target<Functor>())
            return !function_equal(*fp, g);
          else return true;
        }

      template<typename Function, typename Functor>
        bool
        compare_not_equal(const Function& f,
                          const reference_wrapper<Functor>& g, int,
                          mpl::bool_<false>)
        {
          if (const Functor* fp = f.template target<Functor>())
            return fp != g.get_pointer();
          else return true;
        }
#endif // BOOST_NO_SFINAE

      //  The "generic typed/void-void invoker pointer is also stored here so
      // that it can (more easily) be placed at the beginning of the vtable so
      // that a vtable pointer would actually point directly to it (thus
      // avoiding pointer offset calculation on invocation).
      //  This also gives a unique/non-template vtable that can be held by
      // function_base entirely but it also opens a window for erroneous vtable
      // copying/assignment between different boost::function<> instantiations.
      // A typed wrapper should therefor be added to the boost::function<>
      // class to catch such errors at compile-time.
      struct vtable
      {
        typedef void ( function_buffer::* this_call_invoker_placeholder_type )( void );
        typedef void (                  * free_call_invoker_placeholder_type )( void );
        typedef mpl::if_
        <
            thiscall_optimization_available,
            this_call_invoker_placeholder_type,
            free_call_invoker_placeholder_type
        >::type invoker_placeholder_type;

        template<typename TargetInvokerType>
        TargetInvokerType const & invoker() const { return reinterpret_cast<TargetInvokerType const &>( void_invoker ); }

        void clone  ( function_buffer const & in_buffer, function_buffer & out_buffer ) const { do_clone( in_buffer, out_buffer ); }
        void move   ( function_buffer       & in_buffer, function_buffer & out_buffer ) const { do_move ( in_buffer, out_buffer ); }
        BF_NOTHROW
        void destroy( function_buffer       & buffer                                  ) const { do_destroy( buffer );              }

        // The possibly-decorated-invoker-wrapper is not used here because MSVC
        // (9.0 SP1) needlessly copy-constructs the returned typed_functor
        // object even if the thin invoker-wrapper is __forceinlined.
        //typed_functor get_typed_functor( function_buffer const & buffer ) const { return do_get_typed_functor( buffer ); }

      public: // "Private but not private" to enable aggregate-style initialization.
        invoker_placeholder_type const void_invoker;

        void (BF_FASTCALL_WORKAROUND BF_VT_REF do_clone   )( function_buffer const & in_buffer, function_buffer & out_buffer );
        void (BF_FASTCALL_WORKAROUND BF_VT_REF do_move    )( function_buffer       & in_buffer, function_buffer & out_buffer );
        void (BF_FASTCALL_WORKAROUND BF_VT_REF do_destroy )( function_buffer       & buffer                                  );

      #ifndef BOOST_FUNCTION_NO_RTTI
      public:
        // Because of the MSVC issue described above we mark the
        // get_typed_functor function as nothrow like this, explicitly (because
        // MSVC does not properly support exception specifications this is not a
        // pessimization...it is equivalent to __declspec( nothrow )).
        // OTOH, All GCCs and Clang break down here if they find the fastcall
        // attribute so they get it undecorated...
        #ifdef _MSC_VER
            typed_functor ( BF_FASTCALL BF_VT_REF get_typed_functor )( function_buffer const & ) throw();
        #else
            typed_functor (             BF_VT_REF get_typed_functor )( function_buffer const & );
        #endif // BOOST_MSVC

      #endif // BOOST_FUNCTION_NO_RTTI


/*    ALLOCATOR SUPPORT TEMPORARILY COMMENTED OUT
          template<typename FunctionObj,typename Allocator>
          void 
          assign_functor_a(FunctionObj f, function_buffer& functor, Allocator a, mpl::false_) const
          {
              typedef functor_wrapper<FunctionObj,Allocator> functor_wrapper_type;
              typedef typename Allocator::template rebind<functor_wrapper_type>::other
                  wrapper_allocator_type;
              typedef typename wrapper_allocator_type::pointer wrapper_allocator_pointer_type;
              wrapper_allocator_type wrapper_allocator(a);
              wrapper_allocator_pointer_type copy = wrapper_allocator.allocate(1);
              wrapper_allocator.construct(copy, functor_wrapper_type(f,a));
              functor_wrapper_type* new_f = static_cast<functor_wrapper_type*>(copy);
              functor.obj_ptr = new_f;
          }
*/
      };

      template <class Invoker, class Manager>
      struct vtable_holder
      {
          static vtable::this_call_invoker_placeholder_type get_invoker_pointer( mpl::true_ /*this call*/ )
          {
              return reinterpret_cast<vtable::this_call_invoker_placeholder_type>( &Invoker::bound_invoke );
          }

          static vtable::free_call_invoker_placeholder_type get_invoker_pointer( mpl::false_ /*free call*/ )
          {
              return reinterpret_cast<vtable::free_call_invoker_placeholder_type>( &Invoker::free_invoke );
          }

          static vtable const stored_vtable;
      };

      // Note: it is extremely important that this initialization use
      // static initialization. Otherwise, we will have a race
      // condition here in multi-threaded code. See
      // http://thread.gmane.org/gmane.comp.lib.boost.devel/164902/.
      template <class Invoker, class Manager>
      vtable const vtable_holder<Invoker, Manager>::stored_vtable =
      {
          vtable_holder<Invoker, Manager>::get_invoker_pointer( thiscall_optimization_available() ),
          BF_VT_DEREF &Manager::clone,
          BF_VT_DEREF &Manager::move,
          BF_VT_DEREF &Manager::destroy
        #ifndef BOOST_FUNCTION_NO_RTTI
          ,BF_VT_DEREF &Manager::get_typed_functor
        #endif // BOOST_FUNCTION_NO_RTTI
      };
    } // end namespace function
  } // end namespace detail

/**
 * The function_base class contains the basic elements needed for the
 * function1, function2, function3, etc. classes. It is common to all
 * functions (and as such can be used to tell if we have one of the
 * functionN objects).
 */
class function_base
{
private: // Private helper guard classes.
  // ...(definition) to be moved out of body

  // ...if the is_stateless<EmptyHandler> requirement sticks this will not need
  // to be a template...
  template <class EmptyHandler>
  class cleaner : noncopyable
  {
  typedef detail::function::vtable vtable;
  public:
      cleaner
      (
          function_base       & function,
          vtable        const & empty_handler_vtable
      )
          :
          pFunction_           ( &function            ),
          empty_handler_vtable_( empty_handler_vtable )
      {}

      ~cleaner() { conditional_clear( pFunction_ != 0 ); }

      void cancel() { assert( pFunction_ ); pFunction_ = 0; }

  private:
      void conditional_clear( bool const clear )
      {
          using namespace detail::function;
          if ( clear )
          {
              BOOST_ASSERT( pFunction_ );
              typedef          functor_traits     <EmptyHandler>                                  empty_handler_traits ;
              typedef typename get_functor_manager<EmptyHandler, fallocator<EmptyHandler> >::type empty_handler_manager;
              // remove completely or replace with a simple is_stateless<>?
              BOOST_FUNCTION_CLANG_AND_OLD_GCC_BROKEN_STATIC_ASSERT
              (
                empty_handler_traits::allowsPODOptimization &&
                empty_handler_traits::allowsSmallObjectOptimization
              );
              empty_handler_manager::assign( EmptyHandler(), pFunction_->functor, fallocator<EmptyHandler>() );
              pFunction_->pVTable = &empty_handler_vtable_;
          }
      }

  private:
      function_base       * pFunction_;
      vtable        const & empty_handler_vtable_;
  };

  class safe_mover_base;
  template <class EmptyHandler>
  class safe_mover;

public:
    function_base( detail::function::vtable const & vtable ) : pVTable( &vtable ) { }
    ~function_base() { destroy(); }

  template <class EmptyHandler>
  void swap( function_base & other, detail::function::vtable const & empty_handler_vtable );

#ifndef BOOST_FUNCTION_NO_RTTI

  /// Retrieve the type of the stored function object.
  const detail::sp_typeinfo& target_type() const
  {
    return get_vtable().get_typed_functor( this->functor ).functor_type_info();
  }

  template <typename Functor>
  Functor * target()
  {
    return get_vtable().get_typed_functor( this->functor ).target<Functor>();
  }

  template <typename Functor>
  Functor const * target() const
  {
      return const_cast<function_base &>( *this ).target<Functor const>();
  }

  template<typename F>
    bool contains(const F& f) const
    {
#if defined(BOOST_MSVC) && BOOST_WORKAROUND(BOOST_MSVC, < 1300)
      if (const F* fp = this->target( (F*)0 ))
#else
      if (const F* fp = this->template target<F>())
#endif
      {
        return function_equal(*fp, f);
      } else {
        return false;
      }
    }

#if defined(__GNUC__) && __GNUC__ == 3 && __GNUC_MINOR__ <= 3
  // GCC 3.3 and newer cannot copy with the global operator==, due to
  // problems with instantiation of function return types before it
  // has been verified that the argument types match up.
  template<typename Functor>
    BOOST_FUNCTION_ENABLE_IF_NOT_INTEGRAL(Functor, bool)
    operator==(Functor g) const
    {
      if (const Functor* fp = target<Functor>())
        return function_equal(*fp, g);
      else return false;
    }

  template<typename Functor>
    BOOST_FUNCTION_ENABLE_IF_NOT_INTEGRAL(Functor, bool)
    operator!=(Functor g) const
    {
      if (const Functor* fp = target<Functor>())
        return !function_equal(*fp, g);
      else return true;
    }
#endif

#endif // BOOST_FUNCTION_NO_RTTI

public: // should be protected, but GCC 2.95.3 will fail to allow access
  detail::function::vtable const & get_vtable() const
  {
      BF_ASSUME( pVTable );
      return *pVTable;
  }

protected:
  template <class EmptyHandler>
  BF_NOTHROW
  void clear( detail::function::vtable const & empty_handler_vtable )
  {
      //  If we hold to the is_stateless<EmptyHandler> requirement a full assign
      // is not necessary here but a simple
      // this->pVTable = &empty_handler_vtable...
      EmptyHandler /*const*/ emptyHandler;
      assign<false, EmptyHandler>
      (
        emptyHandler,
        empty_handler_vtable,
        empty_handler_vtable, 
        detail::function::fallocator<EmptyHandler>()
      );
  }

private: // Assignment from another boost function helpers.
  BF_NOINLINE
  void assign_direct( function_base const & source )
  {
      source.pVTable->clone( source.functor, this->functor );
      pVTable = source.pVTable;
  }

  template <class EmptyHandler>
  BF_NOINLINE
  void assign_guarded( function_base const & source, detail::function::vtable const & empty_handler_vtable )
  {
      this->destroy();
      cleaner<EmptyHandler> guard( *this, empty_handler_vtable );
      assign_direct( source );
      guard.cancel();
  }

protected:
  // Assignment from another boost function.
  template <bool direct, typename EmptyHandler, typename FunctionObj, typename Allocator>
  typename enable_if<is_base_of<function_base, FunctionObj> >::type
  assign
  (
    FunctionObj const & f,
    detail::function::vtable const & functor_vtable,
    detail::function::vtable const & empty_handler_vtable,
    Allocator
  )
  {
    BOOST_ASSERT( &functor_vtable == f.pVTable );
    ignore_unused_variable_warning( functor_vtable );
    if ( direct )
    {
        BOOST_ASSERT( &static_cast<function_base const &>( f ) != this );
        BOOST_ASSERT( this->pVTable == &empty_handler_vtable );
        assign_direct( f );
    }
    else if( &static_cast<function_base const &>( f ) != this )
    {
        assign_guarded<EmptyHandler>( f, empty_handler_vtable );
    }
  }

  // General actual assignment.
  template <bool direct, typename EmptyHandler, typename FunctionObj, typename Allocator>
  typename disable_if<is_base_of<function_base, FunctionObj> >::type
  assign
  (
    FunctionObj const & f,
    detail::function::vtable const & functor_vtable,
    detail::function::vtable const & empty_handler_vtable,
    Allocator
  );

  template <typename EmptyHandler, typename FunctionObj, typename Allocator>
  BF_NOTHROW
  void actual_assign
  (
    FunctionObj              const & f,
    detail::function::vtable const & functor_vtable,
    detail::function::vtable const & /*empty_handler_vtable*/,
    Allocator                const   a,
    mpl::true_ /*can use direct assign*/
  )
  {
      typedef typename detail::function::get_functor_manager<FunctionObj, Allocator>::type functor_manager;
      this->destroy();
      functor_manager::assign( f, this->functor, a );
      this->pVTable = &functor_vtable;
  }

  template<typename EmptyHandler, typename FunctionObj, typename Allocator>
  void actual_assign
  (
    FunctionObj              const & f,
    detail::function::vtable const & functor_vtable,
    detail::function::vtable const & empty_handler_vtable,
    Allocator                const   a,
    mpl::false_ /*must use safe assignment*/
  )
  {
      // This most generic case needs to be reworked [currently uses redundant
      // copying (generic one, through function pointers) and does not use all
      // the type information it can...]...
      typedef typename detail::function::get_functor_manager<FunctionObj, Allocator>::type functor_manager;
      function_base tmp( empty_handler_vtable );
      functor_manager::assign( f, tmp.functor, a );
      tmp.pVTable = &functor_vtable;
      this->swap<EmptyHandler>( tmp, empty_handler_vtable );
  }

private:
    void destroy() { get_vtable().destroy( this->functor ); }

protected:
  // Fix/properly encapsulate these members and use the function_buffer_holder.
          detail::function::vtable          const * pVTable;
  mutable detail::function::function_buffer         functor;
};


class function_base::safe_mover_base : noncopyable
{
protected:
    typedef detail::function::vtable          vtable;
    typedef detail::function::function_buffer functor;

protected:
    safe_mover_base( function_base & functionToGuard, function_base & emptyFunctionToMoveTo )
        :
        pFunctionToRestoreTo  ( &functionToGuard                   ),
        emptyFunctionToMoveTo_( emptyFunctionToMoveTo              ),
        empty_handler_vtable_ ( emptyFunctionToMoveTo.get_vtable() )
    {
        BOOST_ASSERT( emptyFunctionToMoveTo.pVTable == &empty_handler_vtable_ );
        move( functionToGuard, emptyFunctionToMoveTo, empty_handler_vtable_ );
    }

    ~safe_mover_base() {}

public:
    void cancel() { BOOST_ASSERT( pFunctionToRestoreTo ); pFunctionToRestoreTo = 0; }

    static void move( function_base & source, function_base & destination, vtable const & empty_handler_vtable )
    {
        source.get_vtable().move( source.functor, destination.functor );
        destination.pVTable = source.pVTable;
        source     .pVTable = &empty_handler_vtable;
    }

protected:
    function_base * pFunctionToRestoreTo  ;
    function_base & emptyFunctionToMoveTo_;
    vtable const  & empty_handler_vtable_ ;
};


// ...if the is_stateless<EmptyHandler> requirement sticks this will not need
// to be a template...
template <class EmptyHandler>
class function_base::safe_mover : public safe_mover_base
{
public:
    safe_mover( function_base & functionToGuard, function_base & emptyFunctionToMoveTo )
        :
        safe_mover_base( functionToGuard, emptyFunctionToMoveTo ) {}
    ~safe_mover()
    {
        if ( pFunctionToRestoreTo )
        {
            cleaner<EmptyHandler> guard( *pFunctionToRestoreTo, empty_handler_vtable_ );
            move( emptyFunctionToMoveTo_, *pFunctionToRestoreTo, empty_handler_vtable_ );
            guard.cancel();
        }
    }
};


template <class EmptyHandler>
void function_base::swap( function_base & other, detail::function::vtable const & empty_handler_vtable )
{
    if ( &other == this )
        return;

    function_base tmp( empty_handler_vtable );

    safe_mover<EmptyHandler> my_restorer   ( *this, tmp   );

    safe_mover<EmptyHandler> other_restorer( other, *this );

    safe_mover_base::move( tmp, other, empty_handler_vtable );

    my_restorer   .cancel();
    other_restorer.cancel();
}


///  The bad_function_call exception class is thrown when an empty
/// boost::function object is invoked that uses the throw_on_empty empty
/// handler.
class bad_function_call : public std::runtime_error
{
public:
  bad_function_call() : std::runtime_error("call to empty boost::function") {}
};

class throw_on_empty
{
private:
    #ifdef BOOST_MSVC
        __declspec( noinline noreturn )
    #else
        BF_NOINLINE
    #endif // BOOST_MSVC
    static void throw_bad_call()
    {
        boost::throw_exception( bad_function_call() );
    }

public:
    template <class result_type>
    #ifdef BOOST_MSVC
        __declspec( noreturn )
    #endif // BOOST_MSVC
    static result_type handle_empty_invoke()
    {
        throw_bad_call();
        #ifndef BOOST_MSVC
            return result_type();
        #endif // BOOST_MSVC
    }
};

class assert_on_empty
{
public:
    template <class result_type>
    static result_type handle_empty_invoke()
    {
        BOOST_ASSERT( !"Call to empty boost::function!" );
        return result_type();
    }
};

class nop_on_empty
{
public:
    template <class result_type>
    static result_type handle_empty_invoke() { return result_type(); }
};


#define BOOST_FUNCTION_ENABLE_IF_FUNCTION                   \
    template <class Function>                               \
    typename enable_if                                      \
             <                                              \
                is_base_of<boost::function_base, Function>, \
                bool                                        \
             >::type

#ifndef BOOST_NO_SFINAE
BOOST_FUNCTION_ENABLE_IF_FUNCTION
inline operator==(const Function& f, detail::function::useless_clear_type*)
{
  return f.empty();
}

BOOST_FUNCTION_ENABLE_IF_FUNCTION
inline operator!=(const Function& f, detail::function::useless_clear_type*)
{
  return !f.empty();
}

BOOST_FUNCTION_ENABLE_IF_FUNCTION
inline operator==(detail::function::useless_clear_type*, const Function& f)
{
  return f.empty();
}

BOOST_FUNCTION_ENABLE_IF_FUNCTION
inline operator!=(detail::function::useless_clear_type*, const Function& f)
{
  return !f.empty();
}
#endif

#ifdef BOOST_NO_SFINAE
// Comparisons between boost::function objects and arbitrary function objects
template<typename Functor>
  inline bool operator==(const function_base& f, Functor g)
  {
    typedef mpl::bool_<(is_integral<Functor>::value)> integral;
    return detail::function::compare_equal(f, g, 0, integral());
  }

template<typename Functor>
  inline bool operator==(Functor g, const function_base& f)
  {
    typedef mpl::bool_<(is_integral<Functor>::value)> integral;
    return detail::function::compare_equal(f, g, 0, integral());
  }

template<typename Functor>
  inline bool operator!=(const function_base& f, Functor g)
  {
    typedef mpl::bool_<(is_integral<Functor>::value)> integral;
    return detail::function::compare_not_equal(f, g, 0, integral());
  }

template<typename Functor>
  inline bool operator!=(Functor g, const function_base& f)
  {
    typedef mpl::bool_<(is_integral<Functor>::value)> integral;
    return detail::function::compare_not_equal(f, g, 0, integral());
  }
#else

#  if !(defined(__GNUC__) && __GNUC__ == 3 && __GNUC_MINOR__ <= 3)
// Comparisons between boost::function objects and arbitrary function
// objects. GCC 3.3 and before has an obnoxious bug that prevents this
// from working.
template<typename Functor>
  BOOST_FUNCTION_ENABLE_IF_NOT_INTEGRAL(Functor, bool)
  operator==(const function_base& f, Functor g)
  {
    if (const Functor* fp = f.template target<Functor>())
      return function_equal(*fp, g);
    else return false;
  }

template<typename Functor>
  BOOST_FUNCTION_ENABLE_IF_NOT_INTEGRAL(Functor, bool)
  operator==(Functor g, const function_base& f)
  {
    if (const Functor* fp = f.template target<Functor>())
      return function_equal(g, *fp);
    else return false;
  }

template<typename Functor>
  BOOST_FUNCTION_ENABLE_IF_NOT_INTEGRAL(Functor, bool)
  operator!=(const function_base& f, Functor g)
  {
    if (const Functor* fp = f.template target<Functor>())
      return !function_equal(*fp, g);
    else return true;
  }

template<typename Functor>
  BOOST_FUNCTION_ENABLE_IF_NOT_INTEGRAL(Functor, bool)
  operator!=(Functor g, const function_base& f)
  {
    if (const Functor* fp = f.template target<Functor>())
      return !function_equal(g, *fp);
    else return true;
  }
#  endif

template<typename Functor>
  BOOST_FUNCTION_ENABLE_IF_NOT_INTEGRAL(Functor, bool)
  operator==(const function_base& f, reference_wrapper<Functor> g)
  {
    if (const Functor* fp = f.template target<Functor>())
      return fp == g.get_pointer();
    else return false;
  }

template<typename Functor>
  BOOST_FUNCTION_ENABLE_IF_NOT_INTEGRAL(Functor, bool)
  operator==(reference_wrapper<Functor> g, const function_base& f)
  {
    if (const Functor* fp = f.template target<Functor>())
      return g.get_pointer() == fp;
    else return false;
  }

template<typename Functor>
  BOOST_FUNCTION_ENABLE_IF_NOT_INTEGRAL(Functor, bool)
  operator!=(const function_base& f, reference_wrapper<Functor> g)
  {
    if (const Functor* fp = f.template target<Functor>())
      return fp != g.get_pointer();
    else return true;
  }

template<typename Functor>
  BOOST_FUNCTION_ENABLE_IF_NOT_INTEGRAL(Functor, bool)
  operator!=(reference_wrapper<Functor> g, const function_base& f)
  {
    if (const Functor* fp = f.template target<Functor>())
      return g.get_pointer() != fp;
    else return true;
  }

#endif // Compiler supporting SFINAE

namespace detail {
  namespace function {
    BOOST_FUNCTION_ENABLE_IF_FUNCTION
    inline has_empty_target( Function const * const f )
    {
      return f->empty();
    }

    template <class FunctionPtr>
    inline bool has_empty_target( reference_wrapper<FunctionPtr> const * const f )
    {
        return has_empty_target( f->get_pointer() );
    }

    template <class FunctionPtr>
    typename enable_if_c
             <
                is_pointer                <FunctionPtr>::value ||
                is_function               <FunctionPtr>::value ||
                is_member_function_pointer<FunctionPtr>::value,
                bool
             >::type
    inline has_empty_target( FunctionPtr const * const funcPtr )
    {
        return funcPtr == 0;
    }

#ifdef BOOST_MSVC // MSVC (9.0 SP1 and prior) cannot inline vararg functions
    inline bool has_empty_target(const void*)
#else
    inline bool has_empty_target(...)
#endif
    {
      return false;
    }

    /* Just an experiment to show how can the current boost::mem_fn implementation
    be 'hacked' to work with custom mem_fn pointer holders (in this case a static
    reference) and/to implement the above. See the assign template overload for
    member functions in function_template.hpp. */
    namespace mem_fn_wrapper
    {
        #define BOOST_MEM_FN_CLASS_F , class F
        #define BOOST_MEM_FN_TYPEDEF(X)
        #define BOOST_MEM_FN_NAME(X) X
        #define BOOST_MEM_FN_CC
        #define BOOST_MEM_FN_RETURN return

        #include "boost/bind/mem_fn_template.hpp"

        #undef BOOST_MEM_FN_CLASS_F
        #undef BOOST_MEM_FN_TYPEDEF
        #undef BOOST_MEM_FN_NAME
        #undef BOOST_MEM_FN_CC
        #undef BOOST_MEM_FN_RETURN
    }
  } // end namespace function
} // end namespace detail

//...zzz...here because of has_empty_target()...GCC & Clang are not lazy enough
template <bool direct, typename EmptyHandler, typename FunctionObj, typename Allocator>
typename disable_if<is_base_of<function_base, FunctionObj> >::type
function_base::assign
(
    FunctionObj              const & f,
    detail::function::vtable const & functor_vtable,
    detail::function::vtable const & empty_handler_vtable,
    Allocator                const   a
)
{
    using namespace detail::function;

    if ( has_empty_target( boost::addressof( f ) ) )
        this->clear<EmptyHandler>( empty_handler_vtable );
    else
    if ( direct )
    {
        BOOST_ASSERT( this->pVTable == &empty_handler_vtable );
        typedef typename get_functor_manager<FunctionObj, Allocator>::type functor_manager;
        functor_manager::assign( f, this->functor, a );
        this->pVTable = &functor_vtable;
        return;
    }
    else
    {
        // This can/should be rewritten because the small-object-optimization
        // condition is too strict (requires a trivial destructor which is not
        // needed for a no fail assignment).
        typedef ::boost::type_traits::ice_and
                <
                    functor_traits<FunctionObj>::allowsSmallObjectOptimization,
                    ::boost::type_traits::ice_or
                    <
                        ::boost::has_nothrow_copy<FunctionObj>::value,
                        ::boost::has_trivial_copy<FunctionObj>::value,
                        ::boost::detail::function::is_msvc_exception_specified_function_pointer<FunctionObj>::value
                    >::value
                > has_no_fail_assignement_t;

        actual_assign<EmptyHandler>
        (
            f,
            functor_vtable,
            empty_handler_vtable,
            a,
            has_no_fail_assignement_t()
        );
    }
}

} // end namespace boost

#undef BOOST_FUNCTION_ENABLE_IF_NOT_INTEGRAL
#undef BOOST_FUNCTION_COMPARE_TYPE_ID
#undef BOOST_FUNCTION_ENABLE_IF_FUNCTION
//...zzz...required in function_template.hpp #undef BOOST_FUNCTION_CLANG_AND_OLD_GCC_BROKEN_STATIC_ASSERT
#undef BF_VT_REF
#undef BF_VT_DEREF

// ...to be moved 'somewhere else'...
namespace boost {
namespace detail {
    template <typename T>
    struct get_non_type_template_parameter_type
    {
        typedef typename boost::mpl::eval_if_c
                <
                    is_integral       <T>::value ||
                    is_pointer        <T>::value ||
                    is_member_pointer <T>::value ||
                    is_reference      <T>::value ||
                    is_function       <T>::value ||
                    is_enum           <T>::value ||
                    function::is_msvc_exception_specified_function_pointer<T>::value,
                    add_const<T>,
                    add_reference<T>
                >::type type;
    };
} // end namespace detail

template
<
    typename T,
    typename detail::get_non_type_template_parameter_type<T>::type static_instance
>
class static_reference_wrapper
{
public:
    typedef typename detail::get_non_type_template_parameter_type<T>::type type;

private:
    static type * get_pointer( type & t  ) { return &t; }
    static type * get_pointer( type * pt ) { return pt; }

public:
    operator type () const { return get(); }

    static type get() { return static_instance; }

    static T const * get_pointer()
    {
        if ( is_reference<type>::value )
            return get_pointer( static_instance );
        else
        {
            static typename add_const<type>::type t_( static_instance );
            return get_pointer( t_ );
        }
    }
};

template
<
    typename T,
    typename detail::get_non_type_template_parameter_type<T>::type static_instance
>
class is_reference_wrapper<static_reference_wrapper<T, static_instance> >
    : public mpl::true_
{
};

template<typename T>
class is_static_reference_wrapper
    : public mpl::false_
{
};

template
<
    typename T,
    typename detail::get_non_type_template_parameter_type<T>::type static_instance
>
class is_static_reference_wrapper<static_reference_wrapper<T, static_instance> >
    : public mpl::true_
{
};

namespace detail {
    template
    <
        typename T,
        // This had to be placed here and not in the struct body because that
        // bugs MSVC.
        typename StorageType = typename get_non_type_template_parameter_type<T>::type
    >
    struct static_reference_maker : public boost::reference_wrapper<T>
    {
        explicit static_reference_maker( T & t ) : reference_wrapper<T>( t ) {}

        template <StorageType object_to_reference>
        static static_reference_wrapper<T, object_to_reference> sref() { return static_reference_wrapper<T, object_to_reference>(); }
    };
} // end namespace detail

template <typename T>
detail::static_reference_maker<T> sref( T & t )
{
    return detail::static_reference_maker<T>( t );
}

template <typename T>
detail::static_reference_maker<T const> sref( T const & t )
{
    return detail::static_reference_maker<T const>( t );
}

#define BOOST_SREF( object ) boost::sref( object ). BOOST_NESTED_TEMPLATE sref<object>()

} // end namespace boost

#if defined(BOOST_MSVC)
#   pragma warning( pop )
#endif       

#endif // BOOST_FUNCTION_BASE_HEADER
