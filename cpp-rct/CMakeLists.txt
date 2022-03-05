cmake_minimum_required(VERSION 3.0)

# set(CMAKE_VERBOSE_MAKEFILE ON)

project(Cpp-programs)

add_executable(Test Test.cpp)

if (WIN32)
  SET (CMAKE_CXX_FLAGS_RELEASE "/Zi /Od")
  SET_TARGET_PROPERTIES(
   Testall PROPERTIES LINK_FLAGS 
   "/DEBUG /OPT:REF /OPT:ICF"
  )
endif (WIN32)

SET_PROPERTY(TARGET Testall PROPERTY CXX_STANDARD 17)

target_include_directories(
 Testall
 PRIVATE
 ${CMAKE_CURRENT_SOURCE_DIR}
 ${CMAKE_CURRENT_SOURCE_DIR}/INCLUDE
)

target_link_libraries(Testall wbemuuid.lib)