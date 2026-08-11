# master, not the 1.17.0 release: releases up to and including it print a
# char8_t by ImplicitCast_ to char32_t, which recent clang rejects under
# -Wcharacter-conversion (googletest#4762 - fixed on master by switching to
# static_cast).
CPMAddPackage( "gh:google/googletest#main" )
include( GoogleTest )

add_executable( functionoid_smoke
    functionoid_smoke.cpp
    function_ref_test.cpp
    callable_move_only_test.cpp
    callable_invoke_test.cpp
    nested_callable_test.cpp
    vtable_attrs_test.cpp
    concurrent_reads_test.cpp
)
target_link_libraries( functionoid_smoke PRIVATE GTest::gtest_main Psi::Functionoid )

set_target_properties( functionoid_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test" )
add_test( NAME functionoid_smoke COMMAND functionoid_smoke )
