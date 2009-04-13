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

if HAVE_LIBSASL2
  TEST_PROGS += wocky-test-sasl-auth

  wocky_test_sasl_auth_SOURCES = \
      wocky-test-sasl-auth.c \
      wocky-test-sasl-auth-server.c \
      wocky-test-sasl-auth-server.h \
      wocky-test-stream.c \
      wocky-test-stream.h

  wocky_test_sasl_auth_LDADD = $(LDADD) @LIBSASL2_LIBS@
  wocky_test_sasl_auth_CFLAGS = $(AM_CFLAGS) @LIBSASL2_CFLAGS@
endif



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

.PHONY: test test-report

check-local: test

SUPPRESSIONS=threadlocal.supp

# valgrind any given test by running make test.valgrind
%.valgrind: %
	G_SLICE=always-malloc               \
	G_DEBUG=gc-friendly              \
	valgrind -q \
	$(foreach s,$(SUPPRESSIONS),--suppressions=$(s))   \
	--tool=memcheck --leak-check=full --trace-children=yes   \
	--leak-resolution=high --num-callers=20         \
	./$* 2>&1 | tee "valgrind.$*.log"
	@if grep "==" "valgrind.$*.log" > /dev/null 2>&1; then   \
		exit 1;                \
	fi

valgrind: $(TEST_PROGS)
	@echo "Valgrinding tests ..."
	@failed=0;                    \
	for t in $(filter-out $(VALGRIND_TESTS_DISABLE),$(TEST_PROGS)); do  \
		$(MAKE) $$t.valgrind;               \
		if test "$$?" -ne 0; then                               \
			echo "Valgrind error for test $$t";    \
			failed=`expr $$failed + 1`;         \
			whicht="$$whicht $$t";           \
		fi;                    \
	done;                      \
	if test "$$failed" -ne 0; then               \
		echo "$$failed tests had leaks or errors under valgrind:";  \
		echo "$$whicht";              \
		false;                     \
	fi


EXTRA_DIST = $(SURPRESSIONS)
