# 当前默认windows使用的CPU架构是x86_64
cmake_minimum_required(VERSION 3.1.3)

project(JoEAPSma LANGUAGES C CXX)

set(RELEASE_VERSION "1.0.1.3")

# 加载自定义模块 - 指定module模式中，find_package 查找 Find<库名>.cmake 的目录
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# set(CMAKE_AR "/usr/bin/aarch64-linux-gnu-gcc-ar")
# set(CMAKE_RANLIB "/usr/bin/aarch64-linux-gnu-gcc-ranlib")

# 添加Poco和OpenCV的cmake module
if(WIN32)
	list(APPEND ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/3rdpart_binary/poco/cmake")
    set(Poco_DIR ${CMAKE_CURRENT_SOURCE_DIR}/3rdpart_prebuilt/poco/cmake)
#	set(OpenCV_DIR ${CMAKE_CURRENT_SOURCE_DIR}/3rdpart_prebuilt/opencv_lib_self)
else()
# set(OpenCV_DIR /usr/local/lib/cmake/opencv4/)
endif()

# 使能c++17
# 考虑国产操作系统，可能需要降低版本
set(CMAKE_CXX_STANDARD 17)

# option 在子目录下的cmakelists中也可识别到， add_definitions 增加的宏定义不能识别，需要增加到list中
option(ENABLE_ASAN "Enable Address Sanitize" OFF)
option(ENABLE_MEM_DEBUG "Enable Memory Debug" OFF)
option(ENABLE_TESTS "Enable Tests" OFF)
option(ENABLE_THIS_SERVICE "Enable This Service" ON)
option(USE_SOLUTION_FOLDERS "Enable solution dir supported" ON)
option(ENABLE_AIRBORNE "Enable airborne deployment function" ON)
option(ENABLE_AI "Enable ai function" ON)
option(ENABLE_ONNXTOENGINE "Enable onnxtoengine function" OFF)
option(ENABLE_AR "Enable ar function" OFF)
option(ENABLE_APPLICATION "Enable application" ON)
option(ENABLE_CLOUD "Enable application" OFF)
option(ENABLE_GROUND "Enable application" OFF)
option(ENABLE_GPU "ENABLE GPU" ON)
option(ENABLE_3588 "ENABLE 3588" OFF)
option(ENABLE_STABLIZE "ENABLE STABLIZE FEATURE" OFF)
option(ENABLE_OPENSET_DETECTION "ENABLE OPENSET DETECT" OFF)
option(ENABLE_VISION_ENHANCE "ENABLE VISION ENHANCE" OFF)
option(ENABLE_PIO "Enable pio function" OFF)
option(ENABLE_FIRE_DETECTION "Enable pio function" OFF)

if(ENABLE_GPU)
	add_definitions(-DENABLE_GPU)
endif()
if(ENABLE_PIO)
	add_definitions(-DENABLE_PIO)
endif()
if(ENABLE_FIRE_DETECTION)
	add_definitions(-DENABLE_FIRE_DETECTION)
endif()
if(ENABLE_3588)
	add_definitions(-DENABLE_3588)
endif()
if(ENABLE_APPLICATION)
    add_definitions(-DAPPLICATION_ENABLE)
endif()
if(ENABLE_CLOUD)
	add_definitions(-DENABLE_CLOUD)
endif()
if(ENABLE_GROUND)
  add_definitions(-DENABLE_GROUND)
endif()
if(ENABLE_AIRBORNE)
  add_definitions(-DENABLE_AIRBORNE)
endif()
##############################################################################
# 默认编译debug
if("${CMAKE_BUILD_TYPE}" STREQUAL "")
  set(CMAKE_BUILD_TYPE "Debug")
endif()
message(STATUS "编译类型: ${CMAKE_BUILD_TYPE}")

# 方便排查编译问题, 需要 FORCE CACHE, 否则需要命令行设置才生效
set(CMAKE_VERBOSE_MAKEFILE ON CACHE INTERNAL "" FORCE)

# 自动添加CMAKE_CURRENT_BINARY_DIR和CMAKE_CURRENT_SOURCE_DIR到当前处理的CMakeLists.txt
set(CMAKE_INCLUDE_CURRENT_DIR OFF)

# 安装路径 CMAKE_INSTALL_PREFIX-install命令安装的前缀，默认值为/usr/local
# CMAKE_INSTALL_PREFIX会被添加到CMAKE_SYSTEM_PREFIX_PATH,所以find_package、find_library等命令也会以改目录为prefix去查找
if(NOT CMAKE_INSTALL_PREFIX)
  if(UNIX)
    set(INSTALL_PATH_LIB     lib${LIB_SUFFIX})
    set(INSTALL_PATH_INCLUDE include)
    set(INSTALL_PATH_RUNTIME bin)
  elseif(WIN32)
    # Windows 下安装到了用户主目录下?
    set(INSTALL_PATH_LIB     $ENV{HOME}/${CMAKE_PROJECT_NAME}/lib)
    set(INSTALL_PATH_INCLUDE $ENV{HOME}/${CMAKE_PROJECT_NAME}/include)
  else()
    message(WARNING "该平台(${CMAKE_SYSTEM_NAME})下暂未设置安装路径")
  endif()
else()
  set(INSTALL_PATH_LIB     ${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX})
  set(INSTALL_PATH_INCLUDE ${CMAKE_INSTALL_PREFIX}/include)
  set(INSTALL_PATH_RUNTIME ${CMAKE_INSTALL_PREFIX}/bin)
endif()

# 字符串处理 - TOLOWER 修改
string(TOLOWER ${CMAKE_SYSTEM_NAME} SYSTEM_NAME_LOWER)
# 设置输出目录, 包括 bin, lib 以及其他文件
# Windows 也不再区分 32/64
set(OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/release/${SYSTEM_NAME_LOWER}/${CMAKE_BUILD_TYPE})
set(LIBRARY_OUTPUT_PATH    ${OUTPUT_DIR})
set(EXECUTABLE_OUTPUT_PATH ${OUTPUT_DIR})

# 添加 git 版本信息
set(COMMIT_HASH "Git_Unkown_commit")
set(COMMIT_TIME "Git_Unkown_time")
set(BRANCH_NAME "Git_Unkown_branch")
set(BUILD_TIME "")

# 字符串处理 - TIMESTAMP 生成
string(TIMESTAMP BUILD_TIME "%Y-%m-%dT%H:%M:%S")

# 查找Git，QUIET静默方式不报错
find_package(Git QUIET)
if(GIT_FOUND)
  execute_process(              # 执行一个子进程
    COMMAND ${GIT_EXECUTABLE} rev-parse --short=7 HEAD # 命令
    OUTPUT_VARIABLE COMMIT_HASH # 输出字符串存入变量
    OUTPUT_STRIP_TRAILING_WHITESPACE # 删除字符串的换行符
    ERROR_QUIET                         # 对执行错误静默
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}) # 执行路径
  execute_process(
    COMMAND ${GIT_EXECUTABLE} symbolic-ref --short -q HEAD
    OUTPUT_VARIABLE BRANCH_NAME
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

  execute_process(
    COMMAND ${GIT_EXECUTABLE} log --format=format:%aI -1
    OUTPUT_VARIABLE COMMIT_TIME
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/version.h.ini
  ${CMAKE_CURRENT_BINARY_DIR}/version.h
  @ONLY)

message(STATUS "Git version is ${BRANCH_NAME} ${COMMIT_HASH}/${COMMIT_TIME} ${BUILD_TIME}")
message(STATUS "library release version: ${RELEASE_VERSION}")

##############################################################################
# 方便修改全局变量
function(update_cached name value)
  set("${name}" "${value}" CACHE INTERNAL "*** Internal ***" FORCE)
endfunction()

function(update_cached_list name)
  set(_tmp_list "${${name}}")
  list(APPEND _tmp_list "${ARGN}")
  list(REMOVE_DUPLICATES _tmp_list)
  update_cached(${name} "${_tmp_list}")
endfunction()

# TODO:
function(set_file_group prefix)
  message(STATUS "set_file_group " ${prefix} " " ${ARGC})
  foreach(FILE IN LISTS ARGN 1)
    # Get the directory of the source file
    get_filename_component(PARENT_DIR "${FILE}" DIRECTORY)

    # Remove common directory prefix to make the group
    string(REPLACE "${prefix}" "" GROUP "${PARENT_DIR}")

    # Make sure we are using windows slashes
    string(REPLACE "/" "\\" GROUP "${GROUP}")

    source_group("${GROUP}" FILES "${FILE}")
  endforeach()
endfunction()

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK    ccache)
endif()

