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
#include <gio/gnio.h>

#include "wocky-test-sasl-auth-server.h"

G_BEGIN_DECLS

#define CONNPROBLEM(x) (0x1 << x)

typedef enum {
  CONNECTOR_PROBLEM_NO_PROBLEM    = 0,
  CONNECTOR_PROBLEM_BAD_XMPP      = CONNPROBLEM (0),
  CONNECTOR_PROBLEM_NO_TLS        = CONNPROBLEM (1),
  CONNECTOR_PROBLEM_TLS_REFUSED   = CONNPROBLEM (2),
  CONNECTOR_PROBLEM_FEATURES      = CONNPROBLEM (3),
  CONNECTOR_PROBLEM_CANNOT_BIND   = CONNPROBLEM (4),
  CONNECTOR_PROBLEM_BIND_INVALID  = CONNPROBLEM (5),
  CONNECTOR_PROBLEM_BIND_DENIED   = CONNPROBLEM (6),
  CONNECTOR_PROBLEM_BIND_CONFLICT = CONNPROBLEM (7),
  CONNECTOR_PROBLEM_BIND_REJECTED = CONNPROBLEM (8),
  CONNECTOR_PROBLEM_BIND_FAILED   = CONNPROBLEM (9),
} ConnectorProblem;


typedef struct _TestConnectorServer TestConnectorServer;
typedef struct _TestConnectorServerClass TestConnectorServerClass;

struct _TestConnectorServerClass {
    GObjectClass parent_class;
};

struct _TestConnectorServer {
    GObject parent;
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
    ConnectorProblem problem,
    ServerProblem sasl_problem);

void test_connector_server_start (GObject *object);

G_END_DECLS

#endif /* #ifndef __TEST_CONNECTOR_SERVER_H__*/
