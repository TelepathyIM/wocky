#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <wocky/wocky-data-form.h>
#include <wocky/wocky-xmpp-stanza.h>
#include <wocky/wocky-namespaces.h>
#include <wocky/wocky-utils.h>

#include "wocky-test-helper.h"

static void
test_new_from_form (void)
{
  WockyXmppStanza *stanza;
  WockyXmppNode *node;
  WockyDataForm *form;
  GError *error = NULL;

  stanza = wocky_xmpp_stanza_build (
      WOCKY_STANZA_TYPE_IQ,WOCKY_STANZA_SUB_TYPE_RESULT,
      NULL, NULL, WOCKY_STANZA_END);

  /* node doesn't contain a form */
  form = wocky_data_form_new_from_form (stanza->node, &error);
  g_assert (form == NULL);
  g_assert_error (error, WOCKY_DATA_FORM_ERROR,
      WOCKY_DATA_FORM_ERROR_NOT_FORM);
  g_clear_error (&error);

  /* add 'x' node */
  node = wocky_xmpp_node_add_child_ns (stanza->node, "x", WOCKY_XMPP_NS_DATA);

  /* the x node doesn't have a 'type' attribute */
  form = wocky_data_form_new_from_form (stanza->node, &error);
  g_assert (form == NULL);
  g_assert_error (error, WOCKY_DATA_FORM_ERROR,
      WOCKY_DATA_FORM_ERROR_WRONG_TYPE);
  g_clear_error (&error);

  /* set wrong type */
  wocky_xmpp_node_set_attribute (node, "type", "badger");

  form = wocky_data_form_new_from_form (stanza->node, &error);
  g_assert (form == NULL);
  g_assert_error (error, WOCKY_DATA_FORM_ERROR,
      WOCKY_DATA_FORM_ERROR_WRONG_TYPE);
  g_clear_error (&error);

  /* set the right type */
  wocky_xmpp_node_set_attribute (node, "type", "form");

  form = wocky_data_form_new_from_form (stanza->node, &error);
  g_assert (form != NULL);
  g_assert_no_error (error);

  g_object_unref (form);
  g_object_unref (stanza);
}

