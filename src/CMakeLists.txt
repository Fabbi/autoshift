cmake_minimum_required(VERSION 3.2)

set (INC "${ROOT}/include")
set(autoshift_HEADERS
  ${INC}/misc/fsettings.hpp
  ${INC}/controlwindow.hpp
  ${INC}/shift.hpp
  ${INC}/request.hpp
  ${INC}/key.hpp
  
  # parser
  ${INC}/parser.hpp
  ${INC}/parser_orcz.hpp
  ${INC}/parser_orcicorn.hpp
  )

set(autoshift_SOURCE
  widgets/qansitextedit.cpp

  request.cpp

  main.cpp
  controlwindow.cpp
  shift.cpp
  key.cpp
  
  # parser
  parser_orcz.cpp
  parser_orcicorn.cpp
  )

if (WIN32)
  add_executable(autoshift
	WIN32 
	${autoshift_HEADERS}
	${autoshift_SOURCE}
  )
else()
  add_executable(autoshift 
	${autoshift_HEADERS}
	${autoshift_SOURCE}
  )
endif(WIN32)

target_link_libraries(autoshift ${MY_Qt5_LIBRARIES})
target_link_libraries(autoshift spinner)
