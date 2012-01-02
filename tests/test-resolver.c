/*
 * test-resolver.c - Source for TestResolver
 * Copyright © 2009 Collabora Ltd.
 * @author Vivek Dasmohapatra <vivek@collabora.co.uk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/* this code largely culled from gunixresolver.c in glib and modified to
 * make a dummy resolver we can insert duff records in on the fly */

/* examples:
 * GResolver *original;
 * GResolver *kludged;
 * original = g_resolver_get_default ();
 * kludged = g_object_new (TEST_TYPE_RESOLVER, "real-resolver", original, NULL);
 * g_resolver_set_default (kludged);
 * test_resolver_add_SRV (TEST_RESOLVER (kludged),
 *     "xmpp-client", "tcp", "jabber.earth.li", "localhost", 1337);
 * test_resolver_add_A (TEST_RESOLVER (kludged), "localhost", "127.0.1.1");
 */

#include <stdio.h>
#include <glib.h>

#ifdef G_OS_WIN32
#include <windows.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include "test-resolver.h"

#ifdef G_LOG_DOMAIN
#undef G_LOG_DOMAIN
#endif
#define G_LOG_DOMAIN "test-resolver"

enum
{
  PROP_REAL_RESOLVER = 1,
};

typedef struct _fake_host { char *key; char *addr; } fake_host;
typedef struct _fake_serv { char *key; GSrvTarget *srv; } fake_serv;

G_DEFINE_TYPE (TestResolver, test_resolver, G_TYPE_RESOLVER);

/* ************************************************************************* */

static gchar *
_service_rrname (const char *service,
                 const char *protocol,
                 const char *domain)
{
  gchar *rrname, *ascii_domain = NULL;

  if (g_hostname_is_non_ascii (domain))
    domain = ascii_domain = g_hostname_to_ascii (domain);

  rrname = g_strdup_printf ("_%s._%s.%s", service, protocol, domain);

  g_free (ascii_domain);
  return rrname;
}

static GList *
find_fake_services (TestResolver *tr, const char *name)
{
  GList *fake = NULL;
  GList *rval = NULL;

  for (fake = tr->fake_SRV; fake; fake = fake->next)
    {
      fake_serv *entry = fake->data;
      if (entry && !g_strcmp0 (entry->key, name))
          rval = g_list_append (rval, g_srv_target_copy (entry->srv));
    }
  return rval;
}

static GList *
find_fake_hosts (TestResolver *tr, const char *name)
{
  GList *fake = NULL;
  GList *rval = NULL;

  for (fake = tr->fake_A; fake; fake = fake->next)
    {
      fake_host *entry = fake->data;
      if (entry && !g_strcmp0 (entry->key, name))
        rval =
          g_list_append (rval, g_inet_address_new_from_string (entry->addr));
    }
  return rval;
}

static GList *
srv_target_list_copy (GList *addr)
{
  GList *copy = NULL;
  GList *l;

  for (l = addr; l != NULL; l = l->next)
    copy = g_list_prepend (copy, g_srv_target_copy (l->data));

  return g_list_reverse (copy);
}

static void
srv_target_list_free (GList *addr)
{
  g_list_foreach (addr, (GFunc) g_srv_target_free, NULL);
  g_list_free (addr);
}

static GList *
object_list_copy (GList *objs)
{
  g_list_foreach (objs, (GFunc) g_object_ref, NULL);
  return g_list_copy (objs);
}

static void
object_list_free (GList *objs)
{
  g_list_foreach (objs, (GFunc) g_object_unref, NULL);
  g_list_free (objs);
}

static void
lookup_service_async (GResolver *resolver,
    const char *rr,
    GCancellable *cancellable,
    GAsyncReadyCallback  cb,
    gpointer data)
{
  TestResolver *tr = TEST_RESOLVER (resolver);
  GList *addr = find_fake_services (tr, rr);
  GObject *source = G_OBJECT (resolver);
  GSimpleAsyncResult *res = NULL;
#ifdef DEBUG_FAKEDNS
  GList *x;
#endif

  if (addr != NULL)
    {
#ifdef DEBUG_FAKEDNS
      for (x = addr; x; x = x->next)
        g_debug ("FAKE SRV: addr: %s; port: %d; prio: %d; weight: %d;\n",
            g_srv_target_get_hostname ((GSrvTarget *) x->data),
            g_srv_target_get_port ((GSrvTarget *) x->data),
            g_srv_target_get_priority ((GSrvTarget *) x->data),
            g_srv_target_get_weight ((GSrvTarget *) x->data));
#endif
      res = g_simple_async_result_new (source, cb, data, lookup_service_async);
    }
  else
    {
      res = g_simple_async_result_new_error (source, cb, data,
          G_RESOLVER_ERROR, G_RESOLVER_ERROR_NOT_FOUND,
          "No fake SRV record registered");
    }

  g_simple_async_result_set_op_res_gpointer (res, addr,
      (GDestroyNotify) srv_target_list_free);
  g_simple_async_result_complete (res);
  g_object_unref (res);
}

