find_path(AIENGINE_INCLUDE_DIR
        NAMES jo_ai_object_detect.h Track.h
        HINTS ${AIENGINE_PATH_ROOT}
        PATH_SUFFIXES include)

if(WIN32)
    find_library(AIENGINE_DETECT_LIBRARY_DEBUG
            NAMES jo_ai_detect
            HINTS ${AIENGINE_PATH_ROOT}
            PATH_SUFFIXES bin/debug lib/debug)

    find_library(AIENGINE_MOT_LIBRARY_DEBUG
            NAMES jo_ai_mot
            HINTS ${AIENGINE_PATH_ROOT}
            PATH_SUFFIXES bin/debug lib/debug)

    find_library(AIENGINE_DETECT_LIBRARY_RELEASE
            NAMES jo_ai_detect
            HINTS ${AIENGINE_PATH_ROOT}
            PATH_SUFFIXES bin/release lib/release)

    find_library(AIENGINE_MOT_LIBRARY_RELEASE
            NAMES jo_ai_mot
            HINTS ${AIENGINE_PATH_ROOT}
            PATH_SUFFIXES bin/release lib/release)

    set(AIENGINE_LIBRARIES_DEBUG ${AIENGINE_DETECT_LIBRARY_DEBUG} ${AIENGINE_MOT_LIBRARY_DEBUG})
    set(AIENGINE_LIBRARIES_RELEASE ${AIENGINE_DETECT_LIBRARY_RELEASE} ${AIENGINE_MOT_LIBRARY_RELEASE})
else()
    find_library(AIENGINE_DETECT_LIBRARY
            NAMES jo_ai_detect
            HINTS ${AIENGINE_PATH_ROOT}
            PATH_SUFFIXES bin lib)

    find_library(AIENGINE_MOT_LIBRARY
            NAMES jo_ai_mot
            HINTS ${AIENGINE_PATH_ROOT}
            PATH_SUFFIXES bin lib)

    set(AIENGINE_LIBRARIES ${AIENGINE_DETECT_LIBRARY} ${AIENGINE_MOT_LIBRARY})
endif(WIN32)

set(AIENGINE_INCLUDE_DIRS ${AIENGINE_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)

if(WIN32)
    find_package_handle_standard_args(AIEngine DEFAULT_MSG AIENGINE_LIBRARIES_DEBUG AIENGINE_LIBRARIES_RELEASE AIENGINE_INCLUDE_DIR)
else()
    find_package_handle_standard_args(AIEngine DEFAULT_MSG AIENGINE_LIBRARIES AIENGINE_INCLUDE_DIR)
endif(WIN32)