static WockyXmppStanza *
create_bot_creation_form_stanza (void)
{
  /* This stanza is inspired from Example 2 of XEP-0004: Data Forms */
  return wocky_xmpp_stanza_build (
      WOCKY_STANZA_TYPE_IQ,WOCKY_STANZA_SUB_TYPE_RESULT,
      NULL, NULL,
      WOCKY_NODE, "x",
        WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_DATA,
        WOCKY_NODE_ATTRIBUTE, "type", "form",
        WOCKY_NODE, "title", WOCKY_NODE_TEXT, "My Title", WOCKY_NODE_END,
        WOCKY_NODE, "instructions", WOCKY_NODE_TEXT, "Badger", WOCKY_NODE_END,
        /* hidden field */
        WOCKY_NODE, "field",
          WOCKY_NODE_ATTRIBUTE, "type", "hidden",
          WOCKY_NODE_ATTRIBUTE, "var", "FORM_TYPE",
          WOCKY_NODE, "value", WOCKY_NODE_TEXT, "jabber:bot", WOCKY_NODE_END,
        WOCKY_NODE_END,
        /* fixed field */
        WOCKY_NODE, "field",
          WOCKY_NODE_ATTRIBUTE, "type", "fixed",
          WOCKY_NODE, "value", WOCKY_NODE_TEXT, "fixed value", WOCKY_NODE_END,
        WOCKY_NODE_END,
        /* text-single field */
        WOCKY_NODE, "field",
          WOCKY_NODE_ATTRIBUTE, "type", "text-single",
          WOCKY_NODE_ATTRIBUTE, "var", "botname",
          WOCKY_NODE_ATTRIBUTE, "label", "The name of your bot",
        WOCKY_NODE_END,
        /* field with no type. type='' is only a SHOULD; the default is
         * text-single */
        WOCKY_NODE, "field",
          WOCKY_NODE_ATTRIBUTE, "var", "pseudonym",
          WOCKY_NODE_ATTRIBUTE, "label", "Your bot's name at the weekend",
        WOCKY_NODE_END,
        /* text-multi field */
        WOCKY_NODE, "field",
          WOCKY_NODE_ATTRIBUTE, "type", "text-multi",
          WOCKY_NODE_ATTRIBUTE, "var", "description",
          WOCKY_NODE_ATTRIBUTE, "label", "Helpful description of your bot",
        WOCKY_NODE_END,
        /* boolean field */
        WOCKY_NODE, "field",
          WOCKY_NODE_ATTRIBUTE, "type", "boolean",
          WOCKY_NODE_ATTRIBUTE, "var", "public",
          WOCKY_NODE_ATTRIBUTE, "label", "Public bot?",
          WOCKY_NODE, "required", WOCKY_NODE_END,
          WOCKY_NODE, "value", WOCKY_NODE_TEXT, "false", WOCKY_NODE_END,
        WOCKY_NODE_END,
        /* text-private field */
        WOCKY_NODE, "field",
          WOCKY_NODE_ATTRIBUTE, "type", "text-private",
          WOCKY_NODE_ATTRIBUTE, "var", "password",
          WOCKY_NODE_ATTRIBUTE, "label", "Password for special access",
        WOCKY_NODE_END,
        /* list-multi field */
        WOCKY_NODE, "field",
          WOCKY_NODE_ATTRIBUTE, "type", "list-multi",
          WOCKY_NODE_ATTRIBUTE, "var", "features",
          WOCKY_NODE_ATTRIBUTE, "label", "What features will the bot support?",
          WOCKY_NODE, "option",
            WOCKY_NODE_ATTRIBUTE, "label", "Contests",
            WOCKY_NODE, "value", WOCKY_NODE_TEXT, "contests", WOCKY_NODE_END,
          WOCKY_NODE_END,
          WOCKY_NODE, "option",
            WOCKY_NODE_ATTRIBUTE, "label", "News",
            WOCKY_NODE, "value", WOCKY_NODE_TEXT, "news", WOCKY_NODE_END,
          WOCKY_NODE_END,
          WOCKY_NODE, "option",
            WOCKY_NODE_ATTRIBUTE, "label", "Polls",
            WOCKY_NODE, "value", WOCKY_NODE_TEXT, "polls", WOCKY_NODE_END,
          WOCKY_NODE_END,
          WOCKY_NODE, "option",
            WOCKY_NODE_ATTRIBUTE, "label", "Reminders",
            WOCKY_NODE, "value", WOCKY_NODE_TEXT, "reminders", WOCKY_NODE_END,
          WOCKY_NODE_END,
          WOCKY_NODE, "option",
            WOCKY_NODE_ATTRIBUTE, "label", "Search",
            WOCKY_NODE, "value", WOCKY_NODE_TEXT, "search", WOCKY_NODE_END,
          WOCKY_NODE_END,
          WOCKY_NODE, "value", WOCKY_NODE_TEXT, "news", WOCKY_NODE_END,
          WOCKY_NODE, "value", WOCKY_NODE_TEXT, "search", WOCKY_NODE_END,
        WOCKY_NODE_END,
        /* list-single field */
        WOCKY_NODE, "field",
          WOCKY_NODE_ATTRIBUTE, "type", "list-single",
          WOCKY_NODE_ATTRIBUTE, "var", "maxsubs",
          WOCKY_NODE_ATTRIBUTE, "label", "Maximum number of subscribers",
          WOCKY_NODE, "value", WOCKY_NODE_TEXT, "20", WOCKY_NODE_END,
          WOCKY_NODE, "option",
            WOCKY_NODE_ATTRIBUTE, "label", "10",
            WOCKY_NODE, "value", WOCKY_NODE_TEXT, "10", WOCKY_NODE_END,
          WOCKY_NODE_END,
          WOCKY_NODE, "option",
            WOCKY_NODE_ATTRIBUTE, "label", "20",
            WOCKY_NODE, "value", WOCKY_NODE_TEXT, "20", WOCKY_NODE_END,
          WOCKY_NODE_END,
          WOCKY_NODE, "option",
            WOCKY_NODE_ATTRIBUTE, "label", "30",
            WOCKY_NODE, "value", WOCKY_NODE_TEXT, "30", WOCKY_NODE_END,
          WOCKY_NODE_END,
          WOCKY_NODE, "option",
            WOCKY_NODE_ATTRIBUTE, "label", "50",
            WOCKY_NODE, "value", WOCKY_NODE_TEXT, "50", WOCKY_NODE_END,
          WOCKY_NODE_END,
          WOCKY_NODE, "option",
            WOCKY_NODE_ATTRIBUTE, "label", "100",
            WOCKY_NODE, "value", WOCKY_NODE_TEXT, "100", WOCKY_NODE_END,
          WOCKY_NODE_END,
          WOCKY_NODE, "option",
            WOCKY_NODE_ATTRIBUTE, "label", "None",
            WOCKY_NODE, "value", WOCKY_NODE_TEXT, "none", WOCKY_NODE_END,
          WOCKY_NODE_END,
        WOCKY_NODE_END,
        /* jid-multi field */
        WOCKY_NODE, "field",
          WOCKY_NODE_ATTRIBUTE, "type", "jid-multi",
          WOCKY_NODE_ATTRIBUTE, "var", "invitelist",
          WOCKY_NODE_ATTRIBUTE, "label", "People to invite",
          WOCKY_NODE, "desc", WOCKY_NODE_TEXT, "Tell friends", WOCKY_NODE_END,
        WOCKY_NODE_END,
        /* jid-single field */
        WOCKY_NODE, "field",
          WOCKY_NODE_ATTRIBUTE, "type", "jid-single",
          WOCKY_NODE_ATTRIBUTE, "var", "botjid",
          WOCKY_NODE_ATTRIBUTE, "label", "The JID of the bot",
        WOCKY_NODE_END,
      WOCKY_NODE_END, WOCKY_STANZA_END);
}

