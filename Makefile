#!/usr/bin/make -f

LIBRARY       = libfio
PROGRAM       = fio
VERSION       = 0.1
LIB_SOURCES   = fio.c
LIB_HEADERS   = fio.h
SOURCES       = main.c
HEADERS       = 
LIB_PACKAGES  = glib-2.0
PACKAGES      = glib-2.0

EXTRA_DIST = Makefile


DISTFILES = $(LIB_SOURCES) \
            $(LIB_HEADERS) \
            $(SOURCES) \
            $(HEADERS) \
            $(EXTRA_DIST)


CC         ?= cc
PKG_CONFIG ?= pkg-config
RM         ?= rm -f
RMDIR      ?= rmdir
GZIP       ?= gzip --best
TAR        ?= tar
TARFILE    ?= $(PROGRAM)-$(VERSION).tar.gz

ifeq ($(CFLAGS), )
	default_cflags = -ansi -pedantic -W -Wall -Werror-implicit-function-declaration -O2 -g
else
	default_cflags = $(CFLAGS)
endif

LIB_CFLAGS  ?= $(default_cflags)
LIB_LIBS    ?=
LIB_LDFLAGS ?=
CFLAGS      ?= $(default_cflags)
LIBS        ?=
LDFLAGS     ?=

LIB_CFLAGS  += $(shell $(PKG_CONFIG) --cflags $(LIB_PACKAGES) 2>/dev/null) -fPIC
LIB_LIBS    += $(shell $(PKG_CONFIG) --libs $(LIB_PACKAGES) 2>/dev/null)
LIB_LDFLAGS +=
CFLAGS      += $(shell $(PKG_CONFIG) --cflags $(PACKAGES) 2>/dev/null) -fPIC
LIBS        += $(shell $(PKG_CONFIG) --libs $(PACKAGES) 2>/dev/null)
LDFLAGS     +=

mk_deps_prepare = test -d .deps || mkdir .deps
mk_deps_cflags  = -MP -MD -MF .deps/$@ -MT $@

libobj_pfx   = $(LIBRARY)_
obj_pfx      = $(PROGRAM)_
LIB_OBJECTS  = $(LIB_SOURCES:%.c=$(libobj_pfx)%.o)
OBJECTS      = $(SOURCES:%.c=$(obj_pfx)%.o)


V ?= 0
mk_v_cc = $(mk_v_cc_$(V))
mk_v_cc_0 = @echo ' CC     $@';
mk_v_cc_1 = 
mk_v_ld = $(mk_v_ld_$(V))
mk_v_ld_0 = @echo ' LD     $@';
mk_v_ld_1 = 
mk_v_gen = $(mk_v_gen_$(V))
mk_v_gen_0 = @echo ' GEN   $@';
mk_v_gen_1 = 
mk_v_silent = $(mk_v_silent_$(V))
mk_v_silent_0 = @
mk_v_silent_1 = 



.PHONY: all clean distclean dist $(TARFILE)

all: $(LIBRARY) $(PROGRAM)

include $(wildcard .deps/*)

.deps:
	@test -d $@ || mkdir $@

$(LIBRARY): $(LIB_OBJECTS)
	$(mk_v_ld) $(CC) -o $@ -shared $^ $(LIB_LDFLAGS) $(LIB_LIBS)

$(libobj_pfx)%.o: %.c
	$(mk_v_silent) $(mk_deps_prepare)
	$(mk_v_cc) $(CC) -o $@ -c $< $(LIB_CFLAGS) $(mk_deps_cflags)

$(PROGRAM): $(OBJECTS)
	$(mk_v_ld) $(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

$(obj_pfx)%.o: %.c
	$(mk_v_silent) $(mk_deps_prepare)
	$(mk_v_cc) $(CC) -o $@ -c $< $(CFLAGS) $(mk_deps_cflags)

clean:
	$(RM) $(LIB_OBJECTS) $(OBJECTS)
	$(RM) $(LIB_OBJECTS:%=.deps/%) $(OBJECTS:%=.deps/%)
distclean: clean
	$(RM) $(LIBRARY) $(PROGRAM)
	test ! -d .deps || $(RMDIR) .deps

dist: $(TARFILE)
$(TARFILE):
	$(mk_v_gen) $(TAR) -c $(DISTFILES) | $(GZIP) -c > $@
