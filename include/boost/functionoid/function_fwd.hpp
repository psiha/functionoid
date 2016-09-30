////////////////////////////////////////////////////////////////////////////////
///
/// Boost.Function library
/// 
/// \file function_fwd.hpp
/// ----------------------
///
///  Copyright (C) Douglas Gregor 2008
///  Copyright (c) Domagoj Saric  2010
///
///  Use, modification and distribution is subject to the Boost Software
///  License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt)
///
/// For more information, see http://www.boost.org
///
////////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_FUNCTION_FWD_HPP
#define BOOST_FUNCTION_FWD_HPP
#include <boost/config.hpp>

namespace boost {
  class assert_on_empty;
  class nop_on_empty   ;
  class throw_on_empty ;

  class bad_function_call;

  struct empty_handler_t {};
  struct is_no_throw_t   {};

  typedef mpl::map0<> default_policies;

  // Preferred syntax
  template<typename Signature, class PolicyList = default_policies> class function;

  template<typename Signature, class PolicyList>
  inline void swap(function<Signature, PolicyList>& f1, function<Signature, PolicyList>& f2)
  {
    f1.swap(f2);
  }
}

#endif
