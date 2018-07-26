
message(STATUS "FINDING RIFFA.")
if(WIN32)
	set(LIBRIFFA_PATH
		"C:/Program Files (x86)/riffa/c_c++/windows/x64/"
		CACHE
		PATH
		"Path to libriffa files. (This is
		generally only needed for Windows
		users who downloaded binary
		distributions.)"
		)
endif()

if(NOT RIFFA_FOUND)
	#pkg_check_modules (RIFFA_PKG riffa)

	find_path(RIFFA_INCLUDE_DIRS 
		NAMES riffa.h
		PATHS
		/usr/local/include/
		${LIBRIFFA_PATH}
		)

	find_library(RIFFA_LIBRARIES 
		NAMES riffa
		PATHS
		/usr/local/lib
		${LIBRIFFA_PATH}
		)


	if(RIFFA_INCLUDE_DIRS AND RIFFA_LIBRARIES)
		set(RIFFA_FOUND TRUE CACHE INTERNAL "libriffa found")
		message(STATUS "Found libriffa: ${RIFFA_INCLUDE_DIRS}, ${RIFFA_LIBRARIES}")
	else(RIFFA_INCLUDE_DIRS AND RIFFA_LIBRARIES)
		set(RIFFA_FOUND FALSE CACHE INTERNAL "libriffa not found")
		message(STATUS "libriffa not found.")
	endif(RIFFA_INCLUDE_DIRS AND RIFFA_LIBRARIES)

	mark_as_advanced(RIFFA_LIBRARIES RIFFA_INCLUDE_DIRS)

endif(NOT RIFFA_FOUND)
