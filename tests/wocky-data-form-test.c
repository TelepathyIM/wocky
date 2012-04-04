#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <wocky/wocky.h>

#include "wocky-test-helper.h"

static void
test_new_from_form (void)
{
  WockyStanza *stanza;
  WockyNode *node;
  WockyDataForm *form;
  GError *error = NULL;

  stanza = wocky_stanza_build (
      WOCKY_STANZA_TYPE_IQ,WOCKY_STANZA_SUB_TYPE_RESULT,
      NULL, NULL, NULL);

  /* node doesn't contain a form */
  form = wocky_data_form_new_from_form (wocky_stanza_get_top_node (stanza),
      &error);
  g_assert (form == NULL);
  g_assert_error (error, WOCKY_DATA_FORM_ERROR,
      WOCKY_DATA_FORM_ERROR_NOT_FORM);
  g_clear_error (&error);

  /* add 'x' node */
  node = wocky_node_add_child_ns (wocky_stanza_get_top_node (stanza),
      "x", WOCKY_XMPP_NS_DATA);

  /* the x node doesn't have a 'type' attribute */
  form = wocky_data_form_new_from_form (wocky_stanza_get_top_node (stanza),
      &error);
  g_assert (form == NULL);
  g_assert_error (error, WOCKY_DATA_FORM_ERROR,
      WOCKY_DATA_FORM_ERROR_WRONG_TYPE);
  g_clear_error (&error);

  /* set wrong type */
  wocky_node_set_attribute (node, "type", "badger");

  form = wocky_data_form_new_from_form (wocky_stanza_get_top_node (stanza),
      &error);
  g_assert (form == NULL);
  g_assert_error (error, WOCKY_DATA_FORM_ERROR,
      WOCKY_DATA_FORM_ERROR_WRONG_TYPE);
  g_clear_error (&error);

  /* set the right type */
  wocky_node_set_attribute (node, "type", "form");

  form = wocky_data_form_new_from_form (wocky_stanza_get_top_node (stanza),
      &error);
  g_assert (form != NULL);
  g_assert_no_error (error);

  g_object_unref (form);
  g_object_unref (stanza);
}

static WockyStanza *
create_bot_creation_form_stanza (void)
{
  /* This stanza is inspired from Example 2 of XEP-0004: Data Forms */
  return wocky_stanza_build (
      WOCKY_STANZA_TYPE_IQ,WOCKY_STANZA_SUB_TYPE_RESULT,
      NULL, NULL,
      '(', "x",
        ':', WOCKY_XMPP_NS_DATA,
        '@', "type", "form",
        '(', "title", '$', "My Title", ')',
        '(', "instructions", '$', "Badger", ')',
        /* hidden field */
        '(', "field",
          '@', "type", "hidden",
          '@', "var", "FORM_TYPE",
          '(', "value", '$', "jabber:bot", ')',
        ')',
        /* fixed field */
        '(', "field",
          '@', "type", "fixed",
          '(', "value", '$', "fixed value", ')',
        ')',
        /* text-single field */
        '(', "field",
          '@', "type", "text-single",
          '@', "var", "botname",
          '@', "label", "The name of your bot",
        ')',
        /* field with no type. type='' is only a SHOULD; the default is
         * text-single */
        '(', "field",
          '@', "var", "pseudonym",
          '@', "label", "Your bot's name at the weekend",
        ')',
        /* text-multi field */
        '(', "field",
          '@', "type", "text-multi",
          '@', "var", "description",
          '@', "label", "Helpful description of your bot",
        ')',
        /* boolean field */
        '(', "field",
          '@', "type", "boolean",
          '@', "var", "public",
          '@', "label", "Public bot?",
          '(', "required", ')',
          '(', "value", '$', "false", ')',
        ')',
        /* text-private field */
        '(', "field",
          '@', "type", "text-private",
          '@', "var", "password",
          '@', "label", "Password for special access",
        ')',
        /* list-multi field */
        '(', "field",
          '@', "type", "list-multi",
          '@', "var", "features",
          '@', "label", "What features will the bot support?",
          '(', "option",
            '@', "label", "Contests",
            '(', "value", '$', "contests", ')',
          ')',
          '(', "option",
            '@', "label", "News",
            '(', "value", '$', "news", ')',
          ')',
          '(', "option",
            '@', "label", "Polls",
            '(', "value", '$', "polls", ')',
          ')',
          '(', "option",
            '@', "label", "Reminders",
            '(', "value", '$', "reminders", ')',
          ')',
          '(', "option",
            '@', "label", "Search",
            '(', "value", '$', "search", ')',
          ')',
          '(', "value", '$', "news", ')',
          '(', "value", '$', "search", ')',
        ')',
        /* list-single field */
        '(', "field",
          '@', "type", "list-single",
          '@', "var", "maxsubs",
          '@', "label", "Maximum number of subscribers",
          '(', "value", '$', "20", ')',
          '(', "option",
            '@', "label", "10",
            '(', "value", '$', "10", ')',
          ')',
          '(', "option",
            '@', "label", "20",
            '(', "value", '$', "20", ')',
          ')',
          '(', "option",
            '@', "label", "30",
            '(', "value", '$', "30", ')',
          ')',
          '(', "option",
            '@', "label", "50",
            '(', "value", '$', "50", ')',
          ')',
          '(', "option",
            '@', "label", "100",
            '(', "value", '$', "100", ')',
          ')',
          '(', "option",
            '@', "label", "None",
            '(', "value", '$', "none", ')',
          ')',
        ')',
        /* jid-multi field */
        '(', "field",
          '@', "type", "jid-multi",
          '@', "var", "invitelist",
          '@', "label", "People to invite",
          '(', "desc", '$', "Tell friends", ')',
        ')',
        /* jid-single field */
        '(', "field",
          '@', "type", "jid-single",
          '@', "var", "botjid",
          '@', "label", "The JID of the bot",
        ')',
      ')', NULL);
}

