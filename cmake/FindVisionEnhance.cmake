
find_path(VISIONENHANCE_INCLUDE_DIRS
    NAMES jo_vision_enhance.h
    PATHS /usr/local/include
)

find_library(VISIONENHANCE_LIBRARIES
    NAMES jo_VisionEnhance
    PATHS /usr/local/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(VisionEnhance
    REQUIRED_VARS VISIONENHANCE_INCLUDE_DIRS VISIONENHANCE_LIBRARIES
    FAIL_MESSAGE "fatal: not found VisionEnhance SDK"
)

mark_as_advanced(VISIONENHANCE_INCLUDE_DIRS VISIONENHANCE_LIBRARIES)