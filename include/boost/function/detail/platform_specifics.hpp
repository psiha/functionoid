////////////////////////////////////////////////////////////////////////////////
///
/// \file platform_specifics.hpp
/// ----------------------------
///
///   A collection of macros that wrap platform specific details/non standard
/// extensions.
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
#ifndef platform_specifics_hpp__37C4B042_4214_41C5_96C9_814CF5243FD1
#define platform_specifics_hpp__37C4B042_4214_41C5_96C9_814CF5243FD1
//------------------------------------------------------------------------------
#include "boost/assert.hpp"
#include "boost/config.hpp"
//------------------------------------------------------------------------------
namespace boost
{
//------------------------------------------------------------------------------
namespace detail
{
//------------------------------------------------------------------------------

#if defined( BOOST_MSVC )

    #define BF_TAKES_FUNCTION_REFERENCES

    #define BF_NOVTABLE                 __declspec( novtable                 )
    #define BF_NOTHROW                  __declspec( nothrow                  )
    #define BF_NOALIAS                  __declspec( noalias                  )
    #define BF_NOTHROWNOALIAS           __declspec( nothrow noalias          )
    #define BF_NOTHROWNORESTRICTNOALIAS __declspec( nothrow restrict noalias )

    #define BF_HAS_NOTHROW

    #define BF_SELECTANY __declspec( selectany )

    #define BF_CDECL    __cdecl
    #define BF_FASTCALL __fastcall

    #define BF_POINTER_RESTRICT   __restrict
    #define BF_REFERENCE_RESTRICT

    #define BF_FORCEINLINE __forceinline
    #define BF_NOINLINE    __declspec( noinline )
    #define BF_NORETURN    __declspec( noreturn )

    #define BF_HAS_NORETURN

    #define BF_OVERRIDE override
    #define BF_SEALED   sealed

    #define BF_UNREACHABLE_CODE    BOOST_ASSERT( !"This code should not be reached." ); __assume( false );
    #define BF_ASSUME( condition ) BOOST_ASSERT( condition ); __assume( condition )

    #define BF_GNU_SPECIFIC(  expression )
    #define BF_MSVC_SPECIFIC( expression ) expression

#elif defined( __clang__ ) || defined( __GNUC__ )

    #define BF_TAKES_FUNCTION_REFERENCES

    #define BF_NOVTABLE
    #define BF_NOTHROW                  __attribute__(( nothrow ))
    #define BF_NOALIAS
    #define BF_NOTHROWNOALIAS           BF_NOTHROW
    #define BF_NOTHROWNORESTRICTNOALIAS BF_NOTHROW

    #define BF_HAS_NOTHROW

    #ifdef _WIN32
        #define BF_SELECTANY __declspec( selectany )
    #else
        #define BF_SELECTANY
    #endif

    #define BF_CDECL    __attribute__(( cdecl    ))
    #define BF_FASTCALL __attribute__(( fastcall ))

    #define BF_POINTER_RESTRICT   __restrict
    #define BF_REFERENCE_RESTRICT __restrict

    #ifdef _DEBUG
        #define BF_FORCEINLINE inline
    #else
        #define BF_FORCEINLINE inline __attribute__(( always_inline ))
    #endif
    #define BF_NOINLINE __attribute__(( noinline ))
    #define BF_NORETURN __attribute__(( noreturn ))

    #define BF_HAS_NORETURN

    #define BF_OVERRIDE
    #define BF_SEALED

    // http://en.chys.info/2010/07/counterpart-of-assume-in-gcc
    #if ( __clang__ >= 2 ) || ( ( ( __GNUC__ * 10 ) + __GNUC_MINOR__ ) >= 45 )
        #define BF_UNREACHABLE_CODE    BOOST_ASSERT( !"This code should not be reached." ); __builtin_unreachable();
        #define BF_ASSUME( condition ) BOOST_ASSERT( condition ); do { if ( !(condition) ) __builtin_unreachable(); } while ( 0 )
    #else
        #define BF_UNREACHABLE_CODE    BOOST_ASSERT( !"This code should not be reached." );
        #define BF_ASSUME( condition ) BOOST_ASSERT( condition )
    #endif

    #define BF_GNU_SPECIFIC(  expression ) expression
    #define BF_MSVC_SPECIFIC( expression )

#elif defined( __BORLANDC__ )

    #define BF_NOVTABLE
    #define BF_NOTHROW
    #define BF_NOALIAS
    #define BF_NOTHROWNOALIAS           BF_NOTHROW
    #define BF_NOTHROWNORESTRICTNOALIAS BF_NOTHROW

    #define BF_SELECTANY

    #define BF_CDECL    __cdecl
    #define BF_FASTCALL __fastcall

    #define BF_POINTER_RESTRICT
    #define BF_REFERENCE_RESTRICT

    #define BF_FORCEINLINE inline
    #define BF_NOINLINE
    #define BF_NORETURN

    #define BF_OVERRIDE
    #define BF_SEALED

    #define BF_UNREACHABLE_CODE    BOOST_ASSERT( !"This code should not be reached." );
    
    #define BF_ASSUME( condition ) BOOST_ASSERT( condition )

    #define BF_GNU_SPECIFIC(  expression )
    #define BF_MSVC_SPECIFIC( expression )

#else

    #define BF_NOVTABLE
    #define BF_NOTHROW
    #define BF_NOALIAS
    #define BF_NOTHROWNOALIAS           BF_NOTHROW
    #define BF_NOTHROWNORESTRICTNOALIAS BF_NOTHROW

    #define BF_SELECTANY

    #define BF_CDECL
    #define BF_FASTCALL

    #define BF_POINTER_RESTRICT
    #define BF_REFERENCE_RESTRICT

    #define BF_FORCEINLINE inline
    #define BF_NOINLINE
    #define BF_NORETURN

    #define BF_OVERRIDE
    #define BF_SEALED

    #define BF_UNREACHABLE_CODE    BOOST_ASSERT( !"This code should not be reached." );
    
    #define BF_ASSUME( condition ) BOOST_ASSERT( condition )

    #define BF_GNU_SPECIFIC(  expression )
    #define BF_MSVC_SPECIFIC( expression )

#endif

#define BF_DEFAULT_CASE_UNREACHABLE default: UNREACHABLE_CODE break;

//------------------------------------------------------------------------------
} // namespace detail
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // platform_specifics_hpp