static void
test_parse_form (void)
{
  WockyStanza *stanza;
  WockyDataForm *form;
  GSList *l;
  /* used to check that fields are stored in the right order */
  WockyDataFormField expected_types[] = {
    { WOCKY_DATA_FORM_FIELD_TYPE_HIDDEN, "FORM_TYPE",
      NULL, NULL, FALSE, NULL, NULL, NULL },
    { WOCKY_DATA_FORM_FIELD_TYPE_FIXED, NULL,
      NULL, NULL, FALSE, NULL, NULL, NULL },
    { WOCKY_DATA_FORM_FIELD_TYPE_TEXT_SINGLE, "botname",
      "The name of your bot", NULL, FALSE, NULL, NULL, NULL },
    { WOCKY_DATA_FORM_FIELD_TYPE_TEXT_SINGLE, "pseudonym",
      "Your bot's name at the weekend", NULL, FALSE, NULL, NULL, NULL },
    { WOCKY_DATA_FORM_FIELD_TYPE_TEXT_MULTI, "description",
      "Helpful description of your bot", NULL, FALSE, NULL, NULL, NULL },
    { WOCKY_DATA_FORM_FIELD_TYPE_BOOLEAN, "public",
      "Public bot?", NULL, TRUE, NULL, NULL, NULL },
    { WOCKY_DATA_FORM_FIELD_TYPE_TEXT_PRIVATE, "password",
      "Password for special access", NULL, FALSE, NULL, NULL, NULL },
    { WOCKY_DATA_FORM_FIELD_TYPE_LIST_MULTI, "features",
      "What features will the bot support?", NULL, FALSE, NULL, NULL, NULL },
    { WOCKY_DATA_FORM_FIELD_TYPE_LIST_SINGLE, "maxsubs",
      "Maximum number of subscribers", NULL, FALSE, NULL, NULL, NULL },
    { WOCKY_DATA_FORM_FIELD_TYPE_JID_MULTI, "invitelist",
      "People to invite", "Tell friends", FALSE, NULL, NULL, NULL },
    { WOCKY_DATA_FORM_FIELD_TYPE_JID_SINGLE, "botjid",
      "The JID of the bot", NULL, FALSE, NULL, NULL, NULL },
  };
  guint i;
  WockyDataFormField *field;
  GStrv strv;
  WockyDataFormFieldOption features_options[] = {
    { "Contests", "contests" },
    { "News", "news" },
    { "Polls", "polls" },
    { "Reminders", "reminders" },
    { "Search", "search" },
  };
  WockyDataFormFieldOption maxsubs_options[] = {
    { "10", "10" },
    { "20", "20" },
    { "30", "30" },
    { "50", "50" },
    { "100", "100" },
    { "None", "none" },
  };

  stanza = create_bot_creation_form_stanza ();
  form = wocky_data_form_new_from_form (wocky_stanza_get_top_node (stanza),
      NULL);
  g_assert (form != NULL);
  g_object_unref (stanza);

  g_assert_cmpstr (wocky_data_form_get_title (form), ==, "My Title");
  g_assert_cmpstr (wocky_data_form_get_instructions (form), ==, "Badger");

  g_assert_cmpuint (g_slist_length (form->fields_list), ==, 11);
  for (l = form->fields_list, i = 0; l != NULL; l = g_slist_next (l), i++)
    {
      field = l->data;

      g_assert (field != NULL);
      g_assert_cmpuint (field->type, ==, expected_types[i].type);
      g_assert_cmpstr (field->var, ==, expected_types[i].var);
      g_assert_cmpstr (field->label, ==, expected_types[i].label);
      g_assert_cmpstr (field->desc, ==, expected_types[i].desc);
      g_assert (field->required == expected_types[i].required);
      g_assert (field->value == NULL);
    }

  g_assert_cmpuint (g_hash_table_size (form->fields), ==, 10);

  /* check hidden field */
  field = g_hash_table_lookup (form->fields, "FORM_TYPE");
  g_assert (field != NULL);
  g_assert (G_VALUE_TYPE (field->default_value) == G_TYPE_STRING);
  g_assert_cmpstr (g_value_get_string (field->default_value), ==, "jabber:bot");
  g_assert (field->options == NULL);

  /* check text-single field */
  field = g_hash_table_lookup (form->fields, "botname");
  g_assert (field != NULL);
  g_assert (field->default_value == NULL);
  g_assert (field->options == NULL);

  /* check implicitly text-single field */
  field = g_hash_table_lookup (form->fields, "pseudonym");
  g_assert (field != NULL);
  g_assert (field->default_value == NULL);
  g_assert (field->options == NULL);

  /* check text-multi field */
  field = g_hash_table_lookup (form->fields, "description");
  g_assert (field != NULL);
  g_assert (field->default_value == NULL);
  g_assert (field->options == NULL);

  /* check boolean field */
  field = g_hash_table_lookup (form->fields, "public");
  g_assert (field != NULL);
  g_assert (G_VALUE_TYPE (field->default_value) == G_TYPE_BOOLEAN);
  g_assert (!g_value_get_boolean (field->default_value));
  g_assert (field->options == NULL);

  /* check text-private field */
  field = g_hash_table_lookup (form->fields, "password");
  g_assert (field != NULL);
  g_assert (field->default_value == NULL);
  g_assert (field->options == NULL);

  /* check list-multi field */
  field = g_hash_table_lookup (form->fields, "features");
  g_assert (field != NULL);
  g_assert (G_VALUE_TYPE (field->default_value) == G_TYPE_STRV);
  strv = g_value_get_boxed (field->default_value);
  g_assert_cmpuint (g_strv_length (strv), ==, 2);
  g_assert_cmpstr (strv[0], ==, "news");
  g_assert_cmpstr (strv[1], ==, "search");
  g_assert_cmpuint (g_slist_length (field->options), ==, 5);
  for (l = field->options, i = 0; l != NULL; l = g_slist_next (l), i++)
    {
      WockyDataFormFieldOption *option = l->data;

      g_assert_cmpstr (option->value, ==, features_options[i].value);
      g_assert_cmpstr (option->label, ==, features_options[i].label);
    }

  /* check list-single field */
  field = g_hash_table_lookup (form->fields, "maxsubs");
  g_assert (field != NULL);
  g_assert (G_VALUE_TYPE (field->default_value) == G_TYPE_STRING);
  g_assert_cmpstr (g_value_get_string (field->default_value), ==, "20");
  g_assert_cmpuint (g_slist_length (field->options), ==, 6);
  for (l = field->options, i = 0; l != NULL; l = g_slist_next (l), i++)
    {
      WockyDataFormFieldOption *option = l->data;

      g_assert_cmpstr (option->value, ==, maxsubs_options[i].value);
      g_assert_cmpstr (option->label, ==, maxsubs_options[i].label);
    }

  /* check jid-multi field */
  field = g_hash_table_lookup (form->fields, "invitelist");
  g_assert (field != NULL);
  g_assert (field->default_value == NULL);
  g_assert (field->options == NULL);

  /* check boolean field */
  field = g_hash_table_lookup (form->fields, "botjid");
  g_assert (field != NULL);
  g_assert (field->default_value == NULL);
  g_assert (field->options == NULL);

  g_object_unref (form);
}

