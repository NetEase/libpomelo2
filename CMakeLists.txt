# This is a cmake script for pomelo_sdk

cmake_minimum_required(VERSION 2.6)

set (VERSION 0.2.1)

project(PomeloSDK)

option(CROSS_BUILD
  "Is it cross-building?" OFF)

if (CROSS_BUILD)
  SET(CMAKE_SYSTEM_NAME Linux)
  SET(CMAKE_C_COMPILER "arm-hisiv100nptl-linux-gcc")
  SET(CMAKE_CXX_COMPILER "arm-hisiv100nptl-linux-g++")

  SET(CMAKE_FIND_ROOT_PATH "/opt/hisi-linux-nptl/arm-hisiv100-linux" 
                           "/opt/hisi-linux-nptl/arm-hisiv100-linux/target")
  SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
  SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
  SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
endif (CROSS_BUILD)

if (WIN32)
	add_definitions("-D_CRT_SECURE_NO_WARNINGS")
endif (WIN32)

add_subdirectory(pomeloclient)

option(BUILD_DEV_EXAMPLE
	"Do you want to build example?" ON)

option(BUILD_DEMO
	"The Demo is dependent on Qt 4.x, if you want to build it, you should install Qt4.x first. Do you want to build Demo" OFF)


if (BUILD_DEV_EXAMPLE)
	add_subdirectory(example)
endif(BUILD_DEV_EXAMPLE)

if (BUILD_DEMO)
	add_subdirectory(demo)
endif(BUILD_DEMO)

