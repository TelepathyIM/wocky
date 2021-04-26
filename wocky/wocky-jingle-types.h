/*
 * wocky-jingle-types.h - Header for Jingle-related enums and typedefs
 * Copyright © 2008–2012 Collabora Ltd.
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

#ifndef __WOCKY_JINGLE_TYPES_H__
#define __WOCKY_JINGLE_TYPES_H__

#ifndef __GTK_DOC_IGNORE__
typedef struct _WockyJingleFactory WockyJingleFactory;
typedef struct _WockyJingleSession WockyJingleSession;
typedef struct _WockyJingleContent WockyJingleContent;
typedef struct _WockyJingleTransportGoogle WockyJingleTransportGoogle;
typedef struct _WockyJingleTransportRawUdp WockyJingleTransportRawUdp;
typedef struct _WockyJingleTransportIceUdp WockyJingleTransportIceUdp;
typedef struct _WockyJingleMediaRtp WockyJingleMediaRtp;
typedef struct _WockyJingleCandidate WockyJingleCandidate;
#endif /* __GTK_DOC_IGNORE__ */

/**
 * WockyJingleDialect:
 * @WOCKY_JINGLE_DIALECT_ERROR: not a jingle message
 * @WOCKY_JINGLE_DIALECT_GTALK3: relict libjingle3 gtalk variant
 * @WOCKY_JINGLE_DIALECT_GTALK4: legacy and latest gtalk variant
 * @WOCKY_JINGLE_DIALECT_V015: relict jingle version <ulink
 *  url="https://xmpp.org/extensions/attic/xep-0166-0.15.html">0.15</ulink>
 * @WOCKY_JINGLE_DIALECT_V032: old jingle version <ulink
 *  url="https://xmpp.org/extensions/attic/xep-0166-0.32.html">0.32</ulink>
 *
 *  The type of jingle session dialect. Currently supports outdated dialects.
 */
typedef enum {
  WOCKY_JINGLE_DIALECT_ERROR,
  WOCKY_JINGLE_DIALECT_GTALK3,
  WOCKY_JINGLE_DIALECT_GTALK4,
  WOCKY_JINGLE_DIALECT_V015,
  WOCKY_JINGLE_DIALECT_V032
} WockyJingleDialect;

#define WOCKY_JINGLE_DIALECT_IS_GOOGLE(d)\
    ((d == WOCKY_JINGLE_DIALECT_GTALK3) || (d == WOCKY_JINGLE_DIALECT_GTALK4))

/**
 * WockyJingleState:
 * @WOCKY_JINGLE_STATE_PENDING_CREATED: on outgoing sessions, no offer has been
 *  sent to the peer yet.
 * @WOCKY_JINGLE_STATE_PENDING_INITIATE_SENT: on outgoing sessions, we have sent
 *  the session-initiate and are awaiting the peer's acknowledgement.
 * @WOCKY_JINGLE_STATE_PENDING_INITIATED: on outgoing sessions, the peer has
 *  received our session-initiate and we're waiting for them to accept; on
 *  incoming sessions, the peer is waiting for us to accept.
 * @WOCKY_JINGLE_STATE_PENDING_ACCEPT_SENT: on incoming sessions, we have sent
 *  session-accept and are waiting for the peer to acknowledge it.
 * @WOCKY_JINGLE_STATE_ACTIVE: the session is active.
 * @WOCKY_JINGLE_STATE_ENDED: the session has ended. The
 *  #WockyJingleSession::terminated signal describes how the session ended.
 *
 * Possible states of a #WockyJingleSession.
 */
typedef enum {
  /*< private >*/
  WOCKY_JINGLE_STATE_INVALID = -1,
  /*< public >*/
  WOCKY_JINGLE_STATE_PENDING_CREATED = 0,
  WOCKY_JINGLE_STATE_PENDING_INITIATE_SENT,
  WOCKY_JINGLE_STATE_PENDING_INITIATED,
  WOCKY_JINGLE_STATE_PENDING_ACCEPT_SENT,
  WOCKY_JINGLE_STATE_ACTIVE,
  WOCKY_JINGLE_STATE_ENDED,
  /*< private >*/
  WOCKY_N_JINGLE_STATES
} WockyJingleState;

