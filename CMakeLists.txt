cmake_minimum_required( VERSION 3.27.0 )

project( Psi.Functionoid CXX )

###################
## Deps
###################

file(
  DOWNLOAD
  https://github.com/cpm-cmake/CPM.cmake/releases/latest/download/get_cpm.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/cmake/get_cpm.cmake
)
set( CPM_SOURCE_CACHE "${CMAKE_CURRENT_BINARY_DIR}/deps" )

if ( APPLE )
    set( GIT_EXECUTABLE /usr/bin/git CACHE FILEPATH "Path to git executable" FORCE )
endif()

include( ${CMAKE_CURRENT_BINARY_DIR}/cmake/get_cpm.cmake )

set( boost_ver boost-1.90.0 )
CPMAddPackage( "gh:boostorg/static_assert#${boost_ver}"   )
CPMAddPackage( "gh:boostorg/throw_exception#${boost_ver}" )
CPMAddPackage( "gh:boostorg/config#${boost_ver}"          )
CPMAddPackage( "gh:boostorg/type_traits#${boost_ver}"     )
CPMAddPackage( "gh:boostorg/assert#${boost_ver}"          )
CPMAddPackage( "gh:boostorg/core#${boost_ver}"            )
CPMAddPackage( "gh:boostorg/move#${boost_ver}"            )

CPMAddPackage( "gh:psiha/config_ex#master" )
CPMAddPackage( "gh:psiha/build#master"     )

if ( NOT TARGET Boost::boost )
    add_library( Boost::boost INTERFACE IMPORTED )
    target_link_libraries( Boost::boost INTERFACE
        Boost::config Boost::assert Boost::core Boost::move Boost::type_traits
    )
    target_include_directories( Boost::boost INTERFACE "${config_ex_SOURCE_DIR}/include" )
endif()

if ( ${CMAKE_SYSTEM_NAME} MATCHES "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
    set( PSI_USE_LINKER "lld" )
endif()
include( ${build_SOURCE_DIR}/build_options.cmake )

set( CMAKE_CXX_STANDARD 26 )
add_compile_definitions( BOOST_ALL_NO_LIB )
if ( WIN32 )
    add_compile_definitions( WIN32_LEAN_AND_MEAN NOMINMAX )
endif()

###################
## Target
###################

include( ${CMAKE_CURRENT_LIST_DIR}/functionoid.cmake )

###################
## Tests
###################

if ( PROJECT_IS_TOP_LEVEL )
    enable_testing()
    add_subdirectory( test )
endif()
