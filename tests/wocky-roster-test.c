#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <wocky/wocky.h>

#include "wocky-test-stream.h"
#include "wocky-test-helper.h"

/* Test to instantiate a WockyRoster object */
static void
test_instantiation (void)
{
  WockyRoster *roster;
  WockyXmppConnection *connection;
  WockyTestStream *stream;
  WockySession *session;

  stream = g_object_new (WOCKY_TYPE_TEST_STREAM, NULL);
  connection = wocky_xmpp_connection_new (stream->stream0);
  session = wocky_session_new_with_connection (connection, "example.com");

  roster = wocky_roster_new (session);

  g_assert (roster != NULL);

  g_object_unref (roster);
  g_object_unref (session);
  g_object_unref (connection);
  g_object_unref (stream);
}

/* Test if the Roster sends the right IQ query when fetching the roster */
static gboolean
fetch_roster_send_iq_cb (WockyPorter *porter,
    WockyStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyStanzaType type;
  WockyStanzaSubType sub_type;
  WockyNode *node;
  WockyStanza *reply;
  const char *id;

  /* Make sure stanza is as expected. */
  wocky_stanza_get_type_info (stanza, &type, &sub_type);

  g_assert (type == WOCKY_STANZA_TYPE_IQ);
  g_assert (sub_type == WOCKY_STANZA_SUB_TYPE_GET);

  node = wocky_node_get_child (wocky_stanza_get_top_node (stanza),
      "query");

  g_assert (wocky_stanza_get_top_node (stanza) != NULL);
  g_assert (!wocky_strdiff (wocky_node_get_ns (node),
          "jabber:iq:roster"));

  id = wocky_node_get_attribute (wocky_stanza_get_top_node (stanza),
      "id");
  g_assert (id != NULL);

  reply = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_RESULT,
      NULL, NULL,
      '@', "id", id,
      '(', "query",
        ':', "jabber:iq:roster",
        '(', "item",
          '@', "jid", "romeo@example.net",
          '@', "name", "Romeo",
          '@', "subscription", "both",
          '(', "group",
            '$', "Friends",
          ')',
        ')',
      ')',
      NULL);

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

  wocky_porter_register_handler_from_anyone (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_GET,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      fetch_roster_send_iq_cb, test, NULL);

  wocky_porter_start (test->sched_out);
  wocky_session_start (test->session_in);

  roster = wocky_roster_new (test->session_in);

  wocky_roster_fetch_roster_async (roster, NULL, fetch_roster_fetched_cb, test);

  test->outstanding += 2;
  test_wait_pending (test);

  test_close_both_porters (test);
  g_object_unref (roster);
  teardown_test (test);
}

/* Test if the Roster object is properly populated when receiving its fetch
 * reply */

static WockyBareContact *
create_romeo (void)
{
  const gchar *groups[] = { "Friends", NULL };

  return g_object_new (WOCKY_TYPE_BARE_CONTACT,
      "jid", "romeo@example.net",
      "name", "Romeo",
      "subscription", WOCKY_ROSTER_SUBSCRIPTION_TYPE_BOTH,
      "groups", groups,
      NULL);
}

