file(GLOB PIO_SRC_LIST
  ${CMAKE_CURRENT_SOURCE_DIR}/*.c
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
  
add_library(eap-sma-pio STATIC ${PIO_SRC_LIST})
add_library(EAPS::EapsPioLib ALIAS eap-sma-pio)

set(LOCATION_LINK_LIBRARIES ${LINK_LIBRARIES})
list(REMOVE_ITEM LOCATION_LINK_LIBRARIES EAPS::EapsPioLib)
set(LOCATION_COMPILE_DEFINITIONS ${COMPILE_DEFINITIONS})
if(ENABLE_AI)	
    list(APPEND LOCATION_COMPILE_DEFINITIONS ENABLE_AI)
    message(STATUS "ENABLE ENABLE_PIO:ENABLE_AI")
endif(ENABLE_AI)

if(ENABLE_PIO)	
    list(APPEND LOCATION_COMPILE_DEFINITIONS ENABLE_PIO)
    message(STATUS "ENABLE ENABLE_PIO:ENABLE_PIO")
endif(ENABLE_PIO)

target_compile_definitions(eap-sma-pio
  PUBLIC ${LOCATION_COMPILE_DEFINITIONS})
target_compile_options(eap-sma-pio
  PRIVATE ${COMPILE_OPTIONS_DEFAULT})

target_link_libraries(eap-sma-pio
  PUBLIC ${LOCATION_LINK_LIBRARIES})
# 动态库link时，可以直接找到头文件目录
target_include_directories(eap-sma-pio
  PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"     
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")

update_cached_list(LINK_LIBRARIES EAPS::EapsPioLib)

# 未在使用
if(ENABLE_CXX_API)
  # 保留目录结构
  install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION ${INSTALL_PATH_INCLUDE}/EDGE
    REGEX ".*[.](md|cpp)$" EXCLUDE)
  install(TARGETS eap-sma-pio
    DESTINATION ${INSTALL_PATH_LIB})
endif ()