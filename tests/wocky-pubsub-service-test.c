#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <wocky/wocky-pubsub-service.h>
#include <wocky/wocky-pubsub-node.h>
#include <wocky/wocky-utils.h>
#include <wocky/wocky-namespaces.h>
#include <wocky/wocky-xmpp-error.h>

#include "wocky-test-stream.h"
#include "wocky-test-helper.h"


/* Test instantiating a WockyPubsubService object */
static WockySession *
create_session (void)
{
  WockyXmppConnection *connection;
  WockyTestStream *stream;
  WockySession *session;

  stream = g_object_new (WOCKY_TYPE_TEST_STREAM, NULL);
  connection = wocky_xmpp_connection_new (stream->stream0);
  session = wocky_session_new (connection);

  g_object_unref (connection);
  g_object_unref (stream);
  return session;
}

static void
test_instantiation (void)
{
  WockyPubsubService *pubsub;
  WockySession *session;

  session = create_session ();

  pubsub = wocky_pubsub_service_new (session, "pubsub.localhost");
  g_assert (pubsub != NULL);

  g_object_unref (pubsub);
  g_object_unref (session);
}

/* Test wocky_pubsub_service_ensure_node */
static void
test_ensure_node (void)
{
  WockyPubsubService *pubsub;
  WockySession *session;
  WockyPubsubNode *node;

  session = create_session ();

  pubsub = wocky_pubsub_service_new (session, "pubsub.localhost");
  node = wocky_pubsub_service_lookup_node (pubsub, "node1");
  g_assert (node == NULL);

  node = wocky_pubsub_service_ensure_node (pubsub, "node1");
  g_assert (node != NULL);

  node = wocky_pubsub_service_lookup_node (pubsub, "node1");
  g_assert (node != NULL);

  /* destroy the node */
  g_object_unref (node);
  node = wocky_pubsub_service_lookup_node (pubsub, "node1");
  g_assert (node == NULL);

  g_object_unref (pubsub);
  g_object_unref (session);
}

/* Test wocky_pubsub_service_get_default_node_configuration_async */
static gboolean
test_get_default_node_configuration_iq_cb (WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyXmppStanza *reply;

  reply = wocky_xmpp_stanza_build_iq_result (stanza,
      WOCKY_NODE, "pubsub",
        WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_PUBSUB_OWNER,
        WOCKY_NODE, "default",
          WOCKY_NODE, "x",
            WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_DATA,
            WOCKY_NODE_ATTRIBUTE, "type", "form",
            WOCKY_NODE, "field",
              WOCKY_NODE_ATTRIBUTE, "type", "hidden",
              WOCKY_NODE_ATTRIBUTE, "var", "FORM_TYPE",
              WOCKY_NODE, "value",
                WOCKY_NODE_TEXT, WOCKY_XMPP_NS_PUBSUB_NODE_CONFIG,
              WOCKY_NODE_END,
            WOCKY_NODE_END,
            WOCKY_NODE, "field",
              WOCKY_NODE_ATTRIBUTE, "var", "pubsub#title",
              WOCKY_NODE_ATTRIBUTE, "type", "text-single",
              WOCKY_NODE_ATTRIBUTE, "label", "Title",
            WOCKY_NODE_END,
            WOCKY_NODE, "field",
              WOCKY_NODE_ATTRIBUTE, "var", "pubsub#deliver_notifications",
              WOCKY_NODE_ATTRIBUTE, "type", "boolean",
              WOCKY_NODE_ATTRIBUTE, "label", "Deliver event notifications",
            WOCKY_NODE_END,
          WOCKY_NODE_END,
        WOCKY_NODE_END,
      WOCKY_NODE_END, WOCKY_STANZA_END);

  wocky_porter_send (porter, reply);
  g_object_unref (reply);

  test->outstanding--;
  g_main_loop_quit (test->loop);
  return TRUE;
}

