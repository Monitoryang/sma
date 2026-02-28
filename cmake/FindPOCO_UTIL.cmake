find_path(POCO_UTIL_INCLUDE_DIR 
		NAMES Poco/Util/Util.h
		HINTS ${POCO_PATH_ROOT}
		PATH_SUFFIXES include)

find_library(POCO_UTIL_LIBRARY
		NAMES PocoUtil
		HINTS ${POCO_PATH_ROOT}
		PATH_SUFFIXES bin lib)
		
set(POCO_UTIL_LIBRARIES ${POCO_UTIL_LIBRARY})
set(POCO_UTIL_INCLUDE_DIRS ${POCO_UTIL_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)	

find_package_handle_standard_args(POCO_UTIL DEFAULT_MSG POCO_UTIL_LIBRARY POCO_UTIL_INCLUDE_DIR)
