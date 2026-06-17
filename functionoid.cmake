#############################################################################
# Psi.Functionoid — header-only callable library.
#
# Embed via include( functionoid.cmake ) from a host CMake project or standalone
# CMakeLists.txt.
#############################################################################

set( _functionoid_include "${CMAKE_CURRENT_LIST_DIR}/include" )

if ( NOT TARGET PsiFunctionoid )
    add_library( PsiFunctionoid INTERFACE )
    add_library( Psi::Functionoid ALIAS PsiFunctionoid )

    target_include_directories( PsiFunctionoid INTERFACE "${_functionoid_include}" )

    # Boost bits functionoid headers pull (host usually supplies Boost::boost).
    if ( TARGET Boost::boost )
        target_link_libraries( PsiFunctionoid INTERFACE Boost::boost )
    endif()
endif()
