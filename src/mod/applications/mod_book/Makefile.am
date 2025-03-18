include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_book

mod_LTLIBRARIES = mod_book.la
mod_book_la_SOURCES  = mod_book.c
mod_book_la_CFLAGS   = $(AM_CFLAGS)
mod_book_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_book_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
