file(GLOB JODECODER_SRC_LIST
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cu
  ${CMAKE_CURRENT_SOURCE_DIR}/*.c
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
  
cuda_add_library(eap-sma-imageenhancercuda STATIC ${JODECODER_SRC_LIST})
add_library(EAPS::ImageEnhancerLib ALIAS eap-sma-imageenhancercuda)

set(LOCATION_LINK_LIBRARIES ${LINK_LIBRARIES})
list(REMOVE_ITEM LOCATION_LINK_LIBRARIES EAPS::ImageEnhancerLib)
set(COMPILE_DEFINITIONS ${VIDEO_APPLICATION_KIT_COMPILE_DEFINITIONS})

target_compile_definitions(eap-sma-imageenhancercuda
  PUBLIC ${COMPILE_DEFINITIONS})
target_compile_options(eap-sma-imageenhancercuda
  PRIVATE ${COMPILE_OPTIONS_DEFAULT})

target_link_libraries(eap-sma-imageenhancercuda
  ${LOCATION_LINK_LIBRARIES})
# 动态库link时，可以直接找到头文件目录
target_include_directories(eap-sma-imageenhancercuda
  PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")

update_cached_list(LINK_LIBRARIES EAPS::ImageEnhancerLib)

# 未在使用
if(ENABLE_CXX_API)
  # 保留目录结构
  install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION ${INSTALL_PATH_INCLUDE}/EAPS
    REGEX ".*[.](md|cpp)$" EXCLUDE)
  install(TARGETS eap-sma-imageenhancercuda
    DESTINATION ${INSTALL_PATH_LIB})
endif ()