static void
test_raw_value_contents (void)
{
  WockyStanza *stanza;
  WockyDataForm *form;
  const gchar *description[] = { "Badger", "Mushroom", "Snake", NULL };
  gboolean set_succeeded;

  /* Create fields without WockyNode to trigger this bug:
   * https://bugs.freedesktop.org/show_bug.cgi?id=43584
   */
  form = g_object_new (WOCKY_TYPE_DATA_FORM, NULL);
  g_assert (form != NULL);

  /* set text-single field */
  set_succeeded = wocky_data_form_set_string (form, "botname",
      "The Jabber Google Bot", FALSE);
  g_assert (!set_succeeded);
  set_succeeded = wocky_data_form_set_string (form, "botname",
      "The Jabber Google Bot", TRUE);
  g_assert (set_succeeded);

  /* set text-multi field */
  set_succeeded = wocky_data_form_set_strv (form, "description",
      description, FALSE);
  g_assert (!set_succeeded);
  set_succeeded = wocky_data_form_set_strv (form, "description",
      description, TRUE);
  g_assert (set_succeeded);

  /* set boolean field */
  set_succeeded = wocky_data_form_set_boolean (form, "public", FALSE, FALSE);
  g_assert (!set_succeeded);
  set_succeeded = wocky_data_form_set_boolean (form, "public", FALSE, TRUE);
  g_assert (set_succeeded);

  stanza = wocky_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
      NULL, NULL, NULL);

  wocky_data_form_add_to_node (form, wocky_stanza_get_top_node (stanza));

  g_object_unref (stanza);
  g_object_unref (form);
}

