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
    static void BF_NORETURN throw_bad_call()
    {
        boost::throw_exception( bad_function_call() );
    }

public:
    template <class result_type>
    static result_type handle_empty_invoke()
    {
        throw_bad_call();
        #ifndef BF_HAS_NORETURN
            return detail::function::get_default_value<result_type>( is_reference<result_type>() );
        #endif // BOOST_MSVC
    }
};

///////////////////////////////////////////////////////////////////////////
struct assert_on_empty
{
	template <class result_type> static result_type handle_empty_invoke() { handle_empty_invoke<void>(); return{}; }
	template <                 > static void        handle_empty_invoke<void>() { BOOST_ASSERT_MSG(false, "Call to empty functionoid!"); }
};

///////////////////////////////////////////////////////////////////////////
struct nop_on_empty
{
	template <class result_type> static result_type handle_empty_invoke() { return{}; }
	template <                 > static void        handle_empty_invoke<void>() {}
};


enum struct support_level
{
    na,
    supported,
    nofail,
    trivial
};

template <support_level Level>
using support_level_t = std::integral_constant<support_level, Level>;

struct std_traits
{
    static constexpr auto copyable   = support_level::supported;
    static constexpr auto moveable   = support_level::supported;
    static constexpr auto destructor = support_level::nofail;
    static constexpr auto rtti       = true;
    static constexpr auto is_empty_queryable = true;

    static constexpr std::uint8_t sbo_size = 4 * sizeof( void * );
    static constexpr std::uint8_t sbo_alignment = alignof( std::max_align_t );

    using EmptyHandler = throw_on_empty;

    template <typename T>
    using Allocator = std::allocator<T>;
};

struct default_traits : std_traits
{
    static constexpr auto moveable   = support_level::nofail;
    static constexpr auto rtti       = false;
    static constexpr auto is_empty_queryable = false;

    using EmptyHandler = assert_on_empty;
};

//------------------------------------------------------------------------------
} // namespace functionoid
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------