if(UNIX)
  # UNIX/Linux/Darwin
  set(COMPILE_OPTIONS_DEFAULT
    "-fPIC"
    "-Wall;-Wextra"
    "-Wno-unused-function;-Wno-unused-parameter;-Wno-unused-variable"
    "-Wno-error=extra;-Wno-error=missing-field-initializers;-Wno-error=type-limits")
elseif(WIN32)
  if (MSVC)
    set(COMPILE_OPTIONS_DEFAULT
            # TODO: /wd4819 应该是不会生效
            "/wd4566;/wd4819"
            # warning C4530: C++ exception handler used, but unwind semantics are not enabled.
            "/EHsc"
			# 为了在使用msvc编译器时，当源代码的字符编码格式为utf-8，以便报错 常量中有换行符
			"$<$<CXX_COMPILER_ID:MSVC>:/source-charset:utf-8>")
            # 设置编译器选项，将编译器选项设置为使用 UTF-8 编码(与 /source-charset:utf-8 冲突)
            # add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
            # 设置生成属性，将生成属性设置为使用 utf-8 编码
            #add_definitions(-DUNICODE -D_UNICODE)
    # disable Windows logo
    #list(APPEND COMPILE_OPTIONS_DEFAULT "/nologo")
    #list(APPEND CMAKE_STATIC_LINKER_FLAGS "/nologo")
  endif()
