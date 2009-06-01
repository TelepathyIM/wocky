/*
 * wocky-connector.c - Source for WockyConnector
 * Copyright (C) 2006-2009 Collabora Ltd.
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

/**
 * SECTION: wocky-connector
 * @title: WockyConnector
 * @short_description: Low-level XMPP connection generator.
 *
 * Sends and receives #WockyXmppStanzas from an underlying GIOStream.
 * Invokes a callback with a WockyConnector (or an error) when it finishes.
 */

/*
 * tcp_srv_connected
 * ├→ tcp_host_connected
 * │  ↓
 * └→ xmpp_init
 *    ↓
 *    xmpp_init_sent_cb ←──────┬───┐
 *    ↓                        │   │
 *    xmpp_init_recv_cb        │   │
 *    ↓                        │   │
 *    xmpp_features_cb         │   │
 *    │ │ ↓                    │   │
 *    │ │ starttls_sent_cb     │   │
 *    │ │ ↓                    │   │
 *    │ │ starttls_recv_cb ────┘   │
 *    │ ↓                          │
 *    │ request-auth               │
 *    │ ↓                          │
 *    │ { username─requested       │
 *    │   password─requested       │
 *    │   authentication─succeeded ┘
 *    │   authentication─failed }
 *    ↓
 *    authed and possibly starttlsed, make the callback call
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <gio/gio.h>

#include "wocky-xmpp-connection.h"
#include "wocky-connector.h"
#include "wocky-signals-marshal.h"

#include "wocky-xmpp-reader.h"
#include "wocky-xmpp-writer.h"
#include "wocky-xmpp-stanza.h"

G_DEFINE_TYPE( WockyConnector, wocky_connector, G_TYPE_OBJECT );

enum
{
  PROP_BASE_STREAM   = 1,
  PROP_AUTH_INSECURE_OK ,
  PROP_TLS_INSECURE_OK  ,
  PROP_JID              ,
  PROP_RESOURCE         ,
  PROP_TLS_REQUIRED     ,
  PROP_XMPP_PORT        ,
  PROP_XMPP_HOST        ,
  PROP_IDENTITY         ,
  PROP_CALLBACK
};

typedef enum 
{
  WCON_DISCONNECTED    ,
  WCON_TCP_CONNECTING  ,
  WCON_TCP_CONNECTED   ,
  WCON_XMPP_INITIALISED,
  WCON_XMPP_TLS_STARTED
} connstate;

typedef struct _WockyConnectorPrivate WockyConnectorPrivate;

typdef void (*WockyConnectorCallback) (WockyConnector *conn, char *error);

struct _WockyConnectorPrivate
{
  /* properties: */
  GIOStream *stream;
  gboolean   auth_insecure_ok;
  gboolean   cert_insecure_ok;
  gchar     *jid;
  gchar     *resource;
  gboolean   tls_required;
  guint      xmpp_port;
  gchar     *xmpp_host;
  WockyConnectorCallback callback;
  /* volatile/derived property: jid + resource, may be updated by server: */
  gchar     *identity;
  gchar     *domain;

  /* misc internal state: */
  GError    *error;

  connstate state;
  gboolean  defunct;
  gboolean  authed;

  GSocketClient     *client;
  GSocketConnection *sock;

  GTLSSession    *tls_sess;  
  GTLSConnection *tls;

  WockyXmppConnection *conn;
};

#define WOCKY_CONNECTOR_GET_PRIVATE(o)  \
  (G_TYPE_INSTANCE_GET_PRIVATE((o),WOCKY_TYPE_CONNECTOR,WockyConnectorPrivate))

#define WOCKY_CONNECTOR_CHOOSE_BY_STATE( p, v, a, b, c ) \
  if( p->authed ) v = a; else if ( p->tls ) v = b; else v = c;

#define WOCKY_CONNECTOR_BAILOUT( obj, message )

static void
wocky_connector_init ( WockyConnector *obj )
{
 WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE( obj );
 
 priv->writer = wocky_xmpp_writer_new();
 priv->reader = wocky_xmpp_reader_new();
}

