find_path(OpenCV_INCLUDE_DIRS
		NAMES opencv2/core.hpp
		HINTS ${OPENCV_PATH_ROOT}
		PATH_SUFFIXES include)

if(WIN32)
	if(ENABLE_GPU)
		find_library(OPENCV_CORE_LIBRARY_DEBUG
			NAMES opencv_core420d
			HINTS ${OPENCV_PATH_ROOT}
			PATH_SUFFIXES x64/vc15/lib)
		find_library(OPENCV_CORE_LIBRARY_RELEASE
			NAMES opencv_core420
			HINTS ${OPENCV_PATH_ROOT}
			PATH_SUFFIXES x64/vc15/lib)
	else()
		find_library(OPENCV_CORE_LIBRARY_DEBUG
			NAMES opencv_world420d
			HINTS ${OPENCV_PATH_ROOT}
			PATH_SUFFIXES x64/vc15/lib)
		find_library(OPENCV_CORE_LIBRARY_RELEASE
			NAMES opencv_world420
			HINTS ${OPENCV_PATH_ROOT}
			PATH_SUFFIXES x64/vc15/lib)
	endif(ENABLE_GPU)
	set(OPENCV_CORE_LIBRARIES_DEBUG ${OPENCV_CORE_LIBRARY_DEBUG})
	set(OPENCV_CORE_LIBRARIES_RELEASE ${OPENCV_CORE_LIBRARY_RELEASE})
else(WIN32)
	if(ENABLE_GPU)
		find_library(OPENCV_CORE_LIBRARY
				NAMES opencv_core
				HINTS ${OPENCV_PATH_ROOT}
				PATH_SUFFIXES bin lib)
	else()
		find_library(OPENCV_CORE_LIBRARY
					NAMES opencv_world
					HINTS ${OPENCV_PATH_ROOT}
					PATH_SUFFIXES bin lib)
	endif(ENABLE_GPU)
	set(OpenCV_LIBS ${OPENCV_CORE_LIBRARY})
endif(WIN32)

set(OPENCV_CORE_INCLUDE_DIRS ${OpenCV_INCLUDE_DIRS})

include(FindPackageHandleStandardArgs)

if(WIN32)
	find_package_handle_standard_args(OpenCVWorld DEFAULT_MSG OPENCV_CORE_LIBRARY_DEBUG OPENCV_CORE_LIBRARY_RELEASE OpenCV_INCLUDE_DIRS)
else()
	find_package_handle_standard_args(OpenCVWorld DEFAULT_MSG OPENCV_CORE_LIBRARY OpenCV_INCLUDE_DIRS)
endif(WIN32)
