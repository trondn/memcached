ADD_LIBRARY(membase SHARED
            convert.c
            memory.c
            membase.c
            uprengine.cc)
SET_TARGET_PROPERTIES(membase PROPERTIES PREFIX "")
TARGET_LINK_LIBRARIES(membase mcd_util platform ${COUCHBASE_NETWORK_LIBS})
INSTALL(TARGETS membase
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib/memcached
        ARCHIVE DESTINATION lib/memcached)

SET_TARGET_PROPERTIES(membase PROPERTIES INSTALL_NAME_DIR ${CMAKE_INSTALL_PREFIX}/lib/memcached)
