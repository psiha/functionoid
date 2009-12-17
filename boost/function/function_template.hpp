// Boost.Function library

//  Copyright Douglas Gregor 2001-2006
//  Copyright Emil Dotchevski 2007
//  Use, modification and distribution is subject to the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

// For more information, see http://www.boost.org

// Note: this header is a header template and must NOT have multiple-inclusion
// protection.
#include <boost/function/detail/prologue.hpp>
#include <boost/detail/no_exceptions_support.hpp>

#if defined(BOOST_MSVC)
#   pragma warning( push )
#   pragma warning( disable : 4100 ) // "unreferenced formal parameter" (for
                                     // base_empty_handler::operator() that
                                     // ignores all of its parameters
#   pragma warning( disable : 4127 ) // "conditional expression is constant"
#   pragma warning( disable : 4512 ) // "assignment operator could not be generated" (exact_signature_mem_invoker)
#   pragma warning( disable : 4702 ) // "unreachable code" (when calling return
                                     // base_empty_handler::operator() when that
                                     // operator does not return (throws)
#endif       

#define BOOST_FUNCTION_TEMPLATE_PARMS BOOST_PP_ENUM_PARAMS(BOOST_FUNCTION_NUM_ARGS, typename T)

#define BOOST_FUNCTION_TEMPLATE_ARGS BOOST_PP_ENUM_PARAMS(BOOST_FUNCTION_NUM_ARGS, T)

#define BOOST_FUNCTION_PARM(J,I,D) BOOST_PP_CAT(T,I) BOOST_PP_CAT(a,I)

#define BOOST_FUNCTION_PARMS BOOST_PP_ENUM(BOOST_FUNCTION_NUM_ARGS,BOOST_FUNCTION_PARM,BOOST_PP_EMPTY)

#define BOOST_FUNCTION_ARGS BOOST_PP_ENUM_PARAMS(BOOST_FUNCTION_NUM_ARGS, a)

#define BOOST_FUNCTION_ARG_TYPE(J,I,D) \
  typedef BOOST_PP_CAT(T,I) BOOST_PP_CAT(BOOST_PP_CAT(arg, BOOST_PP_INC(I)),_type);

#define BOOST_FUNCTION_ARG_TYPES BOOST_PP_REPEAT(BOOST_FUNCTION_NUM_ARGS,BOOST_FUNCTION_ARG_TYPE,BOOST_PP_EMPTY)

// Comma if nonzero number of arguments
#if BOOST_FUNCTION_NUM_ARGS == 0
#  define BOOST_FUNCTION_COMMA
#else
#  define BOOST_FUNCTION_COMMA ,
#endif // BOOST_FUNCTION_NUM_ARGS > 0

// Class names used in this version of the code
#define BOOST_FUNCTION_FUNCTION BOOST_JOIN(function,BOOST_FUNCTION_NUM_ARGS)
#define BOOST_FUNCTION_FUNCTION_OBJ_INVOKER \
  BOOST_JOIN(function_obj_invoker,BOOST_FUNCTION_NUM_ARGS)
#define BOOST_FUNCTION_VOID_FUNCTION_OBJ_INVOKER \
  BOOST_JOIN(void_function_obj_invoker,BOOST_FUNCTION_NUM_ARGS)

#ifndef BOOST_NO_VOID_RETURNS
#  define BOOST_FUNCTION_VOID_RETURN_TYPE void
#  define BOOST_FUNCTION_RETURN(X) X
#else
#  define BOOST_FUNCTION_VOID_RETURN_TYPE boost::detail::function::unusable
#  define BOOST_FUNCTION_RETURN(X) X; return BOOST_FUNCTION_VOID_RETURN_TYPE ()
#endif

namespace boost {
  namespace detail {
    namespace function {