static void
test_get_default_node_configuration_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyDataForms *forms;
  WockyDataFormsField *field;

  forms = wocky_pubsub_service_get_default_node_configuration_finish (
      WOCKY_PUBSUB_SERVICE (source), res, NULL);
  g_assert (forms != NULL);

  field = g_hash_table_lookup (forms->fields, "pubsub#title");
  g_assert (field != NULL);
  field = g_hash_table_lookup (forms->fields, "pubsub#deliver_notifications");
  g_assert (field != NULL);

  g_object_unref (forms);
  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
get_default_node_configuration_test (WockyPorterHandlerFunc iq_cb,
    GAsyncReadyCallback get_cb)
{
  test_data_t *test = setup_test ();
  WockyPubsubService *pubsub;

  test_open_both_connections (test);

  wocky_porter_start (test->sched_out);
  wocky_session_start (test->session_in);

  pubsub = wocky_pubsub_service_new (test->session_in, "pubsub.localhost");

  wocky_porter_register_handler (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_GET, NULL,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      iq_cb, test,
      WOCKY_NODE, "pubsub",
        WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_PUBSUB_OWNER,
      WOCKY_NODE, "default",
      WOCKY_NODE_END, WOCKY_STANZA_END);

  wocky_pubsub_service_get_default_node_configuration_async (pubsub, NULL,
      get_cb, test);

  test->outstanding += 2;
  test_wait_pending (test);

  g_object_unref (pubsub);

  test_close_both_porters (test);
  teardown_test (test);
}

static void
test_get_default_node_configuration (void)
{
  get_default_node_configuration_test (
      test_get_default_node_configuration_iq_cb,
      test_get_default_node_configuration_cb);
}

/* Try to retrieve default node configuration and get a insufficient
 * privileges error */
static gboolean
test_get_default_node_configuration_insufficient_iq_cb (WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyXmppStanza *reply;

  reply = wocky_xmpp_stanza_build_iq_error (stanza,
      WOCKY_NODE, "pubsub",
        WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_PUBSUB_OWNER,
        WOCKY_NODE, "configure",
          WOCKY_NODE_ATTRIBUTE, "node", "node1",
        WOCKY_NODE_END,
      WOCKY_NODE_END,
      WOCKY_NODE, "error",
        WOCKY_NODE_ATTRIBUTE, "type", "auth",
        WOCKY_NODE, "forbidden",
          WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_STANZAS,
        WOCKY_NODE_END,
      WOCKY_STANZA_END);

  wocky_porter_send (porter, reply);
  g_object_unref (reply);

  test->outstanding--;
  g_main_loop_quit (test->loop);
  return TRUE;
}

static void
test_get_default_node_configuration_insufficient_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyDataForms *forms;
  GError *error = NULL;

  forms = wocky_pubsub_service_get_default_node_configuration_finish (
      WOCKY_PUBSUB_SERVICE (source), res, &error);
  g_assert (forms == NULL);
  g_assert_error (error, WOCKY_XMPP_ERROR, WOCKY_XMPP_ERROR_FORBIDDEN);
  g_error_free (error);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_get_default_node_configuration_insufficient (void)
{
  get_default_node_configuration_test (
      test_get_default_node_configuration_insufficient_iq_cb,
      test_get_default_node_configuration_insufficient_cb);
}

/* Create a node with default config */
static gboolean
test_create_node_no_config_iq_cb (WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyXmppStanza *reply;
  WockyXmppNode *node;

  node = wocky_xmpp_node_get_child_ns (stanza->node, "pubsub",
      WOCKY_XMPP_NS_PUBSUB);
  g_assert (node != NULL);
  node = wocky_xmpp_node_get_child (node, "create");
  g_assert (node != NULL);
  g_assert (!wocky_strdiff (wocky_xmpp_node_get_attribute (node, "node"),
        "node1"));

  reply = wocky_xmpp_stanza_build_iq_result (stanza,
      WOCKY_NODE, "pubsub",
        WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_PUBSUB,
        WOCKY_NODE, "create",
          WOCKY_NODE_ATTRIBUTE, "node", "node1",
      WOCKY_NODE_END, WOCKY_STANZA_END);

  wocky_porter_send (porter, reply);
  g_object_unref (reply);

  test->outstanding--;
  g_main_loop_quit (test->loop);
  return TRUE;
}

