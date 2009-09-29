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


/* Test to instantiate a WockyPubsubService object */
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
test_get_default_node_configuration (void)
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

  wocky_pubsub_service_get_default_node_configuration_async (pubsub, NULL,
      test_get_default_node_configuration_cb, test);

  test->outstanding += 2;
  test_wait_pending (test);

  g_object_unref (pubsub);
}

/* Create a node with default config */
static gboolean
test_create_node_no_config_iq_cb (WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyXmppStanza *reply;

  reply = wocky_xmpp_stanza_build_iq_result (stanza, WOCKY_STANZA_END);

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
    GAsyncReadyCallback create_cb)
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
        WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_PUBSUB,
        WOCKY_NODE, "create",
          WOCKY_NODE_ATTRIBUTE, "node", "node1",
        WOCKY_NODE_END,
      WOCKY_STANZA_END);

  wocky_pubsub_service_create_node_async (pubsub, "node1", NULL, NULL,
      create_cb, test);

  test->outstanding += 2;
  test_wait_pending (test);

  g_object_unref (pubsub);
}

static void
test_create_node_no_config (void)
{
  create_node_test (test_create_node_no_config_iq_cb,
      test_create_node_no_config_cb);
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
      test_create_node_unsupported_cb);
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
  g_test_add_func ("/pubsub-service/create-node-no-config",
      test_create_node_no_config);
  g_test_add_func ("/pubsub-service/create-node-unsupported",
      test_create_node_unsupported);

  result = g_test_run ();
  test_deinit ();
  return result;
}
