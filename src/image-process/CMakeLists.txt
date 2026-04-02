file(GLOB IMAGE_SRC_LIST
  ${CMAKE_CURRENT_SOURCE_DIR}/*.c
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

add_library(eap-sma-imageprocess STATIC ${IMAGE_SRC_LIST})
add_library(EAPS::ImageProcessLib ALIAS eap-sma-imageprocess)

set(LOCATION_LINK_LIBRARIES ${LINK_LIBRARIES})
list(REMOVE_ITEM LOCATION_LINK_LIBRARIES EAPS::ImageProcessLib)
set(LOCATION_COMPILE_DEFINITIONS ${COMPILE_DEFINITIONS})

target_compile_definitions(eap-sma-imageprocess
  PUBLIC ${LOCATION_COMPILE_DEFINITIONS})
target_compile_options(eap-sma-imageprocess
  PRIVATE ${COMPILE_OPTIONS_DEFAULT})
target_link_libraries(eap-sma-imageprocess
  PUBLIC ${LOCATION_LINK_LIBRARIES})
# 动态库link时，可以直接找到头文件目录
target_include_directories(eap-sma-imageprocess
  PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")

update_cached_list(LINK_LIBRARIES EAPS::ImageProcessLib)

# 未在使用
if(ENABLE_CXX_API)
  # 保留目录结构
  install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION ${INSTALL_PATH_INCLUDE}/EAPS
    REGEX ".*[.](md|cpp)$" EXCLUDE)
  install(TARGETS eap-sma-imageprocess
    DESTINATION ${INSTALL_PATH_LIB})
endif ()