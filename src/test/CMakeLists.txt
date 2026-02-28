file(GLOB MAIN_SERVICE_SRC_LIST ./*.cpp ./*.h)

set(LOCATION_COMPILE_DEFINITIONS ${COMPILE_DEFINITIONS})
set(LOCATION_LINK_LIBRARIES ${LINK_LIBRARIES})

set(TEST_NAME JoTest)

add_executable(${TEST_NAME} ${MAIN_SERVICE_SRC_LIST})

target_compile_definitions(${TEST_NAME}
  PRIVATE ${LOCATION_COMPILE_DEFINITIONS})
target_compile_options(${TEST_NAME}
  PRIVATE ${COMPILE_OPTIONS_DEFAULT})

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  target_link_libraries(${TEST_NAME} -Wl,--start-group ${LOCATION_LINK_LIBRARIES} -Wl,--end-group)
else()
  target_link_libraries(${TEST_NAME} PRIVATE ${LOCATION_LINK_LIBRARIES})
endif()

install(TARGETS ${TEST_NAME} DESTINATION ${INSTALL_PATH_RUNTIME})