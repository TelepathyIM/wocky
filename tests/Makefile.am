TEST_PROGS = wocky-xmpp-reader-test \
  wocky-xmpp-readwrite-test \
  wocky-xmpp-connection-test

wocky_xmpp_connection_test_SOURCES = \
  wocky-xmpp-connection-test.c \
  wocky-test-stream.c wocky-test-stream.h

wocky_xmpp_readwrite_test_SOURCES = \
  wocky-xmpp-readwrite-test.c

wocky_xmpp_reader_test_SOURCES = \
  wocky-xmpp-reader-test.c

AM_CFLAGS = $(ERROR_CFLAGS) $(GCOV_CFLAGS) @GLIB_CFLAGS@ @LIBXML2_CFLAGS@
AM_LDFLAGS = $(GCOV_LIBS) @GLIB_LIBS@ @LIBXML2_LIBS@
LDADD = $(top_builddir)/wocky/libwocky.la

check_PROGRAMS = $(TEST_PROGS)

test-report: test-report.xml
	gtester-report $(top_builddir)/tests/$@.xml > \
	  $(top_builddir)/tests/$@.html

test-report.xml: ${TEST_PROGS} test

test: ${TEST_PROGS}
	gtester -o test-report.xml -k --verbose ${TEST_PROGS}

.PHONE: test test-report

check-local: test