     template
     <
        typename FunctionObj,
        typename FunctionObjManager,
        typename R BOOST_FUNCTION_COMMA BOOST_FUNCTION_TEMPLATE_PARMS
     >
     struct BOOST_FUNCTION_FUNCTION_OBJ_INVOKER : public function_buffer_holder
      {
        R invoke( BOOST_FUNCTION_PARMS )
        {
            // We provide the invoker with a manager with a minimum amount of
            // type information (because it already knows the stored function
            // object it works with, it only needs to get its address from a
            // function_buffer object). Because of this we must cast the pointer
            // returned by FunctionObjManager::functor_ptr() because it can be
            // a plain void * in case of the trivial managers. In case of the
            // trivial ptr manager it is even a void * * so a double static_cast
            // (or a reinterpret_cast) is necessary.
            FunctionObj & functionObject
            (
                *static_cast<FunctionObj *>
                (
                    static_cast<void *>
                    (
                        FunctionObjManager::functor_ptr( buffer )
                    )
                )
            );
            // unwrap_ref is needed because boost::reference_wrapper<T>, unlike
            // the one from std::tr1,  does not support callable objects
            return unwrap_ref( functionObject )( BOOST_FUNCTION_ARGS );
        }
      };

      template
      <
        typename FunctionObj,
        typename FunctionObjManager,
        typename R BOOST_FUNCTION_COMMA BOOST_FUNCTION_TEMPLATE_PARMS
      >
      struct BOOST_FUNCTION_VOID_FUNCTION_OBJ_INVOKER : public function_buffer_holder
      {
          BOOST_FUNCTION_VOID_RETURN_TYPE invoke(BOOST_FUNCTION_PARMS)
          {
              // see the above comments for the non-void invoker
              FunctionObj & functionObject( *static_cast<FunctionObj *>( static_cast<void *>( FunctionObjManager::functor_ptr( buffer ) ) ) );
              BOOST_FUNCTION_RETURN( unwrap_ref( functionObject )( BOOST_FUNCTION_ARGS ) );
          }
      };
    } // end namespace function
  } // end namespace detail

  template
  <
    typename R BOOST_FUNCTION_COMMA BOOST_FUNCTION_TEMPLATE_PARMS,
    class PolicyList
  >
  class BOOST_FUNCTION_FUNCTION : public function_base

#if BOOST_FUNCTION_NUM_ARGS == 1

    , public std::unary_function<T0,R>

#elif BOOST_FUNCTION_NUM_ARGS == 2

    , public std::binary_function<T0,T1,R>

#endif