static void
test_create_node_no_config_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyPubsubNode *node;

  node = wocky_pubsub_service_create_node_finish (WOCKY_PUBSUB_SERVICE (source),
      res, NULL);
  g_assert (node != NULL);

  g_assert (!wocky_strdiff (wocky_pubsub_node_get_name (node), "node1"));

  g_object_unref (node);
  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
create_node_test (WockyPorterHandlerFunc iq_cb,
    GAsyncReadyCallback create_cb,
    const gchar *node_name)
{
  test_data_t *test = setup_test ();
  WockyPubsubService *pubsub;

  test_open_both_connections (test);

  wocky_porter_start (test->sched_out);
  wocky_session_start (test->session_in);

  pubsub = wocky_pubsub_service_new (test->session_in, "pubsub.localhost");

  wocky_porter_register_handler (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET, NULL,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      iq_cb, test,
      WOCKY_NODE, "pubsub",
        WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_PUBSUB,
        WOCKY_NODE, "create", WOCKY_NODE_END,
      WOCKY_STANZA_END);

  wocky_pubsub_service_create_node_async (pubsub, node_name, NULL, NULL,
      create_cb, test);

  test->outstanding += 2;
  test_wait_pending (test);

  g_object_unref (pubsub);

  test_close_both_porters (test);
  teardown_test (test);
}

static void
test_create_node_no_config (void)
{
  create_node_test (test_create_node_no_config_iq_cb,
      test_create_node_no_config_cb, "node1");
}

/* creation of a node fails because service does not support node creation */
static gboolean
test_create_node_unsupported_iq_cb (WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyXmppStanza *reply;

  reply = wocky_xmpp_stanza_build_iq_error (stanza,
      WOCKY_NODE, "pubsub",
        WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_PUBSUB,
        WOCKY_NODE, "create",
          WOCKY_NODE_ATTRIBUTE, "node", "node1",
        WOCKY_NODE_END,
        WOCKY_NODE, "configure", WOCKY_NODE_END,
      WOCKY_NODE_END,
      WOCKY_NODE, "error",
        WOCKY_NODE_ATTRIBUTE, "type", "cancel",
        WOCKY_NODE, "feature-not-implemented",
          WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_STANZAS,
        WOCKY_NODE_END,
        WOCKY_NODE, "unsupported",
          WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_PUBSUB_ERRORS,
          WOCKY_NODE_ATTRIBUTE, "feature", "create-nodes",
        WOCKY_NODE_END,
      WOCKY_NODE_END,
      WOCKY_STANZA_END);

  wocky_porter_send (porter, reply);
  g_object_unref (reply);

  test->outstanding--;
  g_main_loop_quit (test->loop);
  return TRUE;
}

static void
test_create_node_unsupported_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyPubsubNode *node;
  GError *error = NULL;

  node = wocky_pubsub_service_create_node_finish (WOCKY_PUBSUB_SERVICE (source),
      res, &error);
  g_assert (node == NULL);
  g_assert_error (error, WOCKY_XMPP_ERROR,
      WOCKY_XMPP_ERROR_FEATURE_NOT_IMPLEMENTED);
  g_error_free (error);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_create_node_unsupported (void)
{
  create_node_test (test_create_node_unsupported_iq_cb,
      test_create_node_unsupported_cb, "node1");
}