endif()

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
  if(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "arm64")
    include_directories(SYSTEM "/opt/homebrew/include")
  endif()
endif()

update_cached_list(LINK_LIBRARIES "")
update_cached_list(LINK_LIBRARIES_DEBUG "")
update_cached_list(LINK_LIBRARIES_RELEASE "")
update_cached_list(COMPILE_DEFINITIONS ENABLE_VERSION MSGPACK_NO_BOOST)

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  include(CheckCXXSourceCompiles)
  file(READ ${CMAKE_CURRENT_SOURCE_DIR}/cmake/checks/atomic_check.cpp atomic_check_cpp)
  check_cxx_source_compiles("${atomic_check_cpp}" HAVE_CXX_ATOMICS_WITHOUT_LIB)
  if(NOT HAVE_CXX_ATOMICS_WITHOUT_LIB)
    # cmake --help-policy CMP0075
    list(APPEND CMAKE_REQUIRED_LIBRARIES "atomic")
    check_cxx_source_compiles("${atomic_check_cpp}" HAVE_CXX_ATOMICS_WITH_LIB)
    if(NOT HAVE_CXX_ATOMICS_WITH_LIB)
      message(WARNING "Compiler doesn't support std::atomic<long long>")
    else()
      update_cached_list(LINK_LIBRARIES atomic)
    endif()
  endif()
endif()

if(ENABLE_MEM_DEBUG)
  update_cached_list(LINK_LIBRARIES
    "-Wl,-wrap,free;-Wl,-wrap,malloc;-Wl,-wrap,realloc;-Wl,-wrap,calloc")
  update_cached_list(COMPILE_DEFINITIONS ENABLE_MEM_DEBUG)
  message(STATUS "已启用内存调试功能")
endif()

