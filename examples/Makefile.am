EXAMPLES =

EXAMPLES += wocky-connect
EXAMPLES += wocky-register
EXAMPLES += wocky-unregister

INCLUDES := -I$(top_builddir)/wocky

wocky_connect_SOURCES = connect.c
wocky_connect_DEPENDENCIES = $(top_builddir)/wocky/libwocky.la

wocky_register_SOURCES = register.c
wocky_register_DEPENDENCIES = $(top_builddir)/wocky/libwocky.la

wocky_unregister_SOURCES = unregister.c
wocky_unregister_DEPENDENCIES = $(top_builddir)/wocky/libwocky.la


LDADD = \
    @GLIB_LIBS@ \
    $(top_builddir)/wocky/libwocky.la

AM_CFLAGS = \
    $(WOCKY_CFLAGS) \
    $(ERROR_CFLAGS) \
    @GLIB_CFLAGS@

check_c_sources = $(wocky_connect_SOURCES)    \
                  $(wocky_register_SOURCES)   \
                  $(wocky_unregister_SOURCES)

include $(top_srcdir)/tools/check-coding-style.mk
check-local: check-coding-style

noinst_PROGRAMS = $(EXAMPLES)