static void
wocky_connector_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
  WockyConnector *connector = WOCKY_CONNECTOR (object);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (connector);

  switch (property_id)
    {
    case PROP_BASE_STREAM:
      g_assert( priv->stream == NULL );
      priv->stream = g_value_dup_object( value );
      g_assert( priv->stream != NULL );
      break;
    case PROP_TLS_REQUIRED:
      priv->tls_required = g_value_get_boolean( value );
      break;
    case PROP_AUTH_INSECURE_OK:
      priv->auth_insecure_ok = g_value_get_boolean( value );
      break;
    case PROP_TLS_INSECURE_OK:
      priv->cert_insecure_ok = g_value_get_boolean( value );
      break;
    case PROP_JID:
      g_free( priv->jid );
      priv->jid = g_value_dup_string( value );
      break;
    case PROP_RESOURCE:
      g_free( priv->resource );
      priv->resource = g_value_dup_string( value );
      break;
    case PROP_XMPP_PORT:
      priv->xmpp_port = g_value_get_uint( value );
      break;
    case PROP_XMPP_HOST:
      g_free( priv->xmpp_host );
      priv->xmpp_host = g_value_dup_string( value );
      break;
    case PROP_IDENTITY:
      g_free( priv->identity );
      priv->identity = g_value_dup_string( value );
    case PROP_CALLBACK:
      priv->callback = g_value_get_pointer( value );
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
wocky_connector_get_property(GObject *object,
                             guint property_id,
                             GValue *value,
                             GParamSpec *pspec)
{
  WockyConnector *connector = WOCKY_CONNECTOR (object);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (connector);

  switch (property_id)
    {
    case PROP_BASE_STREAM:
      g_value_set_object( value, priv->stream );
      break;
    case PROP_TLS_REQUIRED:
      g_value_set_boolean( value, priv->tls_required );
      break;
    case PROP_AUTH_INSECURE_OK:
      g_value_set_boolean( value, priv->auth_insecure_ok );
      break;
    case PROP_TLS_INSECURE_OK:
      g_value_set_boolean( value, priv->cert_insecure_ok );
      break;
    case PROP_JID:
      g_value_set_string( value, priv->jid );
      break;
    case PROP_RESOURCE:
      g_value_set_string( value, priv->resource );
      break;
    case PROP_XMPP_PORT:
      g_value_set_uint( value, priv->xmpp_port );
      break;
    case PROP_XMPP_HOST:
      g_value_set_string( value, priv->xmpp_host );
      break;
    case PROP_IDENTITY:
      g_value_set_string( value, priv->identity );
      break;
    case PROP_CALLBACK:
      g_value_set_pointer( value, priv->callback );
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

#define PATTR      ( G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS )
#define INIT_PATTR ( WOCKY_CONNECTOR_PATTR | G_PARAM_CONSTRUCT  )

static void
wocky_connector_class_init ( WockyConnectorClass *klass )
{
  GObjectClass *oclass = G_OBJECT_CLASS( klass );
  GParamSpec *pspec;

  g_type_class_add_private( klass, sizeof(WockyConnectorPrivate) );
  
  oclass->set_property = wocky_connector_set_property;
  oclass->get_property = wocky_connector_get_private;
  oclass->dispose      = wocky_connector_dispose;
  oclass->finalize     = wocky_connector_finalise;

  spec = g_param_spec_object( "base-stream" , "base stream",
      "the XMPP connection IO stream" , G_TYPE_IO_STREAM , PATTR );
  g_object_class_install_property( oclass, PROP_BASE_STREAM, spec );

  spec = g_param_spec_boolean( "insecure-tls-ok", "insecure-tls-ok" ,
      "Whether recoverable TLS errors should be ignored", TRUE, PATTR );
  g_object_class_install_property( oclass, PROP_TLS_INSECURE_OK, spec );

  spec = g_param_spec_boolean( "insecure-auth-ok", "insecure-auth-ok" ,
      "Whether auth info can be sent in the clear", FALSE, PATTR );
  g_object_class_install_property( oclass, PROP_AUTH_INSECURE_OK, spec );

  spec = g_param_spec_boolean( "tls-required", "tls" ,
      "Whether SSL/TLS is required" , TRUE, PATTR );
  g_object_class_install_property( oclass, PROP_TLS_REQUIRED, spec );

  spec = g_param_spec_string( "jid", "jid", "The XMPP jid", NULL, attrib );
  g_object_class_install_property( oclass, PROP_JID, INIT_PATTR );

  spec = g_param_spec_string( "resource", "resource", 
      "XMPP resource to append to the jid", g_strdup("wocky"), PATTR );
  g_object_class_install_property( oclass, PROP_RESOURCE, spec );

  spec = g_param_spec_string( "identity", "identity", 
      "jid + resource (set by XMPP server)", NULL, PATTR );
  g_object_class_install_property( oclass, PROP_IDENTITY, spec );
 
  spec = g_param_spec_string( "xmpp-server", "server", 
      "XMPP connect server", NULL, PATTR );
  g_object_class_install_property( oclass, PROP_XMPP_HOST, spec );

  spec = g_param_spec_uint( "xmpp-port", "port", 
      "XMPP port", 0, 65535, 5222, PATTR );
  g_object_class_install_property( oclass, PROP_XMPP_PORT, spec );  

  spec = g_param_spec_pointer( "callback", "callback", 
      "callback(Connector,Message) is called on completion or error", PATTR );
  g_object_class_install_property( oclass, PROP_CALLBACK, spec )
}

#define UNREF_AND_FORGET(x) if( x ) { g_object_unref( x ); x = NULL; }
#define GFREE_AND_FORGET(x) g_free( x ); x = NULL;

void
wocky_connector_dispose( GObject *object )
{
  WockyConnector *self = WOCKY_CONNECTOR (object);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);

  if( priv->defunct ) 
    return;
  
  priv->defunct = TRUE;

  UNREF_AND_FORGET( priv->stream );
  UNREF_AND_FORGET( priv->reader );
  UNREF_AND_FORGET( priv->writer );

  GFREE_AND_FORGET( priv->jid       );
  GFREE_AND_FORGET( priv->resource  );
  GFREE_AND_FORGET( priv->identity  );
  GFREE_AND_FORGET( priv->xmpp_host );

  if( G_OBJECT_CLASS( wocky_connector_parent_class )->dispose  )
    G_OBJECT_CLASS( wocky_connector_parent_class )->dispose( object );
}

void
wocky_connector_finalise( GObject *object )
{
  G_OBJECT_CLASS( wocky_connector_parent_class )->finalize( object );
}

void
wocky_connector_connect( GObject *object, void *cb )
{
  WockyConnector *self = WOCKY_CONNECTOR (object);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);

  const gchar *host = priv->jid ? rindex( priv->jid, '@' ) : NULL;

  priv->callback = cb;

  if( priv->state != WCON_DISCONNECTED )
    WOCKY_CONNECTOR_BAILOUT( self, "Invalid state: Cannot connect" );
  if( !host )
    WOCKY_CONNECTOR_BAILOUT( self, "Invalid JID" );
  if( !*(++host) )
    WOCKY_CONNECTOR_BAILOUT( self, "Invalid JID: No domain" );

  priv->domain = g_strdup( host );
  priv->client = g_socket_client_new();
  priv->state  = WCON_CONNECTING;

  if( priv->xmpp_host )
    {
      g_socket_client_connect_to_host_async( priv->client,
          priv->xmpp_host, priv->xmpp_port, NULL, 
          tcp_host_connected, object );
    }
  else
    {
      g_socket_client_connect_to_service_async( priv->client, 
          host, "xmpp-client", NULL, tcp_srv_connected, object );
    }
}

static void
tcp_srv_connected (GObject *source,
                   GAsyncResult *result,
                   gpointer connector)
{
  GError *error = NULL;
  WockyConnector *self = WOCKY_CONNECTOR (connector);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);

  priv->sock = 
    g_socket_client_connect_to_service_finish( G_SOCKET_CLIENT( source ),
        result, &error);

  if( !priv->sock )
    {
      const gchar *host = rindex( priv->jid, '@' ) + 1;
      g_message( "SRV connect failed: %s: %d, %s",
          g_quark_to_string( error->domain ), error->code, error->message );
      g_message( "Falling back to direct connection" );
      g_error_free( error );
      priv->
      g_socket_client_connect_to_host_async( priv->client, 
          host, priv->xmpp_port, NULL, tcp_host_connected, connector );
    }
  else
    {
      priv->state = WCON_TCP_CONNECTED;
      xmpp_init( connector );
    }
}

