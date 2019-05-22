CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
SET(LIBRARY_OUTPUT_PATH "${PROJECT_BINARY_DIR}/lib")
AUX_SOURCE_DIRECTORY(${PROJECT_SOURCE_DIR}/src/yunsdr/src YUNSDR_src)
INCLUDE_DIRECTORIES("${PROJECT_SOURCE_DIR}/src/yunsdr/include")
ADD_LIBRARY(yunsdr SHARED ${YUNSDR_src})
ADD_LIBRARY(yunsdr_static STATIC ${YUNSDR_src})
SET_TARGET_PROPERTIES(yunsdr_static PROPERTIES OUTPUT_NAME "yunsdr")

if(MSVC)
	set_target_properties(yunsdr PROPERTIES COMPILE_FLAGS "/MT")
	set_target_properties(yunsdr_static PROPERTIES COMPILE_FLAGS "/MT")
	add_definitions(-D_WINSOCK_DEPRECATED_NO_WARNINGS)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif(MSVC)

if(MINGW)
    ADD_CUSTOM_COMMAND(TARGET yunsdr
        POST_BUILD
        COMMAND cd ${LIBRARY_OUTPUT_PATH} & gendef libyunsdr.dll
        COMMENT "Generate def file for libyunsdr.dll"
    )
    INSTALL(FILES ${LIBRARY_OUTPUT_PATH}/libyunsdr.def
		DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
endif(MINGW)

if(MSVC OR MINGW)
	target_link_libraries(yunsdr ws2_32)
	target_link_libraries(yunsdr_static ws2_32)

	set(CMAKE_STATIC_LIBRARY_PREFIX "lib")
	set(CMAKE_SHARED_LIBRARY_PREFIX "lib")
	INSTALL(TARGETS yunsdr yunsdr_static
		RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
		LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
		ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)

	INSTALL(FILES ${PROJECT_SOURCE_DIR}/src/yunsdr/include/yunsdr_api.h 
		DESTINATION ${CMAKE_INSTALL_PREFIX}/include)
else(MSVC OR MINGW)
	INSTALL(TARGETS yunsdr yunsdr_static
		LIBRARY DESTINATION /usr/local/lib
		ARCHIVE DESTINATION /usr/local/lib)

	INSTALL(FILES ${PROJECT_SOURCE_DIR}/src/yunsdr/include/yunsdr_api.h 
		DESTINATION /usr/local/include)
endif(MSVC OR MINGW)
