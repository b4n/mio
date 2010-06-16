lib_LTLIBRARIES = libfio.la

libfio_la_SOURCES  = fio.c
libfio_la_CPPFLAGS = -DG_LOG_DOMAIN=\"FIO\"
libfio_la_CFLAGS   = @GLIB_CFLAGS@
libfio_la_LIBADD   = @GLIB_LIBS@
libfio_la_LDFLAGS  = -version-info @FIO_LTVERSION@

fio_includedir = $(includedir)/fio
fio_include_HEADERS = fio.h
