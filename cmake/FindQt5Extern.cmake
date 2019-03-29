# ======================================================
# Include directories to add to the user project:
# ======================================================

# linux does need full paths no symlinks
get_filename_component(QT_PATH "${ROOT}/extern_libs/qt" REALPATH)

if(EXISTS ${QT_PATH})
    list (APPEND CMAKE_PREFIX_PATH "${QT_PATH}")
endif(EXISTS ${QT_PATH})

find_package (Qt5Core)
if (Qt5Core_FOUND)
  set (CMAKE_AUTOMOC ON)
  set (CMAKE_AUTOUIC ON)

  find_package(Qt5Gui QUIET)
  find_package(Qt5Widgets QUIET)
  find_package(Qt5Network QUIET)
  find_package(Qt5PrintSupport QUIET)
  # find_package(Qt5UiTools QUIET)

  if (LINUX)
    find_package(Qt5X11Extras QUIET)
  endif (LINUX)
endif()
