CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(YUNSDRLIB)
set(BUILD_USE_64BITS on)
message( STATUS "CMAKE_GENERATOR_PLATFORM: " ${CMAKE_GENERATOR_PLATFORM} )
message( STATUS "CMAKE_SYSTEM: " ${CMAKE_SYSTEM} )
message( STATUS "CMAKE_SYSTEM_PROCESSOR: " ${CMAKE_SYSTEM_PROCESSOR} )
message( STATUS "CMAKE_C_COMPILER: " ${CMAKE_C_COMPILER} )


list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/modules")
#ADD_SUBDIRECTORY(src)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
	message(STATUS "Build type not specified: defaulting to Release.")
endif(NOT CMAKE_BUILD_TYPE)
set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "")

########################################################################
# Options
########################################################################
option(ENABLE_PCIE     "Build library with PCIE Interface support"       ON)
option(ENABLE_SFP  	   "Build library with SFP Interface support"        ON)
option(ENABLE_ETHERNET "Build library with ETHERNET Interface support"   ON)

set(GCC_ARCH native CACHE STRING "GCC compile for specific architecture.")

########################################################################
# Find dependencies
########################################################################
if (ENABLE_PCIE)
	find_package(RIFFA)
	if(RIFFA_FOUND)
		include_directories(${RIFFA_INCLUDE_DIRS})
		link_directories(${RIFFA_LIBRARY_DIRS})
	endif(RIFFA_FOUND)
endif(ENABLE_PCIE)

if(MSVC)
	set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_BINARY_DIR}/install)
endif(MSVC)

if(ENABLE_ETHERNET)
	ADD_SUBDIRECTORY(src/yunsdr)
endif(ENABLE_ETHERNET)

if(ENABLE_PCIE OR ENABLE_SFP)
	ADD_SUBDIRECTORY(src/yunsdr_ss)
endif(ENABLE_PCIE OR ENABLE_SFP)

if(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=${GCC_ARCH} -Wall -Wno-comment -Wno-write-strings -Winline -Wno-unused-result -Wformat -Wmissing-field-initializers -Wtype-limits -std=c99 -D_GNU_SOURCE")

	if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ggdb -O0 -DDEBUG_MODE -DBUILD_TYPE_DEBUG")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb -O0 -DDEBUG_MODE -DBUILD_TYPE_DEBUG")
	else(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
		if(${CMAKE_BUILD_TYPE} STREQUAL "RelWithDebInfo")
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ggdb -DBUILD_TYPE_RELWITHDEBINFO")
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb -DBUILD_TYPE_RELWITHDEBINFO")
		else(${CMAKE_BUILD_TYPE} STREQUAL "RelWithDebInfo")
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -pg -DBUILD_TYPE_RELEASE")
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -pg -DBUILD_TYPE_RELEASE")
		endif(${CMAKE_BUILD_TYPE} STREQUAL "RelWithDebInfo")
	endif(${CMAKE_BUILD_TYPE} STREQUAL "Debug")

	find_package(SSE)
	if (HAVE_AVX2)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfpmath=sse -mavx2 -DLV_HAVE_AVX2 -DLV_HAVE_AVX -DLV_HAVE_SSE")
	else (HAVE_AVX2)
		if(HAVE_AVX)
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfpmath=sse -mavx -DLV_HAVE_AVX -DLV_HAVE_SSE")
		elseif(HAVE_SSE)
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfpmath=sse -msse4.1 -DLV_HAVE_SSE")
		endif(HAVE_AVX)
	endif (HAVE_AVX2)

	if (HAVE_FMA)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfma -DLV_HAVE_FMA")
	endif (HAVE_FMA)

	if (HAVE_AVX512)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mavx512f -mavx512cd -DLV_HAVE_AVX512")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx512f -mavx512cd -DLV_HAVE_AVX512")
	endif(HAVE_AVX512)

	if(NOT ${CMAKE_BUILD_TYPE} STREQUAL "Debug")
		if(HAVE_SSE)
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Ofast -funroll-loops")
		endif(HAVE_SSE)
	endif(NOT ${CMAKE_BUILD_TYPE} STREQUAL "Debug")

	set(CMAKE_REQUIRED_FLAGS ${CMAKE_C_FLAGS})

	if(NOT HAVE_SSE)
		message(FATAL_ERROR "no SIMD instructions found")
	endif(NOT HAVE_SSE)

endif(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")

##############################
# MS Visual Studio
###############################
if(MSVC)
	if(TREAT_WARNINGS_AS_ERRORS)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /WX")
	endif()

	find_package(SSE)
	if (HAVE_AVX2)
		add_definitions(-DLV_HAVE_AVX2 -DLV_HAVE_AVX -DLV_HAVE_SSE)
	else (HAVE_AVX2)
		if(HAVE_AVX)
			add_definitions(-DLV_HAVE_AVX -DLV_HAVE_SSE)
		elseif(HAVE_SSE)
			add_definitions(-DLV_HAVE_SSE)
		endif(HAVE_AVX)
	endif (HAVE_AVX2)

	if (HAVE_FMA)
		#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfma -DLV_HAVE_FMA")
		add_definitions(-DLV_HAVE_FMA)
	endif (HAVE_FMA)

	if (HAVE_AVX512)
		add_definitions(-DLV_HAVE_AVX512)
	endif(HAVE_AVX512)

	set(CMAKE_REQUIRED_FLAGS ${CMAKE_C_FLAGS})

	if(NOT HAVE_SSE)
		message(FATAL_ERROR "no SIMD instructions found")
	endif(NOT HAVE_SSE)

endif(MSVC)

message(STATUS "CMAKE_C_FLAGS is ${CMAKE_C_FLAGS}")
########################################################################
# Install pkg-config file
########################################################################
configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/libyunsdr.pc.in
	${CMAKE_CURRENT_BINARY_DIR}/libyunsdr.pc
	@ONLY)

install(
	FILES ${CMAKE_CURRENT_BINARY_DIR}/libyunsdr.pc
	DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig
	)

# uninstall target
if(NOT TARGET uninstall)
	configure_file(
		"${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
		"${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
		IMMEDIATE @ONLY)

	add_custom_target(uninstall
		COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
endif()
