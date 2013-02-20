/*
 * wocky-xmpp-error.h - Header for Wocky's XMPP error handling API
 * Copyright (C) 2006-2009 Collabora Ltd.
 * Copyright (C) 2006 Nokia Corporation
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
#if !defined (WOCKY_H_INSIDE) && !defined (WOCKY_COMPILATION)
# error "Only <wocky/wocky.h> can be included directly."
#endif

#ifndef __WOCKY_XMPP_ERROR_H__
#define __WOCKY_XMPP_ERROR_H__

#include <glib.h>
#include <glib-object.h>

#include "wocky-enumtypes.h"
#include "wocky-node.h"

/**
 * WockyXmppErrorType:
 * @WOCKY_XMPP_ERROR_TYPE_CANCEL: do not retry (the error is
 *   unrecoverable)
 * @WOCKY_XMPP_ERROR_TYPE_CONTINUE: proceed (the condition was only a
 *   warning)
 * @WOCKY_XMPP_ERROR_TYPE_MODIFY: retry after changing the data sent
 * @WOCKY_XMPP_ERROR_TYPE_AUTH: retry after providing credentials
 * @WOCKY_XMPP_ERROR_TYPE_WAIT: retry after waiting (the error is
 *   temporary)
 *
 * XMPP error types as described in RFC 3920 §9.3.2.
 */
/*< prefix=WOCKY_XMPP_ERROR_TYPE >*/
typedef enum
{
  WOCKY_XMPP_ERROR_TYPE_CANCEL,
  WOCKY_XMPP_ERROR_TYPE_CONTINUE,
  WOCKY_XMPP_ERROR_TYPE_MODIFY,
  WOCKY_XMPP_ERROR_TYPE_AUTH,
  WOCKY_XMPP_ERROR_TYPE_WAIT
} WockyXmppErrorType;

/**
 * WockyXmppError:
 * @WOCKY_XMPP_ERROR_UNDEFINED_CONDITION: the error condition is not one
 *   of those defined by the other conditions in this list
 * @WOCKY_XMPP_ERROR_REDIRECT: the recipient or server is redirecting
 *   requests for this information to another entity
 * @WOCKY_XMPP_ERROR_GONE: the recipient or server can no longer be
 *   contacted at this address
 * @WOCKY_XMPP_ERROR_BAD_REQUEST: the sender has sent XML that is
 *   malformed or that cannot be processed
 * @WOCKY_XMPP_ERROR_UNEXPECTED_REQUEST: the recipient or server
 *   understood the request but was not expecting it at this time
 * @WOCKY_XMPP_ERROR_JID_MALFORMED: the sending entity has provided or
 *   communicated an XMPP address
 * @WOCKY_XMPP_ERROR_NOT_AUTHORIZED: the sender must provide proper
 *   credentials before being allowed to perform the action, or has
 *   provided improper credentials
 * @WOCKY_XMPP_ERROR_PAYMENT_REQUIRED: the requesting entity is not
 *   authorized to access the requested service because payment is
 *   required. This code is no longer defined in RFC 6120, the current version
 *   of XMPP Core. It's preserved here for interoperability, but new
 *   applications should not send it.
 * @WOCKY_XMPP_ERROR_FORBIDDEN: the requesting entity does not possess
 *   the required permissions to perform the action
 * @WOCKY_XMPP_ERROR_ITEM_NOT_FOUND: the addressed JID or item requested
 *   cannot be found
 * @WOCKY_XMPP_ERROR_RECIPIENT_UNAVAILABLE: the intended recipient is
 *   temporarily unavailable
 * @WOCKY_XMPP_ERROR_REMOTE_SERVER_NOT_FOUND: a remote server or
 *   service specified as part or all of the JID of the intended
 *   recipient does not exist
 * @WOCKY_XMPP_ERROR_NOT_ALLOWED: the recipient or server does not
 *   allow any entity to perform the action
 * @WOCKY_XMPP_ERROR_NOT_ACCEPTABLE: the recipient or server
 *   understands the request but is refusing to process it because it
 *   does not meet criteria defined by the recipient or server
 * @WOCKY_XMPP_ERROR_REGISTRATION_REQUIRED: the requesting entity is
 *   not authorized to access the requested service because
 *   registration is required
 * @WOCKY_XMPP_ERROR_SUBSCRIPTION_REQUIRED: the requesting entity is
 *   not authorized to access the requested service because a
 *   subscription is required
 * @WOCKY_XMPP_ERROR_REMOTE_SERVER_TIMEOUT: a remote server or service
 *   specified as part or all of the JID of the intended recipient (or
 *   required to fulfill a request) could not be contacted within a
 *   reasonable amount of time
 * @WOCKY_XMPP_ERROR_CONFLICT: access cannot be granted because an
 *   existing resource or session exists with the same name or address
 * @WOCKY_XMPP_ERROR_INTERNAL_SERVER_ERROR: the server could not
 *   process the stanza because of a misconfiguration or an
 *   otherwise-undefined internal server error
 * @WOCKY_XMPP_ERROR_RESOURCE_CONSTRAINT: the server or recipient lacks
 *   the system resources necessary to service the request
 * @WOCKY_XMPP_ERROR_FEATURE_NOT_IMPLEMENTED: the feature requested is
 *   not implemented by the recipient or server and therefore cannot
 *   be processed
 * @WOCKY_XMPP_ERROR_SERVICE_UNAVAILABLE: the server or recipient does
 *   not currently provide the requested service
 * @WOCKY_XMPP_ERROR_POLICY_VIOLATION: the entity has violated some local
 *   service policy (e.g., a message contains words that are prohibited by the
 *   service) and the server MAY choose to specify the policy as the text of
 *   the error or in an application-specific condition element; the associated
 *   error type SHOULD be %WOCKY_XMPP_ERROR_TYPE_MODIFY or
 *   %WOCKY_XMPP_ERROR_TYPE_WAIT depending on the policy being violated.
 *
 * Possible stanza-level errors, as defined by <ulink
 * url='http://xmpp.org/rfcs/rfc6120.html#stanzas-error-conditions'>RFC 6210
 * §8.3.3</ulink>.
 */