/* Create an instant node (no name passed) */
static gboolean
test_create_instant_node_iq_cb (WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyXmppStanza *reply;

  reply = wocky_xmpp_stanza_build_iq_result (stanza,
      WOCKY_NODE, "pubsub",
        WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_PUBSUB,
        WOCKY_NODE, "create",
          WOCKY_NODE_ATTRIBUTE, "node", "instant_node",
      WOCKY_NODE_END, WOCKY_STANZA_END);

  wocky_porter_send (porter, reply);
  g_object_unref (reply);

  test->outstanding--;
  g_main_loop_quit (test->loop);
  return TRUE;
}

static void
test_create_instant_node_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyPubsubNode *node;

  node = wocky_pubsub_service_create_node_finish (WOCKY_PUBSUB_SERVICE (source),
      res, NULL);
  g_assert (node != NULL);

  g_assert (!wocky_strdiff (wocky_pubsub_node_get_name (node), "instant_node"));

  g_object_unref (node);
  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_create_instant_node (void)
{
  create_node_test (test_create_instant_node_iq_cb,
      test_create_instant_node_cb, NULL);
}

/* Create a node with configuration */
static void
test_create_node_config_config_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyDataForms *forms;
  WockyDataFormsField *field;

  forms = wocky_pubsub_service_get_default_node_configuration_finish (
      WOCKY_PUBSUB_SERVICE (source), res, NULL);
  g_assert (forms != NULL);

  field = g_hash_table_lookup (forms->fields, "pubsub#title");
  g_assert (field != NULL);
  field->value = wocky_g_value_slice_new_string ("Badger");
  field = g_hash_table_lookup (forms->fields, "pubsub#deliver_notifications");
  g_assert (field != NULL);
  field->value = wocky_g_value_slice_new_boolean (FALSE);

  wocky_pubsub_service_create_node_async (WOCKY_PUBSUB_SERVICE (source),
      "node1", forms, NULL, test_create_node_no_config_cb, test);

  g_object_unref (forms);
  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static gboolean
test_create_node_config_create_iq_cb (WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyXmppStanza *reply;
  WockyXmppNode *node;
  GSList *l;
  gboolean form_type = FALSE, title = FALSE, notif = FALSE;

  node = wocky_xmpp_node_get_child_ns (stanza->node, "pubsub",
      WOCKY_XMPP_NS_PUBSUB);
  g_assert (node != NULL);
  node = wocky_xmpp_node_get_child (node, "configure");
  g_assert (node != NULL);
  node = wocky_xmpp_node_get_child_ns (node, "x", WOCKY_XMPP_NS_DATA);
  g_assert (node != NULL);

  for (l = node->children; l != NULL; l = g_slist_next (l))
    {
      WockyXmppNode *field = l->data;
      const gchar *type, *var, *value = NULL;
      WockyXmppNode *v;

      g_assert (!wocky_strdiff (field->name, "field"));
      var = wocky_xmpp_node_get_attribute (field, "var");
      type = wocky_xmpp_node_get_attribute (field, "type");

      v = wocky_xmpp_node_get_child (field, "value");
      g_assert (v != NULL);
      value = v->content;

      if (!wocky_strdiff (var, "FORM_TYPE"))
        {
          g_assert (!wocky_strdiff (type, "hidden"));
          g_assert (!wocky_strdiff (value, WOCKY_XMPP_NS_PUBSUB_NODE_CONFIG));
          form_type = TRUE;
        }
      else if (!wocky_strdiff (var, "pubsub#title"))
        {
          g_assert (!wocky_strdiff (type, "text-single"));
          g_assert (!wocky_strdiff (value, "Badger"));
          title = TRUE;
        }
      else if (!wocky_strdiff (var, "pubsub#deliver_notifications"))
        {
          g_assert (!wocky_strdiff (type, "boolean"));
          g_assert (!wocky_strdiff (value, "0"));
          notif = TRUE;
        }
      else
        g_assert_not_reached ();
    }
  g_assert (form_type && title && notif);

  reply = wocky_xmpp_stanza_build_iq_result (stanza,
      WOCKY_NODE, "pubsub",
        WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_PUBSUB,
        WOCKY_NODE, "create",
          WOCKY_NODE_ATTRIBUTE, "node", "node1",
      WOCKY_NODE_END, WOCKY_STANZA_END);

  wocky_porter_send (porter, reply);
  g_object_unref (reply);

  test->outstanding--;
  g_main_loop_quit (test->loop);
  return TRUE;
}

