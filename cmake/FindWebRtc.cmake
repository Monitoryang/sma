find_path(WEBRTC_INCLUDE_DIR
        NAMES jo_rtc.h
        HINTS ${WEBRTC_PATH_ROOT}
        PATH_SUFFIXES include)

find_library(WEBRTC_LIBRARY
        NAMES jo_webrtc
        HINTS ${WEBRTC_PATH_ROOT}
        PATH_SUFFIXES bin lib)

set(WEBRTC_LIBRARIES ${WEBRTC_LIBRARY})
set(WEBRTC_INCLUDE_DIRS ${WEBRTC_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(WebRtc DEFAULT_MSG WEBRTC_LIBRARY WEBRTC_INCLUDE_DIR)