/**
 * WockyJingleAction:
 * @WOCKY_JINGLE_ACTION_UNKNOWN: Unknown/undefined action type
 * @WOCKY_JINGLE_ACTION_CONTENT_ACCEPT: <ulink
 *  url="https://xmpp.org/extensions/xep-0166.html#def-action-content-accept">
 *  action="content-accept"</ulink>
 * @WOCKY_JINGLE_ACTION_CONTENT_ADD: <ulink
 *  url="https://xmpp.org/extensions/xep-0166.html#def-action-content-add">
 *  action="content-add"</ulink>
 * @WOCKY_JINGLE_ACTION_CONTENT_MODIFY: <ulink
 *  url="https://xmpp.org/extensions/xep-0166.html#def-action-content-modify">
 *  action="content-modify"</ulink>
 * @WOCKY_JINGLE_ACTION_CONTENT_REMOVE: <ulink
 *  url="https://xmpp.org/extensions/xep-0166.html#def-action-content-remove">
 *  action="content-remove"</ulink>
 * @WOCKY_JINGLE_ACTION_CONTENT_REPLACE: deprecated
 * @WOCKY_JINGLE_ACTION_CONTENT_REJECT: <ulink
 *  url="https://xmpp.org/extensions/xep-0166.html#def-action-content-reject">
 *  action="content-reject"</ulink>
 * @WOCKY_JINGLE_ACTION_SESSION_ACCEPT: <ulink
 *  url="https://xmpp.org/extensions/xep-0166.html#def-action-session-accept">
 *  action="session-accept"</ulink>
 * @WOCKY_JINGLE_ACTION_SESSION_INFO: <ulink
 *  url="https://xmpp.org/extensions/xep-0166.html#def-action-security-info">
 *  action="security-info"</ulink>
 * @WOCKY_JINGLE_ACTION_SESSION_INITIATE: <ulink
 *  url="https://xmpp.org/extensions/xep-0166.html#def-action-session-initiate">
 *  action="session-initiate"</ulink>
 * @WOCKY_JINGLE_ACTION_SESSION_TERMINATE: <ulink
 *  url="https://xmpp.org/extensions/xep-0166.html#def-action-session-terminate">
 *  action="session-terminate"</ulink>
 * @WOCKY_JINGLE_ACTION_TRANSPORT_INFO: <ulink
 *  url="https://xmpp.org/extensions/xep-0166.html#def-action-transport-info">
 *  action="transport-info"</ulink>
 * @WOCKY_JINGLE_ACTION_TRANSPORT_ACCEPT: <ulink
 *  url="https://xmpp.org/extensions/xep-0166.html#def-action-transport-accept">
 *  action="transport-accept"</ulink>
 * @WOCKY_JINGLE_ACTION_DESCRIPTION_INFO: <ulink
 *  url="https://xmpp.org/extensions/xep-0166.html#def-action-description-info">
 *  action="description-info"</ulink>
 * @WOCKY_JINGLE_ACTION_INFO: deprecated
 *
 * The Jingle action as per <ulink
 * url="http://xmpp.org/extensions/xep-0166.html#def-action">XEP-0166 Jingle
 * §7.2</ulink> specification.
 * Missing Actions (introduced in v0.35 with ns 'urn:xmpp:jingle:1'):
 * * https://xmpp.org/extensions/xep-0166.html#def-action-security-info
 * * https://xmpp.org/extensions/xep-0166.html#def-action-transport-replace
 * * https://xmpp.org/extensions/xep-0166.html#def-action-transport-reject
 */
typedef enum {
  WOCKY_JINGLE_ACTION_UNKNOWN,
  WOCKY_JINGLE_ACTION_CONTENT_ACCEPT,
  WOCKY_JINGLE_ACTION_CONTENT_ADD,
  WOCKY_JINGLE_ACTION_CONTENT_MODIFY,
  WOCKY_JINGLE_ACTION_CONTENT_REMOVE,
  WOCKY_JINGLE_ACTION_CONTENT_REPLACE,
  WOCKY_JINGLE_ACTION_CONTENT_REJECT,
  WOCKY_JINGLE_ACTION_SESSION_ACCEPT,
  WOCKY_JINGLE_ACTION_SESSION_INFO,
  WOCKY_JINGLE_ACTION_SESSION_INITIATE,
  WOCKY_JINGLE_ACTION_SESSION_TERMINATE,
  WOCKY_JINGLE_ACTION_TRANSPORT_INFO,
  WOCKY_JINGLE_ACTION_TRANSPORT_ACCEPT,
  WOCKY_JINGLE_ACTION_DESCRIPTION_INFO,
  WOCKY_JINGLE_ACTION_INFO
} WockyJingleAction;

typedef enum { /*< skip >*/
  WOCKY_JINGLE_CONTENT_SENDERS_NONE,
  WOCKY_JINGLE_CONTENT_SENDERS_INITIATOR,
  WOCKY_JINGLE_CONTENT_SENDERS_RESPONDER,
  WOCKY_JINGLE_CONTENT_SENDERS_BOTH
} WockyJingleContentSenders;

