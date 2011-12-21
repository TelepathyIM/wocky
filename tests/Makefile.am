############################################################################
# x509 certificates:
TEST_DIR := @abs_top_srcdir@/tests
SUMMARY  := $(TEST_DIR)/summarise-tests.py
CERT_DIR := $(TEST_DIR)/certs
CA_KEY   := $(CERT_DIR)/ca-0-key.pem
CA_CERT  := $(CERT_DIR)/ca-0-cert.pem
SS_KEY   := $(CERT_DIR)/ss-key.pem
SS_CERT  := $(CERT_DIR)/ss-cert.pem
REV_KEY  := $(CERT_DIR)/rev-key.pem
REV_CERT := $(CERT_DIR)/rev-cert.pem
EXP_KEY  := $(CERT_DIR)/exp-key.pem
EXP_CERT := $(CERT_DIR)/exp-cert.pem
NEW_KEY  := $(CERT_DIR)/new-key.pem
NEW_CERT := $(CERT_DIR)/new-cert.pem
TLS_KEY  := $(CERT_DIR)/tls-key.pem
TLS_CERT := $(CERT_DIR)/tls-cert.pem
CA_DIR   := $(CERT_DIR)/cas
CRL_DIR  := $(CERT_DIR)/crl
UNKNOWN_KEY  := $(CERT_DIR)/unknown-key.pem
UNKNOWN_CERT := $(CERT_DIR)/unknown-cert.pem
LIBWOCKY := $(top_builddir)/wocky/libwocky.la
INCLUDES := -I$(top_builddir)/wocky
TLSDEFS  := -DTLS_CA_KEY_FILE='"$(CA_KEY)"'      \
            -DTLS_CA_CRT_FILE='"$(CA_CERT)"'     \
            -DTLS_SS_KEY_FILE='"$(SS_KEY)"'      \
            -DTLS_SS_CRT_FILE='"$(SS_CERT)"'     \
            -DTLS_EXP_KEY_FILE='"$(EXP_KEY)"'    \
            -DTLS_EXP_CRT_FILE='"$(EXP_CERT)"'   \
            -DTLS_NEW_KEY_FILE='"$(NEW_KEY)"'    \
            -DTLS_NEW_CRT_FILE='"$(NEW_CERT)"'   \
            -DTLS_REV_KEY_FILE='"$(REV_KEY)"'    \
            -DTLS_REV_CRT_FILE='"$(REV_CERT)"'   \
            -DTLS_UNKNOWN_KEY_FILE='"$(UNKNOWN_KEY)"'  \
            -DTLS_UNKNOWN_CRT_FILE='"$(UNKNOWN_CERT)"' \
            -DTLS_SERVER_KEY_FILE='"$(TLS_KEY)"' \
            -DTLS_SERVER_CRT_FILE='"$(TLS_CERT)"'\
            -DTLS_CRL_DIR='"$(CRL_DIR)"'         \
            -DTLS_CA_DIR='"$(CA_DIR)"'
############################################################################
TEST_PROGS = wocky-xmpp-reader-test \
  wocky-xmpp-readwrite-test \
  wocky-xmpp-connection-test \
  wocky-porter-test \
  wocky-loopback-test \
  wocky-xmpp-node-test \
  wocky-node-tree-test \
  wocky-stanza-test \
  wocky-roster-test \
  wocky-bare-contact-test \
  wocky-resource-contact-test \
  wocky-contact-factory-test \
  wocky-session-test \
  wocky-pep-service-test \
  wocky-tls-test \
  wocky-data-form-test \
  wocky-pubsub-service-test \
  wocky-pubsub-node-test \
  wocky-connector-test \
  wocky-ping-test \
  wocky-jid-validation-test \
  wocky-sasl-utils-test \
  wocky-utils-test \
  wocky-scram-sha1-test \
  wocky-caps-hash-test

noinst_PROGRAMS=

wocky_tls_test_DEPENDENCIES = $(LIBWOCKY)
wocky_tls_test_SOURCES = \
  wocky-tls-test.c        \
  wocky-test-helper.c wocky-test-helper.h \
  wocky-test-stream.c wocky-test-stream.h
