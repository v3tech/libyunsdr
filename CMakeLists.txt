CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(YUNSDRLIB)
ADD_SUBDIRECTORY(src)

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