static void
tcp_host_connected (GObject *source,
                    GAsyncResult *result,
                    gpointer connector)
{
  GError *error = NULL;

  priv->sock = 
    g_socket_client_connect_to_host_finish( G_SOCKET_CLIENT( source ),
        result, &error);

  if ( !priv->sock )
    {
      g_message( "HOST connect failed: %s: %d, %s\n",
          g_quark_to_string( error->domain ),
          error->code, error->message );
      WOCKY_CONNECTOR_BAILOUT( connector, "connection failed" );
    }
  else
    {
      priv->state = WCON_TCP_CONNECTED;
      xmpp_init( connector );
    }
}

static void
xmpp_init( GObject connector )
{
  WockyConnector *self = WOCKY_CONNECTOR (connector);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);

  priv->conn = wocky_xmpp_connection_new( G_IO_STREAM(priv->sock) );
  wocky_xmpp_connection_send_open_async( priv->conn, priv->domain, NULL, "1.0"
      NULL, NULL, xmpp_init_sent_cb, connector );
}

static void
xmpp_init_sent_cb( GObject source, GAsyncResult *result, gpointer data )
{
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);

  if( !wocky_xmpp_connection_send_open_finish(priv->conn, result, NULL) )
    {
      const char *msg = NULL;
      WOCKY_CONNECTOR_CHOOSE_BY_STATE( priv, msg ,
          "Failed to send post-auth open",
          "Failed to send post-TLS open" ,
          "Failed to send XMPP open"     );
      WOCKY_CONNECTOR_BAILOUT( self, msg );
    }

  wocky_xmpp_connection_recv_open_async( priv->conn, 
      NULL, xmpp_init_recv_cb, data );
}

