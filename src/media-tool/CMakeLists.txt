file(GLOB EAPS_COMMON_SRC_LIST
  ${CMAKE_CURRENT_SOURCE_DIR}/*.c
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cu
  ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

add_library(eap-sma-common STATIC ${EAPS_COMMON_SRC_LIST})
add_library(EAPS::CommonLib ALIAS eap-sma-common)

set(LOCATION_LINK_LIBRARIES ${LINK_LIBRARIES})
list(REMOVE_ITEM LOCATION_LINK_LIBRARIES EAPS::CommonLib)
set(LOCATION_COMPILE_DEFINITIONS ${COMPILE_DEFINITIONS})

if(ENABLE_AIRBORNE)
    message(STATUS "ENABLE AIRBORNE")
    list(APPEND LOCATION_COMPILE_DEFINITIONS ENABLE_AIRBORNE)
else()
    message(STATUS "NOT ENABLE AIRBORNE")
endif(ENABLE_AIRBORNE)

target_compile_definitions(eap-sma-common
  PUBLIC ${LOCATION_COMPILE_DEFINITIONS})
target_compile_options(eap-sma-common
  PRIVATE ${COMPILE_OPTIONS_DEFAULT})
target_link_libraries(eap-sma-common
  PUBLIC ${LOCATION_LINK_LIBRARIES})
# 动态库link时，可以直接找到头文件目录
target_include_directories(eap-sma-common
  PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")

update_cached_list(LINK_LIBRARIES EAPS::CommonLib)

# 未在使用
if(ENABLE_CXX_API)
  # 保留目录结构
  install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION ${INSTALL_PATH_INCLUDE}/EDGE
    REGEX ".*[.](md|cpp)$" EXCLUDE)
  install(TARGETS eap-sma-common
    DESTINATION ${INSTALL_PATH_LIB})
endif ()