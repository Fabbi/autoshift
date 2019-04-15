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

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE})

# Xcode automatically generates Release and Debug Folders
if(CMAKE_GENERATOR STREQUAL Xcode)
  set(CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym")
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif(CMAKE_GENERATOR STREQUAL Xcode)


#########################
#### FIND LIBRARIES #####
set (MY_QT5_COMPONENTS
  Core
  Network
  Gui
  Widgets
  PrintSupport
  Sql
  )
if (LINUX)
  list (APPEND MY_QT5_COMPONENTS X11Extras)
endif(LINUX)

if (WIN32)
  list (APPEND MY_QT5_COMPONENTS WinExtras)
endif(WIN32)

find_package_quiet(Qt5Extern COMPONENTS ${MY_QT5_COMPONENTS} REQUIRED)
# find_package(Qt5Extern COMPONENTS ${MY_QT5_COMPONENTS} REQUIRED)

if (Qt5Core_FOUND)
  set (CMAKE_INCLUDE_CURRENT_DIR ON)
  include_directories(${MY_Qt5_INCLUDE_DIRS})

  if ("${Qt5_VERSION}" STREQUAL "")
    set (Qt5_VERSION ${Qt5Core_VERSION})
  endif()
  message (STATUS "Found Qt5 ${Qt5_VERSION}")

else()
  message (FATAL_ERROR "Qt5 not found.")
endif()
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_AUTOUIC_SEARCH_PATHS} ${ROOT}/include/ui)

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
# -fstandalone-debug
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

# add extra libs
add_subdirectory(extern_libs/QtWaitingSpinner)
include_directories(extern_libs/QtWaitingSpinner)

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
