CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

IF (${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
   # We are being built as a single module
   SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/../tlm/cmake/Modules/")
   INCLUDE(CouchbaseSingleModuleBuild)
ENDIF ()

#
# Set the include path
#
INCLUDE_DIRECTORIES(BEFORE ${DEPS_INCLUDE_DIR}
                           ${INSTALL_ROOT}/include
                           ${CMAKE_CURRENT_SOURCE_DIR}/include
                           ${CMAKE_CURRENT_SOURCE_DIR})

IF (WIN32)
   INCLUDE_DIRECTORIES(AFTER ${CMAKE_CURRENT_SOURCE_DIR}/win32)
   SET(PLATFORM_FILES libplatform/getopt.c
                      libplatform/sockets.c
                      libplatform/cb_win32.c)
   SET(NETWORK_LIBS "Ws2_32")
   SET(LIBEVENT_CORE "libevent_core")
   SET(TCMALLOC_LIBRARY libtcmalloc_minimal)

ELSE (WIN32)
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
   SET_TARGET_PROPERTIES(file_logger PROPERTIES PREFIX "")

   INSTALL(TARGETS file_logger
           RUNTIME DESTINATION lib
           LIBRARY DESTINATION lib
           ARCHIVE DESTINATION lib)

   ADD_EXECUTABLE(mcbasher programs/mcbasher.cc)
   TARGET_LINK_LIBRARIES(mcbasher platform ${NETWORK_LIBS})
   ADD_EXECUTABLE(timedrun programs/timedrun.c)
if (APPLE)
   # The TCMalloc in homebrew cause the application to dump core..
   ADD_DEFINITIONS(-DDONT_HAVE_TCMALLOC=1)
ELSE(APPLE)
   SET(TCMALLOC_LIBRARY tcmalloc_minimal)
ENDIF(APPLE)

ENDIF (WIN32)

#
# Add all of the libraries
#
ADD_LIBRARY(platform SHARED ${PLATFORM_FILES} libplatform/gethrtime.c)

ADD_LIBRARY(mcd_util SHARED
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

ADD_CUSTOM_COMMAND(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/generated_breakdancer_testsuite.c
                  COMMAND
                        python ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/breakdancer/engine_test.py > ${CMAKE_CURRENT_BINARY_DIR}/generated_breakdancer_testsuite.c
                  DEPENDS
                        testsuite/breakdancer/breakdancer.py
                        testsuite/breakdancer/engine_test.py
                  COMMENT "Generating the test suite")

ADD_LIBRARY(breakdancer_testsuite SHARED
            testsuite/breakdancer/suite_stubs.c
            ${CMAKE_CURRENT_BINARY_DIR}/generated_breakdancer_testsuite.c)

SET_TARGET_PROPERTIES(default_engine PROPERTIES PREFIX "")
SET_TARGET_PROPERTIES(bucket_engine PROPERTIES PREFIX "")
SET_TARGET_PROPERTIES(basic_engine_testsuite PROPERTIES PREFIX "")
SET_TARGET_PROPERTIES(blackhole_logger PROPERTIES PREFIX "")
SET_TARGET_PROPERTIES(fragment_rw_ops PROPERTIES PREFIX "")
SET_TARGET_PROPERTIES(stdin_term_handler PROPERTIES PREFIX "")
SET_TARGET_PROPERTIES(tap_mock_engine PROPERTIES PREFIX "")
SET_TARGET_PROPERTIES(bucket_engine_mock_engine PROPERTIES PREFIX "")
SET_TARGET_PROPERTIES(breakdancer_testsuite PROPERTIES PREFIX "")



#
# All of the binaries we're building
#
ADD_EXECUTABLE(bucket_engine_testapp
               engines/bucket_engine/testapp.c
               engines/bucket_engine/genhash.c)
ADD_EXECUTABLE(engine_testapp programs/engine_testapp.c programs/mock_server.c)
ADD_EXECUTABLE(isasladm programs/isasladm.c)
ADD_EXECUTABLE(mcstat programs/mcstat.c)
ADD_EXECUTABLE(memcached_sizes programs/sizes.c)
ADD_EXECUTABLE(mcd
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
TARGET_LINK_LIBRARIES(bucket_engine mcd_util platform ${NETWORK_LIBS})
TARGET_LINK_LIBRARIES(default_engine mcd_util platform ${NETWORK_LIBS})
TARGET_LINK_LIBRARIES(basic_engine_testsuite mcd_util platform ${NETWORK_LIBS})
TARGET_LINK_LIBRARIES(stdin_term_handler platform)
TARGET_LINK_LIBRARIES(fragment_rw_ops mcd_util ${NETWORK_LIBS})
TARGET_LINK_LIBRARIES(engine_testapp mcd_util platform ${NETWORK_LIBS})
TARGET_LINK_LIBRARIES(bucket_engine_testapp mcd_util platform ${NETWORK_LIBS} ${MATH_LIBS})
TARGET_LINK_LIBRARIES(isasladm platform ${NETWORK_LIBS})
TARGET_LINK_LIBRARIES(platform ${THREAD_LIBS} ${NETWORK_LIBS} ${DLOPENLIB})
TARGET_LINK_LIBRARIES(mcstat platform ${NETWORK_LIBS})
TARGET_LINK_LIBRARIES(tap_mock_engine platform ${NETWORK_LIBS})

TARGET_LINK_LIBRARIES(mcd_util platform)
TARGET_LINK_LIBRARIES(mcd mcd_util platform ${TCMALLOC_LIBRARY} ${LIBEVENT_CORE} ${NETWORK_LIBS})
TARGET_LINK_LIBRARIES(memcached_testapp mcd_util platform ${LIBEVENT_CORE} ${NETWORK_LIBS})

IF(WIN32)
    SET_TARGET_PROPERTIES(mcd PROPERTIES
                          LINK_FLAGS "/LIBPATH:${DEPS_LIB_DIR}")
    SET_TARGET_PROPERTIES(memcached_testapp PROPERTIES
                          LINK_FLAGS "/LIBPATH:${DEPS_LIB_DIR}")
ELSE(WIN32)
    SET_TARGET_PROPERTIES(mcd PROPERTIES
                          LINK_FLAGS -L${DEPS_LIB_DIR})
    SET_TARGET_PROPERTIES(memcached_testapp PROPERTIES
                          LINK_FLAGS -L${DEPS_LIB_DIR})
ENDIF(WIN32)

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


SET_TARGET_PROPERTIES(mcd_util PROPERTIES SOVERSION 1.0.0)
INSTALL(TARGETS mcd_util default_engine bucket_engine blackhole_logger fragment_rw_ops stdin_term_handler
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib/memcached
        ARCHIVE DESTINATION lib/memcached)

INSTALL(TARGETS platform
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

INSTALL(TARGETS engine_testapp isasladm mcstat mcd
        RUNTIME DESTINATION bin)

ADD_TEST(memcached-sizes memcached_sizes)
ADD_TEST(memcached-basic-unit-tests memcached_testapp)
ADD_TEST(bucket_engine-basic-unit-tests bucket_engine_testapp)
ADD_TEST(basic-engine-tests engine_testapp -E default_engine.so -T basic_engine_testsuite.so)
ADD_TEST(breakdancer-engine-tests engine_testapp -E default_engine.so -T breakdancer_testsuite.so)