wocky_tls_test_CFLAGS = $(AM_CFLAGS) $(TLSDEFS)

wocky_xmpp_connection_test_DEPENDENCIES = $(LIBWOCKY)
wocky_xmpp_connection_test_SOURCES = \
  wocky-xmpp-connection-test.c \
  wocky-test-helper.c wocky-test-helper.h \
  wocky-test-stream.c wocky-test-stream.h

wocky_xmpp_readwrite_test_DEPENDENCIES = $(LIBWOCKY)
wocky_xmpp_readwrite_test_SOURCES = \
  wocky-test-stream.c wocky-test-stream.h \
  wocky-test-helper.c wocky-test-helper.h \
  wocky-xmpp-readwrite-test.c

wocky_xmpp_reader_test_DEPENDENCIES = $(LIBWOCKY)
wocky_xmpp_reader_test_SOURCES = \
  wocky-test-helper.c wocky-test-helper.h \
  wocky-test-stream.c wocky-test-stream.h \
  wocky-xmpp-reader-test.c

wocky_connector_test_DEPENDENCIES = $(CRL_DIR) $(CA_DIR) certs $(LIBWOCKY)

wocky_connector_test_SOURCES = \
   wocky-connector-test.c        \
   wocky-test-sasl-auth-server.c \
   wocky-test-sasl-auth-server.h \
   wocky-test-connector-server.c \
   wocky-test-connector-server.h \
   wocky-test-helper.c wocky-test-helper.h \
  wocky-test-stream.c wocky-test-stream.h \
   test-resolver.h        	 \
   test-resolver.c
wocky_connector_test_LDADD = $(LDADD) @LIBSASL2_LIBS@
wocky_connector_test_CFLAGS = $(AM_CFLAGS) @LIBSASL2_CFLAGS@ $(TLSDEFS)

if HAVE_LIBSASL2
  TEST_PROGS += wocky-test-sasl-auth
endif

wocky_sasl_utils_test_DEPENDENCIES = $(LIBWOCKY)
wocky_sasl_utils_test_SOURCES = wocky-sasl-utils-test.c

wocky_test_sasl_auth_DEPENDENCIES = $(LIBWOCKY)
wocky_test_sasl_auth_SOURCES = \
   wocky-test-helper.c wocky-test-helper.h \
   wocky-test-sasl-auth.c \
   wocky-test-sasl-handler.c \
   wocky-test-sasl-handler.h \
   wocky-test-sasl-auth-server.c \
   wocky-test-sasl-auth-server.h \
   wocky-test-stream.c \
   wocky-test-stream.h

wocky_test_sasl_auth_LDADD = $(LDADD) @LIBSASL2_LIBS@
wocky_test_sasl_auth_CFLAGS = $(AM_CFLAGS) @LIBSASL2_CFLAGS@

wocky_porter_test_DEPENDENCIES = $(LIBWOCKY)
wocky_porter_test_SOURCES = \
  wocky-porter-test.c \
  wocky-test-helper.c wocky-test-helper.h \
  wocky-test-stream.c wocky-test-stream.h

wocky_loopback_test_DEPENDENCIES = $(LIBWOCKY)
wocky_loopback_test_SOURCES = \
  wocky-test-helper.c wocky-test-helper.h \
  wocky-test-stream.c wocky-test-stream.h \
  wocky-loopback-test.c

wocky_xmpp_node_test_DEPENDENCIES = $(LIBWOCKY)
wocky_xmpp_node_test_SOURCES = \
  wocky-test-helper.c wocky-test-helper.h \
  wocky-test-stream.c wocky-test-stream.h \
  wocky-xmpp-node-test.c

wocky_node_tree_test_DEPENDENCIES = $(LIBWOCKY)
wocky_node_tree_test_SOURCES = \
  wocky-test-helper.c wocky-test-helper.h \
  wocky-test-stream.c wocky-test-stream.h \
  wocky-node-tree-test.c

