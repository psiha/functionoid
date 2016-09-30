////////////////////////////////////////////////////////////////////////////////
///
/// Boost.Functionoid library
/// 
/// \file policies.hpp
/// ------------------
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

#include <boost/core/typeinfo.hpp>
#include <boost/config.hpp>
#include <boost/function_equal.hpp>
#include <boost/throw_exception.hpp>

#include <stdexcept>
#include <cstddef>
#include <memory>
#include <type_traits>
//------------------------------------------------------------------------------
namespace boost
{
//------------------------------------------------------------------------------
namespace functionoid
{
//------------------------------------------------------------------------------

namespace detail
{
    // For transferring stored function object type information back to the
    // interface side.
    class typed_functor
        // Implementation note:
        //   GCC, Clang and VisualAge seem to have serious RVO problems. Because
        // it looks like a widespread problem (and because it is used only as a
        // redundant-copy detection tool), noncopyable is used only with compilers
        // that are known to be able to work/compile with it.
        //                                      (04.11.2010.) (Domagoj Saric)
#if defined( BOOST_MSVC ) || defined( __SUNPRO_CC )
        : noncopyable
#endif // __GNUC__
    {
    public:
        template <typename Functor>
        typed_functor( Functor & functor )
            :
            pFunctor( boost::addressof( functor ) ),
            type_id( BOOST_SP_TYPEID( Functor ) ),
            const_qualified( is_const   <Functor>::value ),
            volatile_qualified( is_volatile<Functor>::value )
        {}

        core::typeinfo const & functor_type_info() const { return type_id; }

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
          core::typeinfo const & other,
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
              ( !const_qualified || other_const_qualified )
                  &&
              ( !volatile_qualified || other_volatile_qualified )
            );
            return types_match ? const_cast<void *>( pFunctor ) : 0;
        }

    private:
        void           const * const pFunctor;
        core::typeinfo const &       type_id;
        // Whether the type is const-qualified.
        bool const const_qualified;
        // Whether the type is volatile-qualified.
        bool const volatile_qualified;
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
            ( (
                ( is_same<typename remove_cv<ActualFunctor>::type, StoredFunctor>::value ) ||
                (
                    is_msvc_exception_specified_function_pointer<ActualFunctor>::value &&
                    is_msvc_exception_specified_function_pointer<StoredFunctor>::value &&
                    sizeof( ActualFunctor ) == sizeof( StoredFunctor )
                    )
                ) );
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
        FunctorManager,
        functor_type_info<ActualFunctor, StoredFunctor, typed_manager<FunctorManager, ActualFunctor, StoredFunctor> >
    {
        typedef StoredFunctor Functor;
        static StoredFunctor * functor_ptr( function_buffer & buffer ) { return static_cast<StoredFunctor *>( static_cast<void *>( FunctorManager::functor_ptr( buffer ) ) ); }
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

      /// Retrieve the type of the stored function object.
      core::typeinfo const & target_type() const
      {
          return get_vtable().get_typed_functor( this->functor_ ).functor_type_info();
      }

      template <typename Functor>
      Functor * target()
      {
          return get_vtable().get_typed_functor( this->functor_ ).target<Functor>();
      }

      template <typename Functor>
      Functor const * target() const
      {
          return const_cast<function_base &>( *this ).target<Functor const>();
      }

      template<typename F>
      bool contains( const F& f ) const
      {
          if ( auto const p_f = this->template target<F>() )
          {
              return function_equal( *p_f, f );
          }
          return false;
      }
} // namespace detail

#if 0
inline bool operator==( const Function& f, detail::function::useless_clear_type* )
{
    return f.empty();
}

BOOST_FUNCTION_ENABLE_IF_FUNCTION
inline operator!=( const Function& f, detail::function::useless_clear_type* )
{
    return !f.empty();
}

BOOST_FUNCTION_ENABLE_IF_FUNCTION
inline operator==( detail::function::useless_clear_type*, const Function& f )
{
    return f.empty();
}

BOOST_FUNCTION_ENABLE_IF_FUNCTION
inline operator!=( detail::function::useless_clear_type*, const Function& f )
{
    return !f.empty();
}
template<typename Functor>
BOOST_FUNCTION_ENABLE_IF_NOT_INTEGRAL( Functor, bool )
operator==( const function_base& f, Functor g )
{
    if ( const Functor* fp = f.template target<Functor>() )
        return function_equal( *fp, g );
    else return false;
}

template<typename Functor>
BOOST_FUNCTION_ENABLE_IF_NOT_INTEGRAL( Functor, bool )
operator==( Functor g, const function_base& f )
{
    if ( const Functor* fp = f.template target<Functor>() )
        return function_equal( g, *fp );
    else return false;
}

template<typename Functor>
BOOST_FUNCTION_ENABLE_IF_NOT_INTEGRAL( Functor, bool )
operator!=( const function_base& f, Functor g )
{
    if ( const Functor* fp = f.template target<Functor>() )
        return !function_equal( *fp, g );
    else return true;
}

template<typename Functor>
BOOST_FUNCTION_ENABLE_IF_NOT_INTEGRAL( Functor, bool )
operator!=( Functor g, const function_base& f )
{
    if ( const Functor* fp = f.template target<Functor>() )
        return !function_equal( g, *fp );
    else return true;
}
template<typename Functor>
BOOST_FUNCTION_ENABLE_IF_NOT_INTEGRAL( Functor, bool )
operator==( const function_base& f, reference_wrapper<Functor> g )
{
    if ( const Functor* fp = f.template target<Functor>() )
        return fp == g.get_pointer();
    else return false;
}

template<typename Functor>
BOOST_FUNCTION_ENABLE_IF_NOT_INTEGRAL( Functor, bool )
operator==( reference_wrapper<Functor> g, const function_base& f )
{
    if ( const Functor* fp = f.template target<Functor>() )
        return g.get_pointer() == fp;
    else return false;
}

template<typename Functor>
BOOST_FUNCTION_ENABLE_IF_NOT_INTEGRAL( Functor, bool )
operator!=( const function_base& f, reference_wrapper<Functor> g )
{
    if ( const Functor* fp = f.template target<Functor>() )
        return fp != g.get_pointer();
    else return true;
}

template<typename Functor>
BOOST_FUNCTION_ENABLE_IF_NOT_INTEGRAL( Functor, bool )
operator!=( reference_wrapper<Functor> g, const function_base& f )
{
    if ( const Functor* fp = f.template target<Functor>() )
        return g.get_pointer() != fp;
    else return true;
}
#endif


//------------------------------------------------------------------------------
} // namespace functionoid
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------