////////////////////////////////////////////////////////////////////////////////
///
/// \file safe_bool.hpp
/// -------------------
///
/// Utility class and macros for reducing boilerplate code related to
/// 'unspecified_bool_type' implementation.
///
/// Copyright (c) Domagoj Saric 2010.
///
///  Use, modification and distribution is subject to the Boost Software License, Version 1.0.
///  (See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt)
///
/// For more information, see http://www.boost.org
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#pragma once
#ifndef safe_bool_hpp__7E590FD4_CE99_442C_82DB_8DC9CB7D3886
#define safe_bool_hpp__7E590FD4_CE99_442C_82DB_8DC9CB7D3886
//------------------------------------------------------------------------------
#include "boost/assert.hpp"
#include "boost/mpl/bool.hpp"
#include "boost/mpl/if.hpp"
//------------------------------------------------------------------------------
namespace boost
{
//------------------------------------------------------------------------------

#if (defined __SUNPRO_CC) && (__SUNPRO_CC <= 0x530) && !(defined BOOST_NO_COMPILER_CONFIG)
    // Sun C++ 5.3 can't handle the safe_bool idiom, so don't use it
    #define BOOST_NO_SAFE_BOOL
#endif

template <class T>
class safe_bool
{
#ifndef BOOST_NO_SAFE_BOOL
private:
    struct unspecified_bool_type_helper
    {
        void member_function() {};
        int  member_data_;
    };

    typedef void (unspecified_bool_type_helper::*unspecified_bool_type_function) ();
    typedef int   unspecified_bool_type_helper::*unspecified_bool_type_data        ;

    #ifdef BOOST_NO_FAST_SAFE_BOOL_FROM_DATA_MEMBER
        union fast_safe_bool
        {
            unsigned long                  plain_pointer_placeholder;
            unspecified_bool_type_function pointer_to_member        ;
        };

        //   It is assumed that if the compiler is able to fit a plain, single
        // inheritance member function pointer into sizeof( void * ) that its null
        // binary representation is identical to a plain null void pointer (all bits
        // zeroed). Without a way to check this at compile time this is asserted at
        // runtime.
        //   The above need not hold for data member pointers (e.g. MSVC++ uses -1
        // for null-data member pointers).
        typedef mpl::bool_
            <
                ( sizeof( fast_safe_bool ) <= sizeof( unsigned long ) )
            > can_use_fast_bool_hack;

    protected:
        typedef typename mpl::if_
            <
                can_use_fast_bool_hack,
                unspecified_bool_type_function,
                unspecified_bool_type_data
            >::type unspecified_bool_type;

    private:
        static
        unspecified_bool_type_function
        make_safe_bool_standard_worker( bool const bool_value, unspecified_bool_type_function const null_value )
        {
            return bool_value ? &unspecified_bool_type_helper::member_function : null_value;
        }
        static
        unspecified_bool_type_data
        make_safe_bool_standard_worker( bool const bool_value, unspecified_bool_type_data     const null_value )
        {
            return bool_value ? &unspecified_bool_type_helper::member_data_    : null_value;
        }

        static
        unspecified_bool_type make_safe_bool_worker( bool const value, mpl::false_ /*use standard version*/ )
        {
            return make_safe_bool_standard_worker( value, unspecified_bool_type( 0 ) );
        }

        static
		unspecified_bool_type make_safe_bool_worker( std::size_t const value, mpl::true_ /*use fast-hack version*/ )
        {
            fast_safe_bool const & fastSafeBool( *static_cast<fast_safe_bool const *>( static_cast<void const *>( &value ) ) );
            BOOST_ASSERT
            (
                ( !!fastSafeBool.pointer_to_member == !!value ) &&
                "The void-pointer-sized member pointer null binary"
                "representation assumption does not hold for this"
                "compiler/platform."
            );
            return fastSafeBool.pointer_to_member;
        }

    public:
        typedef unspecified_bool_type type;

        template <typename implicit_bool>
        static type make( implicit_bool const value )
        {
            return make( !!value );
        }

        static type make( bool const value )
        {
            return make_safe_bool_worker( value, can_use_fast_bool_hack() );
        }
    #else // BOOST_NO_FAST_SAFE_BOOL_FROM_DATA_MEMBER
    public:
        typedef unspecified_bool_type_data type;

        template <typename implicit_bool>
        static type make( implicit_bool const value )
        {
            return make( !!value );
        }

        static type make( bool const value )
        {
            return value ? &unspecified_bool_type_helper::member_data_ : 0;
        }
    #endif // BOOST_NO_FAST_SAFE_BOOL_FROM_DATA_MEMBER
#else // BOOST_NO_SAFE_BOOL
public:
    typedef bool unspecified_bool_type;
    typedef unspecified_bool_type type;

    template <typename implicit_bool>
    static type make( implicit_bool const value )
    {
        return !!value;
    }

    static type make( bool const value )
    {
        return value;
    }
#endif // BOOST_NO_SAFE_BOOL
}; // namespace detail


#define BOOST_SAFE_BOOL_FROM_FUNCTION( classType, constMemberFunction ) \
    operator boost::safe_bool<classType>::type() const { return boost::safe_bool<classType>::make( constMemberFunction() ); }

#define BOOST_SAFE_BOOL_FROM_DATA(     classType, constMemberData     ) \
    operator boost::safe_bool<classType>::type() const { return boost::safe_bool<classType>::make( constMemberData       ); }

#define BOOST_SAFE_BOOL_FOR_TEMPLATE_FROM_FUNCTION( classType, constMemberFunction ) \
    operator typename boost::safe_bool<classType>::type() const { return boost::safe_bool<classType>::make( constMemberFunction() ); }

#define BOOST_SAFE_BOOL_FOR_TEMPLATE_FROM_DATA(     classType, constMemberData     ) \
    operator typename boost::safe_bool<classType>::type() const { return boost::safe_bool<classType>::make( constMemberData       ); }


//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // safe_bool_hpp
