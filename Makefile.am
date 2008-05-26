bin_PROGRAMS = memcached memcached-debug
include_HEADERS = engine.h

memcached_SOURCES = memcached.c memcached.h \
                    thread.c \
                    slabs.c slabs.h \
                    items.c items.h \
                    assoc.c assoc.h \
                    stats.c stats.h \
                    slab_engine.c slab_engine.h

memcached_debug_SOURCES = $(memcached_SOURCES)
memcached_CPPFLAGS = -DNDEBUG
memcached_LDADD = @LIBOBJS@
memcached_debug_LDADD = $(memcached_LDADD)

SUBDIRS = doc
DIST_DIRS = scripts
EXTRA_DIST = doc scripts TODO t memcached.spec

test:	memcached-debug
	prove $(srcdir)/t

dist-hook:
	rm -rf $(distdir)/doc/.svn/
	rm -rf $(distdir)/scripts/.svn/
	rm -rf $(distdir)/t/.svn/
	rm -rf $(distdir)/t/lib/.svn/
