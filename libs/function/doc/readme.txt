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

You can contact me/"the author" at dsaritz at gmail.com.