if(ENABLE_ASAN)
  list(APPEND COMPILE_OPTIONS_DEFAULT
    "-fsanitize=address;-fno-omit-frame-pointer")
  # https://github.com/google/sanitizers/wiki/AddressSanitizer#using-addresssanitizer
  # > In order to use AddressSanitizer you will need to
  # > compile and link your program using clang with the -fsanitize=address switch.
  update_cached_list(LINK_LIBRARIES "-fsanitize=address")
  message(STATUS "已启用 Address Sanitize")
endif()

# 查找 openssl 是否安装
find_package(OpenSSL QUIET)
if(OPENSSL_FOUND)
  message(STATUS "found library: ${OPENSSL_LIBRARIES}")
  include_directories(${OPENSSL_INCLUDE_DIR})
  update_cached_list(COMPILE_DEFINITIONS ENABLE_OPENSSL)
  update_cached_list(LINK_LIBRARIES ${OPENSSL_LIBRARIES})
  if(CMAKE_SYSTEM_NAME MATCHES "Linux" AND OPENSSL_USE_STATIC_LIBS)
    update_cached_list(LINK_LIBRARIES ${CMAKE_DL_LIBS})
  endif()
else()
  message(ERROR "openssl 未找到, 无法使用poco netssl")
endif()

# find poco
if(WIN32)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdpart_prebuilt/poco/include)
  link_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdpart_prebuilt/poco/lib)
  else()
    find_package(Poco REQUIRED Foundation Util Net NetSSL JSON XML Zip)
    if(Poco_FOUND)
      update_cached_list(LINK_LIBRARIES Poco::Foundation Poco::Util Poco::Net  Poco::NetSSL Poco::JSON Poco::XML Poco::Zip)
    else()
      message(error "Poco library not found")
    endif()
endif()

if(WIN32)
	set(GDAL_PATH_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/3rdpart_prebuilt/gdal-v242/x64/native)
endif(WIN32)
 
if(WIN32)
  update_cached_list(LINK_LIBRARIES WS2_32 Iphlpapi shlwapi)
elseif(NOT ANDROID OR IOS)
  update_cached_list(LINK_LIBRARIES pthread)
endif()

# find eap common plugin
if(WIN32)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdpart_prebuilt/eapbase/include)
    link_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdpart_prebuilt/eapbase/lib)
else()
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
      update_cached_list(LINK_LIBRARIES libJoEAPCommond.so libJoEAPPluginsd.so)
    else()
      update_cached_list(LINK_LIBRARIES libJoEAPCommon.so libJoEAPPlugins.so)
    endif()
endif()

if(ENABLE_STABLIZE)
    update_cached_list(LINK_LIBRARIES /usr/local/lib/libjo_stabvision.so)
endif()

# solution folders
if(USE_SOLUTION_FOLDERS)
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)
  set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMakeTargets")
endif(USE_SOLUTION_FOLDERS)

# 全局include lib 
include_directories(${CMAKE_CURRENT_BINARY_DIR})# for version.h
if(ENABLE_AIRBORNE)
  include_directories(/usr/local/include/boost)# for asio
else()
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdpart/rest_rpc/thirdparty/asio)# for asio
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdpart/rest_rpc/thirdparty/msgpack-c/include)# for msgpack
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdpart/rest_rpc/include)# for rest rpc
endif()

if(ENABLE_PIO)
  include_directories(/usr/local/include/jo_sdk
  /usr/local/include/jo_manage
  /usr/local/include/zmqpp)
  update_cached_list(LINK_LIBRARIES zmq zmqpp jo_manage)
endif(ENABLE_PIO)

set(CPPTRACE_USE_EXTERNAL_LIBDWARF TRUE)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdpart/cpptrace/include)# for cpptrace

add_subdirectory(3rdpart/cpptrace)
update_cached_list(LINK_LIBRARIES cpptrace::cpptrace)

# 核心组件
add_subdirectory(src)

# 测试
# if(ENABLE_TESTS)
# 	add_subdirectory(test)
# endif(ENABLE_TESTS)