  {
  private: // Actual policies deduction section.
    //mpl::at<AssocSeq,Key,Default> does not yet exist so...:
    typedef typename mpl::at<PolicyList      , EmptyHandler>::type user_specified_empty_handler;
    typedef typename mpl::at<default_policies, EmptyHandler>::type default_empty_handler;

    typedef typename mpl::at<PolicyList      , Nothrow>::type user_specified_nothrow_policy;
    typedef typename mpl::at<default_policies, Nothrow>::type default_nothrow_policy;

  public: // Public typedefs/introspection section.
#ifndef BOOST_NO_VOID_RETURNS
    typedef R         result_type;
#else
    typedef typename boost::detail::function::function_return_type<R>::type
      result_type;
#endif // BOOST_NO_VOID_RETURNS

    BOOST_STATIC_CONSTANT(int, args = BOOST_FUNCTION_NUM_ARGS);

    // add signature for boost::lambda
    template<typename Args>
    struct sig
    {
      typedef result_type type;
    };

#if BOOST_FUNCTION_NUM_ARGS == 1
    typedef T0 argument_type;
#elif BOOST_FUNCTION_NUM_ARGS == 2
    typedef T0 first_argument_type;
    typedef T1 second_argument_type;
#endif

    BOOST_STATIC_CONSTANT(int, arity = BOOST_FUNCTION_NUM_ARGS);
    BOOST_FUNCTION_ARG_TYPES

    typedef BOOST_FUNCTION_FUNCTION self_type;

    typedef typename mpl::if_
            <
                is_same<user_specified_empty_handler, mpl::void_>,
                default_empty_handler,
                user_specified_empty_handler
            >::type base_empty_handler;

    BOOST_STATIC_ASSERT( is_stateless<base_empty_handler>::value );

  // The nothrow policy and runtime throw detection functionality works only in
  // release mode/with optimizations on. It should also work in debug for plain
  // function pointers with compilers that properly implement the 'exception
  // specification shadow type system' (MSVC 9.0 SP1 does not) - this path yet
  // needs to be tested and properly (re)implemented.
  #ifndef _DEBUG
    typedef typename mpl::if_
        <
            is_same<user_specified_nothrow_policy, mpl::void_>,
            default_nothrow_policy,
            user_specified_nothrow_policy
        >::type nothrow_policy;
  #else
    typedef mpl::false_ nothrow_policy;
  #endif

    typedef R signature_type ( BOOST_FUNCTION_TEMPLATE_ARGS );

  private: // Private implementation types.
    //  We need a specific thin wrapper around the base empty handler that will
    // just consume all the parameters. This way the base empty handler can have
    // one plain simple operator(). As part of ant-code-bloat measures,
    // my_empty_handler is used only when really necessary (with the invoker),
    // otherwise the base_empty_handler type is used.
    struct my_empty_handler : public base_empty_handler
    {
        R operator()( BOOST_FUNCTION_PARMS ) const
        {
            return base_empty_handler::operator()<R>();
        }
    };

    typedef detail::function::vtable vtable_type;

    struct clear_type {};

  public: // Public function interface.

    BOOST_FUNCTION_FUNCTION() : function_base( empty_handler_vtable() ) {}

    // MSVC chokes if the following two constructors are collapsed into
    // one with a default parameter.
    template<typename Functor>
    BOOST_FUNCTION_FUNCTION(Functor BOOST_FUNCTION_TARGET_FIX(const &) f
#ifndef BOOST_NO_SFINAE
                            ,typename enable_if_c<
                            (boost::type_traits::ice_not<
                             (is_integral<Functor>::value)>::value),
                                        int>::type = 0
#endif // BOOST_NO_SFINAE
                            ) :
      function_base( empty_handler_vtable() )
    {
      this->do_assign<true, Functor>( f );
    }

/*    ALLOCATOR SUPPORT TEMPORARILY COMMENTED OUT
    template<typename Functor,typename Allocator>
    BOOST_FUNCTION_FUNCTION(Functor BOOST_FUNCTION_TARGET_FIX(const &) f, Allocator a
#ifndef BOOST_NO_SFINAE
                            ,typename enable_if_c<
                            (boost::type_traits::ice_not<
                             (is_integral<Functor>::value)>::value),
                                        int>::type = 0
#endif // BOOST_NO_SFINAE
                            ) :
      function_base( empty_handler_vtable() )
    {
      this->assign_to_a(f,a);
    }
*/

#ifndef BOOST_NO_SFINAE
    BOOST_FUNCTION_FUNCTION(clear_type*) : function_base( empty_handler_vtable() ) { }
#else
    BOOST_FUNCTION_FUNCTION(int zero) : function_base( empty_handler_vtable() )
    {
      BOOST_ASSERT(zero == 0);
    }
#endif

    BOOST_FUNCTION_FUNCTION(const BOOST_FUNCTION_FUNCTION& f) : function_base( empty_handler_vtable() )
    {
      this->do_assign<true>( f );
    }

    /// Determine if the function is empty (i.e., has empty target).
    bool empty() const { return pVTable == &empty_handler_vtable(); }

    /// Clear out a target (replace it with an empty handler), if there is one.
    void clear()
    {
        function_base::clear<base_empty_handler>( empty_handler_vtable() );
    }

    template<typename FunctionObj>
    void assign( FunctionObj const & f )
    {
        this->do_assign<false, FunctionObj>( f );
    }

    template <signature_type * f>
    void assign()
    {
        this->assign( detail::static_reference_maker<signature_type *>::sref<f>() );
    }

    template <class AClass, R (AClass::*mmf)(BOOST_FUNCTION_TEMPLATE_ARGS)>
    void assign( AClass & object )
    {
        class exact_signature_mem_invoker
        {
        public:
            exact_signature_mem_invoker( AClass & object ) : object( object ) {}
            result_type operator()( BOOST_FUNCTION_PARMS ) const { return (object.*mmf)( BOOST_FUNCTION_ARGS ); }
        private:
            AClass & object;
        };
        this->assign( exact_signature_mem_invoker( object ) );

/* Just an experiment to show how can the current boost::mem_fn implementation
   be 'hacked' to work with custom mem_fn pointer holders (in this case a static
   reference) and/to implement the above.
        typedef static_reference_wrapper
                <
                    R (AClass::*)(BOOST_FUNCTION_TEMPLATE_ARGS),
                    mmf
                > static_mem_fn_reference;

        typedef BOOST_JOIN( detail::function::mem_fn_wrapper::mf, BOOST_FUNCTION_NUM_ARGS )
                <
                    R,
                    AClass
                    BOOST_FUNCTION_COMMA
                    BOOST_FUNCTION_TEMPLATE_ARGS,
                    static_mem_fn_reference
                > mem_fn_wrapper;

        this->assign( bind( mem_fn_wrapper( static_mem_fn_reference() ), &object, _1 ) );
*/
    }

    result_type operator()(BOOST_FUNCTION_PARMS) const
    {
        return invoke( BOOST_FUNCTION_ARGS BOOST_FUNCTION_COMMA nothrow_policy() );
    }


    // ...this one is perhaps no longer needed (the one below can probably "take
    // over"...
    BOOST_FUNCTION_FUNCTION & operator=( BOOST_FUNCTION_FUNCTION const & f )
    {
        this->assign( f )
        return *this;
    }

    template<typename Functor>
#ifndef BOOST_NO_SFINAE
    typename enable_if_c<
               (boost::type_traits::ice_not<
                 (is_integral<Functor>::value)>::value),
               BOOST_FUNCTION_FUNCTION&>::type
#else
    BOOST_FUNCTION_FUNCTION&
#endif
    operator=(Functor BOOST_FUNCTION_TARGET_FIX(const &) f)
    {
      this->assign(f);
      return *this;
    }

/*    ALLOCATOR SUPPORT TEMPORARILY COMMENTED OUT
    template<typename Functor,typename Allocator>
    void assign(Functor BOOST_FUNCTION_TARGET_FIX(const &) f, Allocator a)
    {
      this->assign_to_a(f,a);
    }
*/

#ifndef BOOST_NO_SFINAE
    BOOST_FUNCTION_FUNCTION& operator=(clear_type*)
    {
      this->clear();
      return *this;
    }
#else
    BOOST_FUNCTION_FUNCTION& operator=(int zero)
    {
      BOOST_ASSERT(zero == 0);
      this->clear();
      return *this;
    }
#endif

    void swap(BOOST_FUNCTION_FUNCTION& other)
    {
        BOOST_STATIC_ASSERT( sizeof( BOOST_FUNCTION_FUNCTION ) == sizeof( function_base ) );
        return function_base::swap<base_empty_handler>( other, empty_handler_vtable() );
    }

#if (defined __SUNPRO_CC) && (__SUNPRO_CC <= 0x530) && !(defined BOOST_NO_COMPILER_CONFIG)
    // Sun C++ 5.3 can't handle the safe_bool idiom, so don't use it
    operator bool () const { return !this->empty(); }
#else
  public:
    operator safe_bool () const
      { return (this->empty())? 0 : &dummy::nonnull; }

    bool operator!() const { return this->empty(); }
#endif

private:
    static vtable_type const & empty_handler_vtable() { return vtable_for_functor<base_empty_handler>( my_empty_handler() ); }

    template <class F>
    static bool nothrow_test( F & f BOOST_FUNCTION_COMMA BOOST_FUNCTION_PARMS )
    {
        f( BOOST_FUNCTION_ARGS );
        return true;
    }

    template <class F>
    static bool throw_test( F & f BOOST_FUNCTION_COMMA BOOST_FUNCTION_PARMS )
    {
        try
        {
            f( BOOST_FUNCTION_ARGS );
            return true;
        }
        catch(...)
        {
            return false;
        }
    }

    template <class F>
    static bool is_nothrow()
    {
        typedef bool ( throw_test_signature ) ( unwrap_reference<F>::type & BOOST_FUNCTION_COMMA BOOST_FUNCTION_TEMPLATE_ARGS );
        return detail::function::is_nothrow_helper
                <
                    throw_test_signature,
                    &nothrow_test<unwrap_reference<F>::type>,
                    &throw_test  <unwrap_reference<F>::type>
                >::is_nothrow;
    }


    #ifdef BOOST_MSVC
        __declspec( nothrow )
    #endif
    result_type invoke(BOOST_FUNCTION_PARMS BOOST_FUNCTION_COMMA mpl::true_ /*no throw invoker*/) const
    #ifndef BOOST_MSVC
        throw()
    #endif
    {
        typedef result_type (detail::function::function_buffer_holder::* invoker_type)(BOOST_FUNCTION_TEMPLATE_ARGS);
        return (reinterpret_cast<detail::function::function_buffer_holder &>( this->functor ).*get_vtable().invoker<invoker_type>())
               (BOOST_FUNCTION_ARGS);
    }

    result_type invoke(BOOST_FUNCTION_PARMS BOOST_FUNCTION_COMMA mpl::false_ /*throwable invoker*/) const
    {
        typedef result_type (detail::function::function_buffer_holder::* invoker_type)(BOOST_FUNCTION_TEMPLATE_ARGS);
        return (reinterpret_cast<detail::function::function_buffer_holder &>( this->functor ).*get_vtable().invoker<invoker_type>())
               (BOOST_FUNCTION_ARGS);
    }

    template <typename ActualFunctor, typename StoredFunctor>
    typename enable_if<is_base_of<function_base, StoredFunctor>, vtable_type const &>::type
    static vtable_for_functor( StoredFunctor const & functor )
    {
        return functor.get_vtable();
    }

    template <typename ActualFunctor, typename StoredFunctor>
    typename disable_if<is_base_of<function_base, StoredFunctor>, vtable_type const &>::type
    static vtable_for_functor( StoredFunctor const & /*functor*/ )
    {
      using namespace detail::function;

      // A minimally typed manager is used for the invoker (anti-code-bloat).
      typedef typename get_functor_manager
              <
                StoredFunctor
              >::type invoker_manager_type;

      // For the empty handler we use the manager for the base_empty_handler not
      // my_empty_handler (anti-code-bloat) because they only differ in the
      // operator() member function which is irrelevant for/not used by the
      // manager.
      typedef typename get_typed_functor_manager
              <
                ActualFunctor,
                typename mpl::if_
                <
                  is_same<ActualFunctor, base_empty_handler>,
                  ActualFunctor,
                  StoredFunctor
                >::type
              >::type manager_type;

      typedef typename mpl::if_
              <
                is_void<R>,
                BOOST_FUNCTION_VOID_FUNCTION_OBJ_INVOKER
                <
                    StoredFunctor,
                    invoker_manager_type,
                    R BOOST_FUNCTION_COMMA BOOST_FUNCTION_TEMPLATE_ARGS
                >,
                BOOST_FUNCTION_FUNCTION_OBJ_INVOKER
                <
                    StoredFunctor,
                    invoker_manager_type,
                    R BOOST_FUNCTION_COMMA BOOST_FUNCTION_TEMPLATE_ARGS
                >
            >::type invoker_type;

      return vtable_holder<invoker_type, manager_type>::stored_vtable;
    }

/*    ALLOCATOR SUPPORT TEMPORARILY COMMENTED OUT
    template<typename Functor,typename Allocator>
    static vtable_type const & vtable_for_functor_a()
    {
      using detail::function::vtable_base;

      typedef typename detail::function::get_function_tag<Functor>::type tag;
      typedef detail::function::BOOST_FUNCTION_GET_INVOKER<tag> get_invoker;
      typedef typename get_invoker::
                         template apply_a<Functor, R BOOST_FUNCTION_COMMA 
                         BOOST_FUNCTION_TEMPLATE_ARGS,
                         Allocator>
        handler_type;
      
      typedef typename handler_type::invoker_type                     invoker_type;
      typedef typename boost::mpl::if_
              <
                boost::is_base_of<base_empty_handler, Functor>,
                detail::function::functor_manager<base_empty_handler>,
                detail::function::functor_manager_a<Functor, Allocator>
              >::type manager_type;
      
      return static_cast<vtable_type const &>( detail::function::vtable_holder<invoker_type, manager_type>::stored_vtable );
    }
*/
    // ...direct actually means whether to skip pre-destruction (when not
    // assigning but constructing) so it should probably be renamed to
    // pre_destroy or the whole thing solved in some smarter way...
    template<bool direct, typename FunctionObj>
    void do_assign( FunctionObj const & f )
    {
        typedef typename detail::function::get_function_tag<FunctionObj>::type tag;
        dispatch_assign<direct, FunctionObj>( f, tag() );
    }

    template<bool direct, typename FunctionObj>
    void dispatch_assign( FunctionObj const & f, detail::function::function_obj_tag     ) { do_assign<direct, FunctionObj               >( f      ,        f   ); }
    template<bool direct, typename FunctionObj>
    void dispatch_assign( FunctionObj const & f, detail::function::function_ptr_tag     ) { do_assign<direct, FunctionObj               >( f      ,        f   ); }
    // DPG TBD: Add explicit support for member function
    // objects, so we invoke through mem_fn() but we retain the
    // right target_type() values.
    template<bool direct, typename FunctionObj>
    void dispatch_assign( FunctionObj const & f, detail::function::member_ptr_tag       ) { do_assign<direct, FunctionObj               >( f      , mem_fn( f ) ); }
    template<bool direct, typename FunctionObj>
    void dispatch_assign( FunctionObj const & f, detail::function::function_obj_ref_tag ) { do_assign<direct, typename FunctionObj::type>( f.get(),         f   ); }

    template<bool direct, typename ActualFunctor, typename StoredFunctor>
    void do_assign( ActualFunctor const & original_functor, StoredFunctor const & stored_functor )
    {
        if
        (
            ( nothrow_policy::value == true                              ) &&
            // Assume other copies of the same type of boost::function did
            // their job in detecting (no)throw function objects.
            // This function-object-type-specific detection should probably
            // be moved into the tag dispatched assigns (where 'exception
            // specification shadow type system' detection for function
            // pointer should be implemented also).
            ( !is_base_of<BOOST_FUNCTION_FUNCTION, ActualFunctor>::value ) &&
            ( !is_nothrow<StoredFunctor>()                               )
        )
        {
            // This implementation inserts two calls to destroy() (the one here
            // for the clear and the one, for the else case, further down in 
            // assign) when the nothrow policy is specified...this should be
            // fixed...
            assert( is_nothrow<my_empty_handler>() );
            base_empty_handler const emptyHandler;
            function_base::assign<direct, base_empty_handler>
            (
                emptyHandler,
                empty_handler_vtable(),
                empty_handler_vtable()
            );
            emptyHandler.operator()<R>();
        }
        else
            function_base::assign<direct, base_empty_handler>
            (
                stored_functor,
                vtable_for_functor<ActualFunctor>( stored_functor ),
                empty_handler_vtable()
            );
    }


/*    ALLOCATOR SUPPORT TEMPORARILY COMMENTED OUT
    template<typename Functor,typename Allocator>
    void assign_to_a(Functor f,Allocator a) { assert( !"Implement" ); }
*/
  };

