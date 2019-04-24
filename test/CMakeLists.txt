cmake_minimum_required(VERSION 2.8)

enable_testing()

set(CMAKE_CXX_FLAGS "-g -Wall -std=c++14 -DLOG_VERBOSE -DLOG_TAG_NAME='\"uvcpp\"'")
set(UVCPP_SRC_DIR ${CMAKE_SOURCE_DIR}/../src)
include_directories(${UVCPP_SRC_DIR})

find_package(Threads REQUIRED)


# macro to set library properties
macro(SET_LIBRARY_PROP LIB_NAME DEPENDENCY LIB_FILE)
  # Create a libgmock target to be used as a dependency by test programs
  add_library(${LIB_NAME} IMPORTED STATIC GLOBAL)
  add_dependencies(${LIB_NAME} ${DEPENDENCY})

  # Set libgmock properties
  set_target_properties(${LIB_NAME} PROPERTIES
    "IMPORTED_LOCATION" ${LIB_FILE}
    "IMPORTED_LINK_INTERFACE_LIBRARIES" "${CMAKE_THREAD_LIBS_INIT}"
    )
endmacro()


# Enable ExternalProject CMake module
include(ExternalProject)


# add uvcpp dependency
ExternalProject_Add(uvcpp
  PREFIX "${CMAKE_BINARY_DIR}/deps"
  GIT_REPOSITORY "${CMAKE_SOURCE_DIR}/.."
  GIT_TAG "master"
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=
  #UPDATE_COMMAND ""
  INSTALL_COMMAND ""
  TEST_COMMAND ""
  GIT_SHALLOW 1
  GIT_PROGRESS 1
)
ExternalProject_Get_Property(uvcpp SOURCE_DIR)
set(UVCPP_INCLUDE_DIR ${SOURCE_DIR}/src)
set(NUL_INCLUDE_DIR ${SOURCE_DIR}/../uvcpp-build/nul-prefix/src/nul/src)
set(UV_INCLUDE_DIR ${SOURCE_DIR}/../uvcpp-build/libuv-prefix/src/libuv/include)
set(UV_LIBRARY_DIR ${SOURCE_DIR}/../uvcpp-build/libuv-prefix/src/libuv/.libs)
SET_LIBRARY_PROP(uv uvcpp "${UV_LIBRARY_DIR}/libuv.a")



# add GoogleTest
ExternalProject_Add(gtest
  PREFIX "${CMAKE_BINARY_DIR}/deps"
  GIT_REPOSITORY "https://github.com/google/googletest.git"
  GIT_TAG "master"
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/deps/installed
  GIT_SHALLOW 1
  GIT_PROGRESS 1
  UPDATE_DISCONNECTED 1
)
# Get GTest source and binary directories from CMake project
ExternalProject_Get_Property(gtest INSTALL_DIR)
set(GTEST_INCLUDE_DIR ${INSTALL_DIR}/installed/include)
set(GTEST_LIBRARY_DIR ${INSTALL_DIR}/installed/lib)
SET_LIBRARY_PROP(libgtest gtest "${GTEST_LIBRARY_DIR}/libgtest.a")
SET_LIBRARY_PROP(libgmock gtest "${GTEST_LIBRARY_DIR}/libgmock.a")

include_directories(
  "${GTEST_INCLUDE_DIR}"
  "${UVCPP_INCLUDE_DIR}"
  "${NUL_INCLUDE_DIR}"
  "${UV_INCLUDE_DIR}"
  ) 

set(COMMON_SRCS main.cc)
set(COMMON_LINK_LIBS libgtest libgmock uv)

macro(ADD_UVCPP_TEST TEST_NAME TEST_SOURCE)
  add_executable(${TEST_NAME} ${COMMON_SRCS} ${TEST_SOURCE})
  target_link_libraries(${TEST_NAME} ${COMMON_LINK_LIBS})
  add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endmacro()

ADD_UVCPP_TEST(req uvcpp/req.cc)
ADD_UVCPP_TEST(tcp uvcpp/tcp.cc)
ADD_UVCPP_TEST(udp uvcpp/udp.cc)
ADD_UVCPP_TEST(pipe uvcpp/pipe.cc)
ADD_UVCPP_TEST(timer uvcpp/timer.cc)
ADD_UVCPP_TEST(prepare uvcpp/prepare.cc)
ADD_UVCPP_TEST(work uvcpp/work.cc)
ADD_UVCPP_TEST(poll uvcpp/poll.cc)

# build a executable without gtest, so we can debug the code
#add_executable(testpipe uvcpp/testpipe.cc)
#target_link_libraries(testpipe uv)

#get_cmake_property(_variableNames VARIABLES)
#list (SORT _variableNames)
#foreach (_variableName ${_variableNames})
    #message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()