static void 
xmpp_init_recv_cb( GObject source, GAsyncResult *result, gpointer data )
{
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);

  gchar *version;
  gchar *from;
  
  if( !wocky_xmpp_connection_recv_open_finish( priv->conn, result, NULL,
          &from, &version, NULL, NULL ) )
    {
      const char *msg = NULL;
      WOCKY_CONNECTOR_CHOOSE_BY_STATE( priv, msg, 
          "post-auth open response not received",
          "post-TLS open response not received" ,
          "XMPP open response not received"     );
      WOCKY_CONNECTOR_BAILOUT( self, msg );
    }
  
  if( !version || strcmp( version, "1.0" ) )
    WOCKY_CONNECTOR_BAILOUT( self, "Server not XMPP Compliant" );

  wocky_xmpp_connection_recv_stanza_async( priv->conn, NULL, 
      xmpp_features_cb, data );

  g_free( version );
  g_free( from    );
}

static void
xmpp_features_cb( GObject source, GAsyncResult *result, gpointer data )
{
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);
  WockyXmppStanza *stanza;
  WockyXmppNode   *tls;
  WockyXmppStanza *starttls;

  stanza = wocky_xmpp_connection_recv_stanza_finish( priv->conn, result, NULL );
  
  if( !stanza )
    WOCKY_CONNECTOR_BAILOUT( self, "disconnected before XMPP features stanza" );

  if( strcmp( stanza->node->name, "features" ) ||
      strcmp( wocky_xmpp_node_get_ns( stanza->node ), WOCKY_XMPP_NS_STREAM ) )
    {
      const char *msg
      WOCKY_CONNECTOR_CHOOSE_BY_STATE( priv, msg, 
          "Malformed post-auth feature stanza" ,
          "Malformed post-TLS feature stanza"  ,
          "Malformed XMPP feature stanza"      );
      WOCKY_CONNECTOR_BAILOUT( self, msg );
    }

  tls = wocky_xmpp_node_get_child_ns( stanza->node, WOCKY_XMPP_NS_TLS );
  
  if( !tls && priv->tls_required )
    WOCKY_CONNECTOR_BAILOUT( self, "TLS requested but lack server support" );

  if( priv->authed ) /* already authorised. hopefully we are done: */
    {
      if( !wocky_xmpp_connection_send_open_finish( priv->conn, result, NULL ) )
        WOCKY_CONNECTOR_BAILOUT( self, "post-auth open not sent" );
    }
  else if( priv->tls || !tls ) /* already in tls mode or tls not supported: */
    {
      request_auth( self, stanza );
    }
  else
    {
      starttls = wocky_xmpp_stanza_new( "starttls" );
      wocky_xmpp_node_set_ns( starttls->node, WOCKY_XMPP_NS_TLS );
      wocky_xmpp_connection_send_stanza_async( priv->conn, starttls,
          NULL, starttls_sent_cb, data );
      g_object_unref( starttls );
    }
  
  g_object_unref( stanza );
}

