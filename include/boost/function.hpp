////////////////////////////////////////////////////////////////////////////////
///
/// Boost.Functionoid library
/// 
/// \file function.hpp
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
#ifndef BOOST_FUNCTION_BASE_HEADER
#define BOOST_FUNCTION_BASE_HEADER
//------------------------------------------------------------------------------
#include <boost/config.hpp>

#include "functionoid/functionoid.hpp"
#include "functionoid/policies.hpp"

// Implementation note:
//   BOOST_FUNCTION_TARGET_FIX is still required by tests.
//                                            (03.11.2010.) (Domagoj Saric)
#define BOOST_FUNCTION_TARGET_FIX( x )
//------------------------------------------------------------------------------
namespace boost
{
//------------------------------------------------------------------------------

template <typename Signature>
using function = functionoid::callable<Signature, functionoid::std_traits>;

//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // BOOST_FUNCTION_BASE_HEADER