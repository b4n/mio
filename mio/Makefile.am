lib_LTLIBRARIES = libmio.la

libmio_la_SOURCES  = mio.c
libmio_la_CPPFLAGS = -DG_LOG_DOMAIN=\"MIO\" -DMIO_DEBUG
libmio_la_CFLAGS   = @GLIB_CFLAGS@
libmio_la_LIBADD   = @GLIB_LIBS@
libmio_la_LDFLAGS  = -version-info @MIO_LTVERSION@

EXTRA_DIST = mio-file.c \
             mio-memory.c

mio_includedir = $(includedir)/mio
mio_include_HEADERS = mio.h