static void
test_parse_form (void)
{
  WockyXmppStanza *stanza;
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
  form = wocky_data_form_new_from_form (stanza->node, NULL);
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
test_submit (void)
{
  WockyXmppStanza *stanza;
  WockyDataForm *form;
  WockyXmppNode *x;
  GSList *l;
  const gchar *description[] = { "Badger", "Mushroom", "Snake", NULL };
  const gchar *features[] = { "news", "search", NULL };
  const gchar *invitees[] = { "juliet@example.org", "romeo@example.org", NULL };
  gboolean set_succeeded;

  stanza = create_bot_creation_form_stanza ();
  form = wocky_data_form_new_from_form (stanza->node, NULL);
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

  stanza = wocky_xmpp_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
      NULL, NULL, WOCKY_STANZA_END);
  wocky_data_form_submit (form, stanza->node);

  x = wocky_xmpp_node_get_child_ns (stanza->node, "x", WOCKY_XMPP_NS_DATA);
  g_assert (x != NULL);
  g_assert_cmpstr (wocky_xmpp_node_get_attribute (x, "type"), ==, "submit");

  for (l = x->children; l != NULL; l = g_slist_next (l))
    {
      WockyXmppNode *v, *node = l->data;
      const gchar *var, *type, *value = NULL;

      g_assert_cmpstr (node->name, ==, "field");
      var = wocky_xmpp_node_get_attribute (node, "var");
      g_assert (var != NULL);
      type = wocky_xmpp_node_get_attribute (node, "type");

      v = wocky_xmpp_node_get_child (node, "value");
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
              WockyXmppNode *tmp = m->data;

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
              WockyXmppNode *tmp = m->data;

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
              WockyXmppNode *tmp = m->data;

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
  WockyXmppStanza *stanza, *expected;

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

  stanza = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_SET, NULL, NULL,
      WOCKY_STANZA_END);
  wocky_data_form_submit (form, stanza->node);

  expected = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_SET, NULL, NULL,
      WOCKY_NODE, "x",
        WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_DATA,
        WOCKY_NODE_ATTRIBUTE, "type", "submit",
        WOCKY_NODE, "field",
          WOCKY_NODE_ATTRIBUTE, "type", "hidden",
          WOCKY_NODE_ATTRIBUTE, "var", "FORM_TYPE",
          WOCKY_NODE, "value",
            WOCKY_NODE_TEXT, "http://example.com/band-info",
          WOCKY_NODE_END,
        WOCKY_NODE_END,
        WOCKY_NODE, "field",
          WOCKY_NODE_ATTRIBUTE, "var", "band-name",
          WOCKY_NODE, "value",
            WOCKY_NODE_TEXT, "The XX",
          WOCKY_NODE_END,
        WOCKY_NODE_END,
        WOCKY_NODE, "field",
          WOCKY_NODE_ATTRIBUTE, "var", "band-members",
          WOCKY_NODE, "value",
            WOCKY_NODE_TEXT, "Romy",
          WOCKY_NODE_END,
          WOCKY_NODE, "value",
            WOCKY_NODE_TEXT, "Oliver",
          WOCKY_NODE_END,
          WOCKY_NODE, "value",
            WOCKY_NODE_TEXT, "Jamie",
          WOCKY_NODE_END,
        WOCKY_NODE_END,
        WOCKY_NODE, "field",
          WOCKY_NODE_ATTRIBUTE, "var", "is-meh",
          WOCKY_NODE, "value",
            WOCKY_NODE_TEXT, "1",
          WOCKY_NODE_END,
        WOCKY_NODE_END,
      WOCKY_NODE_END,
      WOCKY_STANZA_END);

  test_assert_stanzas_equal (expected, stanza);

  g_object_unref (expected);
  g_object_unref (stanza);
  g_object_unref (form);
}

