############################################################################
# x509 certificates:
CERTTOOL := $(shell which certtool)
TARDIS   := $(shell which datefudge)
GENKEY   := $(CERTTOOL) --generate-privkey --outfile
TEST_DIR := @abs_top_srcdir@/tests
CERT_DIR := $(TEST_DIR)/certs
CA_CERT2 := $(CERT_DIR)/ca-1-cert.pem
CA_CERT3 := $(CERT_DIR)/ca-2-cert.pem
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
CAS      := $(CA_CERT) $(CA_CERT2) $(CA_CERT3)
CRL      := $(CERT_DIR)/ca-0-crl.pem
UNKNOWN_KEY  := $(CERT_DIR)/unknown-key.pem
UNKNOWN_CERT := $(CERT_DIR)/unknown-cert.pem
CERTS    := $(TLS_CERT) $(NEW_CERT) $(EXP_CERT) $(SS_CERT) $(REV_CERT) \
            $(UNKNOWN_CERT)
LIBWOCKY := $(top_builddir)/wocky/libwocky.la
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
DFCHECK  := $(if $(shell which datefudge),                 \
                 $(info  datefudge installed, proceeding), \
                 $(error datefudge NOT IN PATH - please install datefudge))
DUMMY    := $(if $(shell which certtool),                  \
                 $(info  certtool  installed, proceeding), \
                 $(error certtool NOT IN PATH - please install gnutls-bin))
FUTURE   := $(shell date +'%Y-%m-%d %H:%M' -d @$$(($$(date +%s) + 86400)))
PAST     := $(shell date +'%Y-%m-%d %H:%M' -d @$$(($$(date +%s) - 86400 * 7)))
FILTER_BITS := grep -v '^[	 ]\+\([a-f0-9]\{2\}:\)\+[a-f0-9]\{2\}$$'
############################################################################
TEST_PROGS = wocky-xmpp-reader-test \
  wocky-xmpp-readwrite-test \
  wocky-xmpp-connection-test \
  wocky-porter-test \
  wocky-xmpp-node-test \
  wocky-xmpp-stanza-test \
  wocky-roster-test \
  wocky-bare-contact-test \
  wocky-resource-contact-test \
  wocky-contact-factory-test \
  wocky-connector-test \
  wocky-session-test

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

wocky_connector_test_DEPENDENCIES = $(CRL_DIR) $(CA_DIR) $(CERTS) $(LIBWOCKY)
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

wocky_test_sasl_auth_DEPENDENCIES = $(LIBWOCKY)
wocky_test_sasl_auth_SOURCES = \
   wocky-test-helper.c wocky-test-helper.h \
   wocky-test-sasl-auth.c \
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

wocky_xmpp_node_test_DEPENDENCIES = $(LIBWOCKY)
wocky_xmpp_node_test_SOURCES = \
  wocky-test-helper.c wocky-test-helper.h \
  wocky-test-stream.c wocky-test-stream.h \
  wocky-xmpp-node-test.c

wocky_xmpp_stanza_test_DEPENDENCIES = $(LIBWOCKY)
wocky_xmpp_stanza_test_SOURCES = \
  wocky-test-helper.c wocky-test-helper.h \
  wocky-test-stream.c wocky-test-stream.h \
  wocky-xmpp-stanza-test.c

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

AM_CFLAGS = $(ERROR_CFLAGS) $(GCOV_CFLAGS) @GLIB_CFLAGS@ @LIBXML2_CFLAGS@ @WOCKY_CFLAGS@
AM_LDFLAGS = $(GCOV_LIBS) @GLIB_LIBS@ @LIBXML2_LIBS@
LDADD = $(top_builddir)/wocky/libwocky.la

check_PROGRAMS = $(TEST_PROGS)

check_c_sources = \
  $(wocky_connector_test_SOURCES) \
  $(wocky_test_sasl_auth_SOURCES) \
  $(wocky_xmpp_connection_test_SOURCES) \
  $(wocky_xmpp_readwrite_test_SOURCES) \
  $(wocky_xmpp_reader_test_SOURCES)

.PRECIOUS: %-key.pem $(CA_CERT)

all: $(TEST_PROGS)

certs: $(CERTS)