static void
test_create_node_config (void)
{
  test_data_t *test = setup_test ();
  WockyPubsubService *pubsub;

  test_open_both_connections (test);

  wocky_porter_start (test->sched_out);
  wocky_session_start (test->session_in);

  pubsub = wocky_pubsub_service_new (test->session_in, "pubsub.localhost");

  wocky_porter_register_handler (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_GET, NULL,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      test_get_default_node_configuration_iq_cb, test,
      WOCKY_NODE, "pubsub",
        WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_PUBSUB_OWNER,
      WOCKY_NODE, "default",
      WOCKY_NODE_END, WOCKY_STANZA_END);

  wocky_porter_register_handler (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET, NULL,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      test_create_node_config_create_iq_cb, test,
      WOCKY_NODE, "pubsub",
        WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_PUBSUB,
        WOCKY_NODE, "create", WOCKY_NODE_END,
      WOCKY_STANZA_END);

  wocky_pubsub_service_get_default_node_configuration_async (pubsub, NULL,
      test_create_node_config_config_cb, test);

  test->outstanding += 4;
  test_wait_pending (test);

  test_close_both_porters (test);
  teardown_test (test);

  g_object_unref (pubsub);
}

/* Four examples taken from §5.6 Retrieve Subscriptions */
typedef enum {
    MODE_NORMAL,
    MODE_NO_SUBSCRIPTIONS,
    MODE_BZZT,
    MODE_AT_NODE
} RetrieveSubscriptionsMode;

typedef struct {
    test_data_t *test;
    RetrieveSubscriptionsMode mode;
} RetrieveSubscriptionsCtx;

typedef struct {
    const gchar *node;
    const gchar *jid;
    const gchar *subscription;
    WockyPubsubSubscriptionState state;
    const gchar *subid;
} CannedSubscriptions;

static CannedSubscriptions normal_subs[] = {
  { "node1", "francisco@denmark.lit",
    "subscribed", WOCKY_PUBSUB_SUBSCRIPTION_SUBSCRIBED,
    NULL },
  { "node2", "francisco@denmark.lit/bonghits",
    "subscribed", WOCKY_PUBSUB_SUBSCRIPTION_SUBSCRIBED,
    NULL },
  { "node5", "francisco@denmark.lit",
    "unconfigured", WOCKY_PUBSUB_SUBSCRIPTION_UNCONFIGURED,
    NULL },
  { "node6", "francisco@denmark.lit",
    "pending", WOCKY_PUBSUB_SUBSCRIPTION_PENDING,
    NULL },
  { NULL, }
};

static CannedSubscriptions bonghit_subs[] = {
  { "bonghits", "bernardo@denmark.lit",
    "subscribed", WOCKY_PUBSUB_SUBSCRIPTION_SUBSCRIBED,
    "123-abc" },
  { "bonghits", "bernardo@denmark.lit/i-am-poorly-read",
    "subscribed", WOCKY_PUBSUB_SUBSCRIPTION_SUBSCRIBED,
    "004-yyy" },
  { NULL, }
};

