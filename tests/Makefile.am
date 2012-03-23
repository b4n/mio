if HAVE_GLIB
check_PROGRAMS = test

test_SOURCES  = main.c
test_CPPFLAGS = 
test_CFLAGS   = -I$(top_srcdir) @GLIB_CFLAGS@
test_LDFLAGS  = 
test_LDADD    = @GLIB_LIBS@ ../mio/libmio.la

TESTS = $(check_PROGRAMS)
else
EXTRA_DIST    = main.c
endif
