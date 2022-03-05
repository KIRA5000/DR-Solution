cmake_minimum_required(VERSION 3.15)

project(osl)

add_executable(TestOSL Test.cpp)

if (WIN32)
  SET (CMAKE_CXX_FLAGS_RELEASE "/Zi /Od")
  SET_TARGET_PROPERTIES(
   TestOSL PROPERTIES LINK_FLAGS 
   "/DEBUG /OPT:REF /OPT:ICF"
  )
endif (WIN32)

SET_PROPERTY(TARGET TestOSL PROPERTY CXX_STANDARD 17)

target_include_directories(
  TestOSL
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/INCLUDE
)

target_link_libraries(
  TestOSL
)