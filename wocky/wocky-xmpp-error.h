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

#ifndef __WOCKY_XMPP_ERROR_H__
#define __WOCKY_XMPP_ERROR_H__

#include <glib.h>
#include "wocky-xmpp-stanza.h"

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

    WOCKY_XMPP_ERROR_JINGLE_OUT_OF_ORDER,
    WOCKY_XMPP_ERROR_JINGLE_UNKNOWN_SESSION,
    WOCKY_XMPP_ERROR_JINGLE_UNSUPPORTED_CONTENT,
    WOCKY_XMPP_ERROR_JINGLE_UNSUPPORTED_TRANSPORT,

    WOCKY_XMPP_ERROR_SI_NO_VALID_STREAMS,
    WOCKY_XMPP_ERROR_SI_BAD_PROFILE,

    NUM_WOCKY_XMPP_ERRORS,
} WockyXmppError;

GQuark wocky_xmpp_error_quark (void);
#define WOCKY_XMPP_ERROR (wocky_xmpp_error_quark ())

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

WockyXmppError wocky_xmpp_error_from_node (WockyXmppNode *error_node);
WockyXmppNode *wocky_xmpp_error_to_node (WockyXmppError error,
    WockyXmppNode *parent_node,
    const gchar *errmsg);
const gchar *wocky_xmpp_error_string (WockyXmppError error);
const gchar *wocky_xmpp_error_description (WockyXmppError error);

WockyXmppStreamError wocky_xmpp_stream_error_from_node (WockyXmppNode *node);

/* Error node util */
const gchar *
wocky_xmpp_error_unpack_node (WockyXmppNode *node,
    const gchar **type,
    WockyXmppNode **text,
    WockyXmppNode **orig,
    WockyXmppNode **extra,
    WockyXmppError *errnum);

#endif /* __WOCKY_XMPP_ERROR_H__ */
