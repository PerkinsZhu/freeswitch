include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_helloworld

mod_LTLIBRARIES = mod_helloworld.la
mod_helloworld_la_SOURCES  = mod_helloworld.c
mod_helloworld_la_CFLAGS   = $(AM_CFLAGS)
mod_helloworld_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_helloworld_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
