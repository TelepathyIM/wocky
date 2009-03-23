EXAMPLES =

EXAMPLES += wocky-connect
wocky_connect_SOURCES = connect.c

LDADD = \
    @GLIB_LIBS@ \
    $(top_builddir)/wocky/libwocky.la

AM_CFLAGS = \
    $(ERROR_CFLAGS) \
    @GLIB_CFLAGS@

noinst_PROGRAMS = $(EXAMPLES)