/*< prefix=WOCKY_XMPP_ERROR >*/
typedef enum {
    WOCKY_XMPP_ERROR_UNDEFINED_CONDITION = 0, /* 500 */

    WOCKY_XMPP_ERROR_REDIRECT,                /* 302 */
    WOCKY_XMPP_ERROR_GONE,                    /* 302 */

    WOCKY_XMPP_ERROR_BAD_REQUEST,             /* 400 */
    WOCKY_XMPP_ERROR_UNEXPECTED_REQUEST,      /* 400 */
    WOCKY_XMPP_ERROR_JID_MALFORMED,           /* 400 */

    WOCKY_XMPP_ERROR_NOT_AUTHORIZED,          /* 401 */

    WOCKY_XMPP_ERROR_PAYMENT_REQUIRED,        /* 402 */

    WOCKY_XMPP_ERROR_FORBIDDEN,               /* 403 */

    WOCKY_XMPP_ERROR_ITEM_NOT_FOUND,          /* 404 */
    WOCKY_XMPP_ERROR_RECIPIENT_UNAVAILABLE,   /* 404 */
    WOCKY_XMPP_ERROR_REMOTE_SERVER_NOT_FOUND, /* 404 */

    WOCKY_XMPP_ERROR_NOT_ALLOWED,             /* 405 */

    WOCKY_XMPP_ERROR_NOT_ACCEPTABLE,          /* 406 */

    WOCKY_XMPP_ERROR_REGISTRATION_REQUIRED,   /* 407 */
    WOCKY_XMPP_ERROR_SUBSCRIPTION_REQUIRED,   /* 407 */

    WOCKY_XMPP_ERROR_REMOTE_SERVER_TIMEOUT,   /* 408, 504 */

    WOCKY_XMPP_ERROR_CONFLICT,                /* 409 */

    WOCKY_XMPP_ERROR_INTERNAL_SERVER_ERROR,   /* 500 */
    WOCKY_XMPP_ERROR_RESOURCE_CONSTRAINT,     /* 500 */

    WOCKY_XMPP_ERROR_FEATURE_NOT_IMPLEMENTED, /* 501 */

    WOCKY_XMPP_ERROR_SERVICE_UNAVAILABLE,     /* 502, 503, 510 */

    WOCKY_XMPP_ERROR_POLICY_VIOLATION,

    /*< private >*/
    NUM_WOCKY_XMPP_ERRORS /*< skip >*/ /* don't want this in the GEnum */
} WockyXmppError;

