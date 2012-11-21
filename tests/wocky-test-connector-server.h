/*
 * wocky-test-sasl-auth-server.h - Header for TestSaslAuthServer
 * Copyright (C) 2006 Collabora Ltd.
 * @author Sjoerd Simons <sjoerd@luon.net>
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

#ifndef __TEST_CONNECTOR_SERVER_H__
#define __TEST_CONNECTOR_SERVER_H__

#include <glib-object.h>
#include <gio/gio.h>

#include "wocky-test-sasl-auth-server.h"

G_BEGIN_DECLS

#define CONNPROBLEM(x) (0x1 << x)

typedef enum
{
  XMPP_PROBLEM_NONE        = 0,
  XMPP_PROBLEM_BAD_XMPP    = CONNPROBLEM (0),
  XMPP_PROBLEM_NO_TLS      = CONNPROBLEM (1),
  XMPP_PROBLEM_TLS_REFUSED = CONNPROBLEM (2),
  XMPP_PROBLEM_FEATURES    = CONNPROBLEM (3),
  XMPP_PROBLEM_OLD_SERVER  = CONNPROBLEM (4),
  XMPP_PROBLEM_WEAK_SSL    = CONNPROBLEM (5),
  XMPP_PROBLEM_OLD_SSL     = CONNPROBLEM (6),
  XMPP_PROBLEM_OTHER_HOST  = CONNPROBLEM (7),
  XMPP_PROBLEM_TLS_LOAD    = CONNPROBLEM (8),
  XMPP_PROBLEM_NO_SESSION  = CONNPROBLEM (10),
  XMPP_PROBLEM_CANNOT_BIND = CONNPROBLEM (11),
  XMPP_PROBLEM_OLD_AUTH_FEATURE = CONNPROBLEM (12),
  XMPP_PROBLEM_SEE_OTHER_HOST = CONNPROBLEM (13),
} XmppProblem;

typedef enum
{
  BIND_PROBLEM_NONE = 0,
  BIND_PROBLEM_INVALID     = CONNPROBLEM(0),
  BIND_PROBLEM_DENIED      = CONNPROBLEM(1),
  BIND_PROBLEM_CONFLICT    = CONNPROBLEM(2),
  BIND_PROBLEM_CLASH       = CONNPROBLEM(3),
  BIND_PROBLEM_REJECTED    = CONNPROBLEM(4),
  BIND_PROBLEM_FAILED      = CONNPROBLEM(5),
  BIND_PROBLEM_NO_JID      = CONNPROBLEM(6),
  BIND_PROBLEM_NONSENSE    = CONNPROBLEM(7),
} BindProblem;

typedef enum
{
  SESSION_PROBLEM_NONE = 0,
  SESSION_PROBLEM_NO_SESSION = CONNPROBLEM(0),
  SESSION_PROBLEM_FAILED     = CONNPROBLEM(1),
  SESSION_PROBLEM_DENIED     = CONNPROBLEM(2),
  SESSION_PROBLEM_CONFLICT   = CONNPROBLEM(3),
  SESSION_PROBLEM_REJECTED   = CONNPROBLEM(4),
  SESSION_PROBLEM_NONSENSE   = CONNPROBLEM(5),
} SessionProblem;

typedef enum
{
  SERVER_DEATH_NONE = 0,
  SERVER_DEATH_SERVER_START = CONNPROBLEM(0),
  SERVER_DEATH_CLIENT_OPEN  = CONNPROBLEM(1),
  SERVER_DEATH_SERVER_OPEN  = CONNPROBLEM(2),
  SERVER_DEATH_FEATURES     = CONNPROBLEM(3),
  SERVER_DEATH_TLS_NEG      = CONNPROBLEM(4),
} ServerDeath;

typedef enum
{
  JABBER_PROBLEM_NONE          = 0,
  JABBER_PROBLEM_AUTH_REJECT   = CONNPROBLEM (0),
  JABBER_PROBLEM_AUTH_BIND     = CONNPROBLEM (1),
  JABBER_PROBLEM_AUTH_PARTIAL  = CONNPROBLEM (2),
  JABBER_PROBLEM_AUTH_FAILED   = CONNPROBLEM (3),
  JABBER_PROBLEM_AUTH_STRANGE  = CONNPROBLEM (4),
  JABBER_PROBLEM_AUTH_NIH      = CONNPROBLEM (5),
  JABBER_PROBLEM_AUTH_NONSENSE = CONNPROBLEM (6),
} JabberProblem;

typedef enum
{
  XEP77_PROBLEM_NONE            = 0,
  XEP77_PROBLEM_ALREADY         = CONNPROBLEM(0),
  XEP77_PROBLEM_FAIL_CONFLICT   = CONNPROBLEM(1),
  XEP77_PROBLEM_FAIL_REJECTED   = CONNPROBLEM(2),
  XEP77_PROBLEM_NOT_AVAILABLE   = CONNPROBLEM(3),
  XEP77_PROBLEM_QUERY_NONSENSE  = CONNPROBLEM(4),
  XEP77_PROBLEM_QUERY_ALREADY   = CONNPROBLEM(5),
  XEP77_PROBLEM_NO_ARGS         = CONNPROBLEM(6),
  XEP77_PROBLEM_EMAIL_ARG       = CONNPROBLEM(7),
  XEP77_PROBLEM_STRANGE_ARG     = CONNPROBLEM(8),
  XEP77_PROBLEM_CANCEL_REJECTED = CONNPROBLEM(9),
  XEP77_PROBLEM_CANCEL_DISABLED = CONNPROBLEM(10),
  XEP77_PROBLEM_CANCEL_FAILED   = CONNPROBLEM(11),
  XEP77_PROBLEM_CANCEL_STREAM   = CONNPROBLEM(12),
} XEP77Problem;

typedef enum
{
  CERT_STANDARD,
  CERT_EXPIRED,
  CERT_NOT_YET,
  CERT_UNKNOWN,
  CERT_SELFSIGN,
  CERT_REVOKED,
  CERT_WILDCARD,
  CERT_BADWILD,
  CERT_NONE,
} CertSet;

typedef struct
{
  XmppProblem xmpp;
  BindProblem bind;
  SessionProblem session;
  ServerDeath death;
  JabberProblem jabber;
  XEP77Problem xep77;
} ConnectorProblem;

typedef struct _TestConnectorServer TestConnectorServer;
typedef struct _TestConnectorServerClass TestConnectorServerClass;
typedef struct _TestConnectorServerPrivate TestConnectorServerPrivate;

struct _TestConnectorServerClass {
    GObjectClass parent_class;
};

struct _TestConnectorServer {
    GObject parent;

    TestConnectorServerPrivate *priv;
};

GType test_connector_server_get_type (void);

/* TYPE MACROS */
#define TEST_TYPE_CONNECTOR_SERVER \
  (test_connector_server_get_type ())
