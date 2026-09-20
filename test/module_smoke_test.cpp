// Standalone correctness check for the psi.functionoid module (module.cmake,
// PSI_FUNCTIONOID_MODULE): builds a small executable that only `import`s psi.functionoid and
// exercises real callable<> usage, so a broken module boundary shows up as a build or runtime
// failure here rather than surfacing only downstream in a consumer project.
import psi.functionoid;

#include <boost/assert.hpp>
#include <cstdio>

int main()
{
    int captured{ 10 };
    psi::functionoid::callable<int()> fn{ [&]() noexcept -> int { return captured + 32; } };
    int const result{ fn() };
    BOOST_ASSERT( result == 42 );

    fn = []() noexcept -> int { return 0; };
    int const reassigned{ fn() };

    int const sum{ result + reassigned };
    std::printf( "%d\n", sum );
    return sum == 42 ? 0 : 1;
}
