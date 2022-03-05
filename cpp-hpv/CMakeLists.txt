cmake_minimum_required(VERSION 3.0)

#set(CMAKE_VERBOSE_MAKEFILE ON)

project(cpp-hpv)

add_executable(TestHPV TestHPV.cpp)

if (WIN32)
  SET (CMAKE_CXX_FLAGS_RELEASE "/Zi /Od")
  SET_TARGET_PROPERTIES(
   TestHPV PROPERTIES LINK_FLAGS 
   "/DEBUG /OPT:REF /OPT:ICF"
  )
endif (WIN32)

SET_PROPERTY(TARGET TestHPV PROPERTY CXX_STANDARD 17)

target_include_directories(
 TestHPV
 PRIVATE
 ${CMAKE_CURRENT_SOURCE_DIR}
 ${CMAKE_CURRENT_SOURCE_DIR}/INCLUDE
 ${CMAKE_CURRENT_SOURCE_DIR}/../cpp-vss
 ${CMAKE_CURRENT_SOURCE_DIR}/../cpp-vss/INCLUDE
 ${CMAKE_CURRENT_SOURCE_DIR}/../cpp-osl
 ${CMAKE_CURRENT_SOURCE_DIR}/../cpp-osl/INCLUDE
 ${CMAKE_CURRENT_SOURCE_DIR}/../cpp-npl
 ${CMAKE_CURRENT_SOURCE_DIR}/../cpp-npl/INCLUDE
)

target_link_libraries(
  TestHPV
  ${CMAKE_CURRENT_SOURCE_DIR}/../cpp-vss/build/Release/libvss.lib
  vssapi.lib
  resutils.Lib
  ws2_32.lib
)