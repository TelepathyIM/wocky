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
#include <glib-object.h>

#include "wocky-node.h"

/*< prefix=WOCKY_XMPP_ERROR_TYPE >*/
typedef enum {
    WOCKY_XMPP_ERROR_TYPE_CANCEL,
    WOCKY_XMPP_ERROR_TYPE_CONTINUE,
    WOCKY_XMPP_ERROR_TYPE_MODIFY,
    WOCKY_XMPP_ERROR_TYPE_AUTH,
    WOCKY_XMPP_ERROR_TYPE_WAIT
} WockyXmppErrorType;

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

    NUM_WOCKY_XMPP_ERRORS /*< skip >*/ /* don't want this in the GEnum */
} WockyXmppError;

GQuark wocky_xmpp_error_quark (void);
#define WOCKY_XMPP_ERROR (wocky_xmpp_error_quark ())

typedef struct {
    const gchar *description;
    WockyXmppError specializes;
    gboolean override_type;
    WockyXmppErrorType type;
} WockyXmppErrorSpecialization;

typedef struct {
    GQuark domain;
    GType enum_type;
    WockyXmppErrorSpecialization *codes;
} WockyXmppErrorDomain;

void wocky_xmpp_error_register_domain (WockyXmppErrorDomain *domain);

/*< prefix=WOCKY_JINGLE_ERROR >*/
typedef enum {
    WOCKY_JINGLE_ERROR_OUT_OF_ORDER,
    WOCKY_JINGLE_ERROR_TIE_BREAK,
    WOCKY_JINGLE_ERROR_UNKNOWN_SESSION,
    WOCKY_JINGLE_ERROR_UNSUPPORTED_INFO
} WockyJingleError;

GQuark wocky_jingle_error_quark (void);
#define WOCKY_JINGLE_ERROR (wocky_jingle_error_quark ())

/*< prefix=WOCKY_SI_ERROR >*/
typedef enum {
    WOCKY_SI_ERROR_NO_VALID_STREAMS,
    WOCKY_SI_ERROR_BAD_PROFILE
} WockySIError;

GQuark wocky_si_error_quark (void);
#define WOCKY_SI_ERROR (wocky_si_error_quark ())

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

GError *wocky_xmpp_stream_error_from_node (WockyNode *node);

WockyNode *wocky_stanza_error_to_node (const GError *error,
    WockyNode *parent_node);

void wocky_xmpp_error_extract (WockyNode *error,
    WockyXmppErrorType *type,
    GError **core,
    GError **specialized,
    WockyNode **specialized_node);

void wocky_xmpp_error_init (void);
void wocky_xmpp_error_deinit (void);

#endif /* __WOCKY_XMPP_ERROR_H__ */
