cmake_minimum_required(VERSION 3.0)

# set(CMAKE_VERBOSE_MAKEFILE ON)

project(cpp-vhd)

add_executable(TestVHD TestVHD.cpp)

add_subdirectory(TOOLS)

if (WIN32)
  SET (CMAKE_CXX_FLAGS_RELEASE "/Zi /Od")
  SET_TARGET_PROPERTIES(
   TestVHD PROPERTIES LINK_FLAGS 
   "/DEBUG /OPT:REF /OPT:ICF"
  )
endif (WIN32)

SET_PROPERTY(TARGET TestVHD PROPERTY CXX_STANDARD 17)

target_include_directories(
 TestVHD
 PRIVATE
 ${CMAKE_CURRENT_SOURCE_DIR}
 ${CMAKE_CURRENT_SOURCE_DIR}/INCLUDE
 ${CMAKE_CURRENT_SOURCE_DIR}/../cpp-osl
 ${CMAKE_CURRENT_SOURCE_DIR}/../cpp-osl/INCLUDE
 ${CMAKE_CURRENT_SOURCE_DIR}/../cpp-npl
 ${CMAKE_CURRENT_SOURCE_DIR}/../cpp-npl/INCLUDE
)

target_link_libraries(TestVHD ws2_32.lib Ole32.lib)