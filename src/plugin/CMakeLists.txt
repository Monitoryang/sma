file(GLOB PLUGIN_SRC_LIST
  ${CMAKE_CURRENT_SOURCE_DIR}/*.c
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

add_library(JoEAPPluginSma SHARED ${PLUGIN_SRC_LIST})
add_library(HA::SmaPluginLib ALIAS JoEAPPluginSma)

set(LOCATION_LINK_LIBRARIES ${LINK_LIBRARIES})
list(REMOVE_ITEM LOCATION_LINK_LIBRARIES HA::SmaPluginLib)

set(LOCATION_COMPILE_DEFINITIONS ${COMPILE_DEFINITIONS})
list(APPEND LOCATION_COMPILE_DEFINITIONS -DPLUGIN_EXPORT)

target_compile_definitions(JoEAPPluginSma
  PUBLIC ${LOCATION_COMPILE_DEFINITIONS})
target_compile_options(JoEAPPluginSma
  PRIVATE ${COMPILE_OPTIONS_DEFAULT})
target_link_libraries(JoEAPPluginSma
  PUBLIC ${LOCATION_LINK_LIBRARIES})
# 动态库link时，可以直接找到头文件目录
target_include_directories(JoEAPPluginSma
  PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")

# 会导致循环依赖
# update_cached_list(LINK_LIBRARIES HA::SmaPluginLib)

# 未在使用
if(ENABLE_CXX_API)
  # 保留目录结构
  install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION ${INSTALL_PATH_INCLUDE}/HA
    REGEX ".*[.](md|cpp)$" EXCLUDE)
  install(TARGETS JoEAPPluginSma
    DESTINATION ${INSTALL_PATH_LIB})
endif ()