////////////////////////////////////////////////////////////////////////////////
///
/// Boost.Functionoid library
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

#include <cstdint>
#include <memory>
#include <stdexcept>
//------------------------------------------------------------------------------
namespace boost
{
//------------------------------------------------------------------------------
namespace functionoid
{
//------------------------------------------------------------------------------

/// The bad_function_call exception class is thrown when an empty
/// boost::functionoid object is invoked that uses the throw_on_empty empty
/// handler.
class bad_function_call : public std::runtime_error
{
public:
    bad_function_call() : std::runtime_error( "call to empty boost::functionoid" ) {}
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
}; // struct default_traits

//------------------------------------------------------------------------------
} // namespace functionoid
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
