cmake_minimum_required(VERSION 3.2)

set (INC "${ROOT}/include")
set(autoshift_HEADERS
  ${INC}/misc/fsettings.hpp
  ${INC}/controlwindow.hpp
  ${INC}/shift.hpp
  ${INC}/request.hpp
  ${INC}/key.hpp
  )

set(autoshift_SOURCE
  widgets/qansitextedit.cpp

  request.cpp

  main.cpp
  controlwindow.cpp
  shift.cpp
  key.cpp
  )

add_executable(autoshift
  ${autoshift_HEADERS}
  ${autoshift_SOURCE}
  )

target_link_libraries(autoshift ${MY_Qt5_LIBRARIES})
target_link_libraries(autoshift spinner)

target_include_directories(autoshift SYSTEM PRIVATE ${lib_include_dirs})

target_link_libraries(autoshift ${MY_Boost_LIBRARIES})