wocky_stanza_test_DEPENDENCIES = $(LIBWOCKY)
wocky_stanza_test_SOURCES = \
  wocky-test-helper.c wocky-test-helper.h \
  wocky-test-stream.c wocky-test-stream.h \
  wocky-stanza-test.c

wocky_roster_test_SOURCES = \
  wocky-roster-test.c \
  wocky-test-helper.c wocky-test-helper.h \
  wocky-test-stream.c wocky-test-stream.h

wocky_bare_contact_test_DEPENDENCIES = $(LIBWOCKY)
wocky_bare_contact_test_SOURCES = \
  wocky-test-helper.c wocky-test-helper.h \
  wocky-test-stream.c wocky-test-stream.h \
  wocky-bare-contact-test.c

wocky_resource_contact_test_DEPENDENCIES = $(LIBWOCKY)
wocky_resource_contact_test_SOURCES = \
  wocky-test-helper.c wocky-test-helper.h \
  wocky-test-stream.c wocky-test-stream.h \
  wocky-resource-contact-test.c

wocky_contact_factory_test_DEPENDENCIES = $(LIBWOCKY)
wocky_contact_factory_test_SOURCES = \
  wocky-test-helper.c wocky-test-helper.h \
  wocky-test-stream.c wocky-test-stream.h \
  wocky-contact-factory-test.c

wocky_session_test_DEPENDENCIES = $(LIBWOCKY)
wocky_session_test_SOURCES = \
  wocky-test-helper.c wocky-test-helper.h \
  wocky-test-stream.c wocky-test-stream.h \
  wocky-session-test.c

wocky_pep_service_test_DEPENDENCIES = $(LIBWOCKY)
wocky_pep_service_test_SOURCES = \
  wocky-test-helper.c wocky-test-helper.h \
  wocky-test-stream.c wocky-test-stream.h \
  wocky-pep-service-test.c

wocky_data_form_test_DEPENDENCIES = $(LIBWOCKY)
wocky_data_form_test_SOURCES = \
  wocky-test-helper.c wocky-test-helper.h \
  wocky-test-stream.c wocky-test-stream.h \
  wocky-data-form-test.c

wocky_pubsub_service_test_DEPENDENCIES = $(LIBWOCKY)
wocky_pubsub_service_test_SOURCES = \
  wocky-test-helper.c wocky-test-helper.h \
  wocky-test-stream.c wocky-test-stream.h \
  wocky-pubsub-test-helpers.c wocky-pubsub-test-helpers.h \
  wocky-pubsub-service-test.c

wocky_pubsub_node_test_DEPENDENCIES = $(LIBWOCKY)
wocky_pubsub_node_test_SOURCES = \
  wocky-test-helper.c wocky-test-helper.h \
  wocky-test-stream.c wocky-test-stream.h \
  wocky-pubsub-test-helpers.c wocky-pubsub-test-helpers.h \
  wocky-pubsub-node-test.c

wocky_ping_test_DEPENDENCIES = $(LIBWOCKY)
wocky_ping_test_SOURCES = \
  wocky-ping-test.c \
  wocky-test-helper.c wocky-test-helper.h \
  wocky-test-stream.c wocky-test-stream.h

wocky_jid_validation_test_DEPENDENCIES = $(LIBWOCKY)
wocky_jid_validation_test_SOURCES = \
  wocky-jid-validation-test.c \
  wocky-test-helper.c wocky-test-helper.h \
  wocky-test-stream.c wocky-test-stream.h

wocky_utils_test_DEPENDENCIES = $(LIBWOCKY)
wocky_utils_test_SOURCES = wocky-utils-test.c

wocky_scram_sha1_test_DEPENDENCIES = $(LIBWOCKY)
wocky_scram_sha1_test_SOURCES = wocky-scram-sha1-test.c \
  wocky-test-helper.c wocky-test-helper.h \
  wocky-test-stream.c wocky-test-stream.h

wocky_http_proxy_test_DEPENDENCIES = $(LIBWOCKY)
wocky_http_proxy_test_SOURCES = wocky-http-proxy-test.c \
  wocky-test-helper.c wocky-test-helper.h \
  wocky-test-stream.c wocky-test-stream.h