static void
test_submit (void)
{
  WockyStanza *stanza;
  WockyDataForm *form;
  WockyNode *x;
  GSList *l;
  const gchar *description[] = { "Badger", "Mushroom", "Snake", NULL };
  const gchar *features[] = { "news", "search", NULL };
  const gchar *invitees[] = { "juliet@example.org",
      "romeo@example.org", NULL };
  gboolean set_succeeded;

  stanza = create_bot_creation_form_stanza ();
  form = wocky_data_form_new_from_form (wocky_stanza_get_top_node (stanza),
      NULL);
  g_assert (form != NULL);
  g_object_unref (stanza);

  /* set text-single field */
  set_succeeded = wocky_data_form_set_string (form, "botname",
      "The Jabber Google Bot", FALSE);
  g_assert (set_succeeded);

  /* set text-multi field */
  set_succeeded = wocky_data_form_set_strv (form, "description",
      description, FALSE);
  g_assert (set_succeeded);

  /* set boolean field */
  set_succeeded = wocky_data_form_set_boolean (form, "public", FALSE, FALSE);
  g_assert (set_succeeded);

  /* set text-private field */
  set_succeeded = wocky_data_form_set_string (form, "password",
      "S3cr1t", FALSE);
  g_assert (set_succeeded);

  /* set list-multi field */
  set_succeeded = wocky_data_form_set_strv (form, "features",
      features, FALSE);
  g_assert (set_succeeded);

  /* set list-single field */
  set_succeeded = wocky_data_form_set_string (form, "maxsubs", "20", FALSE);
  g_assert (set_succeeded);

  /* set jid-multi field */
  set_succeeded = wocky_data_form_set_strv (form, "invitelist", invitees,
      FALSE);
  g_assert (set_succeeded);

  /* set jid-single field */
  set_succeeded = wocky_data_form_set_string (form, "botjid",
      "bobot@example.org", FALSE);
  g_assert (set_succeeded);

  stanza = wocky_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
      NULL, NULL, NULL);
  wocky_data_form_submit (form, wocky_stanza_get_top_node (stanza));

  x = wocky_node_get_child_ns (wocky_stanza_get_top_node (stanza),
      "x", WOCKY_XMPP_NS_DATA);
  g_assert (x != NULL);
  g_assert_cmpstr (wocky_node_get_attribute (x, "type"), ==, "submit");

  for (l = x->children; l != NULL; l = g_slist_next (l))
    {
      WockyNode *v, *node = l->data;
      const gchar *var, *type, *value = NULL;

      g_assert_cmpstr (node->name, ==, "field");
      var = wocky_node_get_attribute (node, "var");
      g_assert (var != NULL);
      type = wocky_node_get_attribute (node, "type");

      v = wocky_node_get_child (node, "value");
      if (v != NULL)
        value = v->content;

      if (!wocky_strdiff (var, "FORM_TYPE"))
        {
          g_assert_cmpstr (type, ==, "hidden");
          g_assert_cmpstr (value, ==, "jabber:bot");
        }
      else if (!wocky_strdiff (var, "botname"))
        {
          g_assert_cmpstr (type, ==, "text-single");
          g_assert_cmpstr (value, ==, "The Jabber Google Bot");
        }
      else if (!wocky_strdiff (var, "description"))
        {
          GSList *m;
          gboolean badger = FALSE, mushroom = FALSE, snake = FALSE;

          g_assert_cmpstr (type, ==, "text-multi");
          for (m = node->children; m != NULL; m = g_slist_next (m))
            {
              WockyNode *tmp = m->data;

              g_assert_cmpstr (tmp->name, ==, "value");
              if (!wocky_strdiff (tmp->content, "Badger"))
                badger = TRUE;
              else if (!wocky_strdiff (tmp->content, "Mushroom"))
                mushroom = TRUE;
              else if (!wocky_strdiff (tmp->content, "Snake"))
                snake = TRUE;
              else
                g_assert_not_reached ();
            }
          g_assert (badger && mushroom && snake);
        }
      else if (!wocky_strdiff (var, "public"))
        {
          g_assert_cmpstr (type, ==, "boolean");
          g_assert_cmpstr (value, ==, "0");
        }
      else if (!wocky_strdiff (var, "password"))
        {
          g_assert_cmpstr (type, ==, "text-private");
          g_assert_cmpstr (value, ==, "S3cr1t");
        }
      else if (!wocky_strdiff (var, "features"))
        {
          GSList *m;
          gboolean news = FALSE, search = FALSE;

          g_assert_cmpstr (type, ==, "list-multi");
          for (m = node->children; m != NULL; m = g_slist_next (m))
            {
              WockyNode *tmp = m->data;

              g_assert_cmpstr (tmp->name, ==, "value");
              if (!wocky_strdiff (tmp->content, "news"))
                news = TRUE;
              else if (!wocky_strdiff (tmp->content, "search"))
                search = TRUE;
              else
                g_assert_not_reached ();
            }
          g_assert (news && search);
        }
      else if (!wocky_strdiff (var, "maxsubs"))
        {
          g_assert_cmpstr (type, ==, "list-single");
          g_assert_cmpstr (value, ==, "20");
        }
      else if (!wocky_strdiff (var, "invitelist"))
        {
          GSList *m;
          gboolean juliet = FALSE, romeo = FALSE;

          g_assert_cmpstr (type, ==, "jid-multi");
          for (m = node->children; m != NULL; m = g_slist_next (m))
            {
              WockyNode *tmp = m->data;

              g_assert_cmpstr (tmp->name, ==, "value");
              if (!wocky_strdiff (tmp->content, "juliet@example.org"))
                juliet = TRUE;
              else if (!wocky_strdiff (tmp->content, "romeo@example.org"))
                romeo = TRUE;
              else
                g_assert_not_reached ();
            }
          g_assert (juliet && romeo);
        }
      else if (!wocky_strdiff (var, "botjid"))
        {
          g_assert_cmpstr (type, ==, "jid-single");
          g_assert_cmpstr (value, ==, "bobot@example.org");
        }
      else
        g_assert_not_reached ();
    }

  g_object_unref (stanza);
  g_object_unref (form);
}

