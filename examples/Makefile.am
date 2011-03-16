EXAMPLES =

EXAMPLES += wocky-send-message
EXAMPLES += wocky-receive-messages
EXAMPLES += wocky-register
EXAMPLES += wocky-unregister

INCLUDES := -I$(top_builddir)/wocky

wocky_send_message_SOURCES = send-message.c
wocky_send_message_DEPENDENCIES = $(top_builddir)/wocky/libwocky.la

wocky_receive_messages_SOURCES = receive-messages.c
wocky_receive_messages_DEPENDENCIES = $(top_builddir)/wocky/libwocky.la

wocky_register_SOURCES = register.c
wocky_register_DEPENDENCIES = $(top_builddir)/wocky/libwocky.la

wocky_unregister_SOURCES = unregister.c
wocky_unregister_DEPENDENCIES = $(top_builddir)/wocky/libwocky.la


LDADD = \
    @GLIB_LIBS@ \
    $(top_builddir)/wocky/libwocky.la

AM_CFLAGS = \
    $(WOCKY_CFLAGS) \
    $(ERROR_CFLAGS) \
    @GLIB_CFLAGS@

check_c_sources = $(wocky_send_message_SOURCES)    \
                  $(wocky_receive_messages_SOURCES) \
                  $(wocky_register_SOURCES)   \
                  $(wocky_unregister_SOURCES)

include $(top_srcdir)/tools/check-coding-style.mk
check-local: check-coding-style

noinst_PROGRAMS = $(EXAMPLES)