static void
starttls_sent_cb( GObject source, GAsyncResult *result, gpointer data )
{
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);

  if( !wocky_xmpp_connection_send_stanza_finish( priv->conn, result, NULL) )
    WOCKY_CONNECTOR_BAILOUT( self, "Failed to send STARTTLS stanza" );

  wocky_xmpp_connection_recv_stanza_async( priv->conn,
      NULL, starttls_recv_cb, data );
}

static void
starttls_recv_cb( GObject source, GAsyncResult *result, gpointer data )
{
  WockyXmppStanza *stanza;
  GError *error = NULL;
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);

  stanza = wocky_xmpp_connection_recv_stanza_finish( priv->conn, result, NULL );

  if( !stanza )
    WOCKY_CONNECTOR_BAILOUT( self, "STARTTLS reply not received" );

  if( strcmp( stanza->node->name, "proceed" ) || 
      strcmp( wocky_xmpp_node_get_ns( stanza->node ), WOCKY_XMPP_NS_TLS ) )
    {
      if( priv->tls_required )
        WOCKY_CONNECTOR_BAILOUT( self, "STARTTLS refused by server" );
      else
        request_auth( self, stanza );
    }
  else 
    {
      priv->tls_sess = g_tls_session_new( G_IO_STREAM( priv->sock ) );
      priv->tls = g_tls_session_handshake( priv->tls_sess, NULL, &error );

      if( !priv->tls )
        WOCKY_CONNECTOR_BAILOUT( self, "TLS Handshake Error" );

      priv->conn = wocky_xmpp_connection_new( G_IO_STREAM( priv->tls ) );
      wocky_xmpp_connection_send_open_async( priv->conn, priv->domain, 
          NULL, "1.0", NULL, NULL, xmpp_init_sent_cb, data );
    }
}

static void
request_auth( GObject *object, WockyXmppStanza *stanza )
{
  WockyConnector *self = WOCKY_CONNECTOR (object);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);
  WockySaslAuth *s = priv->sasl = wocky_sasl_auth_new();

  g_signal_connect( s, "username-requested", G_CALLBACK(get_string), self );
  g_signal_connect( s, "password-requested", G_CALLBACK(get_secret), self );
  g_signal_connect( s, "authentication-succeeded", G_CALLBACK(auth_ok ), self );
  g_signal_connect( s, "authentication-failed"   , G_CALLBACK(auth_bad), self );
  
  if( !wocky_sasl_auth_authenticate( s, priv->domain, priv->conn, 
          stanza, TRUE, &error,  ) )
    {
      WOCKY_CONNECTOR_BAILOUT( self, "SASL auth start failed" );
    }
}

static void 
auth_ok( WockySaslAuth *sasl,  gpointer data )
{
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);

  priv->authed = TRUE;
  wocky_xmpp_connection_reset( self->conn );
  wocky_xmpp_connection_send_open_async( priv->conn, priv->domain, NULL, 
      "1.0", NULL, NULL, xmpp_init_sent_cb, self );
}

static void
auth_bad( WockySaslAuth *sasl, GQuark dom, int err, gchar *msg,  gpointer data )
{
  WOCKY_CONNECTOR_BAILOUT( data, msg );
}
