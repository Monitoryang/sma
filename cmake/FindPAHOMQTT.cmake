find_path(PAHOMQTT_INCLUDE_DIR 
		NAMES MQTTClient.h 
		HINTS ${PAHOMQTT_PATH_ROOT} 
		PATH_SUFFIXES include)
if(WIN32)
	find_library(PAHOMQTT_LIBRARY 
			NAMES paho-mqtt3c
			HINTS ${PAHOMQTT_PATH_ROOT} 
			PATH_SUFFIXES lib bin)
else()
	find_library(PAHOMQTT_LIBRARY
		NAMES paho-mqtt3c
		HINTS ${PAHOMQTT_PATH_ROOT} 
		PATH_SUFFIXES bin lib)
endif(WIN32)

set(PAHOMQTT_INCLUDE_DIRS ${PAHOMQTT_INCLUDE_DIR})
set(PAHOMQTT_LIBRARIES ${PAHOMQTT_LIBRARY})

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(PAHOMQTT DEFAULT_MSG PAHOMQTT_LIBRARY PAHOMQTT_INCLUDE_DIR)
