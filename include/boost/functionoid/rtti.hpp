////////////////////////////////////////////////////////////////////////////////
///
/// Boost.Functionoid library
/// 
/// \file rtti.hpp
/// --------------
///
///  Copyright (c) Domagoj Saric 2010 - 2019
///
///  Use, modification and distribution is subject to the Boost Software
///  License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt)
///
/// For more information, see http://www.boost.org
///
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <boost/config.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/core/ref.hpp>
#include <boost/core/typeinfo.hpp>
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
template <typename T> struct is_reference_wrapper<std::reference_wrapper<T>               > : std::true_type {};
template <typename T> struct is_reference_wrapper<std::reference_wrapper<T> const         > : std::true_type {};
template <typename T> struct is_reference_wrapper<std::reference_wrapper<T> volatile      > : std::true_type {};
template <typename T> struct is_reference_wrapper<std::reference_wrapper<T> const volatile> : std::true_type {};
//------------------------------------------------------------------------------
namespace functionoid
{
//------------------------------------------------------------------------------

// For transferring stored function object type information back to the
// interface side.
class typed_functor
{
public:
    typed_functor( typed_functor const & ) = delete;
    typed_functor( typed_functor      && ) = default;

    template <typename Functor>
    typed_functor( Functor & functor ) noexcept
        :
        p_functor_         ( std::addressof   ( functor )       ),
        type_id_           ( BOOST_CORE_TYPEID( Functor )       ),
        const_qualified_   ( std::is_const     <Functor>::value ),
        volatile_qualified_( std::is_volatile  <Functor>::value )
    {}

    core::typeinfo const & functor_type_info() const noexcept { return type_id_; }

    template <typename Functor>
    Functor * BOOST_CC_FASTCALL target() noexcept
    {
        return static_cast<Functor *>
        (
            get_functor_if_types_match
            (
                BOOST_CORE_TYPEID( Functor ),
                std::is_const     <Functor>::value,
                std::is_volatile  <Functor>::value
            )
        );
    }

private:
    void * BOOST_CC_FASTCALL get_functor_if_types_match
    (
        core::typeinfo const & other,
        bool const other_const_qualified,
        bool const other_volatile_qualified
    ) const noexcept
    {
        // Check whether we have the same type. We can add
        // cv-qualifiers, but we can't take them away.
        bool const types_match
        (
            ( type_id_ == other )
                &
            ( (!const_qualified_) | other_const_qualified )
                &
            ( (!volatile_qualified_) | other_volatile_qualified )
        );
        return types_match ? const_cast<void *>( p_functor_ ) : nullptr;
    }

private:
    void           const * const p_functor_;
    core::typeinfo const &       type_id_;
    // Whether the type is const-qualified.
    bool const const_qualified_;
    // Whether the type is volatile-qualified.
    bool const volatile_qualified_;
}; // class typed_functor

namespace detail
{
    union function_buffer_base;

    /// A helper class that can construct a typed_functor object from a
    /// function_buffer instance for given template parameters.
    template
    <
        typename ActualFunctor,
        typename StoredFunctor,
        typename FunctorManager
    >
    class functor_type_info
    {
    private:
        static ActualFunctor * actual_functor_ptr( StoredFunctor * const pStoredFunctor, std::true_type /*is_ref_wrapper*/, std::false_type /*is_member_pointer*/ ) noexcept
        {
            // needed when StoredFunctor is a static reference
            ignore_unused( pStoredFunctor );
            return pStoredFunctor->get_pointer();
        }

        static ActualFunctor * actual_functor_ptr( StoredFunctor * const pStoredFunctor, std::false_type /*is_ref_wrapper*/, std::true_type /*is_member_pointer*/ ) noexcept
        {
            static_assert( sizeof( StoredFunctor ) == sizeof( ActualFunctor ), "" );
            return static_cast<ActualFunctor *>( static_cast<void *>( pStoredFunctor ) );
        }

        static ActualFunctor * actual_functor_ptr( StoredFunctor * const pStoredFunctor, std::false_type /*is_ref_wrapper*/, std::false_type /*is_member_pointer*/ ) noexcept
        {
            return pStoredFunctor;
        }

    public:
        static typed_functor BOOST_CC_FASTCALL get_typed_functor( function_buffer_base const & buffer ) noexcept
        {
            auto          * const pFunctor      ( FunctorManager::functor_ptr( const_cast<function_buffer_base &>( buffer ) ) );
            StoredFunctor * const pStoredFunctor( static_cast<StoredFunctor *>( static_cast<void *>( pFunctor ) ) );
            ActualFunctor * const pActualFunctor( actual_functor_ptr( pStoredFunctor, std::integral_constant<bool, is_reference_wrapper<StoredFunctor>::value>(), std::is_member_pointer<ActualFunctor>() ) );
            return typed_functor( *pActualFunctor );
        }
    }; // functor_type_info

    template <typename Traits>
    class function_base;
} // namespace detail

template <typename Signature, typename Traits>
class callable;

// http://stackoverflow.com/questions/20833453/comparing-stdfunctions-for-equality
// https://stackoverflow.com/questions/3629835/why-is-stdfunction-not-equality-comparable
// http://www.boost.org/doc/libs/release/doc/html/function/faq.html
// http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2004/n1667.pdf

template <typename Signature, typename Traits, typename Functor> bool operator==( callable<Signature, Traits> const & f, std::nullptr_t ) { return f.empty(); }
template <typename Signature, typename Traits, typename Functor> bool operator==( std::nullptr_t, callable<Signature, Traits> const & f ) { return f.empty(); }

template <typename Signature, typename Traits, typename Functor> bool operator!=( callable<Signature, Traits> const & f, std::nullptr_t ) { return !f.empty(); }
template <typename Signature, typename Traits, typename Functor> bool operator!=( std::nullptr_t, callable<Signature, Traits> const & f ) { return !f.empty(); }


template <typename Traits, typename Functor> bool operator==( detail::function_base<Traits> const & f, Functor & g ) { return f.contains( g ); }
template <typename Traits, typename Functor> bool operator==( Functor & g, detail::function_base<Traits> const & f ) { return f == g; }

template <typename Traits, typename Functor> bool operator!=( detail::function_base<Traits> const & f, Functor g ) { return !f.contains( g ); }
template <typename Traits, typename Functor> bool operator!=( Functor g, detail::function_base<Traits> const & f ) { return f != g; }

template <typename Traits, typename Functor>
bool operator==( detail::function_base<Traits> const & f, reference_wrapper<Functor> const g )
{
    auto const p_f( f. template target<Functor>() );
    return p_f == g.get_pointer();
}
template <typename Traits, typename Functor>
bool operator==( reference_wrapper<Functor> const g, detail::function_base<Traits> const & f ) { return f == g; }

template <typename Traits, typename Functor>
bool operator!=( detail::function_base<Traits> const & f, reference_wrapper<Functor> const g ) { return !( f == g ); }
template <typename Traits, typename Functor>
bool operator!=( reference_wrapper<Functor> const g, detail::function_base<Traits> const & f ) { return f != g; }

//------------------------------------------------------------------------------
} // namespace functionoid
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------