#### Old notes (before conversion to C++14, renaming to functionoid/callable)

An alternate boost::function implementation that
- minimizes template and RTTI bloat
- has (much) less runtime overhead (for invocation, copy-construction and assignment)
- provides configurability through policies.

It is intended to be a drop-in replacement hence the same folder name as the
original.

For more information see the relevant discussion threads at boost.devel:
http://lists.boost.org/Archives/boost/2010/10/172593.php
http://thread.gmane.org/gmane.comp.lib.boost.devel/194514/focus=195351
http://article.gmane.org/gmane.comp.lib.boost.devel/196906.

Related undertakings:
- https://github.com/mamedev/delegates
- http://www.codeproject.com/Articles/7150/Member-Function-Pointers-and-the-Fastest-Possible
- http://www.codeproject.com/Articles/11015/The-Impossibly-Fast-C-Delegates
- http://www.codeproject.com/Articles/18389/Fast-C-Delegate-Boost-Function-drop-in-replacement
- http://www.codeproject.com/Articles/13287/Fast-C-Delegate
- http://stackoverflow.com/questions/4298408/5-years-later-is-there-something-better-than-the-fastest-possible-c-delegate
- http://www.gamedev.net/topic/549468-fast-c-delegates/
- http://codereview.stackexchange.com/questions/14730/impossibly-fast-delegate-in-c11
- https://probablydance.com/2013/01/13/a-faster-implementation-of-stdfunction
- move-only problem:
  - http://lwg.github.io/issues/lwg-defects.html#1287
  - http://stackoverflow.com/questions/25330716/move-only-version-of-stdfunction
  - https://groups.google.com/forum/#!topic/comp.lang.c++.moderated/OFAMx695NJg
  - https://groups.google.com/a/isocpp.org/forum/#!topic/std-proposals/hFBaATcX0Wo

Currently this (alternate) code is based on original code from the
boost::function official 1.43 release (but it has been rebased on the current master
as part of the move from the sandbox to github). For the originial documentation see
http://www.boost.org/doc/libs/1_43_0/doc/html/function.html.

It was tested (compiling and running real world projects, building Boost and
running Boost regression tests) with the following compilers:
 - MSVC++ 8.0 SP1, 10.0 (x86 and x64)
 - GCC 4.0.1, 4.2.1, 4.5.1
 - Clang 2.0, 2.8
 - Sun Studio 5.9, 5.10
 - VisualAge 11.1.


For the sake of brevity of future commit messages, comments etc. allow me to 
add a few terminology definitions. Looking at the boost/tr1::function<> design/
idea we can identify two 'domains':
 - the "front side" or the interface side or the "fixed type" or ABI side. The
interface that the user sees and the code that exists and executes _before_ the
call through a function pointer in the vtable (e.g. the code in operator()
before the invoke call is made, or code in operator= before calls to the manager
are made ...)
 - the "back side" or that which stands behind the 'function pointer brick wall'/
'type erasure' (the actual invoker and the actual manager), which changes with
each assignement, which, at the point of change or creation has all the type
information it wants.

A few more notes:
- the default interface and behaviour remain the same (current code should work
the same without modification with these changes, "or so it seems to me")
- the empty handler objects are required to satisfy the is_stateless<>
condition (even though the current code should work even w/o this requirement
being satisfied)
- all function objects used with boost::function<> are expected to have a
nothrow destructor
- the swap and non-trivial assignment function do not offer the strong
exception safety guarantee in the special case when one or both of the objects
at stake is or has (if it is a boost::function<> object "holding" some function
object) a function object that fits in the function_buffer but does not have a
nothrow copy constructor and therefore does not have a no-fail move
operation...in this case it can happen that the final move operation (in the
swap procedure) from the tmp object to *this can fail and then the attempt to
restore (move) the original *this from a backup location can also fail leaving
us with no valid object in *this...in this situation the *this function object
is cleared (the EmptyHandler is assigned)...as far as i could tell the original
boost::function<> implementation had the same behaviour...
- a few more ideas of what can be made a matter of policy: size and alignment
of the function buffer (perhaps some library uses only complex targets so it
would benefit if the function_buffer optimization would be eliminated all
together...on the other some other library would benefit if the buffer would be
better aligned or enlarged by "just one int" and then no memory allocation
would take place for that library ...etc...)
- there is the ever present problem of what to return from empty handlers for 
non-void and non-default constructible return types...
...
