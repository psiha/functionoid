CPMAddPackage( "gh:google/googletest@1.15.2" )
include( GoogleTest )

add_executable( functionoid_smoke
    functionoid_smoke.cpp
    function_ref_test.cpp
    callable_move_only_test.cpp
    callable_invoke_test.cpp
    vtable_attrs_test.cpp
)
target_link_libraries( functionoid_smoke PRIVATE GTest::gtest_main Psi::Functionoid )

set_target_properties( functionoid_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test" )
add_test( NAME functionoid_smoke COMMAND functionoid_smoke )
