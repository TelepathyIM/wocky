#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "wocky-pubsub-test-helpers.h"

#include <wocky/wocky.h>

void
test_pubsub_add_subscription_nodes (
    WockyNode *subscriptions_node,
    CannedSubscriptions *subs,
    gboolean include_node)
{
  CannedSubscriptions *l;

  for (l = subs; l != NULL && l->node != NULL; l++)
    {
      WockyNode *sub = wocky_node_add_child (subscriptions_node,
          "subscription");

      if (include_node)
        wocky_node_set_attribute (sub, "node", l->node);

      wocky_node_set_attribute (sub, "jid", l->jid);
      wocky_node_set_attribute (sub, "subscription", l->subscription);

      if (l->subid != NULL)
        wocky_node_set_attribute (sub, "subid", l->subid);
    }
}

void
test_pubsub_check_and_free_subscriptions (
    GList *subscriptions,
    const CannedSubscriptions *expected_subs)
{
  GList *l;
  guint i = 0;

  for (l = subscriptions;
       l != NULL;
       l = l->next, i++)
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
