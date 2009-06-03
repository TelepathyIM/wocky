#ifndef __WOCKY_TEST_HELPER_H__
#define __WOCKY_TEST_HELPER_H__

G_BEGIN_DECLS

#include <wocky/wocky-xmpp-connection.h>
#include <wocky/wocky-xmpp-scheduler.h>
#include "wocky-test-stream.h"

typedef struct {
  GMainLoop *loop;
  gboolean parsed_stanza;
  GQueue *expected_stanzas;
  WockyXmppConnection *in;
  WockyXmppConnection *out;
  WockyXmppScheduler *sched_in;
  WockyXmppScheduler *sched_out;
  WockyTestStream *stream;
  guint outstanding;
} test_data_t;

test_data_t * setup_test (void);

void teardown_test (test_data_t *data);

void test_wait_pending (test_data_t *test);

gboolean test_timeout_cb (gpointer data);

void test_open_connection (test_data_t *test);

void test_close_connection (test_data_t *test);

G_END_DECLS

#endif /* #ifndef __WOCKY_TEST_HELPER_H__*/
