test_programs = wocky-xmpp-connection-test

wocky_xmpp_connection_test_SOURCES = \
   wocky-xmpp-connection-test.c

AM_CFLAGS = $(ERROR_CFLAGS) $(GCOV_CFLAGS) @GLIB_CFLAGS@ @LIBXML2_CFLAGS@
AM_LDFLAGS = $(GCOV_LIBS) @GLIB_LIBS@ @LIBXML2_LIBS@

check_PROGRAMS = $(test_programs)
