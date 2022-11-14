# 默认
include $(top_srcdir)/build/modmake.rulesam
# 模块名称，对应模块的文件夹名称
MODNAME=mod_event_redis
# 模块编译后lib文件名称
mod_LTLIBRARIES = mod_event_redis.la
# 模块入口资源文件
mod_event_redis_la_SOURCES  = mod_event_redis.c
# 默认
mod_event_redis_la_CFLAGS   = $(AM_CFLAGS)
mod_event_redis_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_event_redis_la_LDFLAGS  = -avoid-version -module -no-undefined -shared