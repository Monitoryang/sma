file(GLOB assembly_src_list ./*.cpp ./*.h)

add_library(eap-sma-assemblyjson ${assembly_src_list})
add_library(EAPS::AssemblyJson ALIAS eap-sma-assemblyjson)

set(LOCATION_LINK_LIBRARIES ${LINK_LIBRARIES})
list(REMOVE_ITEM LOCATION_LINK_LIBRARIES EAPS::AssemblyJson)
set(LOCATION_COMPILE_DEFINITIONS ${COMPILE_DEFINITIONS})

if(ENABLE_AR)
    list(APPEND LOCATION_COMPILE_DEFINITIONS ENABLE_AR)
endif(ENABLE_AR)

if(ENABLE_AR)
#gdal##################
find_package(Gdal REQUIRED)
find_package(Eigen3 REQUIRED)
if(GDAL_FOUND)
  message(STATUS "Found Gdal: ${GDAL_INCLUDE_DIRS}")
  message(STATUS "Found Gdal library: ${GDAL_LIBRARIES}")
else()
  message(WARNING "Could not find Gdal library.")
endif()
endif()

target_compile_definitions(eap-sma-assemblyjson
  PUBLIC ${LOCATION_COMPILE_DEFINITIONS})
target_compile_options(eap-sma-assemblyjson
  PRIVATE ${COMPILE_OPTIONS_DEFAULT})
target_link_libraries(eap-sma-assemblyjson
  PUBLIC ${LOCATION_LINK_LIBRARIES}
          ${GDAL_LIBRARIES} Eigen3::Eigen)
if(ENABLE_AR)
target_include_directories(eap-sma-assemblyjson
        PRIVATE
          "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
        PUBLIC
          "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
           ${GDAL_INCLUDE_DIRS})
else()
target_include_directories(eap-sma-assemblyjson
        PRIVATE
          "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
        PUBLIC
          "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")
endif()

update_cached_list(LINK_LIBRARIES EAPS::AssemblyJson)