test-report: test-report.xml
	gtester-report $(top_builddir)/tests/$@.xml > \
	  $(top_builddir)/tests/$@.html

test-report.xml: ${TEST_PROGS} test

test: ${TEST_PROGS}
	gtester -o test-report.xml -k --verbose ${TEST_PROGS}

test-%: wocky-%-test
	gtester -o $@-report.xml -k --verbose $<

.PHONY: test test-report

include $(top_srcdir)/tools/check-coding-style.mk
check-local: test check-coding-style

############################################################################
# x509 certificates:
%-key.pem:
	$(GENKEY) $@

%/ss-cert.pem: %/ss-key.pem %/ss-cert.cfg
	$(CERTTOOL) --generate-self-signed            \
	            --load-privkey $<                 \
	            --template     $(basename $@).cfg \
	            --outfile      $@ 2>&1 | $(FILTER_BITS)

%/unknown-ca-cert.pem: %/unknown-ca-key.pem %/unknown-ca-cert.cfg
	$(CERTTOOL) --generate-self-signed            \
	            --load-privkey $<                 \
	            --template     $(basename $@).cfg \
	            --outfile      $@ 2>&1 | $(FILTER_BITS)

%/unknown-cert.pem: %/unknown-key.pem %/unknown-cert.cfg %/unknown-ca-cert.pem
	$(CERTTOOL) --generate-certificate                           \
                --load-ca-certificate $*/unknown-ca-cert.pem     \
	            --load-ca-privkey     $*/unknown-ca-key.pem      \
	            --load-privkey        $<                         \
	            --template            $*/unknown-cert.cfg        \
	            --outfile             $@ 2>&1 | $(FILTER_BITS)

$(CERT_DIR)/ca-%-cert.pem: $(CERT_DIR)/ca-%-key.pem $(CERT_DIR)/ca-%-cert.cfg
	$(CERTTOOL) --generate-self-signed            \
	            --load-privkey $<                 \
	            --template     $(basename $@).cfg \
	            --outfile      $@ 2>&1 | $(FILTER_BITS)

%/rev-cert.pem: export CERTCMD = $(CERTTOOL)
%/tls-cert.pem: export CERTCMD = $(CERTTOOL)
%/new-cert.pem: export CERTCMD = $(TARDIS) "$(FUTURE)" $(CERTTOOL)
%/exp-cert.pem: export CERTCMD = $(TARDIS) "$(PAST)"   $(CERTTOOL)

%-cert.pem: %-key.pem %-cert.cfg $(CA_CERT)
	$(CERTCMD)  --generate-certificate                   \
                --load-ca-certificate $(CA_CERT)         \
	            --load-ca-privkey     $(CA_KEY)          \
	            --load-privkey        $<                 \
	            --template            $*-cert.cfg        \
	            --outfile             $@ 2>&1 | $(FILTER_BITS)

$(CA_DIR): $(CAS)
	@mkdir -p $@ && cp -av $(CAS) $(CA_DIR)

$(CRL): $(REV_CERT) $(CA_CERT) $(CA_KEY)
	$(CERTTOOL) --generate-crl        \
	            --template $(basename $@).cfg     \
	            --load-ca-privkey     $(CA_KEY)   \
	            --load-ca-certificate $(CA_CERT)  \
	            --load-certificate    $(REV_CERT) \
                --outfile             $@ 2>&1 | $(FILTER_BITS)

$(CRL_DIR): $(CRL)
	@( mkdir -p $@ &&                                \
	   if cmp $(CRL) $(CRL_DIR)/$(notdir $(CRL));    \
	   then                                          \
	       echo -n;                                  \
	   else                                          \
	       ln -sfv $(CRL) $(CRL_DIR);                \
	   fi );

clean-local:
	rm -fv certs/*.crl
	rm -fv certs/*.pem
	rm -fv certs/cas/*.pem
	rm -fv certs/crl/*.pem

############################################################################

SUPPRESSIONS=threadlocal.supp

CLEANFILES = test-report.xml \
	sasl-test.db

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

EXTRA_DIST = $(SUPPRESSIONS)     \
    $(wildcard certs/*.cfg)      \
	$(wildcard certs/*.pem)      \
	$(wildcard certs/cas/*.pem)  \
	certs/crl                    \
	$(wildcard certs/crl/*.pem)

