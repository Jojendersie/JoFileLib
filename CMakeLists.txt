cmake_minimum_required(VERSION 2.6)

# Project Name
project(JoFileLib)

# Configuration
option(BUILD_SHARED "link shared libraries only" OFF)
if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS /DWINAPI_FAMILY")
endif()

# Files
file(GLOB JoFileLib_SRC "${CMAKE_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE deps_SRC "${CMAKE_SOURCE_DIR}/dependencies/*.c")
file(GLOB JoMemoryLib_SRC "${CMAKE_SOURCE_DIR}/../JoMemory/src/*.cpp")
list(APPEND JoFileLib_SRC ${JoMemoryLib_SRC} ${deps_SRC})

file(GLOB JoFileLib_INCLUDE "${CMAKE_SOURCE_DIR}/include/*.hpp")
#file(GLOB_RECURSE deps_INCLUDE "${CMAKE_SOURCE_DIR}/dependencies/*.h")
file(GLOB JoMemoryLib_INCLUDE "${CMAKE_SOURCE_DIR}/../JoMemory/include/*.hpp")
list(APPEND JoFileLib_INCLUDE ${JoMemoryLib_INCLUDE})

include_directories(
	"${CMAKE_SOURCE_DIR}/include"
	"${CMAKE_SOURCE_DIR}/../JoMemory/include"
	"${CMAKE_SOURCE_DIR}/dependencies/zlib128"
	"${CMAKE_SOURCE_DIR}/dependencies/libpng166"
)

# Create Libraries
if(BUILD_SHARED)
  add_library(jofile SHARED ${JoFileLib_SRC})
else()
  add_library(jofile STATIC ${JoFileLib_SRC})
endif()

# Installation
install(FILES ${JoFileLib_INCLUDE} DESTINATION include)

if(CMAKE_CONFIGURATION_TYPES) # multi-configuration generator
  foreach(CONFIG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    install(TARGETS jofile
      LIBRARY DESTINATION "lib/${CONFIG_TYPE}" CONFIGURATIONS ${CONFIG_TYPE}
      ARCHIVE DESTINATION "lib/${CONFIG_TYPE}" CONFIGURATIONS ${CONFIG_TYPE}
    )
  endforeach()
else() # single-configuration generator
  install(TARGETS jofile
    LIBRARY DESTINATION "lib/${CMAKE_BUILD_TYPE}"
    ARCHIVE DESTINATION "lib/${CMAKE_BUILD_TYPE}"
  )
endif()