/* Test creating and submitting a form response blindly, without first asking
 * the server for the form fields.
 */
static void
test_submit_blindly (void)
{
  WockyDataForm *form = g_object_new (WOCKY_TYPE_DATA_FORM, NULL);
  const gchar * const the_xx[] = { "Romy", "Oliver", "Jamie", NULL };
  gboolean succeeded;
  WockyStanza *stanza, *expected;

  /* We didn't actually parse a form, so it doesn't have any pre-defined
   * fields. Thus, the setters should all fail if we don't tell them to create
   * the fields if missing.
   */
  succeeded = wocky_data_form_set_string (form, "band-name", "The XX", FALSE);
  g_assert (!succeeded);

  succeeded = wocky_data_form_set_strv (form, "band-members", the_xx, FALSE);
  g_assert (!succeeded);

  succeeded = wocky_data_form_set_boolean (form, "is-meh", TRUE, FALSE);
  g_assert (!succeeded);

  g_assert (form->fields_list == NULL);
  g_assert_cmpuint (0, ==, g_hash_table_size (form->fields));

  /* Since the form doesn't have a FORM_TYPE yet, we should be able to set it.
   */
  succeeded = wocky_data_form_set_type (form, "http://example.com/band-info");
  g_assert (succeeded);

  /* But now that it does have one, we shouldn't be able to change it. */
  succeeded = wocky_data_form_set_type (form, "stoats");
  g_assert (!succeeded);

  /* If we forcibly create the fields we care about, setting them should
   * succeed, and they should show up when we submit the form!
   */
  succeeded = wocky_data_form_set_string (form, "band-name", "The XX", TRUE);
  g_assert (succeeded);

  succeeded = wocky_data_form_set_strv (form, "band-members", the_xx, TRUE);
  g_assert (succeeded);

  succeeded = wocky_data_form_set_boolean (form, "is-meh", TRUE, TRUE);
  g_assert (succeeded);

  stanza = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_SET, NULL, NULL,
      NULL);
  wocky_data_form_submit (form, wocky_stanza_get_top_node (stanza));

  expected = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_SET, NULL, NULL,
      '(', "x",
        ':', WOCKY_XMPP_NS_DATA,
        '@', "type", "submit",
        '(', "field",
          '@', "type", "hidden",
          '@', "var", "FORM_TYPE",
          '(', "value",
            '$', "http://example.com/band-info",
          ')',
        ')',
        '(', "field",
          '@', "var", "band-name",
          '(', "value",
            '$', "The XX",
          ')',
        ')',
        '(', "field",
          '@', "var", "band-members",
          '(', "value",
            '$', "Romy",
          ')',
          '(', "value",
            '$', "Oliver",
          ')',
          '(', "value",
            '$', "Jamie",
          ')',
        ')',
        '(', "field",
          '@', "var", "is-meh",
          '(', "value",
            '$', "1",
          ')',
        ')',
      ')',
      NULL);

  test_assert_stanzas_equal_no_id (expected, stanza);

  g_object_unref (expected);
  g_object_unref (stanza);
  g_object_unref (form);
}