static WockyXmppStanza *
create_search_form_stanza (void)
{
  /* This stanza is inspired from Example 6 of XEP-0004: Data Forms */
  return wocky_xmpp_stanza_build (
      WOCKY_STANZA_TYPE_IQ,WOCKY_STANZA_SUB_TYPE_RESULT,
      NULL, NULL,
      WOCKY_NODE, "x",
        WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_DATA,
        WOCKY_NODE_ATTRIBUTE, "type", "form",
        WOCKY_NODE, "title", WOCKY_NODE_TEXT, "My Title", WOCKY_NODE_END,
        WOCKY_NODE, "instructions", WOCKY_NODE_TEXT, "Badger", WOCKY_NODE_END,
        WOCKY_NODE, "field",
          WOCKY_NODE_ATTRIBUTE, "type", "text-single",
          WOCKY_NODE_ATTRIBUTE, "var", "search_request",
        WOCKY_NODE_END,
      WOCKY_NODE_END, WOCKY_STANZA_END);
}

static void
test_parse_multi_result (void)
{
  WockyXmppStanza *stanza;
  WockyDataForm *form;
  GSList *l;
  gboolean item1 = FALSE, item2 = FALSE;

  stanza = create_search_form_stanza ();
  form = wocky_data_form_new_from_form (stanza->node, NULL);
  g_assert (form != NULL);
  g_object_unref (stanza);

  /* create the result stanza */
  stanza = wocky_xmpp_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_RESULT,
      NULL, NULL,
      WOCKY_NODE, "x",
        WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_DATA,
        WOCKY_NODE_ATTRIBUTE, "type", "result",
        WOCKY_NODE, "title", WOCKY_NODE_TEXT, "Search result", WOCKY_NODE_END,
        WOCKY_NODE, "reported",
          WOCKY_NODE, "field",
          WOCKY_NODE_ATTRIBUTE, "var", "name",
          WOCKY_NODE_ATTRIBUTE, "type", "text-single",
          WOCKY_NODE_END,
          WOCKY_NODE, "field",
          WOCKY_NODE_ATTRIBUTE, "var", "url",
          WOCKY_NODE_ATTRIBUTE, "type", "text-single",
          WOCKY_NODE_END,
        WOCKY_NODE_END,
        /* first item */
        WOCKY_NODE, "item",
          WOCKY_NODE, "field",
            WOCKY_NODE_ATTRIBUTE, "var", "name",
            WOCKY_NODE, "value", WOCKY_NODE_TEXT, "name1", WOCKY_NODE_END,
          WOCKY_NODE_END,
          WOCKY_NODE, "field",
            WOCKY_NODE_ATTRIBUTE, "var", "url",
            WOCKY_NODE, "value", WOCKY_NODE_TEXT, "url1", WOCKY_NODE_END,
          WOCKY_NODE_END,
        WOCKY_NODE_END,
        /* second item */
        WOCKY_NODE, "item",
          WOCKY_NODE, "field",
            WOCKY_NODE_ATTRIBUTE, "var", "name",
            WOCKY_NODE, "value", WOCKY_NODE_TEXT, "name2", WOCKY_NODE_END,
          WOCKY_NODE_END,
          WOCKY_NODE, "field",
            WOCKY_NODE_ATTRIBUTE, "var", "url",
            WOCKY_NODE, "value", WOCKY_NODE_TEXT, "url2", WOCKY_NODE_END,
          WOCKY_NODE_END,
        WOCKY_NODE_END,
      WOCKY_NODE_END,
      WOCKY_STANZA_END);

  g_assert (wocky_data_form_parse_result (form, stanza->node, NULL));
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
  WockyXmppStanza *stanza;
  WockyDataForm *form;
  GSList *result, *l;
  gboolean form_type = FALSE, botname = FALSE;

  stanza = create_bot_creation_form_stanza ();
  form = wocky_data_form_new_from_form (stanza->node, NULL);
  g_assert (form != NULL);
  g_object_unref (stanza);

  /* create the result stanza */
  stanza = wocky_xmpp_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_RESULT,
      NULL, NULL,
      WOCKY_NODE, "x",
        WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_DATA,
        WOCKY_NODE_ATTRIBUTE, "type", "result",
        /* hidden field */
        WOCKY_NODE, "field",
          WOCKY_NODE_ATTRIBUTE, "type", "hidden",
          WOCKY_NODE_ATTRIBUTE, "var", "FORM_TYPE",
          WOCKY_NODE, "value", WOCKY_NODE_TEXT, "jabber:bot", WOCKY_NODE_END,
        WOCKY_NODE_END,
        /* text-single field */
        WOCKY_NODE, "field",
          WOCKY_NODE_ATTRIBUTE, "type", "text-single",
          WOCKY_NODE_ATTRIBUTE, "var", "botname",
          WOCKY_NODE, "value", WOCKY_NODE_TEXT, "The Bot", WOCKY_NODE_END,
        WOCKY_NODE_END,
      WOCKY_STANZA_END);

  g_assert (wocky_data_form_parse_result (form, stanza->node, NULL));
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
  g_test_add_func ("/data-form/submit", test_submit);
  g_test_add_func ("/data-form/submit-blindly", test_submit_blindly);
  g_test_add_func ("/data-form/parse-multi-result", test_parse_multi_result);
  g_test_add_func ("/data-form/parse-single-result", test_parse_single_result);

  result = g_test_run ();
  test_deinit ();
  return result;
}