GQuark wocky_xmpp_error_quark (void);
#define WOCKY_XMPP_ERROR (wocky_xmpp_error_quark ())

/**
 * WockyXmppErrorSpecialization:
 * @description: description of the error
 * @specializes: which #WockyXmppError this error specializes
 * @override_type: %TRUE if @type should be used, or %FALSE if the
 *   default error type for @specializes should be used
 * @type: the XMPP error type
 *
 * A struct to represent a specialization of an existing
 * #WockyXmppError member.
 */
typedef struct _WockyXmppErrorSpecialization WockyXmppErrorSpecialization;
struct _WockyXmppErrorSpecialization
{
  const gchar *description;
  WockyXmppError specializes;
  gboolean override_type;
  WockyXmppErrorType type;
};

/**
 * WockyXmppErrorDomain:
 * @domain: a #GQuark of the error domain
 * @enum_type: the #GType of the error enum
 * @codes: a %NULL-terminated array of of #WockyXmppErrorSpecialization<!--
 *   -->s
 *
 * A struct to represent extra XMPP error domains added.
 */
typedef struct _WockyXmppErrorDomain WockyXmppErrorDomain;
struct _WockyXmppErrorDomain
{
  GQuark domain;
  GType enum_type;
  WockyXmppErrorSpecialization *codes;
};

void wocky_xmpp_error_register_domain (WockyXmppErrorDomain *domain);

/**
 * WockyJingleError:
 * @WOCKY_JINGLE_ERROR_OUT_OF_ORDER: the request cannot occur at this
 *   point in the state machine
 * @WOCKY_JINGLE_ERROR_TIE_BREAK: the request is rejected because it
 *   was sent while the initiator was awaiting a reply on a similar
 *   request
 * @WOCKY_JINGLE_ERROR_UNKNOWN_SESSION: the 'sid' attribute specifies
 *   a session that is unknown to the recipient
 * @WOCKY_JINGLE_ERROR_UNSUPPORTED_INFO: the recipient does not
 *   support the informational payload of a session-info action.
 *
 * Jingle specific errors.
 */
/*< prefix=WOCKY_JINGLE_ERROR >*/
typedef enum {
    WOCKY_JINGLE_ERROR_OUT_OF_ORDER,
    WOCKY_JINGLE_ERROR_TIE_BREAK,
    WOCKY_JINGLE_ERROR_UNKNOWN_SESSION,
    WOCKY_JINGLE_ERROR_UNSUPPORTED_INFO
} WockyJingleError;

GQuark wocky_jingle_error_quark (void);
#define WOCKY_JINGLE_ERROR (wocky_jingle_error_quark ())

/**
 * WockySIError:
 * @WOCKY_SI_ERROR_NO_VALID_STREAMS: none of the available streams are
 *   acceptable
 * @WOCKY_SI_ERROR_BAD_PROFILE: the profile is not understood or
 *   invalid
 *
 * SI specific errors.
 */
/*< prefix=WOCKY_SI_ERROR >*/
typedef enum {
    WOCKY_SI_ERROR_NO_VALID_STREAMS,
    WOCKY_SI_ERROR_BAD_PROFILE
} WockySIError;

GQuark wocky_si_error_quark (void);
#define WOCKY_SI_ERROR (wocky_si_error_quark ())

