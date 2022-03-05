add_executable(Blockdiff Blockdiff.cpp)
add_executable(Dumpsectors Dumpsectors.cpp)

if (WIN32)
  SET (CMAKE_CXX_FLAGS_RELEASE "/Zi /Od")
  SET_TARGET_PROPERTIES(
   Dumpsectors PROPERTIES LINK_FLAGS 
   "/DEBUG /OPT:REF /OPT:ICF"
  )
  SET_TARGET_PROPERTIES(
   Blockdiff PROPERTIES LINK_FLAGS 
   "/DEBUG /OPT:REF /OPT:ICF"
  )
endif (WIN32)

SET_PROPERTY(TARGET Dumpsectors PROPERTY CXX_STANDARD 17)
SET_PROPERTY(TARGET Blockdiff PROPERTY CXX_STANDARD 17)