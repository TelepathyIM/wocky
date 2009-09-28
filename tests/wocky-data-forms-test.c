#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <wocky/wocky-data-forms.h>
#include <wocky/wocky-xmpp-stanza.h>
#include <wocky/wocky-namespaces.h>
#include <wocky/wocky-utils.h>

#include "wocky-test-helper.h"

static void
test_new_from_form (void)
{
  WockyXmppStanza *stanza;
  WockyXmppNode *node;
  WockyDataForms *forms;

  stanza = wocky_xmpp_stanza_build (
      WOCKY_STANZA_TYPE_IQ,WOCKY_STANZA_SUB_TYPE_RESULT,
      NULL, NULL, WOCKY_STANZA_END);

  /* node doesn't contain a form */
  forms = wocky_data_forms_new_from_form (stanza->node);
  g_assert (forms == NULL);

  /* add 'x' node */
  node = wocky_xmpp_node_add_child_ns (stanza->node, "x", WOCKY_XMPP_NS_DATA);

  /* the x node doesn't have a 'type' attribute */
  forms = wocky_data_forms_new_from_form (stanza->node);
  g_assert (forms == NULL);

  /* set wrong type */
  wocky_xmpp_node_set_attribute (node, "type", "badger");

  forms = wocky_data_forms_new_from_form (stanza->node);
  g_assert (forms == NULL);

  /* set the right type */
  wocky_xmpp_node_set_attribute (node, "type", "form");

  forms = wocky_data_forms_new_from_form (stanza->node);
  g_assert (forms != NULL);

  g_object_unref (forms);
  g_object_unref (stanza);
}

