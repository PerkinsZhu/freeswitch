include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_mbs

mod_LTLIBRARIES = mod_mbs.la
mod_mbs_la_SOURCES  = mod_mbs.cpp
mod_mbs_la_CFLAGS   = $(AM_CFLAGS)
mod_mbs_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_mbs_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
MOD_LDFLAGS += -lavcodec -lavutil -lavformat