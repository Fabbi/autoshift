# ======================================================
# Include directories to add to the user project:
# ======================================================

get_filename_component(BOOST_PATH "${ROOT}/extern_libs/boost" REALPATH)

if(EXISTS ${BOOST_PATH})
    # list (APPEND CMAKE_PREFIX_PATH "${BOOST_PATH}")
    # set (USE_EXTERN_BOOST ON)
endif(EXISTS ${BOOST_PATH})

# Locate Project Prerequisites
if (WIN32)
  set(Boost_USE_STATIC_LIBS        ON)
  set(Boost_USE_MULTITHREADED      ON)
endif(WIN32)
set(Boost_USE_STATIC_RUNTIME    OFF)
# set(Boost_DIR "${BOOST_PATH}")

if (USE_EXTERN_BOOST)
  message (STATUS "EXTERN BOOST")
  if (EXISTS "${BOOST_PATH}/include")
    set (BOOST_INCLUDEDIR "${BOOST_PATH}/include")
  elseif (EXISTS "${BOOST_PATH}/boost")
    set (BOOST_INCLUDEDIR "${BOOST_PATH}")
  endif()
  set(Boost_NO_SYSTEM_PATHS ON)
endif (USE_EXTERN_BOOST)


find_package (Boost COMPONENTS
  # thread
  # timer
  filesystem
  regex
  system
  program_options
  # serialization
  REQUIRED)