static WockyXmppStanza *
make_subscriptions_response (WockyXmppStanza *stanza,
    const gchar *node,
    CannedSubscriptions *subs)
{
  WockyXmppStanza *reply;
  WockyXmppNode *s;
  CannedSubscriptions *l;

  reply = wocky_xmpp_stanza_build_iq_result (stanza,
        WOCKY_NODE, "pubsub",
          WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_PUBSUB,
          WOCKY_NODE, "subscriptions",
            WOCKY_NODE_ASSIGN_TO, &s,
          WOCKY_NODE_END,
        WOCKY_NODE_END,
      WOCKY_STANZA_END);

  if (node != NULL)
    wocky_xmpp_node_set_attribute (s, "node", node);

  for (l = subs; l != NULL && l->node != NULL; l++)
    {
      WockyXmppNode *sub = wocky_xmpp_node_add_child (s, "subscription");

      if (node == NULL)
	wocky_xmpp_node_set_attribute (sub, "node", l->node);

      wocky_xmpp_node_set_attribute (sub, "jid", l->jid);
      wocky_xmpp_node_set_attribute (sub, "subscription", l->subscription);

      if (l->subid != NULL)
        wocky_xmpp_node_set_attribute (sub, "subid", l->subid);
    }

  return reply;
}

static gboolean
test_retrieve_subscriptions_iq_cb (
    WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  RetrieveSubscriptionsCtx *ctx = user_data;
  test_data_t *test = ctx->test;
  WockyXmppStanza *reply, *expected;
  WockyXmppNode *subscriptions;

  expected = wocky_xmpp_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_GET,
      NULL, "pubsub.localhost",
        WOCKY_NODE, "pubsub",
          WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_PUBSUB,
          WOCKY_NODE, "subscriptions",
            WOCKY_NODE_ASSIGN_TO, &subscriptions,
          WOCKY_NODE_END,
        WOCKY_NODE_END,
      WOCKY_STANZA_END);

  if (ctx->mode == MODE_AT_NODE)
    wocky_xmpp_node_set_attribute (subscriptions, "node", "bonghits");

  test_assert_stanzas_equal (stanza, expected);
  g_object_unref (expected);

  switch (ctx->mode)
    {
    case MODE_NORMAL:
      reply = make_subscriptions_response (stanza, NULL, normal_subs);
      break;
    case MODE_NO_SUBSCRIPTIONS:
      reply = make_subscriptions_response (stanza, NULL, NULL);
      break;
    case MODE_BZZT:
      {
	GError e = { WOCKY_XMPP_ERROR, WOCKY_XMPP_ERROR_FEATURE_NOT_IMPLEMENTED,
	    "FIXME: <unsupported feature='retrieve-subscriptions'/>" };

	reply = wocky_xmpp_stanza_build_iq_error (stanza, WOCKY_STANZA_END);
	wocky_stanza_error_to_node (&e, reply->node);
	break;
      }
    case MODE_AT_NODE:
      reply = make_subscriptions_response (stanza, "bonghits", bonghit_subs);
      break;
    default:
      g_assert_not_reached ();
    }

  wocky_porter_send (porter, reply);
  g_object_unref (reply);

  test->outstanding--;
  g_main_loop_quit (test->loop);
  return TRUE;
}

static void
check_subscriptions (
    GObject *source,
    GAsyncResult *res,
    CannedSubscriptions *expected_subs)
{
  GList *subscriptions, *l;
  guint i = 0;

  g_assert (wocky_pubsub_service_retrieve_subscriptions_finish (
      WOCKY_PUBSUB_SERVICE (source), res, &subscriptions, NULL));

  for (l = subscriptions; l != NULL; l = l->next, i++)
    {
      WockyPubsubSubscription *sub = l->data;

      g_assert (expected_subs[i].jid != NULL);
      g_assert_cmpstr (expected_subs[i].jid, ==, sub->jid);
      g_assert_cmpstr (expected_subs[i].node, ==,
          wocky_pubsub_node_get_name (sub->node));
      g_assert_cmpuint (expected_subs[i].state, ==, sub->state);
      g_assert_cmpstr (expected_subs[i].subid, ==, sub->subid);
    }

  g_assert_cmpstr (expected_subs[i].jid, ==, NULL);

  wocky_pubsub_subscription_list_free (subscriptions);
}

