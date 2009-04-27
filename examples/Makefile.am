EXAMPLES =

EXAMPLES += wocky-connect
wocky_connect_SOURCES = connect.c

LDADD = \
    @GLIB_LIBS@ \
    $(top_builddir)/wocky/libwocky.la

AM_CFLAGS = \
    $(ERROR_CFLAGS) \
    @GLIB_CFLAGS@

check_c_sources = $(wocky_connect_SOURCES)

include $(top_srcdir)/tools/check-coding-style.mk
check-local: check-coding-style

noinst_PROGRAMS = $(EXAMPLES)
