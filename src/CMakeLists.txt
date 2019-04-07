cmake_minimum_required(VERSION 3.2)

set(autoshift_HEADERS
  ${ROOT}/include/misc/fsettings.hpp
  ${ROOT}/include/controlwindow.hpp
  ${ROOT}/include/shift.hpp
  ${ROOT}/include/request.hpp
  )

set(autoshift_SOURCE
  widgets/qansitextedit.cpp

  request.cpp

  main.cpp
  controlwindow.cpp
  shift.cpp
  )

add_executable(autoshift
  ${autoshift_HEADERS}
  ${autoshift_SOURCE}
  )

target_link_libraries(autoshift ${MY_Qt5_LIBRARIES})
target_link_libraries(autoshift spinner)

target_include_directories(autoshift SYSTEM PRIVATE ${lib_include_dirs})

target_link_libraries(autoshift ${MY_Boost_LIBRARIES})