typedef enum { /*< skip >*/
  JINGLE_TRANSPORT_UNKNOWN,
  JINGLE_TRANSPORT_GOOGLE_P2P,
  JINGLE_TRANSPORT_RAW_UDP,
  JINGLE_TRANSPORT_ICE_UDP,
} WockyJingleTransportType;

typedef enum { /*< skip >*/
  WOCKY_JINGLE_TRANSPORT_PROTOCOL_UDP,
  WOCKY_JINGLE_TRANSPORT_PROTOCOL_TCP
} WockyJingleTransportProtocol;

/**
 * WockyJingleCandidateType:
 * @WOCKY_JINGLE_CANDIDATE_TYPE_LOCAL: local candidate (direct endpoint)
 * @WOCKY_JINGLE_CANDIDATE_TYPE_STUN: candidate is mediated via STUN
 * @WOCKY_JINGLE_CANDIDATE_TYPE_RELAY: candidate is mediated via TURN
 */
typedef enum {
  WOCKY_JINGLE_CANDIDATE_TYPE_LOCAL,
  WOCKY_JINGLE_CANDIDATE_TYPE_STUN,
  WOCKY_JINGLE_CANDIDATE_TYPE_RELAY
} WockyJingleCandidateType;

/**
 * WockyJingleReason:
 * @WOCKY_JINGLE_REASON_UNKNOWN: no known reason
 * @WOCKY_JINGLE_REASON_ALTERNATIVE_SESSION: &lt;alternative-session/>
 * @WOCKY_JINGLE_REASON_BUSY: &lt;busy/>
 * @WOCKY_JINGLE_REASON_CANCEL: &lt;cancel/>
 * @WOCKY_JINGLE_REASON_CONNECTIVITY_ERROR: &lt;connectivity-error/>
 * @WOCKY_JINGLE_REASON_DECLINE: &lt;decline/>
 * @WOCKY_JINGLE_REASON_EXPIRED: &lt;expired/>
 * @WOCKY_JINGLE_REASON_FAILED_APPLICATION: &lt;failed-application/>
 * @WOCKY_JINGLE_REASON_FAILED_TRANSPORT: &lt;failed-transport/>
 * @WOCKY_JINGLE_REASON_GENERAL_ERROR: &lt;general-error/>
 * @WOCKY_JINGLE_REASON_GONE: &lt;gone/>
 * @WOCKY_JINGLE_REASON_INCOMPATIBLE_PARAMETERS: &lt;incompatible-parameters/>
 * @WOCKY_JINGLE_REASON_MEDIA_ERROR: &lt;media-error/>
 * @WOCKY_JINGLE_REASON_SECURITY_ERROR: &lt;security-error/>
 * @WOCKY_JINGLE_REASON_SUCCESS: &lt;success/>
 * @WOCKY_JINGLE_REASON_TIMEOUT: &lt;timeout/>
 * @WOCKY_JINGLE_REASON_UNSUPPORTED_APPLICATIONS: &lt;unsupported-applications/>
 * @WOCKY_JINGLE_REASON_UNSUPPORTED_TRANSPORTS:&lt;unsupported-transports/>
 *
 * The reason for a Jingle action occurring—specifically, the reason for
 * terminating a call. See <ulink
 * url="http://xmpp.org/extensions/xep-0166.html#def-reason">XEP-0166 Jingle
 * §7.4</ulink> for definitions of the codes.
 */
typedef enum
{
  WOCKY_JINGLE_REASON_UNKNOWN,
  WOCKY_JINGLE_REASON_ALTERNATIVE_SESSION,
  WOCKY_JINGLE_REASON_BUSY,
  WOCKY_JINGLE_REASON_CANCEL,
  WOCKY_JINGLE_REASON_CONNECTIVITY_ERROR,
  WOCKY_JINGLE_REASON_DECLINE,
  WOCKY_JINGLE_REASON_EXPIRED,
  WOCKY_JINGLE_REASON_FAILED_APPLICATION,
  WOCKY_JINGLE_REASON_FAILED_TRANSPORT,
  WOCKY_JINGLE_REASON_GENERAL_ERROR,
  WOCKY_JINGLE_REASON_GONE,
  WOCKY_JINGLE_REASON_INCOMPATIBLE_PARAMETERS,
  WOCKY_JINGLE_REASON_MEDIA_ERROR,
  WOCKY_JINGLE_REASON_SECURITY_ERROR,
  WOCKY_JINGLE_REASON_SUCCESS,
  WOCKY_JINGLE_REASON_TIMEOUT,
  WOCKY_JINGLE_REASON_UNSUPPORTED_APPLICATIONS,
  WOCKY_JINGLE_REASON_UNSUPPORTED_TRANSPORTS
} WockyJingleReason;


#endif /* __WOCKY_JINGLE_TYPES_H__ */
