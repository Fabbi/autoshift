# ======================================================
# Include directories to add to the user project:
# ======================================================

# linux does need full paths no symlinks
get_filename_component(QT_PATH "${ROOT}/extern_libs/qt" REALPATH)

if(EXISTS ${QT_PATH})
    list (APPEND CMAKE_PREFIX_PATH "${QT_PATH}")
endif(EXISTS ${QT_PATH})

set (CMAKE_AUTOMOC ON)
set (CMAKE_AUTOUIC ON)

set (MY_Qt5_INCLUDE_DIRS "")
set (MY_Qt5_LIBRARIES "")
if (Qt5Extern_FIND_COMPONENTS)
  foreach (component ${Qt5Extern_FIND_COMPONENTS})
    find_package (Qt5${component})
    list (APPEND MY_Qt5_INCLUDE_DIRS ${Qt5${component}_INCLUDE_DIRS})
    list (APPEND MY_Qt5_LIBRARIES Qt5::${component})
  endforeach()
endif()

message (${MY_Qt5_LIBRARIES})
