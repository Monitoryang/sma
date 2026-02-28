find_path(ARENGINE_INCLUDE_DIR
        NAMES jo_ar_engine_interface.h
        HINTS ${ARENGINE_PATH_ROOT}
        PATH_SUFFIXES include)
		
if(WIN32)
	find_library(ARENGINE_LIBRARY_DEBUG
        NAMES jo_ar_engine
        HINTS ${ARENGINE_PATH_ROOT}
        PATH_SUFFIXES bin/debug lib/debug)

	find_library(ARENGINE_LIBRARY_RELEASE
			NAMES jo_ar_engine
			HINTS ${ARENGINE_PATH_ROOT}
			PATH_SUFFIXES bin/release lib/release)
			
	set(ARENGINE_LIBRARIES_DEBUG ${ARENGINE_LIBRARY_DEBUG})
	set(ARENGINE_LIBRARIES_RELEASE ${ARENGINE_LIBRARY_RELEASE})
else()
	find_library(ARENGINE_LIBRARY
			NAMES jo_ar_engine
			HINTS ${ARENGINE_PATH_ROOT}
			PATH_SUFFIXES bin lib)
			
	set(ARENGINE_LIBRARIES ${ARENGINE_LIBRARY})
endif(WIN32)

set(ARENGINE_INCLUDE_DIRS ${ARENGINE_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)

if(WIN32)
	find_package_handle_standard_args(AREngine DEFAULT_MSG ARENGINE_LIBRARY_DEBUG ARENGINE_LIBRARY_RELEASE ARENGINE_INCLUDE_DIR)
else()
	find_package_handle_standard_args(AREngine DEFAULT_MSG ARENGINE_LIBRARY ARENGINE_INCLUDE_DIR)
endif(WIN32)
