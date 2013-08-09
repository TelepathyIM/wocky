EXAMPLES =

if ! USING_OPENSSL
EXAMPLES += wocky-dump-certificates
endif

EXAMPLES += wocky-send-message
EXAMPLES += wocky-receive-messages
EXAMPLES += wocky-register
EXAMPLES += wocky-unregister

INCLUDES := -I$(top_builddir)/wocky

wocky_dump_certificates_SOURCES = dump-certificates.c
wocky_dump_certificates_CFLAGS = $(TLS_CFLAGS) $(AM_CFLAGS)
wocky_dump_certificates_LDADD = $(TLS_LIBS) $(LDADD)

wocky_send_message_SOURCES = send-message.c

wocky_receive_messages_SOURCES = receive-messages.c

wocky_register_SOURCES = register.c

wocky_unregister_SOURCES = unregister.c


LDADD = \
    @GLIB_LIBS@ \
    $(top_builddir)/wocky/libwocky.la

AM_CFLAGS = \
    $(WOCKY_CFLAGS) \
    $(ERROR_CFLAGS) \
    @GLIB_CFLAGS@

check_c_sources = \
    $(wocky_dump_certificates_SOURCES) \
    $(wocky_send_message_SOURCES) \
    $(wocky_receive_messages_SOURCES) \
    $(wocky_register_SOURCES) \
    $(wocky_unregister_SOURCES)

include $(top_srcdir)/tools/check-coding-style.mk
check-local: check-coding-style

noinst_PROGRAMS = $(EXAMPLES)
