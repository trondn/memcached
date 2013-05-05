CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
#
# Inherits the following from the toplevel file..
#
#SET(DEPS_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/deps/include)
#SET(INSTALL_ROOT ${CMAKE_SOURCE_DIR}/install)
#SET(CMAKE_BUILD_WITH_INSTALL_RPATH true)

SET(INSTALL_ROOT ../install)

SET(DEPS_INCLUDE_DIR ../deps/include)
#SET(SOURCE_ROOT ${CMAKE_SOURCE_DIR}/memcached)
SET(SOURCE_ROOT .)

# TROND: er dette noe jeg vil bruke:
#  SET_TARGET_PROPERTIES(dl PROPERTIES COMPILE_FLAGS "xxx")
# og kan jeg da appende ting?


# @todo move this to the toplevel Makefile when we're done
#
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_POSIX_PTHREAD_SEMANTICS -D_GNU_SOURCE=1")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_POSIX_PTHREAD_SEMANTICS -D_GNU_SOURCE=1")

SET(CB_GNU_CPPFLAGS "-Wall -Werror -pedantic -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls -fno-strict-aliasing -fvisibility=hidden -pthread")
SET(CB_CLANG_CPPFLAGS "-Wall -pedantic -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls -fno-strict-aliasing -fvisibility=hidden -pthread")
SET(CB_MSVC_CPPFLAGS "")
SET(CB_SPRO_CPPFLAGS "-errfmt=error -errwarn -errshort=tags -xldscope=hidden -mt -D__EXTENSIONS__=1")
SET(CB_GNU_LDFLAGS "-pthread")

#
# Set flags for the C Compiler
#
if ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CB_GNU_CPPFLAGS}")
  SET(CMAKE_LINK_FLAGS "${CMAKE_LINK_FLAGS} ${CB_GNU_LDFLAGS}")
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CB_CLANG_CPPFLAGS}")
  IF (!APPLE)
     SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
  ENDIF(!APPLE)
  SET(CMAKE_LINK_FLAGS "${CMAKE_LINK_FLAGS} ${CB_GNU_LDFLAGS}")
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "MSVC")
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CB_MSVC_CPPFLAGS}")
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "SunPro")
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CB_SPRO_CPPFLAGS} -xc99=all")
endif ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")

#
# Set flags for the C++ compiler
#
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CB_GNU_CPPFLAGS}")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CB_CLANG_CPPFLAGS}")
  IF (!APPLE)
     SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
  ENDIF(!APPLE)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CB_MSVC_CPPFLAGS}")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "SunPro")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CB_SPRO_CPPFLAGS} -xlang=c99")
endif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")

#
# Set the include path
#
IF (WIN32)
   INCLUDE_DIRECTORIES(BEFORE ${DEPS_INCLUDE_DIR}
                              ${INSTALL_ROOT}/include
                              ${SOURCE_ROOT}/include
                              ${SOURCE_ROOT}
                              ${SOURCE_ROOT}/win32)
   ADD_LIBRARY(dl STATIC win32/dlfcn.c)
   SET_TARGET_PROPERTIES(dl PROPERTIES SOVERSION 1.0.0)
   INSTALL(TARGETS dl
           RUNTIME DESTINATION bin
           LIBRARY DESTINATION lib
           ARCHIVE DESTINATION lib)

   SET(PLATFORM_FILES libplatform/getopt.c
                      libplatform/sockets.c
                      libplatform/cb_win32.c)
   SET(NETWORK_LIBS "Ws2_32")
   SET(LIBEVENT_CORE "libevent_core")
   SET(GENERATE_BREAKDANCER_COMMAND "testsuite/breakdancer/generate.bat")
   SET(TCMALLOC_LIBRARY libtcmalloc_minimal-debug)

ELSE (WIN32)
   INCLUDE_DIRECTORIES(BEFORE ${INSTALL_ROOT}/include
                              ${SOURCE_ROOT}/include
                              ${SOURCE_ROOT})

   SET(PLATFORM_FILES libplatform/cb_pthreads.c)
   SET(MATH_LIBS "m")
   SET(THREAD_LIBS "pthread")
   SET(LIBEVENT_CORE "event_core")
   SET(DLOPENLIB "dl")

if (!APPLE)
   SET(NETWORK_LIBS "socket")
endif (!APPLE)

if ("${CMAKE_SYSTEM_NAME}" STREQUAL "SunOS")
   SET(NETWORK_LIBS ${NETWORK_LIBS} "nsl")
endif ("${CMAKE_SYSTEM_NAME}" STREQUAL "SunOS")

   ADD_LIBRARY(file_logger SHARED extensions/loggers/file_logger.c)
   TARGET_LINK_LIBRARIES(file_logger platform z)

   INSTALL(TARGETS file_logger
           RUNTIME DESTINATION lib
           LIBRARY DESTINATION lib
           ARCHIVE DESTINATION lib)

   ADD_EXECUTABLE(mcbasher programs/mcbasher.cc)
   TARGET_LINK_LIBRARIES(mcbasher platform ${NETWORK_LIBS})
   ADD_EXECUTABLE(timedrun programs/timedrun.c)
   SET(GENERATE_BREAKDANCER_COMMAND "testsuite/breakdancer/generate.sh")
   SET(TCMALLOC_LIBRARY tcmalloc_minimal)
ENDIF (WIN32)

#
# Add all of the libraries
#
ADD_LIBRARY(platform SHARED ${PLATFORM_FILES} libplatform/gethrtime.c)

