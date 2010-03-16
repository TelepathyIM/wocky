#ifndef __WOCKY_TEST_HELPER_H__
#define __WOCKY_TEST_HELPER_H__

G_BEGIN_DECLS

#include <wocky/wocky-xmpp-connection.h>
#include <wocky/wocky-porter.h>
#include <wocky/wocky-session.h>
#include "wocky-test-stream.h"

typedef struct {
  GMainLoop *loop;
  gboolean parsed_stanza;
  GQueue *expected_stanzas;
  WockyXmppConnection *in;
  WockyXmppConnection *out;
  WockyPorter *sched_in;
  WockyPorter *sched_out;
  WockySession *session_in;
  WockySession *session_out;
  WockyTestStream *stream;
  guint outstanding;
  GCancellable *cancellable;
} test_data_t;

test_data_t * setup_test (void);

void teardown_test (test_data_t *data);

void test_wait_pending (test_data_t *test);

gboolean test_timeout_cb (gpointer data);

void test_open_connection (test_data_t *test);

void test_close_connection (test_data_t *test);

void test_open_both_connections (test_data_t *test);

void test_close_porter (test_data_t *test);

void test_expected_stanza_received (test_data_t *test,
    WockyXmppStanza *stanza);

void test_close_both_porters (test_data_t *test);

#define test_assert_nodes_equal(n1, n2) \
  G_STMT_START { \
    if (!wocky_xmpp_node_equal ((n1), (n2))) \
      g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
          g_strdup_printf ("Nodes not equal:\n%s\n\n%s", \
              wocky_xmpp_node_to_string (n1), \
              wocky_xmpp_node_to_string (n2))); \
  } G_STMT_END

#define test_assert_nodes_not_equal(n1, n2) \
  G_STMT_START { \
    if (wocky_xmpp_node_equal ((n1), (n2))) \
      g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
          g_strdup_printf ("Nodes unexpectedly equal:\n%s\n\n%s", \
              wocky_xmpp_node_to_string (n1), \
              wocky_xmpp_node_to_string (n2))); \
  } G_STMT_END

/* Slightly evil macro that tests that two stanzas are equal, except that if
 * one has an id and the other does not this is not considered a difference. It
 * modifies the stanzas because I am lazy.
 */
#define test_assert_stanzas_equal(s1, s2) \
  G_STMT_START { \
    const gchar *_id1 = wocky_xmpp_node_get_attribute ((s1)->node, "id"); \
    const gchar *_id2 = wocky_xmpp_node_get_attribute ((s2)->node, "id"); \
    if (_id1 == NULL && _id2 != NULL) \
      wocky_xmpp_node_set_attribute ((s1)->node, "id", _id2); \
    else if (_id1 != NULL && _id2 == NULL) \
      wocky_xmpp_node_set_attribute ((s2)->node, "id", _id1); \
    test_assert_nodes_equal ((s1)->node, (s2)->node); \
  } G_STMT_END

void test_init (int argc,
    char **argv);

void test_deinit (void);

G_END_DECLS

#endif /* #ifndef __WOCKY_TEST_HELPER_H__*/
