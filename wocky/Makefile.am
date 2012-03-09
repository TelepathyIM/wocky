include $(top_srcdir)/tools/flymake.mk

if ENABLE_SHARED_SUFFIX
lib_LTLIBRARIES = libwocky.la

libwocky_la_LDFLAGS = \
  -no-undefined \
  -release $(SHARED_SUFFIX) \
  $(NULL)
else
noinst_LTLIBRARIES = libwocky.la
endif

EXTRA_DIST =

built_headers = \
  wocky-auth-registry-enumtypes.h \
  wocky-connector-enumtypes.h \
  wocky-data-form-enumtypes.h \
  wocky-muc-enumtypes.h \
  wocky-pubsub-node-enumtypes.h \
  wocky-pubsub-service-enumtypes.h \
  wocky-signals-marshal.h \
  wocky-tls-enumtypes.h \
  wocky-xmpp-error-enumtypes.h \
  wocky-xmpp-reader-enumtypes.h

built_sources = \
  wocky-auth-registry-enumtypes.c \
  wocky-connector-enumtypes.c \
  wocky-data-form-enumtypes.c \
  wocky-muc-enumtypes.c \
  wocky-pubsub-node-enumtypes.c \
  wocky-pubsub-service-enumtypes.c \
  wocky-signals-marshal.c \
  wocky-signals-marshal.list \
  wocky-tls-enumtypes.c \
  wocky-xmpp-error-enumtypes.c \
  wocky-xmpp-reader-enumtypes.c

BUILT_SOURCES = $(built_headers) $(built_sources)

OPENSSL_SRC = \
  wocky-openssl.c \
  wocky-openssl-dh512.c  \
  wocky-openssl-dh1024.c \
  wocky-openssl-dh2048.c \
  wocky-openssl-dh4096.c

GNUTLS_SRC = wocky-tls.c

PROXY_SRC = \
  wocky-http-proxy.h \
  wocky-http-proxy.c

handwritten_headers = \
  wocky.h \
  wocky-auth-handler.h \
  wocky-auth-registry.h \
  wocky-bare-contact.h \
  wocky-c2s-porter.h \
  wocky-caps-cache.h \
  wocky-ll-connection-factory.h \
  wocky-caps-hash.h \
  wocky-connector.h \
  wocky-contact.h \
  wocky-contact-factory.h \
  wocky-data-form.h \
  wocky-debug.h \
  wocky-disco-identity.h \
  wocky-jabber-auth.h \
  wocky-jabber-auth-digest.h \
  wocky-jabber-auth-password.h \
  wocky-ll-connector.h \
  wocky-ll-contact.h \
  wocky-loopback-stream.h \
  wocky-meta-porter.h \
  wocky-muc.h \
  wocky-namespaces.h \
  wocky-node.h \
  wocky-node-tree.h \
  wocky-pep-service.h \
  wocky-ping.h \
  wocky-porter.h \
  wocky-pubsub-helpers.h \
  wocky-pubsub-node.h \
  wocky-pubsub-node-protected.h \
  wocky-pubsub-service.h \
  wocky-pubsub-service-protected.h \
  wocky-resource-contact.h \
  wocky-roster.h \
  wocky-sasl-auth.h \
  wocky-sasl-utils.h \
  wocky-sasl-digest-md5.h \
  wocky-sasl-scram.h \
  wocky-sasl-plain.h \
  wocky-session.h \
  wocky-stanza.h \
  wocky-tls.h \
  wocky-tls-handler.h \
  wocky-tls-connector.h \
  wocky-types.h \
  wocky-utils.h \
  wocky-xep-0115-capabilities.h \
  wocky-xmpp-connection.h \
  wocky-xmpp-error.h \
  wocky-xmpp-reader.h \
  wocky-xmpp-writer.h

handwritten_sources = \
  wocky.c \
  wocky-auth-handler.c \
  wocky-auth-registry.c \
  wocky-bare-contact.c \
  wocky-c2s-porter.c \
  wocky-caps-cache.c \
  wocky-ll-connection-factory.c \
  wocky-caps-hash.c \
  wocky-connector.c \
  wocky-contact.c \
  wocky-contact-factory.c \
  wocky-data-form.c \
  wocky-debug.c \
  wocky-debug-internal.h \
  wocky-disco-identity.c \
  wocky-heartbeat-source.c \
  wocky-heartbeat-source.h \
  wocky-jabber-auth.c \
  wocky-jabber-auth-digest.c \
  wocky-jabber-auth-password.c \
  wocky-ll-connector.c \
  wocky-ll-contact.c \
  wocky-loopback-stream.c \
  wocky-meta-porter.c \
  wocky-muc.c \
  wocky-node.c \
  wocky-node-private.h \
  wocky-node-tree.c \
  wocky-pep-service.c \
  wocky-ping.c \
  wocky-porter.c \
  wocky-pubsub-helpers.c \
  wocky-pubsub-node.c \
  wocky-pubsub-node-internal.h \
  wocky-pubsub-service.c \
  wocky-resource-contact.c \
  wocky-roster.c \
  wocky-sasl-auth.c \
  wocky-sasl-digest-md5.c \
  wocky-sasl-scram.c \
  wocky-sasl-utils.c \
  wocky-sasl-plain.c \
  wocky-session.c \
  wocky-stanza.c \
  wocky-utils.c \
  wocky-tls-common.c \
  wocky-tls-handler.c \
  wocky-tls-connector.c \
  wocky-xep-0115-capabilities.c \
  wocky-xmpp-connection.c \
  wocky-xmpp-error.c \
  wocky-xmpp-reader.c \
  wocky-xmpp-writer.c

