find_path(MEDIAKIT_INCLUDE_DIR
        NAMES mk_mediakit.h
        HINTS ${MEDIAKIT_PATH_ROOT}
        PATH_SUFFIXES include)

find_library(MEDIAKIT_LIBRARY
        NAMES jo_mk_api
        HINTS ${MEDIAKIT_PATH_ROOT}
        PATH_SUFFIXES bin lib)

set(MEDIAKIT_LIBRARIES ${MEDIAKIT_LIBRARY})
set(MEDIAKIT_INCLUDE_DIRS ${MEDIAKIT_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(MediaKit DEFAULT_MSG MEDIAKIT_LIBRARY MEDIAKIT_INCLUDE_DIR)