static WockyStanza *
create_search_form_stanza (void)
{
  /* This stanza is inspired from Example 6 of XEP-0004: Data Forms */
  return wocky_stanza_build (
      WOCKY_STANZA_TYPE_IQ,WOCKY_STANZA_SUB_TYPE_RESULT,
      NULL, NULL,
      '(', "x",
        ':', WOCKY_XMPP_NS_DATA,
        '@', "type", "form",
        '(', "title", '$', "My Title", ')',
        '(', "instructions", '$', "Badger", ')',
        '(', "field",
          '@', "type", "text-single",
          '@', "var", "search_request",
        ')',
      ')', NULL);
}

static void
test_parse_multi_result (void)
{
  WockyStanza *stanza;
  WockyDataForm *form;
  GSList *l;
  gboolean item1 = FALSE, item2 = FALSE;

  stanza = create_search_form_stanza ();
  form = wocky_data_form_new_from_form (wocky_stanza_get_top_node (stanza),
      NULL);
  g_assert (form != NULL);
  g_object_unref (stanza);

  /* create the result stanza */
  stanza = wocky_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_RESULT,
      NULL, NULL,
      '(', "x",
        ':', WOCKY_XMPP_NS_DATA,
        '@', "type", "result",
        '(', "title", '$', "Search result", ')',
        '(', "reported",
          '(', "field",
          '@', "var", "name",
          '@', "type", "text-single",
          ')',
          '(', "field",
          '@', "var", "url",
          '@', "type", "text-single",
          ')',
        ')',
        /* first item */
        '(', "item",
          '(', "field",
            '@', "var", "name",
            '(', "value", '$', "name1", ')',
          ')',
          '(', "field",
            '@', "var", "url",
            '(', "value", '$', "url1", ')',
          ')',
        ')',
        /* second item */
        '(', "item",
          '(', "field",
            '@', "var", "name",
            '(', "value", '$', "name2", ')',
          ')',
          '(', "field",
            '@', "var", "url",
            '(', "value", '$', "url2", ')',
          ')',
        ')',
      ')',
      NULL);

  g_assert (wocky_data_form_parse_result (form,
      wocky_stanza_get_top_node (stanza), NULL));
  g_object_unref (stanza);

  g_assert_cmpuint (g_slist_length (form->results), ==, 2);

  for (l = form->results; l != NULL; l = g_slist_next (l))
    {
      GSList *result = l->data, *m;
      gboolean name = FALSE, url = FALSE;

      for (m = result; m != NULL; m = g_slist_next (m))
        {
          WockyDataFormField *field = m->data;

          if (!wocky_strdiff (field->var, "name"))
            {
              if (!wocky_strdiff (g_value_get_string (field->value), "name1"))
                item1 = TRUE;
              else if (!wocky_strdiff (g_value_get_string (field->value),
                    "name2"))
                item2 = TRUE;
              else
                g_assert_not_reached ();

              name = TRUE;
            }
          else if (!wocky_strdiff (field->var, "url"))
            {
              if (item2)
                g_assert_cmpstr (g_value_get_string (field->value), ==, "url2");
              else if (item1)
                g_assert_cmpstr (g_value_get_string (field->value), ==, "url1");
              else
                g_assert_not_reached ();

              url = TRUE;
            }
          else
            g_assert_not_reached ();
        }
      g_assert (name && url);
    }
  g_assert (item1 && item2);

  g_object_unref (form);
}

