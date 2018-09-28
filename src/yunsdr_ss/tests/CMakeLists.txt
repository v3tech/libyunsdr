CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
SET(EXECUTABLE_OUTPUT_PATH "${PROJECT_BINARY_DIR}/tests")
SET(YUNSDR_TXRX_SRC "")
list(APPEND YUNSDR_TXRX_SRC ${PROJECT_SOURCE_DIR}/src/yunsdr_ss/tests/yunsdr_ss_txrx.c)
if(MSVC)
	list(APPEND YUNSDR_TXRX_SRC
		${PROJECT_SOURCE_DIR}/src/yunsdr_ss/tests/wingetopt.c)
endif(MSVC)
INCLUDE_DIRECTORIES("${PROJECT_SOURCE_DIR}/src/yunsdr_ss/include")

ADD_EXECUTABLE(yunsdr_ss_txrx ${YUNSDR_TXRX_SRC})
if(MSVC)
	set_target_properties(yunsdr_ss_txrx PROPERTIES COMPILE_FLAGS "/MT")
endif(MSVC)
if(NOT MSVC)
	target_link_libraries(yunsdr_ss_txrx m)
endif(NOT MSVC)
target_link_libraries(yunsdr_ss_txrx yunsdr_ss)

if(MSVC OR MINGW)
	INSTALL(TARGETS yunsdr_ss_txrx
		RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/tests/yunsdr_ss
		LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/tests/yunsdr_ss
		ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/tests/yunsdr_ss)
endif(MSVC OR MINGW)