static void
test_parse_form (void)
{
  WockyXmppStanza *stanza;
  WockyDataForms *forms;
  GSList *l;
  /* used to check that fields are stored in the right order */
  wocky_data_forms_field expected_types[] = {
    { WOCKY_DATA_FORMS_FIELD_TYPE_HIDDEN, "FORM_TYPE",
      NULL, NULL, FALSE, NULL, NULL, NULL },
    { WOCKY_DATA_FORMS_FIELD_TYPE_FIXED, NULL,
      NULL, NULL, FALSE, NULL, NULL, NULL },
    { WOCKY_DATA_FORMS_FIELD_TYPE_TEXT_SINGLE, "botname",
      "The name of your bot", NULL, FALSE, NULL, NULL, NULL },
    { WOCKY_DATA_FORMS_FIELD_TYPE_TEXT_MULTI, "description",
      "Helpful description of your bot", NULL, FALSE, NULL, NULL, NULL },
    { WOCKY_DATA_FORMS_FIELD_TYPE_BOOLEAN, "public",
      "Public bot?", NULL, TRUE, NULL, NULL, NULL },
    { WOCKY_DATA_FORMS_FIELD_TYPE_TEXT_PRIVATE, "password",
      "Password for special access", NULL, FALSE, NULL, NULL, NULL },
    { WOCKY_DATA_FORMS_FIELD_TYPE_LIST_MULTI, "features",
      "What features will the bot support?", NULL, FALSE, NULL, NULL, NULL },
    { WOCKY_DATA_FORMS_FIELD_TYPE_LIST_SINGLE, "maxsubs",
      "Maximum number of subscribers", NULL, FALSE, NULL, NULL, NULL },
    { WOCKY_DATA_FORMS_FIELD_TYPE_JID_MULTI, "invitelist",
      "People to invite", "Tell friends", FALSE, NULL, NULL, NULL },
    { WOCKY_DATA_FORMS_FIELD_TYPE_JID_SINGLE, "botjid",
      "The JID of the bot", NULL, FALSE, NULL, NULL, NULL },
  };
  guint i;
  wocky_data_forms_field *field;
  GStrv strv;
  wocky_data_forms_field_option features_options[] = {
    { "Contests", "contests" },
    { "News", "news" },
    { "Polls", "polls" },
    { "Reminders", "reminders" },
    { "Search", "search" },
  };
  wocky_data_forms_field_option maxsubs_options[] = {
    { "10", "10" },
    { "20", "20" },
    { "30", "30" },
    { "50", "50" },
    { "100", "100" },
    { "None", "none" },
  };

  /* This stanza is inspired from Example 2 of XEP-0004: Data Forms */
  stanza = wocky_xmpp_stanza_build (
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

  forms = wocky_data_forms_new_from_form (stanza->node);
  g_assert (forms != NULL);
  g_object_unref (stanza);

  g_assert (!wocky_strdiff (wocky_data_forms_get_title (forms),
        "My Title"));
  g_assert (!wocky_strdiff (wocky_data_forms_get_instructions (forms),
        "Badger"));

  g_assert_cmpuint (g_slist_length (forms->fields_list), ==, 10);
  for (l = forms->fields_list, i = 0; l != NULL; l = g_slist_next (l), i++)
    {
      field = l->data;

      g_assert (field != NULL);
      g_assert_cmpuint (field->type, ==, expected_types[i].type);
      g_assert (!wocky_strdiff (field->var, expected_types[i].var));
      g_assert (!wocky_strdiff (field->label, expected_types[i].label));
      g_assert (!wocky_strdiff (field->desc, expected_types[i].desc));
      g_assert (field->required == expected_types[i].required);
      g_assert (field->value == NULL);
    }

  g_assert_cmpuint (g_hash_table_size (forms->fields), ==, 9);

  /* check hidden field */
  field = g_hash_table_lookup (forms->fields, "FORM_TYPE");
  g_assert (field != NULL);
  g_assert (G_VALUE_TYPE (field->default_value) == G_TYPE_STRING);
  g_assert (!wocky_strdiff (g_value_get_string (field->default_value),
        "jabber:bot"));
  g_assert (field->options == NULL);

  /* check text-single field */
  field = g_hash_table_lookup (forms->fields, "botname");
  g_assert (field != NULL);
  g_assert (field->default_value == NULL);
  g_assert (field->options == NULL);

  /* check text-multi field */
  field = g_hash_table_lookup (forms->fields, "description");
  g_assert (field != NULL);
  g_assert (field->default_value == NULL);
  g_assert (field->options == NULL);

  /* check boolean field */
  field = g_hash_table_lookup (forms->fields, "public");
  g_assert (field != NULL);
  g_assert (field->default_value == NULL);
  g_assert (field->options == NULL);

  /* check text-private field */
  field = g_hash_table_lookup (forms->fields, "password");
  g_assert (field != NULL);
  g_assert (field->default_value == NULL);
  g_assert (field->options == NULL);

  /* check list-multi field */
  field = g_hash_table_lookup (forms->fields, "features");
  g_assert (field != NULL);
  g_assert (G_VALUE_TYPE (field->default_value) == G_TYPE_STRV);
  strv = g_value_get_boxed (field->default_value);
  g_assert_cmpuint (g_strv_length (strv), ==, 2);
  g_assert (!wocky_strdiff (strv[0], "news"));
  g_assert (!wocky_strdiff (strv[1], "search"));
  g_assert_cmpuint (g_slist_length (field->options), ==, 5);
  for (l = field->options, i = 0; l != NULL; l = g_slist_next (l), i++)
    {
      wocky_data_forms_field_option *option = l->data;

      g_assert (!wocky_strdiff (option->value, features_options[i].value));
      g_assert (!wocky_strdiff (option->label, features_options[i].label));
    }

  /* check list-single field */
  field = g_hash_table_lookup (forms->fields, "maxsubs");
  g_assert (field != NULL);
  g_assert (G_VALUE_TYPE (field->default_value) == G_TYPE_STRING);
  g_assert (!wocky_strdiff (g_value_get_string (field->default_value),
        "20"));
  g_assert_cmpuint (g_slist_length (field->options), ==, 6);
  for (l = field->options, i = 0; l != NULL; l = g_slist_next (l), i++)
    {
      wocky_data_forms_field_option *option = l->data;

      g_assert (!wocky_strdiff (option->value, maxsubs_options[i].value));
      g_assert (!wocky_strdiff (option->label, maxsubs_options[i].label));
    }

  /* check jid-multi field */
  field = g_hash_table_lookup (forms->fields, "invitelist");
  g_assert (field != NULL);
  g_assert (field->default_value == NULL);
  g_assert (field->options == NULL);

  /* check boolean field */
  field = g_hash_table_lookup (forms->fields, "botjid");
  g_assert (field != NULL);
  g_assert (field->default_value == NULL);
  g_assert (field->options == NULL);

  g_object_unref (forms);
}

int
main (int argc, char **argv)
{
  int result;

  test_init (argc, argv);

  g_test_add_func ("/data-forms/instantiation", test_new_from_form);
  g_test_add_func ("/data-forms/parse-form", test_parse_form);

  result = g_test_run ();
  test_deinit ();
  return result;
}