static void
test_parse_single_result (void)
{
  WockyStanza *stanza;
  WockyDataForm *form;
  GSList *result, *l;
  gboolean form_type = FALSE, botname = FALSE;

  stanza = create_bot_creation_form_stanza ();
  form = wocky_data_form_new_from_form (wocky_stanza_get_top_node (stanza),
      NULL);
  g_assert (form != NULL);
  g_object_unref (stanza);

  /* create the result stanza */
  stanza = wocky_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_RESULT,
      NULL, NULL,
      '(', "x",
        ':', WOCKY_XMPP_NS_DATA,
        '@', "type", "result",
        /* hidden field */
        '(', "field",
          '@', "type", "hidden",
          '@', "var", "FORM_TYPE",
          '(', "value", '$', "jabber:bot", ')',
        ')',
        /* text-single field */
        '(', "field",
          '@', "type", "text-single",
          '@', "var", "botname",
          '(', "value", '$', "The Bot", ')',
        ')',
      ')',
      NULL);

  g_assert (wocky_data_form_parse_result (form,
      wocky_stanza_get_top_node (stanza), NULL));
  g_object_unref (stanza);

  g_assert_cmpuint (g_slist_length (form->results), ==, 1);
  result = form->results->data;
  g_assert_cmpuint (g_slist_length (result), ==, 2);

  for (l = result; l != NULL; l = g_slist_next (l))
    {
      WockyDataFormField *field = l->data;

      if (!wocky_strdiff (field->var, "FORM_TYPE"))
        {
          g_assert_cmpstr (g_value_get_string (field->value), ==, "jabber:bot");
          g_assert (field->type == WOCKY_DATA_FORM_FIELD_TYPE_HIDDEN);
          form_type = TRUE;
        }
      else if (!wocky_strdiff (field->var, "botname"))
        {
          g_assert_cmpstr (g_value_get_string (field->value), ==, "The Bot");
          g_assert (field->type == WOCKY_DATA_FORM_FIELD_TYPE_TEXT_SINGLE);
          botname = TRUE;
        }
      else
        g_assert_not_reached ();
    }
  g_assert (form_type && botname);

  g_object_unref (form);
}

int
main (int argc, char **argv)
{
  int result;

  test_init (argc, argv);

  g_test_add_func ("/data-form/instantiation", test_new_from_form);
  g_test_add_func ("/data-form/parse-form", test_parse_form);
  g_test_add_func ("/data-form/raw_value_contents", test_raw_value_contents);
  g_test_add_func ("/data-form/submit", test_submit);
  g_test_add_func ("/data-form/submit-blindly", test_submit_blindly);
  g_test_add_func ("/data-form/parse-multi-result", test_parse_multi_result);
  g_test_add_func ("/data-form/parse-single-result", test_parse_single_result);

  result = g_test_run ();
  test_deinit ();
  return result;
}