static void
retrieve_subscriptions_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  RetrieveSubscriptionsCtx *ctx = user_data;
  test_data_t *test = ctx->test;
  GError *error = NULL;

  switch (ctx->mode)
    {
    case MODE_NORMAL:
      check_subscriptions (source, res, normal_subs);
      break;
    case MODE_NO_SUBSCRIPTIONS:
      check_subscriptions (source, res, normal_subs + 4);
      break;
    case MODE_BZZT:
      g_assert (!wocky_pubsub_service_retrieve_subscriptions_finish (
          WOCKY_PUBSUB_SERVICE (source), res, NULL, &error));
      /* FIXME: moar detail */
      g_assert_error (error, WOCKY_XMPP_ERROR,
          WOCKY_XMPP_ERROR_FEATURE_NOT_IMPLEMENTED);
      g_clear_error (&error);
      break;
    case MODE_AT_NODE:
      check_subscriptions (source, res, bonghit_subs);
      break;
    default:
      g_assert_not_reached ();
    }

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_retrieve_subscriptions (gconstpointer mode_)
{
  test_data_t *test = setup_test ();
  RetrieveSubscriptionsMode mode = GPOINTER_TO_UINT (mode_);
  RetrieveSubscriptionsCtx ctx = { test, mode };
  WockyPubsubService *pubsub;
  WockyPubsubNode *node = NULL;

  test_open_both_connections (test);

  wocky_porter_start (test->sched_out);
  wocky_session_start (test->session_in);

  pubsub = wocky_pubsub_service_new (test->session_in, "pubsub.localhost");

  wocky_porter_register_handler (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_GET, NULL,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      test_retrieve_subscriptions_iq_cb, &ctx,
        WOCKY_NODE, "pubsub",
          WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_PUBSUB,
          WOCKY_NODE, "subscriptions", WOCKY_NODE_END,
        WOCKY_NODE_END,
      WOCKY_STANZA_END);

  if (mode == MODE_AT_NODE)
    node = wocky_pubsub_service_ensure_node (pubsub, "bonghits");

  wocky_pubsub_service_retrieve_subscriptions_async (pubsub, node, NULL,
      retrieve_subscriptions_cb, &ctx);

  test->outstanding += 2;
  test_wait_pending (test);

  if (node != NULL)
    g_object_unref (node);

  test_close_both_porters (test);
  teardown_test (test);

  g_object_unref (pubsub);
}

int
main (int argc, char **argv)
{
  int result;

  test_init (argc, argv);

  g_test_add_func ("/pubsub-service/instantiation", test_instantiation);
  g_test_add_func ("/pubsub-service/ensure_node", test_ensure_node);
  g_test_add_func ("/pubsub-service/get-default-node-configuration",
      test_get_default_node_configuration);
  g_test_add_func (
      "/pubsub-service/get-default-node-configuration-insufficient",
      test_get_default_node_configuration_insufficient);

  g_test_add_data_func ("/pubsub-service/retrieve-subscriptions/normal",
      GUINT_TO_POINTER (MODE_NORMAL),
      test_retrieve_subscriptions);
  g_test_add_data_func ("/pubsub-service/retrieve-subscriptions/none",
      GUINT_TO_POINTER (MODE_NO_SUBSCRIPTIONS),
      test_retrieve_subscriptions);
  g_test_add_data_func ("/pubsub-service/retrieve-subscriptions/error",
      GUINT_TO_POINTER (MODE_BZZT),
      test_retrieve_subscriptions);
  g_test_add_data_func ("/pubsub-service/retrieve-subscriptions/for-node",
      GUINT_TO_POINTER (MODE_AT_NODE),
      test_retrieve_subscriptions);

  g_test_add_func ("/pubsub-service/create-node-no-config",
      test_create_node_no_config);
  g_test_add_func ("/pubsub-service/create-node-unsupported",
      test_create_node_unsupported);
  g_test_add_func ("/pubsub-service/create-instant-node",
      test_create_instant_node);
  g_test_add_func ("/pubsub-service/create-node-config",
      test_create_node_config);

  result = g_test_run ();
  test_deinit ();
  return result;
}
