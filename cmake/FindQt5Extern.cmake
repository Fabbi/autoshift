# ======================================================
# Include directories to add to the user project:
# ======================================================

# linux does need full paths no symlinks
get_filename_component(QT_PATH "${ROOT}/extern_libs/qt" REALPATH)

if(EXISTS ${QT_PATH})
    list (APPEND CMAKE_PREFIX_PATH "${QT_PATH}")
endif(EXISTS ${QT_PATH})

# because we need to find all kinds of stuff for windows manually....
if (WIN32 AND NOT PKG_CONFIG_FOUND)
    find_package(PkgConfig QUIET REQUIRED)
    if (BUILD_STATIC)
      add_definitions(-DQT_STATICPLUGIN)
    endif()
endif()

set (CMAKE_AUTOMOC ON)
set (CMAKE_AUTOUIC ON)

if (Qt5Extern_FIND_COMPONENTS)
  foreach (component ${Qt5Extern_FIND_COMPONENTS})
    set(QT_MODULE "Qt5${component}")
    find_package (${QT_MODULE})
    list (APPEND MY_Qt5_INCLUDE_DIRS ${${QT_MODULE}_INCLUDE_DIRS})
    list (APPEND MY_Qt5_LIBRARIES Qt5::${component})

    # link static libraries on windows
    if (WIN32 AND BUILD_STATIC)
      pkg_check_modules(Qt5${component}_PkgConfig QUIET Qt5${component})

      set_target_properties(Qt5::${component} PROPERTIES
        INTERFACE_LINK_LIBRARIES "${_Qt5${component}_LIB_DEPENDENCIES};${Qt5${component}_PkgConfig_LDFLAGS}")

    endif(WIN32 AND BUILD_STATIC)
  endforeach()
endif()

# find some more plugins and libraries on windows
if (WIN32 AND BUILD_STATIC)
  get_target_property(Qt5_LOCATION Qt5::Core LOCATION)
  get_filename_component(Qt5_DIRECTORY "${Qt5_LOCATION}" DIRECTORY)
  get_filename_component(Qt5_DIRECTORY "${Qt5_DIRECTORY}" DIRECTORY)
  set(Qt5_LIBRARY_DIRECTORIES
    "${Qt5_DIRECTORY}/lib"
    "${Qt5_DIRECTORY}/plugins/platforms"
    "${Qt5_DIRECTORY}/plugins/styles"
    "${Qt5_DIRECTORY}/plugins/sqldrivers")

  set (_qt_libraries
    Qt5WindowsUIAutomationSupport
    Qt5FontDatabaseSupport
    Qt5EventDispatcherSupport
    Qt5ThemeSupport
    qwindows
    qsqlite
    )

  foreach(_lib ${_qt_libraries})
    find_library(${_lib}_LIBRARIES ${_lib}
      HINTS ${Qt5_LIBRARY_DIRECTORIES}
      NO_CMAKE_ENVIRONMENT_PATH NO_CMAKE_PATH NO_SYSTEM_ENVIRONMENT_PATH
      NO_CMAKE_SYSTEM_PATH NO_CMAKE_FIND_ROOT_PATH)

    list (INSERT MY_Qt5_LIBRARIES 0 ${${_lib}_LIBRARIES})
  endforeach()

  list (APPEND MY_Qt5_LIBRARIES wtsapi32)
  list (APPEND MY_Qt5_LIBRARIES sqlite3)
endif()
