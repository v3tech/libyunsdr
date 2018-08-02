CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
SET(EXECUTABLE_OUTPUT_PATH "${PROJECT_BINARY_DIR}/tests")
AUX_SOURCE_DIRECTORY(${PROJECT_SOURCE_DIR}/src/yunsdr_ss/tests YUNSDR_test_src)
INCLUDE_DIRECTORIES("${PROJECT_SOURCE_DIR}/src/yunsdr_ss/include")

########################################################################
# Find dependencies
########################################################################
if(MSVC)
	find_package(LibPThreadsWin32 REQUIRED)
else(MSVC)
	find_package(Threads REQUIRED)
endif(MSVC)
ADD_EXECUTABLE(tests ${YUNSDR_test_src})
target_link_libraries(tests m)
target_link_libraries(tests yunsdr_ss)
target_link_libraries(tests ${CMAKE_THREAD_LIBS_INIT})

