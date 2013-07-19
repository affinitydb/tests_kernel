IF(CMAKE_SYSTEM_NAME MATCHES Windows)
        cmake_minimum_required(VERSION 2.8)
ENDIF(CMAKE_SYSTEM_NAME MATCHES Windows)
if(NOT CMAKE_BUILD_TYPE)
set (CMAKE_BUILD_TYPE "Debug")
#SET (CMAKE_BUILD_TYPE "Release")
endif(NOT CMAKE_BUILD_TYPE)

message ( "Processing affinity tests ...")
message ("System: ${CMAKE_SYSTEM}")
message ("CPU type: ${CMAKE_SYSTEM_PROCESSOR}")
message ( "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE} )
message ( "Project dir: " ${PROJECT_SOURCE_DIR})
#setting the name for source dir
#SET(TESTS_SRC_DIR ${PROJECT_SOURCE_DIR}/src)
#SET(TESTS_GLOBAL_HEADER ${PROJECT_SOURCE_DIR}/include)

#The line below will create ${CPUTYPE} variable, and set it 
#either to  'arm' or  to 'x86' 
STRING( REGEX MATCH "86" CPUTYPE ${CMAKE_SYSTEM_PROCESSOR})
IF(${CPUTYPE} MATCHES "86")
	SET(CPUTYPE "x86")
	message ("CPU: " ${CPUTYPE} )
ELSE (${CPUTYPE} MATCHES "86")
	STRING( REGEX MATCH "^..." CPUTYPE ${CMAKE_SYSTEM_PROCESSOR})
	message ("CPU: " ${CPUTYPE} )
ENDIF(${CPUTYPE} MATCHES "86")

#specifying location of affinity lib
#for now, it is just hard coded in, assuming that both project are located side by side... 
SET(AFFINITY_PRJ "${PROJECT_SOURCE_DIR}/../kernel")

INCLUDE_DIRECTORIES(  "./" "${AFFINITY_PRJ}/include" )


#setting the name of the output executable
#SET(TESTS_TARGET_NAME "tests")

#setting the output directory for the library
SET(EXECUTABLE_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/bin")

#adding some generic definitions and compiler flags for Windows ...
IF(CMAKE_SYSTEM_NAME MATCHES Windows)
	ADD_DEFINITIONS(-DWIN32)
#	ADD_DEFINITIONS(-D_DEBUG)
	ADD_DEFINITIONS(-D_WINDOWS)
	ADD_DEFINITIONS(-D_USRDLL)
	ADD_DEFINITIONS(-DAFFINITY_EXPORTS)
	ADD_DEFINITIONS(-DMVCORE_PROJECT)
	ADD_DEFINITIONS(-D_WINDLL)
	ADD_DEFINITIONS(-D_MBCS)
    ADD_DEFINITIONS(-D_CRT_SECURE_NO_DEPRECATE)
    ADD_DEFINITIONS(-D_CRT_NONSTDC_NO_DEPRECATE)
	
	SET( CMAKE_CXX_FLAGS  "/Gm /Zi /EHsc /GS /W3 /nologo /c /Gd /TP ")
#	SET( CMAKE_CXX_FLAGS  "/Od /Gm /EHsc /RTC1 /MDd /GS /W3 /nologo /c /Wp64 /ZI /Gd /TP ")
ENDIF(CMAKE_SYSTEM_NAME MATCHES Windows)


#adding some generic definitions and compiler flags for Linuxi ...
IF(CMAKE_SYSTEM_NAME MATCHES Linux)
IF(${CPUTYPE} MATCHES "x86")
   ADD_DEFINITIONS(-D_LINUX)
   ADD_DEFINITIONS(-DPOSIX)
   ADD_DEFINITIONS(-DIA32)
IF(CMAKE_BUILD_TYPE MATCHES Debug)
	ADD_DEFINITIONS(-DDEBUG)
	ADD_DEFINITIONS(-D_DEBUG)
IF(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
	SET( CMAKE_CXX_FLAGS  "  -g -O0 -march=nocona -m128bit-long-double -m64 -pthread -fexceptions -fno-strict-aliasing -W -Wall -Wno-parentheses -Wno-unused-parameter -fnon-call-exceptions -Wno-non-virtual-dtor -Wno-deprecated -Wno-write-strings ")
ELSE(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
	SET( CMAKE_CXX_FLAGS  "  -g -O0 -march=i686 -m32 -pthread -fexceptions -fno-strict-aliasing -W -Wall -Wno-parentheses -Wno-unused-parameter -fnon-call-exceptions -Wno-non-virtual-dtor -Wno-deprecated -Wno-write-strings ")
ENDIF(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
ELSE(CMAKE_BUILD_TYPE MATCHES Debug)
	ADD_DEFINITIONS(-DNDEBUG)
IF(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
	SET( CMAKE_CXX_FLAGS  "  -g -O3 -march=nocona -m128bit-long-double -m64 -pthread -fexceptions -fno-strict-aliasing -W -Wall -Wno-parentheses -Wno-unused-parameter -fnon-call-exceptions -Wno-non-virtual-dtor -Wno-deprecated -Wno-write-strings ")
ELSE(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
	SET( CMAKE_CXX_FLAGS  "  -g -O3 -march=i686 -m32 -pthread -fexceptions -fno-strict-aliasing -W -Wall -Wno-parentheses -Wno-unused-parameter -fnon-call-exceptions -Wno-non-virtual-dtor -Wno-deprecated -Wno-write-strings ")
ENDIF (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
ENDIF(CMAKE_BUILD_TYPE MATCHES Debug)
ENDIF(${CPUTYPE} MATCHES "x86")

#ADDING command for 'arm' CPU familes...
IF(${CPUTYPE} MATCHES "arm")
   ADD_DEFINITIONS(-D_LINUX)
   ADD_DEFINITIONS(-DPOSIX)
#   ADD_DEFINITIONS(-DIA32)
IF(CMAKE_BUILD_TYPE MATCHES Debug)
	ADD_DEFINITIONS(-DDEBUG)
	ADD_DEFINITIONS(-D_DEBUG)
	SET( CMAKE_CXX_FLAGS  "  -g -O0 -march=armv7-a -pthread -fexceptions -fno-strict-aliasing -W -Wall -Wno-parentheses -Wno-unused-parameter -fnon-call-exceptions -Wno-non-virtual-dtor -Wno-deprecated -Wno-write-strings ")
ELSE(CMAKE_BUILD_TYPE MATCHES Debug)
	ADD_DEFINITIONS(-DNDEBUG)
	SET( CMAKE_CXX_FLAGS  "  -g -O3 -march=armv7-a -pthread -fexceptions -fno-strict-aliasing -W -Wall -Wno-parentheses -Wno-unused-parameter -fnon-call-exceptions -Wno-non-virtual-dtor -Wno-deprecated -Wno-write-strings ")
ENDIF(CMAKE_BUILD_TYPE MATCHES Debug)
ENDIF(${CPUTYPE} MATCHES "arm")
ENDIF(CMAKE_SYSTEM_NAME MATCHES Linux)

#adding some generic definitions and compiler flags for Darwin (Mac OSX) ...
IF(CMAKE_SYSTEM_NAME MATCHES Darwin)
	ADD_DEFINITIONS(-D_LINUX)
	ADD_DEFINITIONS(-DPOSIX)
	ADD_DEFINITIONS(-DDarwin)
	ADD_DEFINITIONS(-DIA32)
	ADD_DEFINITIONS(-DDEBUG)
	ADD_DEFINITIONS(-D_DEBUG)
#ADD_DEFINITIONS(-D__x86_64__)
	   SET( CMAKE_CXX_FLAGS  "-m64  -g -O0 -pthread -fexceptions -fno-strict-aliasing -W -Wall -Wno-unused-parameter -fnon-call-exceptions -Wno-non-virtual-dtor ")
ENDIF(CMAKE_SYSTEM_NAME MATCHES Darwin)


#setting the list of sources for the kernel. 
# TODO:
# For now, I'm going to use GLOB to pick up all the files within directory for sources. 
# The drawback of such approach is that if a new source file is added to source directory, 
# then cmake should be re-run manually. 
# 
# Alternative is to list all the files as source files explicitely. Then, when the new file
# is added to the directory, it should be also added - manually - to the CMakeLists.txt. 
# In such scenario, the CMake will pick-up the modification of the CMakeLists.txt and run 
# cmake automatically. 
#
# For now, the first option is choossen:
FILE(GLOB TESTS_SRCS_H   "*.h")
FILE(GLOB TESTS_SRCS  "*.cpp")
FILE(GLOB TESTS_SRCS_C  "*.c")

# Use the stuff below to produce a 'onetest' makefile (in this example, using testservices1.cpp):
IF(0)
	FILE(GLOB _TESTS_UNWANTED "test*.cpp")
	FILE(GLOB _TESTS_WANTED "tests.cpp" "testservices1.cpp")
	LIST(REMOVE_ITEM TESTS_SRCS ${_TESTS_UNWANTED})
	LIST(APPEND TESTS_SRCS ${_TESTS_WANTED})
ENDIF(0)

#Important for Windows: configures how the source code should be seen 
#within the source tree: 
# commented out -> headers and source file in different dirs. 
# uncommented   -> headers and sources in the same directory...
#source_group(src FILES ${KERNEL_SRCS} FILES ${KERNEL_SRCS_H} )

##link_directories(../../kernel/lib)
link_directories(${AFFINITY_PRJ}/lib)
  
#adding the build target - affinity dynamic library...
add_executable (${TESTS_TARGET_NAME}  ${TESTS_SRCS_H} ${TESTS_SRCS} ${TESTS_SRCS_C})

#it is possible to have no source files the first time cmake is running... 
# consequently, I can not relay on compiler/linker autodetection  and have 
# to force them to be C++... 
SET_TARGET_PROPERTIES(${TESTS_TARGET_NAME} PROPERTIES LINKER_LANGUAGE CXX)

IF(CMAKE_SYSTEM_NAME MATCHES Linux)
target_link_libraries (${TESTS_TARGET_NAME} affinity rt dl pthread)
ENDIF(CMAKE_SYSTEM_NAME MATCHES Linux)

IF(CMAKE_SYSTEM_NAME MATCHES Windows)
target_link_libraries (${TESTS_TARGET_NAME} affinity)
ENDIF(CMAKE_SYSTEM_NAME MATCHES Windows)

IF(CMAKE_SYSTEM_NAME MATCHES Darwin)
target_link_libraries (${TESTS_TARGET_NAME} affinity)
ENDIF(CMAKE_SYSTEM_NAME MATCHES Darwin)
