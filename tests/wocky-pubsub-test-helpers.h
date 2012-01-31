#ifndef WOCKY_PUBSUB_TEST_HELPERS_H
#define WOCKY_PUBSUB_TEST_HELPERS_H

#include <glib.h>
#include <wocky/wocky.h>

typedef struct {
    const gchar *node;
    const gchar *jid;
    const gchar *subscription;
    WockyPubsubSubscriptionState state;
    const gchar *subid;
} CannedSubscriptions;

void test_pubsub_add_subscription_nodes (
    WockyNode *subscriptions_node,
    CannedSubscriptions *subs,
    gboolean include_node);

void test_pubsub_check_and_free_subscriptions (
    GList *subscriptions,
    const CannedSubscriptions *expected_subs);

#endif
