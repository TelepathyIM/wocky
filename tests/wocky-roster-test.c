#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <wocky/wocky-roster.h>
#include <wocky/wocky-porter.h>
#include <wocky/wocky-utils.h>
#include <wocky/wocky-xmpp-connection.h>
#include <wocky/wocky-contact.h>
#include <wocky/wocky-namespaces.h>

#include "wocky-test-stream.h"
#include "wocky-test-helper.h"

/* Test to instantiate a WockyRoster object */
static void
test_instantiation (void)
{
  WockyRoster *roster;
  WockyXmppConnection *connection;
  WockyPorter *porter;
  WockyTestStream *stream;

  stream = g_object_new (WOCKY_TYPE_TEST_STREAM, NULL);
  connection = wocky_xmpp_connection_new (stream->stream0);
  porter = wocky_porter_new (connection);

  roster = wocky_roster_new (porter);

  g_assert (roster != NULL);

  g_object_unref (roster);
  g_object_unref (porter);
  g_object_unref (connection);
  g_object_unref (stream);
}

/* Test if the Roster sends the right IQ query when fetching the roster */
static gboolean
fetch_roster_send_iq_cb (WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyStanzaType type;
  WockyStanzaSubType sub_type;
  WockyXmppNode *node;
  WockyXmppStanza *reply;
  const char *id;

  /* Make sure stanza is as expected. */
  wocky_xmpp_stanza_get_type_info (stanza, &type, &sub_type);

  g_assert (type == WOCKY_STANZA_TYPE_IQ);
  g_assert (sub_type == WOCKY_STANZA_SUB_TYPE_GET);

  node = wocky_xmpp_node_get_child (stanza->node, "query");

  g_assert (stanza->node != NULL);
  g_assert (!wocky_strdiff (wocky_xmpp_node_get_ns (node),
          "jabber:iq:roster"));

  id = wocky_xmpp_node_get_attribute (stanza->node, "id");
  g_assert (id != NULL);

  reply = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_RESULT,
      NULL, NULL,
      WOCKY_NODE_ATTRIBUTE, "id", id,
      WOCKY_NODE, "query",
        WOCKY_NODE_XMLNS, "jabber:iq:roster",
        WOCKY_NODE, "item",
          WOCKY_NODE_ATTRIBUTE, "jid", "romeo@example.net",
          WOCKY_NODE_ATTRIBUTE, "name", "Romeo",
          WOCKY_NODE_ATTRIBUTE, "subscription", "both",
          WOCKY_NODE, "group",
            WOCKY_NODE_TEXT, "Friends",
          WOCKY_NODE_END,
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
fetch_roster_fetched_cb (GObject *source_object,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;

  g_return_if_fail (wocky_roster_fetch_roster_finish (
          WOCKY_ROSTER (source_object), res, NULL));

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_fetch_roster_send_iq (void)
{
  WockyRoster *roster;
  test_data_t *test = setup_test ();

  test_open_both_connections (test);

  wocky_porter_register_handler (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_GET, NULL,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      fetch_roster_send_iq_cb, test, WOCKY_STANZA_END);

  wocky_porter_start (test->sched_out);
  wocky_porter_start (test->sched_in);

  roster = wocky_roster_new (test->sched_in);

  wocky_roster_fetch_roster_async (roster, NULL, fetch_roster_fetched_cb, test);

  test->outstanding += 2;
  test_wait_pending (test);

  test_close_both_porters (test);
  g_object_unref (roster);
  teardown_test (test);
}

/* Test if the Roster object is properly populated when receiving its fetch
 * reply */

static WockyContact *
create_romeo (void)
{
  const gchar *groups[] = { "Friends", NULL };

  return g_object_new (WOCKY_TYPE_CONTACT,
      "jid", "romeo@example.net",
      "name", "Romeo",
      "subscription", WOCKY_ROSTER_SUBSCRIPTION_TYPE_BOTH,
      "groups", groups,
      NULL);
}

static WockyContact *
create_juliet (void)
{
  const gchar *groups[] = { "Friends", "Girlz", NULL };

  return g_object_new (WOCKY_TYPE_CONTACT,
      "jid", "juliet@example.net",
      "name", "Juliet",
      "subscription", WOCKY_ROSTER_SUBSCRIPTION_TYPE_TO,
      "groups", groups,
      NULL);
}

static int
find_contact (gconstpointer a,
    gconstpointer b)
{
  if (wocky_contact_equal (WOCKY_CONTACT (a), WOCKY_CONTACT (b)))
    return 0;

  return 1;
}

static void
fetch_roster_reply_roster_cb (GObject *source_object,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyContact *contact;
  WockyRoster *roster = WOCKY_ROSTER (source_object);
  WockyContact *romeo, *juliet;
  GSList *contacts;

  g_return_if_fail (wocky_roster_fetch_roster_finish (roster, res, NULL));

  contacts = wocky_roster_get_all_contacts (roster);
  g_assert_cmpuint (g_slist_length (contacts), ==, 2);

  contact = wocky_roster_get_contact (roster, "romeo@example.net");
  romeo = create_romeo ();
  g_assert (wocky_contact_equal (contact, romeo));
  g_assert (g_slist_find_custom (contacts, romeo, find_contact) != NULL);
  g_object_unref (romeo);

  contact = wocky_roster_get_contact (roster, "juliet@example.net");
  juliet = create_juliet ();
  g_assert (wocky_contact_equal (contact, juliet));
  g_assert (g_slist_find_custom (contacts, juliet, find_contact) != NULL);
  g_object_unref (juliet);

  g_slist_foreach (contacts, (GFunc) g_object_unref, NULL);
  g_slist_free (contacts);
  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static gboolean
fetch_roster_reply_cb (WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  WockyXmppStanza *reply;

  /* We're acting like the server here. The client doesn't need to send a
   * "from" attribute, and in fact it doesn't when fetch_roster is called. It
   * is left up to the server to know which client is the user and then throw
   * in a correct to attribute. Here we're just adding a from attribute so the
   * IQ result builder doesn't complain. */
  if (wocky_xmpp_node_get_attribute (stanza->node, "from") == NULL)
    wocky_xmpp_node_set_attribute (stanza->node, "from",
        "juliet@example.com/balcony");

  reply = wocky_xmpp_stanza_build_iq_result (stanza,
      WOCKY_NODE, "query",
        WOCKY_NODE_XMLNS, "jabber:iq:roster",
        /* Romeo */
        WOCKY_NODE, "item",
          WOCKY_NODE_ATTRIBUTE, "jid", "romeo@example.net",
          WOCKY_NODE_ATTRIBUTE, "name", "Romeo",
          WOCKY_NODE_ATTRIBUTE, "subscription", "both",
          WOCKY_NODE, "group",
            WOCKY_NODE_TEXT, "Friends",
          WOCKY_NODE_END,
        /* Juliet */
        WOCKY_NODE_END,
        WOCKY_NODE, "item",
          WOCKY_NODE_ATTRIBUTE, "jid", "juliet@example.net",
          WOCKY_NODE_ATTRIBUTE, "name", "Juliet",
          WOCKY_NODE_ATTRIBUTE, "subscription", "to",
          WOCKY_NODE, "group",
            WOCKY_NODE_TEXT, "Friends",
          WOCKY_NODE_END,
          WOCKY_NODE, "group",
            WOCKY_NODE_TEXT, "Girlz",
          WOCKY_NODE_END,
        WOCKY_NODE_END,
      WOCKY_NODE_END,
      WOCKY_STANZA_END);

  wocky_porter_send (porter, reply);

  g_object_unref (reply);

  return TRUE;
}

static WockyRoster *
create_initial_roster (test_data_t *test)
{
  WockyRoster *roster;

  wocky_porter_register_handler (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_GET, NULL,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      fetch_roster_reply_cb, test, WOCKY_STANZA_END);

  wocky_porter_start (test->sched_out);
  wocky_porter_start (test->sched_in);

  roster = wocky_roster_new (test->sched_in);

  wocky_roster_fetch_roster_async (roster, NULL,
      fetch_roster_reply_roster_cb, test);

  test->outstanding++;
  test_wait_pending (test);

  return roster;
}

static void
test_fetch_roster_reply (void)
{
  WockyRoster *roster;
  test_data_t *test = setup_test ();

  test_open_both_connections (test);

  roster = create_initial_roster (test);

  test_close_both_porters (test);
  g_object_unref (roster);
  teardown_test (test);
}

/* Test if roster is properly upgraded when a contact is added to it */
static WockyContact *
create_nurse (void)
{
  const gchar *groups[] = { NULL };

  return g_object_new (WOCKY_TYPE_CONTACT,
      "jid", "nurse@example.net",
      "name", "Nurse",
      "subscription", WOCKY_ROSTER_SUBSCRIPTION_TYPE_NONE,
      "groups", groups,
      NULL);
}

static void
roster_added_cb (WockyRoster *roster,
    WockyContact *contact,
    test_data_t *test)
{
  WockyContact *nurse;
  GSList *contacts;

  /* Is that the right contact? */
  nurse = create_nurse ();
  g_assert (wocky_contact_equal (contact, nurse));

  /* Check if the contact has been added to the roster */
  g_assert (wocky_roster_get_contact (roster, "nurse@example.net") == contact);
  contacts = wocky_roster_get_all_contacts (roster);
  g_assert (g_slist_find_custom (contacts, nurse, (GCompareFunc) find_contact));

  g_object_unref (nurse);
  g_slist_foreach (contacts, (GFunc) g_object_unref, NULL);
  g_slist_free (contacts);
  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
roster_update_reply_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyXmppStanza *reply;
  WockyStanzaType type;
  WockyStanzaSubType sub_type;

  reply = wocky_porter_send_iq_finish (WOCKY_PORTER (source), res, NULL);
  g_assert (reply != NULL);

  wocky_xmpp_stanza_get_type_info (reply, &type, &sub_type);
  g_assert (type == WOCKY_STANZA_TYPE_IQ);
  g_assert (sub_type == WOCKY_STANZA_SUB_TYPE_RESULT);

  g_object_unref (reply);
  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
send_roster_update (test_data_t *test,
    const gchar *jid,
    const gchar *name,
    const gchar *subscription,
    const gchar **groups)
{
  WockyXmppStanza *iq;
  WockyXmppNode *item;
  guint i;

  iq = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_SET, NULL, NULL,
    WOCKY_NODE, "query",
      WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_ROSTER,
      WOCKY_NODE, "item",
        WOCKY_NODE_ASSIGN_TO, &item,
      WOCKY_NODE_END,
    WOCKY_NODE_END,
    WOCKY_STANZA_END);

  if (jid != NULL)
    wocky_xmpp_node_set_attribute (item, "jid", jid);

  if (name != NULL)
    wocky_xmpp_node_set_attribute (item, "name", name);

  if (subscription != NULL)
    wocky_xmpp_node_set_attribute (item, "subscription", subscription);

  for (i = 0; groups[i] != NULL; i++)
    {
      WockyXmppNode *node;

      node = wocky_xmpp_node_add_child (item, "group");
      wocky_xmpp_node_set_content (node, groups[i]);
    }

  wocky_porter_send_iq_async (test->sched_out, iq, NULL,
      roster_update_reply_cb, test);
  g_object_unref (iq);

  test->outstanding++;
}

static void
test_roster_upgrade_add (void)
{
  WockyRoster *roster;
  test_data_t *test = setup_test ();
  const gchar *no_group[] = { NULL };

  test_open_both_connections (test);

  roster = create_initial_roster (test);

  g_signal_connect (roster, "added", G_CALLBACK (roster_added_cb), test);
  test->outstanding++;

  send_roster_update (test, "nurse@example.net", "Nurse", "none", no_group);
  test_wait_pending (test);

  test_close_both_porters (test);
  g_object_unref (roster);
  teardown_test (test);
}

/* Test if roster is properly upgraded when a contact is removed from it */
static void
roster_removed_cb (WockyRoster *roster,
    WockyContact *contact,
    test_data_t *test)
{
  WockyContact *romeo;
  GSList *contacts;

  /* Is that the right contact? */
  romeo = create_romeo ();
  g_assert (wocky_contact_equal (contact, romeo));

  /* Check if the contact has been removed from the roster */
  g_assert (wocky_roster_get_contact (roster, "romeo@example.net") == NULL);
  contacts = wocky_roster_get_all_contacts (roster);
  g_assert (g_slist_find_custom (contacts, romeo, (GCompareFunc) find_contact)
      == NULL);

  g_object_unref (romeo);
  g_slist_foreach (contacts, (GFunc) g_object_unref, NULL);
  g_slist_free (contacts);
  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_roster_upgrade_remove (void)
{
  WockyRoster *roster;
  test_data_t *test = setup_test ();
  const gchar *no_group[] = { NULL };

  test_open_both_connections (test);

  roster = create_initial_roster (test);

  g_signal_connect (roster, "removed", G_CALLBACK (roster_removed_cb), test);
  test->outstanding++;

  send_roster_update (test, "romeo@example.net", NULL, "remove", no_group);
  test_wait_pending (test);

  test_close_both_porters (test);
  g_object_unref (roster);
  teardown_test (test);
}

/* Test if WockyContact objects are properly upgraded */
static void
contact_notify_cb (WockyContact *contact,
    GParamSpec *pspec,
    test_data_t *test)
{
  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_roster_upgrade_change (void)
{
  WockyRoster *roster;
  test_data_t *test = setup_test ();
  GSList *contacts, *l;
  WockyContact *romeo, *contact;
  const gchar *groups_init[] = { "Friends", NULL };
  const gchar *groups[] = { "Badger", NULL };

  test_open_both_connections (test);

  roster = create_initial_roster (test);

  contact = wocky_roster_get_contact (roster, "romeo@example.net");
  g_assert (contact != NULL);
  romeo = create_romeo ();

  contacts = wocky_roster_get_all_contacts (roster);
  for (l = contacts; l != NULL; l = g_slist_next (l))
    {
      g_signal_connect (l->data, "notify", G_CALLBACK (contact_notify_cb),
          test);
    }
  g_slist_foreach (contacts, (GFunc) g_object_unref, NULL);
  g_slist_free (contacts);

  /* change name */
  test->outstanding++;
  send_roster_update (test, "romeo@example.net", "Romeooo",  "both",
      groups_init);
  test_wait_pending (test);

  /* Name has been changed */
  wocky_contact_set_name (romeo, "Romeooo");
  g_assert (wocky_contact_equal (contact, romeo));

  /* change subscription */
  test->outstanding++;
  send_roster_update (test, "romeo@example.net", "Romeooo",  "to",
      groups_init);
  test_wait_pending (test);

  /* Subscription has been changed */
  wocky_contact_set_subscription (romeo, WOCKY_ROSTER_SUBSCRIPTION_TYPE_TO);
  g_assert (wocky_contact_equal (contact, romeo));

  /* change groups */
  test->outstanding++;
  send_roster_update (test, "romeo@example.net", "Romeooo",  "to",
      groups);
  test_wait_pending (test);

  /* Groups have been changed */
  wocky_contact_set_groups (romeo, (gchar **) groups);
  g_assert (wocky_contact_equal (contact, romeo));

  g_object_unref (romeo);
  test_close_both_porters (test);
  g_object_unref (roster);
  teardown_test (test);
}

/* Test adding a contact to the roster */
static gboolean
add_contact_send_iq_cb (WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyStanzaType type;
  WockyStanzaSubType sub_type;
  WockyXmppNode *node;
  GSList *l;
  gboolean group_friend = FALSE, group_badger = FALSE;
  const gchar *id;
  WockyXmppStanza *reply;
  const gchar *groups[] = { "Friends", "Badger", NULL };

  /* Make sure stanza is as expected. */
  wocky_xmpp_stanza_get_type_info (stanza, &type, &sub_type);

  g_assert (type == WOCKY_STANZA_TYPE_IQ);
  g_assert (sub_type == WOCKY_STANZA_SUB_TYPE_SET);

  node = wocky_xmpp_node_get_child_ns (stanza->node, "query",
      WOCKY_XMPP_NS_ROSTER);
  g_assert (node != NULL);

  node = wocky_xmpp_node_get_child (node, "item");
  g_assert (node != NULL);
  g_assert (!wocky_strdiff (wocky_xmpp_node_get_attribute (node, "jid"),
      "mercutio@example.net"));
  g_assert (!wocky_strdiff (wocky_xmpp_node_get_attribute (node, "name"),
      "Mercutio"));
  g_assert (wocky_xmpp_node_get_attribute (node, "subscription") == NULL);
  g_assert_cmpuint (g_slist_length (node->children), ==, 2);

  for (l = node->children; l != NULL; l = g_slist_next (l))
    {
      WockyXmppNode *group = (WockyXmppNode *) l->data;

      g_assert (!wocky_strdiff (group->name, "group"));

      if (!wocky_strdiff (group->content, "Friends"))
        group_friend = TRUE;
      else if (!wocky_strdiff (group->content, "Badger"))
        group_badger = TRUE;
      else
        g_assert_not_reached ();
    }
  g_assert (group_friend && group_badger);

  send_roster_update (test, "mercutio@example.net", "Mercutio", "none", groups);

  /* Ack the IQ */
  id = wocky_xmpp_node_get_attribute (stanza->node, "id");
  g_assert (id != NULL);

  reply = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_RESULT,
      NULL, NULL,
      WOCKY_NODE_ATTRIBUTE, "id", id,
      WOCKY_STANZA_END);

  wocky_porter_send (porter, reply);
  g_object_unref (reply);

  test->outstanding--;
  g_main_loop_quit (test->loop);
  return TRUE;
}

static void
contact_added_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyRoster *roster = WOCKY_ROSTER (source);

  g_assert (wocky_roster_add_contact_finish (roster, res, NULL));

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static WockyContact *
create_mercutio (void)
{
  const gchar *groups[] = { "Friends", "Badger", NULL };

  return g_object_new (WOCKY_TYPE_CONTACT,
      "jid", "mercutio@example.net",
      "name", "Mercutio",
      "groups", groups,
      NULL);
}

static void
test_roster_add_contact (void)
{
  WockyRoster *roster;
  test_data_t *test = setup_test ();
  WockyContact *mercutio, *contact;
  const gchar *groups[] = { "Friends", "Badger", NULL };

  test_open_both_connections (test);

  roster = create_initial_roster (test);

  wocky_porter_register_handler (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET, NULL,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      add_contact_send_iq_cb, test,
      WOCKY_NODE, "query",
        WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_ROSTER,
      WOCKY_NODE_END,
      WOCKY_STANZA_END);

  mercutio = create_mercutio ();
  /* Add the Mercutio to our roster */
  wocky_roster_add_contact_async (roster, "mercutio@example.net", "Mercutio",
      groups, NULL, contact_added_cb, test);

  test->outstanding += 2;
  test_wait_pending (test);

  /* check if the contact has been actually added */
  contact = wocky_roster_get_contact (roster, "mercutio@example.net");
  g_assert (wocky_contact_equal (contact, mercutio));

  /* try to re-add the same contact. Operation succeeds immediately */
  wocky_roster_add_contact_async (roster, "mercutio@example.net", "Mercutio",
      groups, NULL, contact_added_cb, test);

  test->outstanding += 1;
  test_wait_pending (test);

  test_close_both_porters (test);
  g_object_unref (mercutio);
  g_object_unref (roster);
  teardown_test (test);
}

/* Test removing a contact from the roster */
static gboolean
remove_contact_send_iq_cb (WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyStanzaType type;
  WockyStanzaSubType sub_type;
  WockyXmppNode *node;
  const gchar *id;
  WockyXmppStanza *reply;
  const gchar *no_group[] = { NULL };

  /* Make sure stanza is as expected. */
  wocky_xmpp_stanza_get_type_info (stanza, &type, &sub_type);

  g_assert (type == WOCKY_STANZA_TYPE_IQ);
  g_assert (sub_type == WOCKY_STANZA_SUB_TYPE_SET);

  node = wocky_xmpp_node_get_child_ns (stanza->node, "query",
      WOCKY_XMPP_NS_ROSTER);
  g_assert (node != NULL);

  node = wocky_xmpp_node_get_child (node, "item");
  g_assert (node != NULL);
  g_assert (!wocky_strdiff (wocky_xmpp_node_get_attribute (node, "jid"),
      "romeo@example.net"));

  /* item node is not supposed to have any child */
  g_assert_cmpuint (g_slist_length (node->children), == , 0);

  send_roster_update (test, "romeo@example.net", NULL, "remove", no_group);

  /* Ack the IQ */
  id = wocky_xmpp_node_get_attribute (stanza->node, "id");
  g_assert (id != NULL);

  reply = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_RESULT,
      NULL, NULL,
      WOCKY_NODE_ATTRIBUTE, "id", id,
      WOCKY_STANZA_END);

  wocky_porter_send (porter, reply);
  g_object_unref (reply);

  test->outstanding--;
  g_main_loop_quit (test->loop);
  return TRUE;
}

static void
contact_removed_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyRoster *roster = WOCKY_ROSTER (source);

  g_assert (wocky_roster_remove_contact_finish (roster, res, NULL));

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_roster_remove_contact (void)
{
  WockyRoster *roster;
  test_data_t *test = setup_test ();
  WockyContact *contact;

  test_open_both_connections (test);

  roster = create_initial_roster (test);

  contact = wocky_roster_get_contact (roster, "romeo@example.net");
  g_assert (contact != NULL);

  wocky_porter_register_handler (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET, NULL,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      remove_contact_send_iq_cb, test,
      WOCKY_NODE, "query",
        WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_ROSTER,
      WOCKY_NODE_END,
      WOCKY_STANZA_END);

  /* Keep a ref on the contact as the roster will release its ref when
   * removing it */
  g_object_ref (contact);

  wocky_roster_remove_contact_async (roster, contact, NULL,
      contact_removed_cb, test);

  test->outstanding += 2;
  test_wait_pending (test);

  /* check if the contact has actually been removed */
  g_assert (wocky_roster_get_contact (roster, "romeo@example.net") == NULL);

  /* try to re-remove the same contact. Operation succeeds immediately */
  wocky_roster_remove_contact_async (roster, contact, NULL,
      contact_removed_cb, test);

  test->outstanding += 1;
  test_wait_pending (test);

  test_close_both_porters (test);
  g_object_unref (roster);
  g_object_unref (contact);
  teardown_test (test);
}

/* test changing the name of a roster item */
static gboolean
change_name_send_iq_cb (WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyStanzaType type;
  WockyStanzaSubType sub_type;
  WockyXmppNode *node;
  const gchar *id;
  WockyXmppStanza *reply;
  const gchar *group[] = { "Friends", NULL };

  /* Make sure stanza is as expected. */
  wocky_xmpp_stanza_get_type_info (stanza, &type, &sub_type);

  g_assert (type == WOCKY_STANZA_TYPE_IQ);
  g_assert (sub_type == WOCKY_STANZA_SUB_TYPE_SET);

  node = wocky_xmpp_node_get_child_ns (stanza->node, "query",
      WOCKY_XMPP_NS_ROSTER);
  g_assert (node != NULL);

  node = wocky_xmpp_node_get_child (node, "item");
  g_assert (node != NULL);
  g_assert (!wocky_strdiff (wocky_xmpp_node_get_attribute (node, "jid"),
      "romeo@example.net"));
  g_assert (!wocky_strdiff (wocky_xmpp_node_get_attribute (node, "name"),
        "Badger"));
  g_assert (!wocky_strdiff (wocky_xmpp_node_get_attribute (node,
        "subscription"), "both"));

  g_assert_cmpuint (g_slist_length (node->children), ==, 1);
  node = wocky_xmpp_node_get_child (node, "group");
  g_assert (node != NULL);
  g_assert (!wocky_strdiff (node->content, "Friends"));

  send_roster_update (test, "romeo@example.net", "Badger", "both", group);

  /* Ack the IQ */
  id = wocky_xmpp_node_get_attribute (stanza->node, "id");
  g_assert (id != NULL);

  reply = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_RESULT,
      NULL, NULL,
      WOCKY_NODE_ATTRIBUTE, "id", id,
      WOCKY_STANZA_END);

  wocky_porter_send (porter, reply);
  g_object_unref (reply);

  test->outstanding--;
  g_main_loop_quit (test->loop);
  return TRUE;
}

static void
contact_name_changed_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyRoster *roster = WOCKY_ROSTER (source);

  g_assert (wocky_roster_change_contact_name_finish (roster, res, NULL));

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
contact_name_changed_not_in_roster_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyRoster *roster = WOCKY_ROSTER (source);
  GError *error = NULL;

  g_assert (!wocky_roster_change_contact_name_finish (roster, res, &error));
  g_assert_error (error, WOCKY_ROSTER_ERROR, WOCKY_ROSTER_ERROR_NOT_IN_ROSTER);
  g_error_free (error);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_roster_change_name (void)
{
  WockyRoster *roster;
  test_data_t *test = setup_test ();
  WockyContact *contact, *romeo, *mercutio;

  test_open_both_connections (test);

  roster = create_initial_roster (test);

  romeo = create_romeo ();

  contact = wocky_roster_get_contact (roster, "romeo@example.net");
  g_assert (contact != NULL);
  g_assert (wocky_contact_equal (contact, romeo));

  wocky_porter_register_handler (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET, NULL,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      change_name_send_iq_cb, test,
      WOCKY_NODE, "query",
        WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_ROSTER,
      WOCKY_NODE_END,
      WOCKY_STANZA_END);

  wocky_roster_change_contact_name_async (roster, contact, "Badger", NULL,
      contact_name_changed_cb, test);

  test->outstanding += 2;
  test_wait_pending (test);

  /* check if the contact name has actually been change */
  wocky_contact_set_name (romeo, "Badger");
  g_assert (wocky_contact_equal (contact, romeo));

  /* Retry to do the same change; operation succeeds immediately */
  wocky_roster_change_contact_name_async (roster, contact, "Badger", NULL,
      contact_name_changed_cb, test);

  test->outstanding += 1;
  test_wait_pending (test);

  /* try to change name of a contact which is not in the roster */
  mercutio = create_mercutio ();

  wocky_roster_change_contact_name_async (roster, mercutio, "Badger", NULL,
      contact_name_changed_not_in_roster_cb, test);

  test->outstanding += 1;
  test_wait_pending (test);

  test_close_both_porters (test);
  g_object_unref (roster);
  g_object_unref (romeo);
  g_object_unref (mercutio);
  teardown_test (test);
}

/* test adding a group to a contact */
static gboolean
add_group_send_iq_cb (WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyStanzaType type;
  WockyStanzaSubType sub_type;
  WockyXmppNode *node;
  const gchar *id;
  WockyXmppStanza *reply;
  const gchar *groups[] = { "Friends", "Badger", NULL };
  GSList *l;
  gboolean group_friend = FALSE, group_badger = FALSE;

  /* Make sure stanza is as expected. */
  wocky_xmpp_stanza_get_type_info (stanza, &type, &sub_type);

  g_assert (type == WOCKY_STANZA_TYPE_IQ);
  g_assert (sub_type == WOCKY_STANZA_SUB_TYPE_SET);

  node = wocky_xmpp_node_get_child_ns (stanza->node, "query",
      WOCKY_XMPP_NS_ROSTER);
  g_assert (node != NULL);

  node = wocky_xmpp_node_get_child (node, "item");
  g_assert (node != NULL);
  g_assert (!wocky_strdiff (wocky_xmpp_node_get_attribute (node, "jid"),
      "romeo@example.net"));
  g_assert (!wocky_strdiff (wocky_xmpp_node_get_attribute (node, "name"),
        "Romeo"));
  g_assert (!wocky_strdiff (wocky_xmpp_node_get_attribute (node,
        "subscription"), "both"));

  g_assert_cmpuint (g_slist_length (node->children), ==, 2);
  for (l = node->children; l != NULL; l = g_slist_next (l))
    {
      WockyXmppNode *group = (WockyXmppNode *) l->data;

      g_assert (!wocky_strdiff (group->name, "group"));

      if (!wocky_strdiff (group->content, "Friends"))
        group_friend = TRUE;
      else if (!wocky_strdiff (group->content, "Badger"))
        group_badger = TRUE;
      else
        g_assert_not_reached ();
    }
  g_assert (group_friend && group_badger);

  send_roster_update (test, "romeo@example.net", "Romeo", "both", groups);

  /* Ack the IQ */
  id = wocky_xmpp_node_get_attribute (stanza->node, "id");
  g_assert (id != NULL);

  reply = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_RESULT,
      NULL, NULL,
      WOCKY_NODE_ATTRIBUTE, "id", id,
      WOCKY_STANZA_END);

  wocky_porter_send (porter, reply);
  g_object_unref (reply);

  test->outstanding--;
  g_main_loop_quit (test->loop);
  return TRUE;
}

static void
contact_group_added_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyRoster *roster = WOCKY_ROSTER (source);

  g_assert (wocky_roster_contact_add_group_finish (roster, res, NULL));

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
contact_group_added_not_in_roster_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyRoster *roster = WOCKY_ROSTER (source);
  GError *error = NULL;

  g_assert (!wocky_roster_contact_add_group_finish (roster, res, &error));
  g_assert_error (error, WOCKY_ROSTER_ERROR, WOCKY_ROSTER_ERROR_NOT_IN_ROSTER);
  g_error_free (error);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_contact_add_group (void)
{
  WockyRoster *roster;
  test_data_t *test = setup_test ();
  WockyContact *contact, *romeo, *mercutio;

  test_open_both_connections (test);

  roster = create_initial_roster (test);

  romeo = create_romeo ();

  contact = wocky_roster_get_contact (roster, "romeo@example.net");
  g_assert (contact != NULL);
  g_assert (wocky_contact_equal (contact, romeo));

  wocky_porter_register_handler (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET, NULL,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      add_group_send_iq_cb, test,
      WOCKY_NODE, "query",
        WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_ROSTER,
      WOCKY_NODE_END,
      WOCKY_STANZA_END);

  wocky_roster_contact_add_group_async (roster, contact, "Badger", NULL,
      contact_group_added_cb, test);

  test->outstanding += 2;
  test_wait_pending (test);

  /* check if the group has actually been added */
  wocky_contact_add_group (romeo, "Badger");
  g_assert (wocky_contact_equal (contact, romeo));

  /* Retry to do the same change; operation succeeds immediately */
  wocky_roster_contact_add_group_async (roster, contact, "Badger", NULL,
      contact_group_added_cb, test);

  test->outstanding += 1;
  test_wait_pending (test);

  /* try to add a group to a contact which is not in the roster */
  mercutio = create_mercutio ();

  wocky_roster_contact_add_group_async (roster, mercutio, "Badger", NULL,
      contact_group_added_not_in_roster_cb, test);

  test->outstanding += 1;
  test_wait_pending (test);

  test_close_both_porters (test);
  g_object_unref (roster);
  g_object_unref (romeo);
  g_object_unref (mercutio);
  teardown_test (test);
}

/* test removing a group from a contact */
static gboolean
remove_group_send_iq_cb (WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyStanzaType type;
  WockyStanzaSubType sub_type;
  WockyXmppNode *node;
  const gchar *id;
  WockyXmppStanza *reply;
  const gchar *groups[] = { NULL };

  /* Make sure stanza is as expected. */
  wocky_xmpp_stanza_get_type_info (stanza, &type, &sub_type);

  g_assert (type == WOCKY_STANZA_TYPE_IQ);
  g_assert (sub_type == WOCKY_STANZA_SUB_TYPE_SET);

  node = wocky_xmpp_node_get_child_ns (stanza->node, "query",
      WOCKY_XMPP_NS_ROSTER);
  g_assert (node != NULL);

  node = wocky_xmpp_node_get_child (node, "item");
  g_assert (node != NULL);
  g_assert (!wocky_strdiff (wocky_xmpp_node_get_attribute (node, "jid"),
      "romeo@example.net"));
  g_assert (!wocky_strdiff (wocky_xmpp_node_get_attribute (node, "name"),
        "Romeo"));
  g_assert (!wocky_strdiff (wocky_xmpp_node_get_attribute (node,
        "subscription"), "both"));

  g_assert_cmpuint (g_slist_length (node->children), ==, 0);

  send_roster_update (test, "romeo@example.net", "Romeo", "both", groups);

  /* Ack the IQ */
  id = wocky_xmpp_node_get_attribute (stanza->node, "id");
  g_assert (id != NULL);

  reply = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_RESULT,
      NULL, NULL,
      WOCKY_NODE_ATTRIBUTE, "id", id,
      WOCKY_STANZA_END);

  wocky_porter_send (porter, reply);
  g_object_unref (reply);

  test->outstanding--;
  g_main_loop_quit (test->loop);
  return TRUE;
}

