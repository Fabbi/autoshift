cmake_minimum_required(VERSION 3.2)

add_executable(autoshift
  widgets/qansitextedit.cpp
  ${ROOT}/include/fsettings.hpp

  main.cpp
  controlwindow.cpp
  )

target_link_libraries(autoshift Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Network Qt5::PrintSupport)

target_include_directories(autoshift SYSTEM PRIVATE ${lib_include_dirs})

target_link_libraries(autoshift ${MY_Boost_LIBRARIES})
