cmake_minimum_required(VERSION 3.2)

## Policies
cmake_policy(SET CMP0071 NEW)
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.12)
    cmake_policy(SET CMP0074 NEW)
endif()

set (ROOT ${CMAKE_CURRENT_SOURCE_DIR})
list (APPEND CMAKE_MODULE_PATH ${ROOT}/cmake)

# include own modules
include (ListToString)
include (CheckOS)
include (FindQuiet)
include (SetDefault)
include (GetSystemIncludes)

# do we still need this?
# if (MAC)
#   set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum OS X deployment version")
#   set (CMAKE_OSX_SYSROOT "${ROOT}/extern_libs/MacOSX10.13.sdk")
# endif(MAC)

project(autoshift)

# set options from command line if present
if ("${CMAKE_RULE_MESSAGES}" STREQUAL "")
  set (CMAKE_RULE_MESSAGES OFF)
else()
  message (STATUS "using CMAKE_RULE_MESSAGES ${CMAKE_RULE_MESSAGES}")
endif()

if ("${CMAKE_VERBOSE_MAKEFILE}" STREQUAL "")
  set (CMAKE_VERBOSE_MAKEFILE 0)
else()
  message (STATUS "using CMAKE_VERBOSE_MAKEFILE ${CMAKE_VERBOSE_MAKEFILE}")
endif()

if ("${CMAKE_BUILD_TYPE}" STREQUAL "")
  set (CMAKE_BUILD_TYPE Debug)
endif()

# set build type
if(CMAKE_BUILD_TYPE MATCHES Debug)
  message (STATUS "Debug build")
  # enable testing etc
  set (CMAKE_EXPORT_COMPILE_COMMANDS 1)

  # find_package_quiet(GTest)
  # if (GTEST_FOUND)
  #   enable_testing()
  # endif()

else(CMAKE_BUILD_TYPE MATCHES Debug)
  message (STATUS "Release build")
endif()

# setup library and output paths
set (LIBRARY_TYPE STATIC)

set (BUILD_DIR "build")

set (EXECUTABLE_OUTPUT_PATH "${ROOT}/${BUILD_DIR}/bin/${CMAKE_BUILD_TYPE}")
set (LIBRARY_OUTPUT_PATH "${ROOT}/${BUILD_DIR}/lib/${CMAKE_BUILD_TYPE}")

# Xcode automatically generates Release and Debug Folders
if(CMAKE_GENERATOR STREQUAL Xcode)
  set(CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym")
  set (EXECUTABLE_OUTPUT_PATH "${ROOT}/${BUILD_DIR}/bin")
  set (LIBRARY_OUTPUT_PATH "${ROOT}/${BUILD_DIR}/lib")
endif(CMAKE_GENERATOR STREQUAL Xcode)


#########################
#### FIND LIBRARIES #####
find_package_quiet(Qt5Extern REQUIRED)

if (Qt5Core_FOUND)
  set (CMAKE_INCLUDE_CURRENT_DIR ON)
  include_directories(${Qt5Core_INCLUDE_DIRS})
  include_directories(${Qt5Network_INCLUDE_DIRS})
  include_directories(${Qt5Gui_INCLUDE_DIRS})
  include_directories(${Qt5Widgets_INCLUDE_DIRS})
  include_directories(${Qt5PrintSupport_INCLUDE_DIRS})

  if (LINUX)
    include_directories(${QT5X11Extras_INCLUDE_DIRS})
  endif (LINUX)

  if ("${Qt5_VERSION}" STREQUAL "")
    set (Qt5_VERSION ${Qt5Core_VERSION})
  endif()
  message (STATUS "Found Qt5 ${Qt5_VERSION}")

else()
  message (FATAL_ERROR "Qt5 not found.")
endif()

## boost
# find_package_quiet(BoostExtern REQUIRED)
find_package(BoostExtern REQUIRED)

if (Boost_FOUND)
  # message (STATUS "Found Boost ${Boost_MAJOR_VERSION}.${Boost_MINOR_VERSION}.${Boost_SUBMINOR_VERSION}")
  include_directories(${Boost_INCLUDE_DIRS})
  link_directories(${Boost_LIBRARY_DIRS})
  set (MY_Boost_LIBRARIES ${Boost_LIBRARIES})
else()
  message (FATAL_ERROR "Boost not found.")
endif()

#############
### FLAGS ###

set (FLAGS_STR "")

if (MSVC)
else(MSVC)
  set (CMAKE_POSITION_INDEPENDENT_CODE ON) # -fPIC
  set (CMAKE_CXX_STANDARD 11)              # -std=gnu++11/c++11

  if (CMAKE_BUILD_TYPE MATCHES Release)
    set (FLAGS
      -O2
      -ffast-math
      )

    # DEBUG
  else(CMAKE_BUILD_TYPE MATCHES Release)
    set (FLAGS
      # -g
      -O0
      -w
      -Winline
      )
  endif(CMAKE_BUILD_TYPE MATCHES Release)
  if (NOT MAC)
    list (APPEND FLAGS -pg)
  endif (NOT MAC)
  string (REPLACE ";" " " FLAGS_STR "${FLAGS}")
endif(MSVC)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FLAGS_STR}")

set_default(NODEBUG no STRING "no debug output")
set_property(CACHE NODEBUG PROPERTY STRINGS no yes)
set_default(NOINFO no STRING "no info output")
set_property(CACHE NOINFO PROPERTY STRINGS no yes)

if (NODEBUG MATCHES yes)
  add_definitions(-DNODEBUG)
endif()
if (NOINFO MATCHES yes)
  add_definitions(-DNOINFO)
endif()

# setup directories
include_directories(include)

# add source
add_subdirectory(src)

# import tests if possible
if (CMAKE_BUILD_TYPE MATCHES Debug)
  if (GTEST_FOUND)
    include_directories(${GTEST_INCLUDE_DIRS})
    add_subdirectory(tests)
  endif (GTEST_FOUND)
endif(CMAKE_BUILD_TYPE MATCHES Debug)