static void
contact_group_removed_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyRoster *roster = WOCKY_ROSTER (source);

  g_assert (wocky_roster_contact_remove_group_finish (roster, res, NULL));

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
contact_group_removed_not_in_roster_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyRoster *roster = WOCKY_ROSTER (source);
  GError *error = NULL;

  g_assert (!wocky_roster_contact_remove_group_finish (roster, res, &error));
  g_assert_error (error, WOCKY_ROSTER_ERROR, WOCKY_ROSTER_ERROR_NOT_IN_ROSTER);
  g_error_free (error);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_contact_remove_group (void)
{
  WockyRoster *roster;
  test_data_t *test = setup_test ();
  WockyContact *contact, *romeo, *mercutio;

  test_open_both_connections (test);

  roster = create_initial_roster (test);

  romeo = create_romeo ();

  contact = wocky_roster_get_contact (roster, "romeo@example.net");
  g_assert (contact != NULL);
  g_assert (wocky_contact_equal (contact, romeo));

  wocky_porter_register_handler (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET, NULL,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      remove_group_send_iq_cb, test,
      WOCKY_NODE, "query",
        WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_ROSTER,
      WOCKY_NODE_END,
      WOCKY_STANZA_END);

  wocky_roster_contact_remove_group_async (roster, contact, "Friends", NULL,
      contact_group_removed_cb, test);

  test->outstanding += 2;
  test_wait_pending (test);

  /* check if the group has actually been added */
  wocky_contact_remove_group (romeo, "Friends");
  g_assert (wocky_contact_equal (contact, romeo));

  /* Retry to do the same change; operation succeeds immediately */
  wocky_roster_contact_remove_group_async (roster, contact, "Friends", NULL,
      contact_group_removed_cb, test);

  test->outstanding += 1;
  test_wait_pending (test);

  /* try to remove a group from a contact which is not in the roster */
  mercutio = create_mercutio ();

  wocky_roster_contact_remove_group_async (roster, mercutio, "Friends", NULL,
      contact_group_removed_not_in_roster_cb, test);

  test->outstanding += 1;
  test_wait_pending (test);

  test_close_both_porters (test);
  g_object_unref (roster);
  g_object_unref (romeo);
  g_object_unref (mercutio);
  teardown_test (test);

}

int
main (int argc, char **argv)
{
  int result;

  test_init (argc, argv);

  g_test_add_func ("/xmpp-roster/instantiation", test_instantiation);
  g_test_add_func ("/xmpp-roster/fetch-roster-send-iq",
      test_fetch_roster_send_iq);
  g_test_add_func ("/xmpp-roster/fetch-roster-reply", test_fetch_roster_reply);
  g_test_add_func ("/xmpp-roster/roster-upgrade-add", test_roster_upgrade_add);
  g_test_add_func ("/xmpp-roster/roster-upgrade-remove",
      test_roster_upgrade_remove);
  g_test_add_func ("/xmpp-roster/roster-upgrade-change",
      test_roster_upgrade_change);
  g_test_add_func ("/xmpp-roster/roster-add-contact", test_roster_add_contact);
  g_test_add_func ("/xmpp-roster/roster-remove-contact",
      test_roster_remove_contact);
  g_test_add_func ("/xmpp-roster/roster-change-name", test_roster_change_name);
  g_test_add_func ("/xmpp-roster/contact-add-group", test_contact_add_group);
  g_test_add_func ("/xmpp-roster/contact-remove-group",
      test_contact_remove_group);

  result = g_test_run ();
  test_deinit ();
  return result;
}
