find_path(POCO_NET_INCLUDE_DIR 
		NAMES Poco/Net/Net.h
		HINTS ${POCO_PATH_ROOT}
		PATH_SUFFIXES include)

find_library(POCO_NET_LIBRARY
		NAMES PocoNet
		HINTS ${POCO_PATH_ROOT}
		PATH_SUFFIXES bin lib)
		
set(POCO_NET_LIBRARIES ${POCO_NET_LIBRARY})
set(POCO_NET_INCLUDE_DIRS ${POCO_NET_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)	

find_package_handle_standard_args(POCO_NET DEFAULT_MSG POCO_NET_LIBRARY POCO_NET_INCLUDE_DIR)
