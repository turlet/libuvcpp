cmake_minimum_required(VERSION 2.8)

set(CONFIGURE_COMMMAND ./autogen.sh && ./configure)

SET(GIT_REPOSITORY_DIR "/home/turlet/Desktop/github")

IF(CMAKE_ANDROID_STANDALONE_TOOLCHAIN)
  set(CONFIGURE_COMMMAND ${CONFIGURE_COMMMAND}
    --with-sysroot=${CMAKE_ANDROID_STANDALONE_TOOLCHAIN}
    --host=arm-linux-androideabi
    --enable-static
    --disable-shared
    )
ENDIF()

################################
#            libuv             #
################################
include(ExternalProject)
ExternalProject_Add(libuv
  GIT_REPOSITORY "https://github.com/libuv/libuv.git"
  #GIT_REPOSITORY "${GIT_REPOSITORY_DIR}/libuv"
  GIT_TAG "v1.x"
  #GIT_TAG "v1.22.0"
  CONFIGURE_COMMAND ${CONFIGURE_COMMMAND}
  BUILD_IN_SOURCE 1
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND "" # remove install step
  TEST_COMMAND "" # remove test step
  UPDATE_COMMAND "" # remove update step
  GIT_SHALLOW 1
  GIT_PROGRESS 1
  )
#ExternalProject_Get_Property(libuv source_dir)
#set(UV_INCLUDE_DIR ${source_dir}/include)
#set(UV_LIBRARY ${source_dir}/.libs/libuv.a)
#file(MAKE_DIRECTORY ${UV_INCLUDE_DIR})

#add_library(uv STATIC IMPORTED)
#add_dependencies(uv libuv)
#set_target_properties(uv PROPERTIES
  #INTERFACE_INCLUDE_DIRECTORIES ${UV_INCLUDE_DIR}
  #IMPORTED_LOCATION ${UV_LIBRARY}
  #INTERFACE_LINK_LIBRARIES "pthread"
  #)

ExternalProject_Add(libutil
  GIT_REPOSITORY "https://github.com/turlet/libutil.git"
  #GIT_REPOSITORY "${GIT_REPOSITORY_DIR}/libutil"
  GIT_TAG "0.0.1"
  CMAKE_ARGS ""
  UPDATE_COMMAND ""
  INSTALL_COMMAND ""
  TEST_COMMAND ""
  GIT_SHALLOW 1
  GIT_PROGRESS 1
)

add_library(uvcpp INTERFACE)
target_include_directories(uvcpp INTERFACE src)