#define TEST_CONNECTOR_SERVER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), TEST_TYPE_CONNECTOR_SERVER, \
   TestConnectorServer))
#define TEST_CONNECTOR_SERVER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), TEST_TYPE_CONNECTOR_SERVER, \
   TestConnectorServerClass))
#define TEST_IS_CONNECTOR_SERVER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), TEST_TYPE_CONNECTOR_SERVER))
#define TEST_IS_CONNECTOR_SERVER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), TEST_TYPE_CONNECTOR_SERVER))
#define TEST_CONNECTOR_SERVER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TEST_TYPE_CONNECTOR_SERVER, \
   TestConnectorServerClass))

TestConnectorServer * test_connector_server_new (GIOStream *stream,
    gchar *mech,
    const gchar *user,
    const gchar *pass,
    const gchar *version,
    ConnectorProblem *problem,
    ServerProblem sasl_problem,
    CertSet cert);

void test_connector_server_start (TestConnectorServer *self);

void test_connector_server_set_other_host (TestConnectorServer *self,
    const gchar *host,
    guint port);

void test_connector_server_teardown (TestConnectorServer *self,
  GAsyncReadyCallback callback,
  gpointer user_data);

gboolean test_connector_server_teardown_finish (TestConnectorServer *self,
  GAsyncResult *result,
  GError *error);

const gchar *test_connector_server_get_used_mech (TestConnectorServer *self);

G_END_DECLS

#endif /* #ifndef __TEST_CONNECTOR_SERVER_H__*/