static WockyBareContact *
create_juliet (void)
{
  const gchar *groups[] = { "Friends", "Girlz", NULL };

  return g_object_new (WOCKY_TYPE_BARE_CONTACT,
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
  if (wocky_bare_contact_equal (WOCKY_BARE_CONTACT (a), WOCKY_BARE_CONTACT (b)))
    return 0;

  return 1;
}

static void
fetch_roster_reply_roster_cb (GObject *source_object,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyBareContact *contact;
  WockyRoster *roster = WOCKY_ROSTER (source_object);
  WockyBareContact *romeo, *juliet;
  GSList *contacts;

  g_return_if_fail (wocky_roster_fetch_roster_finish (roster, res, NULL));

  contacts = wocky_roster_get_all_contacts (roster);
  g_assert_cmpuint (g_slist_length (contacts), ==, 2);

  contact = wocky_roster_get_contact (roster, "romeo@example.net");
  romeo = create_romeo ();
  g_assert (wocky_bare_contact_equal (contact, romeo));
  g_assert (g_slist_find_custom (contacts, romeo, find_contact) != NULL);
  g_object_unref (romeo);

  contact = wocky_roster_get_contact (roster, "juliet@example.net");
  juliet = create_juliet ();
  g_assert (wocky_bare_contact_equal (contact, juliet));
  g_assert (g_slist_find_custom (contacts, juliet, find_contact) != NULL);
  g_object_unref (juliet);

  g_slist_foreach (contacts, (GFunc) g_object_unref, NULL);
  g_slist_free (contacts);
  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static gboolean
fetch_roster_reply_cb (WockyPorter *porter,
    WockyStanza *stanza,
    gpointer user_data)
{
  WockyStanza *reply;

  /* We're acting like the server here. The client doesn't need to send a
   * "from" attribute, and in fact it doesn't when fetch_roster is called. It
   * is left up to the server to know which client is the user and then throw
   * in a correct to attribute. Here we're just adding a from attribute so the
   * IQ result builder doesn't complain. */
  if (wocky_stanza_get_from (stanza) == NULL)
    wocky_node_set_attribute (wocky_stanza_get_top_node (stanza), "from",
        "juliet@example.com/balcony");

  reply = wocky_stanza_build_iq_result (stanza,
      '(', "query",
        ':', "jabber:iq:roster",
        /* Romeo */
        '(', "item",
          '@', "jid", "romeo@example.net",
          '@', "name", "Romeo",
          '@', "subscription", "both",
          '(', "group",
            '$', "Friends",
          ')',
        /* Juliet */
        ')',
        '(', "item",
          '@', "jid", "juliet@example.net",
          '@', "name", "Juliet",
          '@', "subscription", "to",
          '(', "group",
            '$', "Friends",
          ')',
          '(', "group",
            '$', "Girlz",
          ')',
        ')',
      ')',
      NULL);

  wocky_porter_send (porter, reply);

  g_object_unref (reply);

  return TRUE;
}

static WockyRoster *
create_initial_roster (test_data_t *test)
{
  WockyRoster *roster;

  wocky_porter_register_handler_from_anyone (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_GET,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      fetch_roster_reply_cb, test, NULL);

  wocky_porter_start (test->sched_out);
  wocky_session_start (test->session_in);

  roster = wocky_roster_new (test->session_in);

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
static WockyBareContact *
create_nurse (void)
{
  const gchar *groups[] = { NULL };

  return g_object_new (WOCKY_TYPE_BARE_CONTACT,
      "jid", "nurse@example.net",
      "name", "Nurse",
      "subscription", WOCKY_ROSTER_SUBSCRIPTION_TYPE_NONE,
      "groups", groups,
      NULL);
}

static void
roster_added_cb (WockyRoster *roster,
    WockyBareContact *contact,
    test_data_t *test)
{
  WockyBareContact *nurse;
  GSList *contacts;

  /* Is that the right contact? */
  nurse = create_nurse ();
  g_assert (wocky_bare_contact_equal (contact, nurse));

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
  WockyStanza *reply;
  WockyStanzaType type;
  WockyStanzaSubType sub_type;

  reply = wocky_porter_send_iq_finish (WOCKY_PORTER (source), res, NULL);
  g_assert (reply != NULL);

  wocky_stanza_get_type_info (reply, &type, &sub_type);
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
  WockyStanza *iq;
  WockyNode *item;
  guint i;

  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_SET, NULL, NULL,
    '(', "query",
      ':', WOCKY_XMPP_NS_ROSTER,
      '(', "item",
        '*', &item,
      ')',
    ')',
    NULL);

  if (jid != NULL)
    wocky_node_set_attribute (item, "jid", jid);

  if (name != NULL)
    wocky_node_set_attribute (item, "name", name);

  if (subscription != NULL)
    wocky_node_set_attribute (item, "subscription", subscription);

  for (i = 0; groups != NULL && groups[i] != NULL; i++)
    {
      WockyNode *node;

      node = wocky_node_add_child (item, "group");
      wocky_node_set_content (node, groups[i]);
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
    WockyBareContact *contact,
    test_data_t *test)
{
  WockyBareContact *romeo;
  GSList *contacts;

  /* Is that the right contact? */
  romeo = create_romeo ();
  g_assert (wocky_bare_contact_equal (contact, romeo));

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

/* Test if WockyBareContact objects are properly upgraded */
static void
contact_notify_cb (WockyBareContact *contact,
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
  WockyBareContact *romeo, *contact;
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
  wocky_bare_contact_set_name (romeo, "Romeooo");
  g_assert (wocky_bare_contact_equal (contact, romeo));

  /* change subscription */
  test->outstanding++;
  send_roster_update (test, "romeo@example.net", "Romeooo",  "to",
      groups_init);
  test_wait_pending (test);

  /* Subscription has been changed */
  wocky_bare_contact_set_subscription (romeo, WOCKY_ROSTER_SUBSCRIPTION_TYPE_TO);
  g_assert (wocky_bare_contact_equal (contact, romeo));

  /* change groups */
  test->outstanding++;
  send_roster_update (test, "romeo@example.net", "Romeooo",  "to",
      groups);
  test_wait_pending (test);

  /* Groups have been changed */
  wocky_bare_contact_set_groups (romeo, (gchar **) groups);
  g_assert (wocky_bare_contact_equal (contact, romeo));

  g_object_unref (romeo);
  test_close_both_porters (test);
  g_object_unref (roster);
  teardown_test (test);
}

static void
ack_iq (WockyPorter *porter,
    WockyStanza *stanza)
{
  WockyStanza *reply;
  const gchar *id;

  id = wocky_node_get_attribute (wocky_stanza_get_top_node (stanza),
      "id");
  g_assert (id != NULL);

  reply = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_RESULT,
      NULL, NULL,
      '@', "id", id,
      NULL);

  wocky_porter_send (porter, reply);
  g_object_unref (reply);
}

/* Test adding a contact to the roster */
static void
check_edit_roster_stanza (WockyStanza *stanza,
    const gchar *jid,
    const gchar *name,
    const gchar *subscription,
    const gchar **groups)
{
  WockyStanzaType type;
  WockyStanzaSubType sub_type;
  WockyNode *node;
  GSList *l;
  guint i;
  GHashTable *expected_groups;

  wocky_stanza_get_type_info (stanza, &type, &sub_type);

  g_assert (type == WOCKY_STANZA_TYPE_IQ);
  g_assert (sub_type == WOCKY_STANZA_SUB_TYPE_SET);

  node = wocky_node_get_child_ns (wocky_stanza_get_top_node (stanza),
      "query", WOCKY_XMPP_NS_ROSTER);
  g_assert (node != NULL);

  node = wocky_node_get_child (node, "item");
  g_assert (node != NULL);
  g_assert (!wocky_strdiff (wocky_node_get_attribute (node, "jid"), jid));

  if (name != NULL)
    g_assert (!wocky_strdiff (wocky_node_get_attribute (node, "name"),
          name));
  else
    g_assert (wocky_node_get_attribute (node, "name") == NULL);

  if (subscription != NULL)
    g_assert (!wocky_strdiff (wocky_node_get_attribute (node,
            "subscription"), subscription));
  else
    g_assert (wocky_node_get_attribute (node, "subscription") == NULL);

  if (groups == NULL)
    {
      /* No group children */
      g_assert_cmpuint (g_slist_length (node->children), == , 0);
      return;
    }

  expected_groups = g_hash_table_new (g_str_hash, g_str_equal);
  for (i = 0; groups[i] != NULL; i++)
    {
      g_hash_table_insert (expected_groups, (gchar *) groups[i],
          GUINT_TO_POINTER (TRUE));
    }

  for (l = node->children; l != NULL; l = g_slist_next (l))
    {
      WockyNode *group = (WockyNode *) l->data;

      g_assert (!wocky_strdiff (group->name, "group"));

      g_assert (g_hash_table_remove (expected_groups, group->content));
    }

  g_assert (g_hash_table_size (expected_groups) == 0);
  g_hash_table_unref (expected_groups);
}

static void
check_add_contact_stanza (WockyStanza *stanza,
    const gchar *jid,
    const gchar *name,
    const gchar **groups)
{
  check_edit_roster_stanza (stanza, jid, name, NULL, groups);
}

static gboolean first_add = TRUE;

static gboolean
add_contact_send_iq_cb (WockyPorter *porter,
    WockyStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  const gchar *groups[] = { "Friends", "Badger", NULL };

  if (first_add)
    {
      check_add_contact_stanza (stanza, "mercutio@example.net", "Mercutio",
          groups);

      send_roster_update (test, "mercutio@example.net", "Mercutio", "none",
          groups);
      first_add = FALSE;
    }
  else
    {
      /* the second time the name is changed */
      check_add_contact_stanza (stanza, "mercutio@example.net", "Badger",
          groups);

      send_roster_update (test, "mercutio@example.net", "Badger", "none",
          groups);
    }

  /* Ack the IQ */
  ack_iq (porter, stanza);

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

static WockyBareContact *
create_mercutio (void)
{
  const gchar *groups[] = { "Friends", "Badger", NULL };

  return g_object_new (WOCKY_TYPE_BARE_CONTACT,
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
  WockyBareContact *mercutio, *contact;
  const gchar *groups[] = { "Friends", "Badger", NULL };

  test_open_both_connections (test);

  roster = create_initial_roster (test);

  wocky_porter_register_handler_from_anyone (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      add_contact_send_iq_cb, test,
      '(', "query",
        ':', WOCKY_XMPP_NS_ROSTER,
      ')',
      NULL);

  mercutio = create_mercutio ();
  /* Add the Mercutio to our roster */
  wocky_roster_add_contact_async (roster, "mercutio@example.net", "Mercutio",
      groups, NULL, contact_added_cb, test);

  test->outstanding += 2;
  test_wait_pending (test);

  /* check if the contact has been actually added */
  contact = wocky_roster_get_contact (roster, "mercutio@example.net");
  g_assert (wocky_bare_contact_equal (contact, mercutio));

  /* try to re-add the same contact. Operation succeeds immediately */
  wocky_roster_add_contact_async (roster, "mercutio@example.net", "Mercutio",
      groups, NULL, contact_added_cb, test);

  test->outstanding += 1;
  test_wait_pending (test);

  /* try to re-add the same contact but with a different name. The name is
   * changed */
  wocky_roster_add_contact_async (roster, "mercutio@example.net", "Badger",
      groups, NULL, contact_added_cb, test);

  test->outstanding += 2;
  test_wait_pending (test);

  /* check if the contact has been updated */
  contact = wocky_roster_get_contact (roster, "mercutio@example.net");
  wocky_bare_contact_set_name (mercutio, "Badger");
  g_assert (wocky_bare_contact_equal (contact, mercutio));

  test_close_both_porters (test);
  g_object_unref (mercutio);
  g_object_unref (roster);
  teardown_test (test);
}

static void
check_remove_contact_stanza (WockyStanza *stanza,
    const gchar *jid)
{
  check_edit_roster_stanza (stanza, jid, NULL, "remove", NULL);
}

/* Test removing a contact from the roster */
static gboolean
remove_contact_send_iq_cb (WockyPorter *porter,
    WockyStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  const gchar *no_group[] = { NULL };

  check_remove_contact_stanza (stanza, "romeo@example.net");

  send_roster_update (test, "romeo@example.net", NULL, "remove", no_group);

  /* Ack the IQ */
  ack_iq (porter, stanza);

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
  WockyBareContact *contact;

  test_open_both_connections (test);

  roster = create_initial_roster (test);

  contact = wocky_roster_get_contact (roster, "romeo@example.net");
  g_assert (contact != NULL);

  wocky_porter_register_handler_from_anyone (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      remove_contact_send_iq_cb, test,
      '(', "query",
        ':', WOCKY_XMPP_NS_ROSTER,
      ')',
      NULL);

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
    WockyStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyStanzaType type;
  WockyStanzaSubType sub_type;
  WockyNode *node;
  const gchar *group[] = { "Friends", NULL };

  /* Make sure stanza is as expected. */
  wocky_stanza_get_type_info (stanza, &type, &sub_type);

  g_assert (type == WOCKY_STANZA_TYPE_IQ);
  g_assert (sub_type == WOCKY_STANZA_SUB_TYPE_SET);

  node = wocky_node_get_child_ns (wocky_stanza_get_top_node (stanza),
      "query", WOCKY_XMPP_NS_ROSTER);
  g_assert (node != NULL);

  node = wocky_node_get_child (node, "item");
  g_assert (node != NULL);
  g_assert (!wocky_strdiff (wocky_node_get_attribute (node, "jid"),
      "romeo@example.net"));
  g_assert (!wocky_strdiff (wocky_node_get_attribute (node, "name"),
        "Badger"));
  g_assert (!wocky_strdiff (wocky_node_get_attribute (node,
        "subscription"), "both"));

  g_assert_cmpuint (g_slist_length (node->children), ==, 1);
  node = wocky_node_get_child (node, "group");
  g_assert (node != NULL);
  g_assert (!wocky_strdiff (node->content, "Friends"));

  send_roster_update (test, "romeo@example.net", "Badger", "both", group);

  /* Ack the IQ */
  ack_iq (porter, stanza);

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
  WockyBareContact *contact, *romeo, *mercutio;

  test_open_both_connections (test);

  roster = create_initial_roster (test);

  romeo = create_romeo ();

  contact = wocky_roster_get_contact (roster, "romeo@example.net");
  g_assert (contact != NULL);
  g_assert (wocky_bare_contact_equal (contact, romeo));

  wocky_porter_register_handler_from_anyone (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      change_name_send_iq_cb, test,
      '(', "query",
        ':', WOCKY_XMPP_NS_ROSTER,
      ')',
      NULL);

  wocky_roster_change_contact_name_async (roster, contact, "Badger", NULL,
      contact_name_changed_cb, test);

  test->outstanding += 2;
  test_wait_pending (test);

  /* check if the contact name has actually been change */
  wocky_bare_contact_set_name (romeo, "Badger");
  g_assert (wocky_bare_contact_equal (contact, romeo));

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
    WockyStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyStanzaType type;
  WockyStanzaSubType sub_type;
  WockyNode *node;
  const gchar *groups[] = { "Friends", "Badger", NULL };
  GSList *l;
  gboolean group_friend = FALSE, group_badger = FALSE;

  /* Make sure stanza is as expected. */
  wocky_stanza_get_type_info (stanza, &type, &sub_type);

  g_assert (type == WOCKY_STANZA_TYPE_IQ);
  g_assert (sub_type == WOCKY_STANZA_SUB_TYPE_SET);

  node = wocky_node_get_child_ns (wocky_stanza_get_top_node (stanza),
      "query", WOCKY_XMPP_NS_ROSTER);
  g_assert (node != NULL);

  node = wocky_node_get_child (node, "item");
  g_assert (node != NULL);
  g_assert (!wocky_strdiff (wocky_node_get_attribute (node, "jid"),
      "romeo@example.net"));
  g_assert (!wocky_strdiff (wocky_node_get_attribute (node, "name"),
        "Romeo"));
  g_assert (!wocky_strdiff (wocky_node_get_attribute (node,
        "subscription"), "both"));

  g_assert_cmpuint (g_slist_length (node->children), ==, 2);
  for (l = node->children; l != NULL; l = g_slist_next (l))
    {
      WockyNode *group = (WockyNode *) l->data;

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
  ack_iq (porter, stanza);

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
  WockyBareContact *contact, *romeo, *mercutio;

  test_open_both_connections (test);

  roster = create_initial_roster (test);

  romeo = create_romeo ();

  contact = wocky_roster_get_contact (roster, "romeo@example.net");
  g_assert (contact != NULL);
  g_assert (wocky_bare_contact_equal (contact, romeo));

  wocky_porter_register_handler_from_anyone (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      add_group_send_iq_cb, test,
      '(', "query",
        ':', WOCKY_XMPP_NS_ROSTER,
      ')',
      NULL);

  wocky_roster_contact_add_group_async (roster, contact, "Badger", NULL,
      contact_group_added_cb, test);

  test->outstanding += 2;
  test_wait_pending (test);

  /* check if the group has actually been added */
  wocky_bare_contact_add_group (romeo, "Badger");
  g_assert (wocky_bare_contact_equal (contact, romeo));

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
    WockyStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyStanzaType type;
  WockyStanzaSubType sub_type;
  WockyNode *node;
  const gchar *groups[] = { NULL };

  /* Make sure stanza is as expected. */
  wocky_stanza_get_type_info (stanza, &type, &sub_type);

  g_assert (type == WOCKY_STANZA_TYPE_IQ);
  g_assert (sub_type == WOCKY_STANZA_SUB_TYPE_SET);

  node = wocky_node_get_child_ns (wocky_stanza_get_top_node (stanza),
      "query", WOCKY_XMPP_NS_ROSTER);
  g_assert (node != NULL);

  node = wocky_node_get_child (node, "item");
  g_assert (node != NULL);
  g_assert (!wocky_strdiff (wocky_node_get_attribute (node, "jid"),
      "romeo@example.net"));
  g_assert (!wocky_strdiff (wocky_node_get_attribute (node, "name"),
        "Romeo"));
  g_assert (!wocky_strdiff (wocky_node_get_attribute (node,
        "subscription"), "both"));

  g_assert_cmpuint (g_slist_length (node->children), ==, 0);

  send_roster_update (test, "romeo@example.net", "Romeo", "both", groups);

  /* Ack the IQ */
  ack_iq (porter, stanza);

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
  WockyBareContact *contact, *romeo, *mercutio;

  test_open_both_connections (test);

  roster = create_initial_roster (test);

  romeo = create_romeo ();

  contact = wocky_roster_get_contact (roster, "romeo@example.net");
  g_assert (contact != NULL);
  g_assert (wocky_bare_contact_equal (contact, romeo));

  wocky_porter_register_handler_from_anyone (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      remove_group_send_iq_cb, test,
      '(', "query",
        ':', WOCKY_XMPP_NS_ROSTER,
      ')',
      NULL);

  wocky_roster_contact_remove_group_async (roster, contact, "Friends", NULL,
      contact_group_removed_cb, test);

  test->outstanding += 2;
  test_wait_pending (test);

  /* check if the group has actually been added */
  wocky_bare_contact_remove_group (romeo, "Friends");
  g_assert (wocky_bare_contact_equal (contact, romeo));

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

/* Remove a contact and re-add it before the remove operation is completed */
static WockyStanza *received_iq = NULL;

static gboolean
iq_set_cb (WockyPorter *porter,
    WockyStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;

  g_assert (received_iq == NULL);
  received_iq = g_object_ref (stanza);

  test->outstanding--;
  g_main_loop_quit (test->loop);
  return TRUE;
}

static void
test_remove_contact_re_add (void)
{
  WockyRoster *roster;
  test_data_t *test = setup_test ();
  WockyBareContact *contact, *romeo;
  const gchar *groups[] = { "Friends", NULL };
  const gchar *no_group[] = { NULL };

  test_open_both_connections (test);

  roster = create_initial_roster (test);

  contact = wocky_roster_get_contact (roster, "romeo@example.net");
  g_assert (contact != NULL);

  wocky_porter_register_handler_from_anyone (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      iq_set_cb, test,
      '(', "query",
        ':', WOCKY_XMPP_NS_ROSTER,
      ')',
      NULL);

  wocky_roster_remove_contact_async (roster, contact, NULL,
      contact_removed_cb, test);

  test->outstanding += 1;
  test_wait_pending (test);
  g_assert (received_iq != NULL);
  /* The IQ has been sent but the server didn't send the upgrade and the reply
   * yet.
   * Now try to re-add the contact */

  check_remove_contact_stanza (received_iq, "romeo@example.net");

  wocky_roster_add_contact_async (roster, "romeo@example.net", "Romeo",
      groups, NULL, contact_added_cb, test);

  /* Now the server send the roster upgrade and reply to the remove IQ */
  send_roster_update (test, "romeo@example.net", NULL, "remove", no_group);
  ack_iq (test->sched_out, received_iq);
  g_object_unref (received_iq);
  received_iq = NULL;

  /* Wait for:
   * - completion of the remove_contact operation
   * - server receives the "add contact" IQ
   */
  test->outstanding += 2;
  test_wait_pending (test);

  /* At this point, the contact has been removed */
  g_assert (wocky_roster_get_contact (roster, "romeo@example.net") == NULL);

  check_add_contact_stanza (received_iq, "romeo@example.net", "Romeo",
      groups);

  /* Now the server send the roster upgrade and reply to the add IQ */
  send_roster_update (test, "romeo@example.net", "Romeo", "none", groups);
  ack_iq (test->sched_out, received_iq);
  g_object_unref (received_iq);
  received_iq = NULL;

  /* Wait for completion of the add_contact operation */
  test->outstanding += 1;
  test_wait_pending (test);

  /* Check that the contact is back */
  contact = wocky_roster_get_contact (roster, "romeo@example.net");
  g_assert (contact != NULL);
  romeo = create_romeo ();
  wocky_bare_contact_set_subscription (romeo, WOCKY_ROSTER_SUBSCRIPTION_TYPE_NONE);
  g_assert (wocky_bare_contact_equal (contact, romeo));

  test_close_both_porters (test);
  g_object_unref (romeo);
  g_object_unref (roster);
  teardown_test (test);
}

/* Remove a contact and then try to edit it before the remove operation is
 * completed */
static void
test_remove_contact_edit (void)
{
  WockyRoster *roster;
  test_data_t *test = setup_test ();
  WockyBareContact *contact;
  const gchar *no_group[] = { NULL };

  test_open_both_connections (test);

  roster = create_initial_roster (test);

  contact = wocky_roster_get_contact (roster, "romeo@example.net");
  g_assert (contact != NULL);

  wocky_porter_register_handler_from_anyone (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      iq_set_cb, test,
      '(', "query",
        ':', WOCKY_XMPP_NS_ROSTER,
      ')',
      NULL);

  /* Keep a ref on the contact as the roster will release its ref when
   * removing it */
  g_object_ref (contact);

  wocky_roster_remove_contact_async (roster, contact, NULL,
      contact_removed_cb, test);

  test->outstanding += 1;
  test_wait_pending (test);
  g_assert (received_iq != NULL);
  /* The IQ has been sent but the server didn't send the upgrade and the reply
   * yet.
   * Now try to re-add the contact */

  check_remove_contact_stanza (received_iq, "romeo@example.net");

  /* Try to change the name of the contact we are removing */
  wocky_roster_change_contact_name_async (roster, contact, "Badger", NULL,
      contact_name_changed_not_in_roster_cb, test);

  /* Now the server send the roster upgrade and reply to the remove IQ */
  send_roster_update (test, "romeo@example.net", NULL, "remove", no_group);
  ack_iq (test->sched_out, received_iq);
  g_object_unref (received_iq);
  received_iq = NULL;

  /* Wait for:
   * - completion of the remove_contact operation
   * - completion (with an error) for the change_name operation
   */
  test->outstanding += 2;
  test_wait_pending (test);

  /* At this point, the contact has been removed */
  g_assert (wocky_roster_get_contact (roster, "romeo@example.net") == NULL);

  test_close_both_porters (test);
  g_object_unref (contact);
  g_object_unref (roster);
  teardown_test (test);
}

/* Queue some edit operations on the same contact */
static void
test_multi_contact_edit (void)
{
  WockyRoster *roster;
  test_data_t *test = setup_test ();
  WockyBareContact *contact, *juliet;
  const gchar *groups[] = { "Friends", "Girlz", NULL };
  const gchar *groups_changed[] = { "Friends", "School", NULL };

  test_open_both_connections (test);

  roster = create_initial_roster (test);

  contact = wocky_roster_get_contact (roster, "juliet@example.net");
  g_assert (contact != NULL);

  juliet = create_juliet ();

  wocky_porter_register_handler_from_anyone (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      iq_set_cb, test,
      '(', "query",
        ':', WOCKY_XMPP_NS_ROSTER,
      ')',
      NULL);

  /* Change's Romeo's name */
  wocky_roster_change_contact_name_async (roster, contact, "Badger", NULL,
      contact_name_changed_cb, test);

  test->outstanding += 1;
  test_wait_pending (test);
  g_assert (received_iq != NULL);
  /* The IQ has been sent but the server didn't send the upgrade and the reply
   * yet */

  g_assert (wocky_bare_contact_equal (contact, juliet));

  check_edit_roster_stanza (received_iq, "juliet@example.net", "Badger", "to",
      groups);

  /* Try to re-change the name of the contact */
  wocky_roster_change_contact_name_async (roster, contact, "Snake", NULL,
      contact_name_changed_cb, test);

  /* Add two groups */
  wocky_roster_contact_add_group_async (roster, contact, "Hacker", NULL,
      contact_group_added_cb, test);
  wocky_roster_contact_add_group_async (roster, contact, "School", NULL,
      contact_group_added_cb, test);

  /* Remove a group we just added */
  wocky_roster_contact_remove_group_async (roster, contact, "Hacker", NULL,
      contact_group_removed_cb, test);
  /* Remove the 2 default groups */
  wocky_roster_contact_remove_group_async (roster, contact, "Friends", NULL,
      contact_group_removed_cb, test);
  wocky_roster_contact_remove_group_async (roster, contact, "Girlz", NULL,
      contact_group_removed_cb, test);

  /* Re-add a removed group */
  wocky_roster_contact_add_group_async (roster, contact, "Friends", NULL,
      contact_group_added_cb, test);

  /* Now the server sends the roster upgrade and reply to the remove IQ */
  send_roster_update (test, "juliet@example.net", "Badger", "to", groups);
  ack_iq (test->sched_out, received_iq);
  g_object_unref (received_iq);
  received_iq = NULL;

  /* Wait for:
   * - completion of the first change_name operation
   * - server receives the second edit IQ
   */
  test->outstanding += 2;
  test_wait_pending (test);

  /* At this point, the contact has still his initial name */
  wocky_bare_contact_set_name (juliet, "Badger");
  g_assert (wocky_bare_contact_equal (contact, juliet));

  check_edit_roster_stanza (received_iq, "juliet@example.net", "Snake", "to",
      groups_changed);

  /* Now the server send the roster upgrade and reply to the add IQ */
  send_roster_update (test, "juliet@example.net", "Snake", "to",
      groups_changed);
  ack_iq (test->sched_out, received_iq);
  g_object_unref (received_iq);
  received_iq = NULL;

  /* Wait for completion of:
   * - the second change_name (Snake)
   * - the first add_group (Hacker)
   * - the second add_group (School)
   * - the first remove_group (Hacker)
   * - the second remove_group (Friends)
   * - the third remove_group (Girlz)
   * - the third add_group (Friends)
   */
  test->outstanding += 7;
  test_wait_pending (test);

  /* Check that the contact is back */
  wocky_bare_contact_set_name (juliet, "Snake");
  wocky_bare_contact_set_groups (juliet, (gchar **) groups_changed);
  g_assert (wocky_bare_contact_equal (contact, juliet));

  test_close_both_porters (test);
  g_object_unref (roster);
  g_object_unref (juliet);
  teardown_test (test);
}

/* test editing a contact and then remove it from the roster */
static void
test_edit_contact_remove (void)
{
  WockyRoster *roster;
  test_data_t *test = setup_test ();
  WockyBareContact *contact;
  const gchar *groups[] = { "Friends", NULL };

  test_open_both_connections (test);

  roster = create_initial_roster (test);

  contact = wocky_roster_get_contact (roster, "romeo@example.net");
  g_assert (contact != NULL);

  wocky_porter_register_handler_from_anyone (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      iq_set_cb, test,
      '(', "query",
        ':', WOCKY_XMPP_NS_ROSTER,
      ')',
      NULL);

  /* change contact's name */
  wocky_roster_change_contact_name_async (roster, contact, "Badger", NULL,
      contact_name_changed_cb, test);

  test->outstanding += 1;
  test_wait_pending (test);
  g_assert (received_iq != NULL);
  /* The IQ has been sent but the server didn't send the upgrade and the reply
   * yet.
   * Now try to re-add the contact */

  check_edit_roster_stanza (received_iq, "romeo@example.net", "Badger",
      "both", groups);

  /* remove the contact */
  wocky_roster_remove_contact_async (roster, contact, NULL,
      contact_removed_cb, test);

  /* Now the server send the roster upgrade and reply to the change name IQ */
  send_roster_update (test, "romeo@example.net", "Badger", "both", groups);
  ack_iq (test->sched_out, received_iq);
  g_object_unref (received_iq);
  received_iq = NULL;

  /* Wait for:
   * - completion of the change name operation
   * - server receives the "remove contact" IQ
   */
  test->outstanding += 2;
  test_wait_pending (test);

  /* At this point, the contact has not been removed yet */
  g_assert (wocky_roster_get_contact (roster, "romeo@example.net") != NULL);

  check_remove_contact_stanza (received_iq, "romeo@example.net");

  /* Now the server send the roster upgrade and reply to the remove IQ */
  send_roster_update (test, "romeo@example.net", NULL, "remove", NULL);
  ack_iq (test->sched_out, received_iq);
  g_object_unref (received_iq);
  received_iq = NULL;

  /* Wait for completion of the remove_contact operation */
  test->outstanding += 1;
  test_wait_pending (test);

  /* Contact is now removed */
  g_assert (wocky_roster_get_contact (roster, "romeo@example.net") == NULL);

  test_close_both_porters (test);
  g_object_unref (roster);
  teardown_test (test);
}

/* Change twice to the same name */
static void
test_change_name_twice (void)
{
  WockyRoster *roster;
  test_data_t *test = setup_test ();
  WockyBareContact *contact, *romeo;
  const gchar *groups[] = { "Friends", NULL };

  test_open_both_connections (test);

  roster = create_initial_roster (test);

  contact = wocky_roster_get_contact (roster, "romeo@example.net");
  g_assert (contact != NULL);

  romeo = create_romeo ();

  wocky_porter_register_handler_from_anyone (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      iq_set_cb, test,
      '(', "query",
        ':', WOCKY_XMPP_NS_ROSTER,
      ')',
      NULL);

  /* Change's Romeo's name */
  wocky_roster_change_contact_name_async (roster, contact, "Badger", NULL,
      contact_name_changed_cb, test);

  test->outstanding += 1;
  test_wait_pending (test);
  g_assert (received_iq != NULL);
  /* The IQ has been sent but the server didn't send the upgrade and the reply
   * yet */

  g_assert (wocky_bare_contact_equal (contact, romeo));

  check_edit_roster_stanza (received_iq, "romeo@example.net", "Badger", "both",
      groups);

  /* Try to reset the same name */
  wocky_roster_change_contact_name_async (roster, contact, "Badger", NULL,
      contact_name_changed_cb, test);

  /* Now the server sends the roster upgrade and reply to the first IQ */
  send_roster_update (test, "romeo@example.net", "Badger", "both", groups);
  ack_iq (test->sched_out, received_iq);
  g_object_unref (received_iq);
  received_iq = NULL;

  /* Wait for completion of the 2 change_name operations.
   * No IQ has been sent for the second as no change was needed. */
  test->outstanding += 2;
  test_wait_pending (test);

  g_assert (received_iq == NULL);

  /* Name has been changed */
  wocky_bare_contact_set_name (romeo, "Badger");
  g_assert (wocky_bare_contact_equal (contact, romeo));

  test_close_both_porters (test);
  g_object_unref (roster);
  g_object_unref (romeo);
  teardown_test (test);
}

/* Remove twice the same contact */
static void
test_remove_contact_twice (void)
{
  WockyRoster *roster;
  test_data_t *test = setup_test ();
  WockyBareContact *contact;
  const gchar *no_group[] = { NULL };

  test_open_both_connections (test);

  roster = create_initial_roster (test);

  contact = wocky_roster_get_contact (roster, "romeo@example.net");
  g_assert (contact != NULL);

  wocky_porter_register_handler_from_anyone (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      iq_set_cb, test,
      '(', "query",
        ':', WOCKY_XMPP_NS_ROSTER,
      ')',
      NULL);

  /* Keep a ref on the contact as the roster will release its ref when
   * removing it */
  g_object_ref (contact);

  wocky_roster_remove_contact_async (roster, contact, NULL,
      contact_removed_cb, test);

  test->outstanding += 1;
  test_wait_pending (test);
  g_assert (received_iq != NULL);
  /* The IQ has been sent but the server didn't send the upgrade and the reply
   * yet */

  check_remove_contact_stanza (received_iq, "romeo@example.net");

  /* Re-ask to remove the contact */
  wocky_roster_remove_contact_async (roster, contact, NULL,
      contact_removed_cb, test);

  /* Now the server send the roster upgrade and reply to the remove IQ */
  send_roster_update (test, "romeo@example.net", NULL, "remove", no_group);
  ack_iq (test->sched_out, received_iq);
  g_object_unref (received_iq);
  received_iq = NULL;

  /* Wait for completion of the 2 remove operations. No IQ has been sent for
   * the second as no change was needed. */
  test->outstanding += 2;
  test_wait_pending (test);

  g_assert (received_iq == NULL);

  /* At this point, the contact has been removed */
  g_assert (wocky_roster_get_contact (roster, "romeo@example.net") == NULL);

  test_close_both_porters (test);
  g_object_unref (contact);
  g_object_unref (roster);
  teardown_test (test);
}

/* Change name of a contact and try to remove and re-add it while change
 * operation is running */
static void
test_change_name_remove_add (void)
{
  WockyRoster *roster;
  test_data_t *test = setup_test ();
  WockyBareContact *contact, *romeo;
  const gchar *groups[] = { "Friends", NULL };

  test_open_both_connections (test);

  roster = create_initial_roster (test);

  contact = wocky_roster_get_contact (roster, "romeo@example.net");
  g_assert (contact != NULL);

  romeo = create_romeo ();

  wocky_porter_register_handler_from_anyone (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      iq_set_cb, test,
      '(', "query",
        ':', WOCKY_XMPP_NS_ROSTER,
      ')',
      NULL);

  /* Change's Romeo's name */
  wocky_roster_change_contact_name_async (roster, contact, "Badger", NULL,
      contact_name_changed_cb, test);

  test->outstanding += 1;
  test_wait_pending (test);
  g_assert (received_iq != NULL);
  /* The IQ has been sent but the server didn't send the upgrade and the reply
   * yet */

  g_assert (wocky_bare_contact_equal (contact, romeo));

  check_edit_roster_stanza (received_iq, "romeo@example.net", "Badger", "both",
      groups);

  /* Remove the contact */
  wocky_roster_remove_contact_async (roster, contact, NULL,
      contact_removed_cb, test);

  /* Change our mind and re-add it */
  wocky_roster_add_contact_async (roster, "romeo@example.net", "Badger",
      groups, NULL, contact_added_cb, test);

  /* Now the server sends the roster upgrade and reply to the first IQ */
  send_roster_update (test, "romeo@example.net", "Badger", "both", groups);
  ack_iq (test->sched_out, received_iq);
  g_object_unref (received_iq);
  received_iq = NULL;

  /* Wait for completion of:
   * - the change name operation
   * - the remove operation
   * - the add operation
   * No IQ has been sent for the add and remove as no change was needed. */
  test->outstanding += 3;
  test_wait_pending (test);

  g_assert (received_iq == NULL);

  /* Name has been changed */
  wocky_bare_contact_set_name (romeo, "Badger");
  g_assert (wocky_bare_contact_equal (contact, romeo));

  test_close_both_porters (test);
  g_object_unref (roster);
  g_object_unref (romeo);
  teardown_test (test);
}

/* add 2 groups to the same contact */
static void
test_add_two_groups (void)
{
  WockyRoster *roster;
  test_data_t *test = setup_test ();
  WockyBareContact *contact, *romeo;
  const gchar *groups2[] = { "Friends", "School", NULL };
  const gchar *groups3[] = { "Friends", "School", "Hackers", NULL };

  test_open_both_connections (test);

  roster = create_initial_roster (test);

  contact = wocky_roster_get_contact (roster, "romeo@example.net");
  g_assert (contact != NULL);

  romeo = create_romeo ();

  wocky_porter_register_handler_from_anyone (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      iq_set_cb, test,
      '(', "query",
        ':', WOCKY_XMPP_NS_ROSTER,
      ')',
      NULL);

  /* Add a group to Romeo */
  wocky_roster_contact_add_group_async (roster, contact, "School", NULL,
      contact_group_added_cb, test);

  test->outstanding += 1;
  test_wait_pending (test);
  g_assert (received_iq != NULL);
  /* The IQ has been sent but the server didn't send the upgrade and the reply
   * yet */

  g_assert (wocky_bare_contact_equal (contact, romeo));

  check_edit_roster_stanza (received_iq, "romeo@example.net", "Romeo", "both",
      groups2);

  /* Add another group */
  wocky_roster_contact_add_group_async (roster, contact, "Hackers", NULL,
      contact_group_added_cb, test);

  /* Now the server sends the roster upgrade and reply to the first IQ */
  send_roster_update (test, "romeo@example.net", "Romeo", "both", groups2);
  ack_iq (test->sched_out, received_iq);
  g_object_unref (received_iq);
  received_iq = NULL;

  /* Wait that:
   * - the first add_group operation is completed
   * - the server receives the IQ for the second add_group
   */
  test->outstanding += 2;
  test_wait_pending (test);

  wocky_bare_contact_set_groups (romeo, (gchar **) groups2);
  g_assert (wocky_bare_contact_equal (contact, romeo));

  check_edit_roster_stanza (received_iq, "romeo@example.net", "Romeo", "both",
      groups3);

  /* Server sends the roster upgrade and reply to the first IQ */
  send_roster_update (test, "romeo@example.net", "Romeo", "both", groups3);
  ack_iq (test->sched_out, received_iq);
  g_object_unref (received_iq);
  received_iq = NULL;

  /* Wait second add_group operation is completed */
  test->outstanding += 1;
  test_wait_pending (test);

  wocky_bare_contact_set_groups (romeo, (gchar **) groups3);
  g_assert (wocky_bare_contact_equal (contact, romeo));

  test_close_both_porters (test);
  g_object_unref (roster);
  g_object_unref (romeo);
  teardown_test (test);
}

/* remove 2 groups from the same contact */
static void
test_remove_two_groups (void)
{
  WockyRoster *roster;
  test_data_t *test = setup_test ();
  WockyBareContact *contact, *juliet;
  const gchar *groups[] = { "Friends", NULL };
  const gchar *no_group[] = { NULL };

  test_open_both_connections (test);

  roster = create_initial_roster (test);

  contact = wocky_roster_get_contact (roster, "juliet@example.net");
  g_assert (contact != NULL);

  juliet = create_juliet ();

  wocky_porter_register_handler_from_anyone (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      iq_set_cb, test,
      '(', "query",
        ':', WOCKY_XMPP_NS_ROSTER,
      ')',
      NULL);

  /* Remove a group from Juliet */
  wocky_roster_contact_remove_group_async (roster, contact, "Girlz", NULL,
      contact_group_removed_cb, test);

  test->outstanding += 1;
  test_wait_pending (test);
  g_assert (received_iq != NULL);
  /* The IQ has been sent but the server didn't send the upgrade and the reply
   * yet */

  g_assert (wocky_bare_contact_equal (contact, juliet));

  check_edit_roster_stanza (received_iq, "juliet@example.net", "Juliet", "to",
      groups);

  /* remove another group */
  wocky_roster_contact_remove_group_async (roster, contact, "Friends", NULL,
      contact_group_removed_cb, test);

  /* Now the server sends the roster upgrade and reply to the first IQ */
  send_roster_update (test, "juliet@example.net", "Juliet", "to", groups);
  ack_iq (test->sched_out, received_iq);
  g_object_unref (received_iq);
  received_iq = NULL;

  /* Wait that:
   * - the first remove_group operation is completed
   * - the server receives the IQ for the second remove_group
   */
  test->outstanding += 2;
  test_wait_pending (test);

  wocky_bare_contact_set_groups (juliet, (gchar **) groups);
  g_assert (wocky_bare_contact_equal (contact, juliet));

  check_edit_roster_stanza (received_iq, "juliet@example.net", "Juliet", "to",
      no_group);

  /* Server sends the roster upgrade and reply to the first IQ */
  send_roster_update (test, "juliet@example.net", "Juliet", "to", no_group);
  ack_iq (test->sched_out, received_iq);
  g_object_unref (received_iq);
  received_iq = NULL;

  /* Wait second remove_group operation is completed */
  test->outstanding += 1;
  test_wait_pending (test);

  wocky_bare_contact_set_groups (juliet, (gchar **) no_group);
  g_assert (wocky_bare_contact_equal (contact, juliet));

  test_close_both_porters (test);
  g_object_unref (roster);
  g_object_unref (juliet);
  teardown_test (test);
}

/* Try to add twice the same contact */
static void
test_add_contact_twice (void)
{
  WockyRoster *roster;
  test_data_t *test = setup_test ();
  WockyBareContact *mercutio, *contact;
  const gchar *groups[] = { "Friends", "Badger", NULL };

  test_open_both_connections (test);

  roster = create_initial_roster (test);

  wocky_porter_register_handler_from_anyone (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      iq_set_cb, test,
      '(', "query",
        ':', WOCKY_XMPP_NS_ROSTER,
      ')',
      NULL);

  mercutio = create_mercutio ();
  /* Add the Mercutio to our roster */
  wocky_roster_add_contact_async (roster, "mercutio@example.net", "Mercutio",
      groups, NULL, contact_added_cb, test);

  test->outstanding += 1;
  test_wait_pending (test);
  g_assert (received_iq != NULL);
  /* The IQ has been sent but the server didn't send the upgrade and the reply
   * yet */

  /* contact is not added yet */
  g_assert (wocky_roster_get_contact (roster, "mercutio@example.net") == NULL);

  check_add_contact_stanza (received_iq, "mercutio@example.net", "Mercutio",
      groups);

  /* Try to re-add the same contact */
  wocky_roster_add_contact_async (roster, "mercutio@example.net", "Mercutio",
      groups, NULL, contact_added_cb, test);

  /* Now the server sends the roster upgrade and reply to the first IQ */
  send_roster_update (test, "mercutio@example.net", "Mercutio", "none", groups);
  ack_iq (test->sched_out, received_iq);
  g_object_unref (received_iq);
  received_iq = NULL;

  /* Wait that the 2 add_contact operation are completed. No IQ is sent for
   * the second as nothing has changed. */
  test->outstanding += 2;
  test_wait_pending (test);

  /* check if the contact has been actually added */
  contact = wocky_roster_get_contact (roster, "mercutio@example.net");
  g_assert (wocky_bare_contact_equal (contact, mercutio));

  test_close_both_porters (test);
  g_object_unref (mercutio);
  g_object_unref (roster);
  teardown_test (test);
}

int
main (int argc, char **argv)
{
  int result;

  test_init (argc, argv);

  /* basic */
  g_test_add_func ("/xmpp-roster/instantiation", test_instantiation);
  /* roster fetching */
  g_test_add_func ("/xmpp-roster/fetch-roster-send-iq",
      test_fetch_roster_send_iq);
  g_test_add_func ("/xmpp-roster/fetch-roster-reply", test_fetch_roster_reply);
  /* receive upgrade from server */
  g_test_add_func ("/xmpp-roster/roster-upgrade-add", test_roster_upgrade_add);
  g_test_add_func ("/xmpp-roster/roster-upgrade-remove",
      test_roster_upgrade_remove);
  g_test_add_func ("/xmpp-roster/roster-upgrade-change",
      test_roster_upgrade_change);
  /* edit roster */
  g_test_add_func ("/xmpp-roster/roster-add-contact", test_roster_add_contact);
  g_test_add_func ("/xmpp-roster/roster-remove-contact",
      test_roster_remove_contact);
  g_test_add_func ("/xmpp-roster/roster-change-name", test_roster_change_name);
  g_test_add_func ("/xmpp-roster/contact-add-group", test_contact_add_group);
  g_test_add_func ("/xmpp-roster/contact-remove-group",
      test_contact_remove_group);
  /* start a edit operation while another edit operation is running */
  g_test_add_func ("/xmpp-roster/remove-contact-re-add",
      test_remove_contact_re_add);
  g_test_add_func ("/xmpp-roster/remove-contact-edit",
      test_remove_contact_edit);
  g_test_add_func ("/xmpp-roster/multi-contact-edit", test_multi_contact_edit);
  g_test_add_func ("/xmpp-roster/edit-contact-remove",
      test_edit_contact_remove);
  g_test_add_func ("/xmpp-roster/change-name-twice", test_change_name_twice);
  g_test_add_func ("/xmpp-roster/remove-contact-twice",
      test_remove_contact_twice);
  g_test_add_func ("/xmpp-roster/change-name-add-remove",
      test_change_name_remove_add);
  g_test_add_func ("/xmpp-roster/add-two-groups", test_add_two_groups);
  g_test_add_func ("/xmpp-roster/remove-two-groups", test_remove_two_groups);
  g_test_add_func ("/xmpp-roster/add-contact-twice", test_add_contact_twice);

  result = g_test_run ();
  test_deinit ();
  return result;
}