wocky_dummy_xmpp_server_DEPENDENCIES = $(LIBWOCKY)
wocky_dummy_xmpp_server_SOURCES = wocky-dummy-xmpp-server.c \
   wocky-test-connector-server.c wocky-test-connector-server.h \
   wocky-test-sasl-auth-server.c wocky-test-sasl-auth-server.h
wocky_dummy_xmpp_server_LDADD = $(LDADD) @LIBSASL2_LIBS@
wocky_dummy_xmpp_server_CFLAGS = $(AM_CFLAGS) @LIBSASL2_CFLAGS@ $(TLSDEFS)

if HAVE_LIBSASL2
  noinst_PROGRAMS += wocky-dummy-xmpp-server
endif

wocky_caps_hash_test_DEPENDENCIES = $(LIBWOCKY)
wocky_caps_hash_test_SOURCES = wocky-caps-hash-test.c \
  wocky-test-helper.c wocky-test-helper.h \
  wocky-test-stream.c wocky-test-stream.h

if HAVE_GIO_PROXY
  TEST_PROGS += wocky-http-proxy-test
endif

AM_CFLAGS = $(ERROR_CFLAGS) $(GCOV_CFLAGS) @GLIB_CFLAGS@ \
  @LIBXML2_CFLAGS@ @TLS_CFLAGS@ @WOCKY_CFLAGS@

AM_LDFLAGS = $(GCOV_LIBS) @GLIB_LIBS@ @LIBXML2_LIBS@ @TLS_LIBS@
LDADD = $(top_builddir)/wocky/libwocky.la

check_PROGRAMS = $(TEST_PROGS)

check_c_sources = $(notdir $(wildcard $(srcdir)/*.c) $(wildcard $(srcdir)/*.h))

noinst_PROGRAMS = $(TEST_PROGS)

test-report: test-report.xml
	gtester-report $(top_builddir)/tests/$@.xml > \
	  $(top_builddir)/tests/$@.html

test-report.xml: ${TEST_PROGS} test

test: ${TEST_PROGS}
	gtester -o test-report.xml -k --verbose ${TEST_PROGS}
	@if [ -x $(which python) ] ; then \
		$(SUMMARY) $@-report.xml ; \
	else \
		echo "No python available, not summarizing test results" ; \
	fi

test-%: wocky-%-test
	gtester -o $@-report.xml -k --verbose $<
	@if [ -x $(which python) ] ; then \
		$(SUMMARY) $@-report.xml ; \
	else \
		echo "No python available, not summarizing test results" ; \
	fi

.PHONY: test test-report

include $(top_srcdir)/tools/check-coding-style.mk
check-local: test check-coding-style

############################################################################

SUPPRESSIONS = \
	threadlocal.supp \
	gabble.supp \
	tp-glib.supp

CLEANFILES = test-report.xml \
	sasl-test.db

# valgrind any given test by running make test.valgrind
%.valgrind: %
	G_SLICE=always-malloc               \
	G_DEBUG=gc-friendly              \
	valgrind -q \
	$(foreach s,$(SUPPRESSIONS),--suppressions=$(srcdir)/$(s))   \
	--tool=memcheck --leak-check=full --trace-children=yes   \
	--leak-resolution=high --num-callers=20         \
	./$* $(TEST_ARGS) 2>&1 | tee "valgrind.$*.log"
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

EXTRA_DIST = $(SUPPRESSIONS)     \
	README \
	connector-test-plan.txt \
	summarise-tests.py \
	wocky-dummy-xmpp-server.c \
	$(wildcard $(srcdir)/certs/*.cfg)      \
	$(wildcard $(srcdir)/certs/*.pem)      \
	$(wildcard $(srcdir)/certs/cas/*.pem)  \
	$(wildcard $(srcdir)/certs/cas/*.0)    \
	$(wildcard $(srcdir)/certs/crl/*.pem)  \
	$(wildcard $(srcdir)/certs/crl/*.r0)

