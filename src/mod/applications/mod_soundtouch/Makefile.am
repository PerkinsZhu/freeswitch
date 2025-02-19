include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_soundtouch

mod_LTLIBRARIES = mod_soundtouch.la
mod_soundtouch_la_SOURCES  = mod_soundtouch.cpp
mod_soundtouch_la_CFLAGS   = $(AM_CFLAGS)
mod_soundtouch_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_soundtouch_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