ADD_LIBRARY(memcached_utilities SHARED
            utilities/config_parser.c
            utilities/engine_loader.c
            utilities/extension_loggers.c
            utilities/util.c)
ADD_LIBRARY(default_engine SHARED
            engines/default_engine/assoc.c
            engines/default_engine/default_engine.c
            engines/default_engine/items.c
            engines/default_engine/slabs.c)
ADD_LIBRARY(bucket_engine SHARED
            engines/bucket_engine/bucket_engine.c
            engines/bucket_engine/topkeys.c
            engines/bucket_engine/genhash.c)
ADD_LIBRARY(basic_engine_testsuite SHARED testsuite/basic_engine_testsuite.c)
ADD_LIBRARY(blackhole_logger SHARED extensions/loggers/blackhole_logger.c)
ADD_LIBRARY(fragment_rw_ops SHARED extensions/protocol/fragment_rw.c)
ADD_LIBRARY(stdin_term_handler SHARED extensions/daemon/stdin_check.c)
ADD_LIBRARY(tap_mock_engine SHARED engines/tap_mock_engine/tap_mock_engine.cc)
ADD_LIBRARY(bucket_engine_mock_engine SHARED
            engines/bucket_engine/mock_engine.c
            engines/bucket_engine/genhash.c)

ADD_CUSTOM_TARGET(GENERATE_BREAKDANCER_SOURCE
                  COMMAND ${GENERATE_BREAKDANCER_COMMAND})
ADD_LIBRARY(breakdancer_testsuite SHARED
            testsuite/breakdancer/suite_stubs.c
            testsuite/breakdancer/breakdancer_testsuite.c)
ADD_DEPENDENCIES(breakdancer_testsuite GENERATE_BREAKDANCER_SOURCE)


#
# All of the binaries we're building
#
ADD_EXECUTABLE(bucket_engine_testapp
               engines/bucket_engine/testapp.c
               engines/bucket_engine/genhash.c)
ADD_EXECUTABLE(engine_testapp programs/engine_testapp.c programs/mock_server.c)
ADD_EXECUTABLE(isasladm programs/isasladm.c)
ADD_EXECUTABLE(mcstat programs/mcstat.c)
ADD_EXECUTABLE(sizes programs/sizes.c)
ADD_EXECUTABLE(memcached
               daemon/alloc_hooks.c
               daemon/cache.c
               daemon/daemon.c
               daemon/hash.c
               daemon/isasl.c
               daemon/memcached.c
               daemon/privileges.c
               daemon/sasl_defs.c
               daemon/stats.c
               daemon/thread.c)

ADD_EXECUTABLE(memcached_testapp programs/testapp.c daemon/cache.c)


#
# Add linker flags to all of the binaries
#
TARGET_LINK_LIBRARIES(bucket_engine memcached_utilities platform ${NETWORK_LIBS})
TARGET_LINK_LIBRARIES(default_engine memcached_utilities platform ${NETWORK_LIBS})
TARGET_LINK_LIBRARIES(basic_engine_testsuite memcached_utilities platform ${NETWORK_LIBS})
TARGET_LINK_LIBRARIES(stdin_term_handler platform)
TARGET_LINK_LIBRARIES(fragment_rw_ops memcached_utilities ${NETWORK_LIBS})
TARGET_LINK_LIBRARIES(engine_testapp memcached_utilities platform ${NETWORK_LIBS})
TARGET_LINK_LIBRARIES(bucket_engine_testapp memcached_utilities platform ${NETWORK_LIBS} ${MATH_LIBS})
TARGET_LINK_LIBRARIES(isasladm platform ${NETWORK_LIBS})
TARGET_LINK_LIBRARIES(platform ${THREAD_LIBS} ${NETWORK_LIBS})
TARGET_LINK_LIBRARIES(mcstat platform ${NETWORK_LIBS})
TARGET_LINK_LIBRARIES(tap_mock_engine platform ${NETWORK_LIBS})

TARGET_LINK_LIBRARIES(memcached_utilities dl)
TARGET_LINK_LIBRARIES(memcached memcached_utilities platform ${TCMALLOC_LIBRARY} ${LIBEVENT_CORE} ${NETWORK_LIBS})
TARGET_LINK_LIBRARIES(memcached_testapp memcached_utilities platform ${LIBEVENT_CORE} ${NETWORK_LIBS})

INSTALL (FILES include/memcached/allocator_hooks.h
               include/memcached/callback.h
               include/memcached/config_parser.h
               include/memcached/engine.h
               include/memcached/engine_common.h
               include/memcached/engine_testapp.h
               include/memcached/extension.h
               include/memcached/extension_loggers.h
               include/memcached/protocol_binary.h
               include/memcached/protocol_plugin.h
               include/memcached/server_api.h
               include/memcached/types.h
               include/memcached/util.h
               include/memcached/vbucket.h
               include/memcached/visibility.h
         DESTINATION include/memcached)

INSTALL (FILES
         include/platform/platform.h
         include/platform/visibility.h
         DESTINATION include/platform)


SET_TARGET_PROPERTIES(memcached_utilities PROPERTIES SOVERSION 1.0.0)
INSTALL(TARGETS memcached_utilities default_engine bucket_engine blackhole_logger fragment_rw_ops stdin_term_handler
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib/memcached
        ARCHIVE DESTINATION lib/memcached)

INSTALL(TARGETS engine_testapp isasladm mcstat memcached
        RUNTIME DESTINATION bin)


ADD_TEST(sizes sizes)
ADD_TEST(testapp memcached_testapp)