if USING_OPENSSL
  handwritten_sources += $(OPENSSL_SRC)
  EXTRA_DIST += $(GNUTLS_SRC)
else
  handwritten_sources += $(GNUTLS_SRC)
  EXTRA_DIST += $(OPENSSL_SRC)
endif

if HAVE_GIO_PROXY
  handwritten_sources += $(PROXY_SRC)
else
  EXTRA_DIST += $(PROXY_SRC)
endif

libwocky_la_SOURCES = $(handwritten_sources) $(built_sources) \
                      $(handwritten_headers) $(built_headers)

if INSTALL_HEADERS
  libwocky_la_HEADERS = $(handwritten_headers) $(built_headers)
  libwocky_ladir = $(HEADER_DIR)/wocky
endif

# Coding style checks
check_c_sources = \
  $(filter-out wocky-openssl%, \
    $(filter-out wocky-tls.%, \
      $(handwritten_sources) $(handwritten_headers) \
     ) \
   )

include $(top_srcdir)/tools/check-coding-style.mk
check-local: check-coding-style

CLEANFILES=$(built_sources) $(built_headers)
dist-hook:
	$(shell for x in $(CLEANFILES); do rm -f $(distdir)/$$x ; done)

wocky-signals-marshal.list: $(handwritten_sources) Makefile.am
	$(AM_V_GEN)( cd $(srcdir) && \
	sed -n -e 's/.*_wocky_signals_marshal_\([[:upper:][:digit:]]*__[[:upper:][:digit:]_]*\).*/\1/p' \
	$(handwritten_sources) ) \
	| sed -e 's/__/:/' -e 'y/_/,/' | sort -u > $@.tmp
	@if cmp -s $@.tmp $@; then \
		rm $@.tmp; \
	else \
		mv $@.tmp $@; \
	fi

%-signals-marshal.h: %-signals-marshal.list Makefile.am
	$(AM_V_GEN)glib-genmarshal --header --prefix=_$(subst -,_,$*)_signals_marshal $< > $@

%-signals-marshal.c: %-signals-marshal.list Makefile.am
	$(AM_V_GEN){ echo '#include "$*-signals-marshal.h"' && \
	glib-genmarshal --body --prefix=_$(subst -,_,$*)_signals_marshal $< ; \
	} > $@


AM_CFLAGS = $(ERROR_CFLAGS) $(GCOV_CFLAGS) \
            @GLIB_CFLAGS@ @LIBXML2_CFLAGS@ @SQLITE_CFLAGS@ @TLS_CFLAGS@ \
            @LIBIPHB_CFLAGS@ \
            -DG_LOG_DOMAIN=\"wocky\" \
            -DWOCKY_COMPILATION


libwocky_la_LIBADD = \
  $(GCOV_LIBS) \
  @GLIB_LIBS@ \
  @LIBXML2_LIBS@ \
  @SQLITE_LIBS@ \
  @TLS_LIBS@ \
  @LIBIPHB_LIBS@ \
  $(NULL)

# rules for making the glib enum objects
# The shell variable $long is something like WOCKY_XMPP_ERROR for
# wocky-xmpp-error-enumtypes.h, and $short is WOCKY.
$(filter %-enumtypes.h,$(built_headers)): %-enumtypes.h: %.h Makefile.in
	$(AM_V_GEN)set -e; \
	long=`echo $(notdir $*) | tr [:lower:]- [:upper:]_`; \
	short=$${long%%_*}; \
	guard="__$${long}_ENUM_TYPES_H__"; \
	glib-mkenums \
	--fhead "#ifndef $${guard}\n#define $${guard}\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
	--fprod "/* enumerations from \"@filename@\" */\n" \
	--vhead "GType @enum_name@_get_type (void);\n#define $${short}_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n"         \
	--ftail "G_END_DECLS\n\n#endif" \
	$< > $@

$(filter %-enumtypes.c,$(built_sources)): %-enumtypes.c: %.h Makefile.in
	$(AM_V_GEN)glib-mkenums \
	--fhead "#include <$*.h>\n#include <$*-enumtypes.h>" \
	--fprod "\n/* enumerations from \"@filename@\" */" \
	--vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {"     \
	--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
	--vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n  return etype;\n}\n" \
	$< > $@

Android.mk: Makefile.am $(built_sources) $(built_headers)
	androgenizer -:PROJECT wocky -:SHARED wocky -:TAGS eng debug \
	 -:REL_TOP $(top_srcdir) -:ABS_TOP $(abs_top_srcdir) \
	 -:SOURCES $(handwritten_sources) $(built_sources) \
	 -:CFLAGS $(DEFS) $(CFLAGS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CFLAGS) \
	 -:CPPFLAGS $(CPPFLAGS) $(AM_CPPFLAGS) \
	 -:LDFLAGS $(AM_LDFLAGS) $(libwocky_la_LIBADD) \
	> $@
