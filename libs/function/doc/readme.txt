This is an attempt at an alternate boost::function implementation that
- minimizes template and RTTI bloat
- has less (run)time overhead
- provides configurability through policies.

It is intended to be a drop-in replacement hence the same folder name as the
original.

For more information see the relevant discussion threads at boost.devel:
http://thread.gmane.org/gmane.comp.lib.boost.devel/194514/focus=195351
http://article.gmane.org/gmane.comp.lib.boost.devel/196906.

Currently this (alternate) code is based on original code from the
boost::function official 1.40 release. For the originial documentation see
http://www.boost.org/doc/libs/1_40_0/doc/html/function.html.

Development is currently being done with MSVC++ 9.0 SP1 (with tests additionally
performed with MSVC++ 8.0 SP1 and MSVC++ 10.0b2, compiling without /W4 warnings
on all three compiler versions) so no jamfiles are provided yet.

For the sake of brevity of future commit messages, comments etc. allow me to 
add a few terminology definitions. Looking at the boost/tr1::function<> design/
idea we can identify two 'domains':
 - the "front side" or the interface side or the "fixed type" or ABI side. The
interface that the user sees and the code that exists and executes _before_ the
call through a function pointer in the vtable (e.g. the code in operator()
before the invoke call is made, or code in operator= before calls to the manager
are made ...)
 - the "back side" or that which stands behind the 'function pointer brick wall'
(the actual invoker and the actual manager), which changes with each
assignement, which, at the point of change or creation has all the type
information it wants.

You can contact me/"the author" at dsaritz at gmail.com.