  template<typename R BOOST_FUNCTION_COMMA BOOST_FUNCTION_TEMPLATE_PARMS>
  inline void swap(BOOST_FUNCTION_FUNCTION<
                     R BOOST_FUNCTION_COMMA
                     BOOST_FUNCTION_TEMPLATE_ARGS
                   >& f1,
                   BOOST_FUNCTION_FUNCTION<
                     R BOOST_FUNCTION_COMMA
                     BOOST_FUNCTION_TEMPLATE_ARGS
                   >& f2)
  {
    f1.swap(f2);
  }

// Poison comparisons between boost::function objects of the same type.
template<typename R BOOST_FUNCTION_COMMA BOOST_FUNCTION_TEMPLATE_PARMS>
  void operator==(const BOOST_FUNCTION_FUNCTION<
                          R BOOST_FUNCTION_COMMA
                          BOOST_FUNCTION_TEMPLATE_ARGS>&,
                  const BOOST_FUNCTION_FUNCTION<
                          R BOOST_FUNCTION_COMMA
                          BOOST_FUNCTION_TEMPLATE_ARGS>&);
template<typename R BOOST_FUNCTION_COMMA BOOST_FUNCTION_TEMPLATE_PARMS>
  void operator!=(const BOOST_FUNCTION_FUNCTION<
                          R BOOST_FUNCTION_COMMA
                          BOOST_FUNCTION_TEMPLATE_ARGS>&,
                  const BOOST_FUNCTION_FUNCTION<
                          R BOOST_FUNCTION_COMMA
                          BOOST_FUNCTION_TEMPLATE_ARGS>& );

#if !defined(BOOST_FUNCTION_NO_FUNCTION_TYPE_SYNTAX)

#if BOOST_FUNCTION_NUM_ARGS == 0
#define BOOST_FUNCTION_PARTIAL_SPEC R (void), PolicyList
#else
#define BOOST_FUNCTION_PARTIAL_SPEC R (BOOST_PP_ENUM_PARAMS(BOOST_FUNCTION_NUM_ARGS,T)), PolicyList
#endif

template<typename R BOOST_FUNCTION_COMMA
         BOOST_FUNCTION_TEMPLATE_PARMS, class PolicyList>
class function<BOOST_FUNCTION_PARTIAL_SPEC>
  : public BOOST_FUNCTION_FUNCTION<R BOOST_FUNCTION_COMMA BOOST_FUNCTION_TEMPLATE_ARGS, PolicyList>
{
  typedef BOOST_FUNCTION_FUNCTION<R BOOST_FUNCTION_COMMA BOOST_FUNCTION_TEMPLATE_ARGS, PolicyList> base_type;
  typedef function self_type;

  struct clear_type {};

public:
  function() {}

  template<typename Functor>
  function(Functor const & f
#ifndef BOOST_NO_SFINAE
           ,typename enable_if_c<
                            (boost::type_traits::ice_not<
                          (is_integral<Functor>::value)>::value),
                       int>::type = 0
#endif
           ) :
    base_type(f)
  {
  }
/*    ALLOCATOR SUPPORT TEMPORARILY COMMENTED OUT
  template<typename Functor,typename Allocator>
  function(Functor f, Allocator a
#ifndef BOOST_NO_SFINAE
           ,typename enable_if_c<
                            (boost::type_traits::ice_not<
                          (is_integral<Functor>::value)>::value),
                       int>::type = 0
#endif
           ) :
    base_type(f,a)
  {
  }
*/
#ifndef BOOST_NO_SFINAE
  function(clear_type*) : base_type() {}
#endif

  function(const self_type& f) : base_type(static_cast<const base_type&>(f)){}

  function(const base_type& f) : base_type(static_cast<const base_type&>(f)){}

  // The distinction between when to use BOOST_FUNCTION_FUNCTION and
  // when to use self_type is obnoxious. MSVC cannot handle self_type as
  // the return type of these assignment operators, but Borland C++ cannot
  // handle BOOST_FUNCTION_FUNCTION as the type of the temporary to
  // construct.
  self_type& operator=(const self_type& f)
  {
      this->assign( f );
      return *this;
  }

  template<typename Functor>
#ifndef BOOST_NO_SFINAE
  typename enable_if_c<
                            (boost::type_traits::ice_not<
                         (is_integral<Functor>::value)>::value),
                      self_type&>::type
#else
  self_type&
#endif
  operator=(Functor const & f)
  {
    this->assign<Functor>( f );
    return *this;
  }

#ifndef BOOST_NO_SFINAE
  self_type& operator=(clear_type*)
  {
    this->clear();
    return *this;
  }
#endif
};

#undef BOOST_FUNCTION_PARTIAL_SPEC
#endif // have partial specialization

} // end namespace boost

// Cleanup after ourselves...
#undef BOOST_FUNCTION_COMMA
#undef BOOST_FUNCTION_FUNCTION
#undef BOOST_FUNCTION_FUNCTION_OBJ_INVOKER
#undef BOOST_FUNCTION_VOID_FUNCTION_OBJ_INVOKER
#undef BOOST_FUNCTION_TEMPLATE_PARMS
#undef BOOST_FUNCTION_TEMPLATE_ARGS
#undef BOOST_FUNCTION_PARMS
#undef BOOST_FUNCTION_PARM
#undef BOOST_FUNCTION_ARGS
#undef BOOST_FUNCTION_ARG_TYPE
#undef BOOST_FUNCTION_ARG_TYPES
#undef BOOST_FUNCTION_VOID_RETURN_TYPE
#undef BOOST_FUNCTION_RETURN

#if defined(BOOST_MSVC)
#   pragma warning( pop )
#endif       
