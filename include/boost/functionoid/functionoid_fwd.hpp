////////////////////////////////////////////////////////////////////////////////
///
/// Boost.Functionoid library
/// 
/// \file functionoid_fwd.hpp
/// -------------------------
///
///  Copyright (C) Douglas Gregor 2008
///  Copyright (c) Domagoj Saric  2010 - 2016
///
///  Use, modification and distribution is subject to the Boost Software
///  License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt)
///
/// For more information, see http://www.boost.org
///
////////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_FUNCTIONOID_FWD_HPP
#define BOOST_FUNCTIONOID_FWD_HPP
//------------------------------------------------------------------------------
namespace boost
{
//------------------------------------------------------------------------------
namespace functionoid
{
//------------------------------------------------------------------------------

struct assert_on_empty;
struct nop_on_empty   ;
class  throw_on_empty ;

class bad_function_call;
  
struct std_traits;
struct default_traits;

class typed_functor;

template <typename Signature, typename Traits = default_traits>
class callable;

template<typename Signature, typename Traits>
void swap( callable<Signature, Traits> & f1, callable<Signature, Traits> & f2 ) { f1.swap( f2 ); }

//------------------------------------------------------------------------------
} // namespace functionoid
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // BOOST_FUNCTIONOID_FWD_HPP