/**
 * WockyXmppStreamError:
 * @WOCKY_XMPP_STREAM_ERROR_BAD_FORMAT: the entity has sent XML that
 *   cannot be processed
 * @WOCKY_XMPP_STREAM_ERROR_BAD_NAMESPACE_PREFIX: the entity has sent
 *   a namespace prefix that is unsupported, or has sent no namespace
 *   prefix on an element that requires such a prefix
 * @WOCKY_XMPP_STREAM_ERROR_CONFLICT: the server is closing the active
 *   stream for this entity because a new stream has been initiated
 *   that conflicts with the existing stream
 * @WOCKY_XMPP_STREAM_ERROR_CONNECTION_TIMEOUT: the entity has not
 *   generated any traffic over the stream for some period of time
 * @WOCKY_XMPP_STREAM_ERROR_HOST_GONE: the value of the 'to' attribute
 *   provided by the initiating entity in the stream header
 *   corresponds to a hostname that is no longer hosted by the server
 * @WOCKY_XMPP_STREAM_ERROR_HOST_UNKNOWN: the value of the 'to'
 *   attribute provided by the initiating entity in the stream header
 *   does not correspond to a hostname that is hosted by the server
 * @WOCKY_XMPP_STREAM_ERROR_IMPROPER_ADDRESSING: a stanza sent between
 *   two servers lacks a 'to' or 'from' attribute (or the attribute
 *   has no value)
 * @WOCKY_XMPP_STREAM_ERROR_INTERNAL_SERVER_ERROR: the server has
 *   experienced a misconfiguration or an otherwise-undefined internal
 *   error that prevents it from servicing the stream
 * @WOCKY_XMPP_STREAM_ERROR_INVALID_FROM: the JID or hostname provided
 *   in a 'from' address does not match an authorized JID or validated
 *   domain negotiated between servers via SASL or dialback, or
 *   between a client and a server via authentication and resource
 *   binding
 * @WOCKY_XMPP_STREAM_ERROR_INVALID_ID: the stream ID or dialback ID
 *   is invalid or does not match an ID previously provided
 * @WOCKY_XMPP_STREAM_ERROR_INVALID_NAMESPACE: the streams namespace
 *   name is something other than "http://etherx.jabber.org/streams"
 *   or the dialback namespace name is something other than
 *   "jabber:server:dialback"
 * @WOCKY_XMPP_STREAM_ERROR_INVALID_XML: the entity has sent invalid
 *   XML over the stream to a server that performs validation
 * @WOCKY_XMPP_STREAM_ERROR_NOT_AUTHORIZED: the entity has attempted
 *   to send data before the stream has been authenticated, or
 *   otherwise is not authorized to perform an action related to
 *   stream negotiation
 * @WOCKY_XMPP_STREAM_ERROR_POLICY_VIOLATION: the entity has violated
 *   some local service policy
 * @WOCKY_XMPP_STREAM_ERROR_REMOTE_CONNECTION_FAILED: the server is
 *   unable to properly connect to a remote entity that is required
 *   for authentication or authorization
 * @WOCKY_XMPP_STREAM_ERROR_RESOURCE_CONSTRAINT: the server lacks the
 *   system resources necessary to service the stream
 * @WOCKY_XMPP_STREAM_ERROR_RESTRICTED_XML: the entity has attempted
 *   to send restricted XML features such as a comment, processing
 *   instruction, DTD, entity reference, or unescaped character
 * @WOCKY_XMPP_STREAM_ERROR_SEE_OTHER_HOST: the server will not
 *   provide service to the initiating entity but is redirecting
 *   traffic to another host
 * @WOCKY_XMPP_STREAM_ERROR_SYSTEM_SHUTDOWN: the server is being shut
 *   down and all active streams are being closed
 * @WOCKY_XMPP_STREAM_ERROR_UNDEFINED_CONDITION: the error condition
 *   is not one of those defined by the other conditions in this list
 * @WOCKY_XMPP_STREAM_ERROR_UNSUPPORTED_ENCODING: the initiating
 *   entity has encoded the stream in an encoding that is not
 *   supported by the server
 * @WOCKY_XMPP_STREAM_ERROR_UNSUPPORTED_STANZA_TYPE: the initiating
 *   entity has sent a first-level child of the stream that is not
 *   supported by the server
 * @WOCKY_XMPP_STREAM_ERROR_UNSUPPORTED_VERSION: the value of the
 *   'version' attribute provided by the initiating entity in the
 *   stream header specifies a version of XMPP that is not supported
 *   by the server
 * @WOCKY_XMPP_STREAM_ERROR_XML_NOT_WELL_FORMED: the initiating entity
 *   has sent XML that is not well-formed
 * @WOCKY_XMPP_STREAM_ERROR_UNKNOWN: an unknown stream error
 *
 * Stream-level error conditions as described in RFC 3920 §4.7.3.
 */
