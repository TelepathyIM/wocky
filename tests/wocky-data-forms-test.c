#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <wocky/wocky-data-forms.h>
#include <wocky/wocky-xmpp-stanza.h>
#include <wocky/wocky-namespaces.h>

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

int
main (int argc, char **argv)
{
  int result;

  test_init (argc, argv);

  g_test_add_func ("/data-forms/instantiation", test_new_from_form);

  result = g_test_run ();
  test_deinit ();
  return result;
}
