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
    concurrent_reads_test.cpp
)
target_link_libraries( functionoid_smoke PRIVATE GTest::gtest_main Psi::Functionoid )

set_target_properties( functionoid_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test" )
add_test( NAME functionoid_smoke COMMAND functionoid_smoke )

#   Its own binary, and it must stay that way: the TU defines
# PSI_FUNCTIONOID_DETAIL_INVOKE_FN_ATTR, which changes the DECLARATION of the
# vtable's invoke slot. Linking it beside TUs compiled without that macro makes
# the same class template two different types - an ODR violation whose symptom
# lands in the OTHER tests, not in this one: GCC propagates the surviving
# `pure` declaration to callers that were compiled believing the call writes
# memory, and dead-store elimination then removes stores it may not remove
# (segfaults under gcc-16 -O2 -DNDEBUG; -fno-ipa-modref / -fno-tree-dse /
# -fno-inline each hide it).
#   Keeping it separate also keeps the attribute honest: `pure` holds for this
# test's stateless target, not for callables in general.
add_executable( functionoid_vtable_attrs vtable_attrs_test.cpp )
target_link_libraries( functionoid_vtable_attrs PRIVATE GTest::gtest_main Psi::Functionoid )

set_target_properties( functionoid_vtable_attrs PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test" )
add_test( NAME functionoid_vtable_attrs COMMAND functionoid_vtable_attrs )

# Module correctness smoke test (module.cmake, PSI_FUNCTIONOID_MODULE) — only built when the
# option is on, so it adds nothing to the default (off) CI matrix.
if ( PSI_FUNCTIONOID_MODULE AND TARGET PsiFunctionoidModule )
    add_executable( functionoid_module_smoke_test EXCLUDE_FROM_ALL "${CMAKE_CURRENT_SOURCE_DIR}/module_smoke_test.cpp" )
    target_link_libraries( functionoid_module_smoke_test PRIVATE Psi::FunctionoidModule )
    set_target_properties( functionoid_module_smoke_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test"
        CXX_SCAN_FOR_MODULES ON
        DISABLE_PRECOMPILE_HEADERS ON
    )
    add_test( NAME functionoid_module_smoke_test COMMAND functionoid_module_smoke_test )
endif()
