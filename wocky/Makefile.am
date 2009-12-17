include $(top_srcdir)/tools/flymake.mk

noinst_LTLIBRARIES = libwocky.la

EXTRA_DIST =

BUILT_SOURCES = \
  wocky-signals-marshal.list \
  wocky-signals-marshal.h \
  wocky-signals-marshal.c \
  wocky-xmpp-reader-enumtypes.h \
  wocky-xmpp-reader-enumtypes.c

OPENSSL_SRC = \
  wocky-openssl.c        \
  wocky-tls.h            \
  wocky-openssl-dh512.c  \
  wocky-openssl-dh1024.c \
  wocky-openssl-dh2048.c \
  wocky-openssl-dh4096.c

GNUTLS_SRC = wocky-tls.c wocky-tls.h

HANDWRITTEN_SOURCES =            \
  wocky-types.h                  \
  wocky.c                        \
  wocky.h                        \
  wocky-connector.c              \
  wocky-connector.h              \
  wocky-xmpp-connection.c        \
  wocky-xmpp-connection.h        \
  wocky-debug.c                  \
  wocky-debug.h                  \
  wocky-namespaces.h             \
  wocky-utils.c                  \
  wocky-utils.h                  \
  wocky-xmpp-node.c              \
  wocky-xmpp-node.h              \
  wocky-xmpp-reader.c            \
  wocky-xmpp-reader.h            \
  wocky-xmpp-stanza.c            \
  wocky-xmpp-stanza.h            \
  wocky-xmpp-writer.c            \
  wocky-xmpp-writer.h            \
  wocky-xmpp-error.c             \
  wocky-xmpp-error.h             \
  wocky-sasl-auth.c              \
  wocky-sasl-auth.h              \
  wocky-sasl-digest-md5.h        \
  wocky-sasl-digest-md5.c        \
  wocky-sasl-handler.c           \
  wocky-sasl-handler.h           \
  wocky-sasl-plain.c             \
  wocky-sasl-plain.h             \
  wocky-porter.c                 \
  wocky-porter.h                 \
  wocky-contact.c                \
  wocky-contact.h                \
  wocky-bare-contact.c           \
  wocky-bare-contact.h           \
  wocky-resource-contact.c       \
  wocky-resource-contact.h       \
  wocky-roster.c                 \
  wocky-roster.h                 \
  wocky-contact-factory.c        \
  wocky-contact-factory.h        \
  wocky-session.c                \
  wocky-session.h                \
  wocky-pep-service.c            \
  wocky-pep-service.h

if USING_OPENSSL
  HANDWRITTEN_SOURCES += $(OPENSSL_SRC)
  EXTRA_DIST += $(GNUTLS_SRC)
else
  HANDWRITTEN_SOURCES += $(GNUTLS_SRC)
  EXTRA_DIST += $(OPENSSL_SRC)
endif

libwocky_la_SOURCES = $(HANDWRITTEN_SOURCES) $(BUILT_SOURCES)

# Coding style checks
check_c_sources = \
  $(filter-out wocky-openssl%,$(filter-out wocky-tls.%, $(HANDWRITTEN_SOURCES)))

include $(top_srcdir)/tools/check-coding-style.mk
check-local: check-coding-style

CLEANFILES=$(BUILT_SOURCES)
dist-hook:
	$(shell for x in $(BUILT_SOURCES); do rm -f $(distdir)/$$x ; done)

wocky-signals-marshal.list: $(HANDWRITTEN_SOURCES) Makefile.am
	$(AM_V_GEN)( cd $(srcdir) && \
	sed -n -e 's/.*_wocky_signals_marshal_\([[:upper:][:digit:]]*__[[:upper:][:digit:]_]*\).*/\1/p' \
	$(HANDWRITTEN_SOURCES) ) \
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
            @GLIB_CFLAGS@ @LIBXML2_CFLAGS@ @TLS_CFLAGS@ \
            -DG_LOG_DOMAIN=\"wocky\"

AM_LDFLAGS = $(GCOV_LIBS) @GLIB_LIBS@ @LIBXML2_LIBS@ @TLS_LIBS@

# rules for making the glib enum objects
%-enumtypes.h: %.h Makefile.in
	$(AM_V_GEN)glib-mkenums \
	--fhead "#ifndef __$(shell echo $* | tr [:lower:]- [:upper:]_)_ENUM_TYPES_H__\n#define __$(shell echo $* | tr [:lower:]- [:upper:]_)_ENUM_TYPES_H__\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
	--fprod "/* enumerations from \"@filename@\" */\n" \
	--vhead "GType @enum_name@_get_type (void);\n#define $(shell echo $* | tr [:lower:]- [:upper:]_ | sed 's/_.*//')_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n"         \
	--ftail "G_END_DECLS\n\n#endif /* __$(shell echo $* | tr [:lower:]- [:upper:]_)_ENUM_TYPES_H__ */" \
	$< > $@

%-enumtypes.c: %.h Makefile.in
	$(AM_V_GEN)glib-mkenums \
	--fhead "#include <$*.h>\n#include <$*-enumtypes.h>" \
	--fprod "\n/* enumerations from \"@filename@\" */" \
	--vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {"     \
	--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@VALUENAME@\" }," \
	--vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n  return etype;\n}\n" \
	$< > $@


