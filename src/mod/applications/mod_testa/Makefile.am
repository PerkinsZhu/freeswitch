include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_testa

mod_LTLIBRARIES = mod_testa.la
mod_testa_la_SOURCES  = mod_testa.c
mod_testa_la_CFLAGS   = $(AM_CFLAGS)
mod_testa_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_testa_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