static GList *
lookup_service_finish (GResolver *resolver,
                       GAsyncResult *result,
                       GError **error)
{
  GList *res = NULL;
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);

  if (g_simple_async_result_propagate_error (simple, error))
    return NULL;

  res = g_simple_async_result_get_op_res_gpointer (simple);
  return srv_target_list_copy (res);
}

static void
lookup_by_name_async (GResolver *resolver,
                      const gchar *hostname,
                      GCancellable *cancellable,
                      GAsyncReadyCallback  cb,
                      gpointer data)
{
  TestResolver *tr = TEST_RESOLVER (resolver);
  GList *addr = find_fake_hosts (tr, hostname);
  GObject *source = G_OBJECT (resolver);
  GSimpleAsyncResult *res = NULL;
#ifdef DEBUG_FAKEDNS
  GList *x;
  char a[32];
#endif

  if (addr != NULL)
    {
#ifdef DEBUG_FAKEDNS
      for (x = addr; x; x = x->next)
        g_debug ("FAKE HOST: addr: %s;\n",
            inet_ntop (AF_INET,
              g_inet_address_to_bytes (x->data), a, sizeof (a)));
#endif
      res = g_simple_async_result_new (source, cb, data, lookup_by_name_async);
    }
  else
    {
      res = g_simple_async_result_new_error (source, cb, data,
          G_RESOLVER_ERROR, G_RESOLVER_ERROR_NOT_FOUND,
          "No fake hostname record registered");
    }

  g_simple_async_result_set_op_res_gpointer (res, addr,
      (GDestroyNotify) object_list_free);
  g_simple_async_result_complete_in_idle (res);
  g_object_unref (res);
}

static GList *
lookup_by_name_finish (GResolver *resolver,
    GAsyncResult *result,
    GError **error)
{
  GList *res = NULL;
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);

  if (g_simple_async_result_propagate_error (simple, error))
    return NULL;

  res = g_simple_async_result_get_op_res_gpointer (simple);
  return object_list_copy (res);
}


/* ************************************************************************* */

static void
test_resolver_init (TestResolver *tr)
{
}

static void
test_resolver_set_property (GObject *object,
    guint propid,
    const GValue *value,
    GParamSpec *pspec)
{
  TestResolver *resolver = TEST_RESOLVER (object);

  switch (propid)
    {
    case PROP_REAL_RESOLVER:
      if (resolver->real_resolver != NULL)
        g_object_unref (resolver->real_resolver);
      resolver->real_resolver = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, propid, pspec);
      break;
    }
}

static void
test_resolver_get_property (GObject *object,
    guint propid,
    GValue *value,
    GParamSpec *pspec)
{
  TestResolver *resolver = TEST_RESOLVER (object);

  switch (propid)
    {
    case PROP_REAL_RESOLVER:
      g_value_set_object (value, resolver->real_resolver);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, propid, pspec);
      break;
    }
}

static void
test_resolver_class_init (TestResolverClass *klass)
{
  GResolverClass *resolver_class = G_RESOLVER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *spec;

  object_class->set_property = test_resolver_set_property;
  object_class->get_property = test_resolver_get_property;

  spec = g_param_spec_object ("real-resolver", "real-resolver",
      "The real resolver to use when we don't have a kludge entry",
      G_TYPE_RESOLVER,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT);
  g_object_class_install_property (object_class, PROP_REAL_RESOLVER, spec);
  resolver_class->lookup_by_name_async     = lookup_by_name_async;
  resolver_class->lookup_by_name_finish    = lookup_by_name_finish;
  resolver_class->lookup_service_async     = lookup_service_async;
  resolver_class->lookup_service_finish    = lookup_service_finish;
}

void
test_resolver_reset (TestResolver *tr)
{
  GList *fake = NULL;

  for (fake = tr->fake_A; fake; fake = fake->next)
    {
      fake_host *entry = fake->data;
      g_free (entry->key);
      g_free (entry->addr);
      g_free (entry);
    }
  g_list_free (tr->fake_A);
  tr->fake_A = NULL;

  for (fake = tr->fake_SRV; fake; fake = fake->next)
    {
      fake_serv *entry = fake->data;
      g_free (entry->key);
      g_srv_target_free (entry->srv);
      g_free (entry);
    }
  g_list_free (tr->fake_SRV);
  tr->fake_SRV = NULL;
}

gboolean
test_resolver_add_A (TestResolver *tr,
    const char *hostname,
    const char *addr)
{
  fake_host *entry = g_new0( fake_host, 1 );
  entry->key = g_strdup (hostname);
  entry->addr = g_strdup (addr);
  tr->fake_A = g_list_append (tr->fake_A, entry);
  return TRUE;
}

gboolean test_resolver_add_SRV (TestResolver *tr,
    const char *service,
    const char *protocol,
    const char *domain,
    const char *addr,
    guint16     port)
{
  char *key = _service_rrname (service, protocol, domain);
  fake_serv *entry = g_new0 (fake_serv, 1);
  GSrvTarget *serv = g_srv_target_new (addr, port, 0, 0);
  entry->key = key;
  entry->srv = serv;
  tr->fake_SRV = g_list_append (tr->fake_SRV, entry);
  return TRUE;
}