/*< prefix=WOCKY_XMPP_STREAM_ERROR >*/
typedef enum {
  WOCKY_XMPP_STREAM_ERROR_BAD_FORMAT,
  WOCKY_XMPP_STREAM_ERROR_BAD_NAMESPACE_PREFIX,
  WOCKY_XMPP_STREAM_ERROR_CONFLICT,
  WOCKY_XMPP_STREAM_ERROR_CONNECTION_TIMEOUT,
  WOCKY_XMPP_STREAM_ERROR_HOST_GONE,
  WOCKY_XMPP_STREAM_ERROR_HOST_UNKNOWN,
  WOCKY_XMPP_STREAM_ERROR_IMPROPER_ADDRESSING,
  WOCKY_XMPP_STREAM_ERROR_INTERNAL_SERVER_ERROR,
  WOCKY_XMPP_STREAM_ERROR_INVALID_FROM,
  WOCKY_XMPP_STREAM_ERROR_INVALID_ID,
  WOCKY_XMPP_STREAM_ERROR_INVALID_NAMESPACE,
  WOCKY_XMPP_STREAM_ERROR_INVALID_XML,
  WOCKY_XMPP_STREAM_ERROR_NOT_AUTHORIZED,
  WOCKY_XMPP_STREAM_ERROR_POLICY_VIOLATION,
  WOCKY_XMPP_STREAM_ERROR_REMOTE_CONNECTION_FAILED,
  WOCKY_XMPP_STREAM_ERROR_RESOURCE_CONSTRAINT,
  WOCKY_XMPP_STREAM_ERROR_RESTRICTED_XML,
  WOCKY_XMPP_STREAM_ERROR_SEE_OTHER_HOST,
  WOCKY_XMPP_STREAM_ERROR_SYSTEM_SHUTDOWN,
  WOCKY_XMPP_STREAM_ERROR_UNDEFINED_CONDITION,
  WOCKY_XMPP_STREAM_ERROR_UNSUPPORTED_ENCODING,
  WOCKY_XMPP_STREAM_ERROR_UNSUPPORTED_STANZA_TYPE,
  WOCKY_XMPP_STREAM_ERROR_UNSUPPORTED_VERSION,
  WOCKY_XMPP_STREAM_ERROR_XML_NOT_WELL_FORMED,
  WOCKY_XMPP_STREAM_ERROR_UNKNOWN,
} WockyXmppStreamError;

GQuark wocky_xmpp_stream_error_quark (void);

/**
 * WOCKY_XMPP_STREAM_ERROR:
 *
 * Get access to the error quark of the xmpp stream errors.
 */
#define WOCKY_XMPP_STREAM_ERROR (wocky_xmpp_stream_error_quark ())

const gchar *wocky_xmpp_error_string (WockyXmppError error);
const gchar *wocky_xmpp_error_description (WockyXmppError error);

GError *wocky_xmpp_stream_error_from_node (WockyNode *error);

WockyNode *wocky_stanza_error_to_node (const GError *error,
    WockyNode *parent_node);
const gchar *wocky_xmpp_stanza_error_to_string (GError *error);

void wocky_xmpp_error_extract (WockyNode *error,
    WockyXmppErrorType *type,
    GError **core,
    GError **specialized,
    WockyNode **specialized_node);

void wocky_xmpp_error_init (void);
void wocky_xmpp_error_deinit (void);

#endif /* __WOCKY_XMPP_ERROR_H__ */
