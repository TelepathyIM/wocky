#if !defined (WOCKY_H_INSIDE) && !defined (WOCKY_COMPILATION)
# error "Only <wocky/wocky.h> can be included directly."
#endif

#ifndef __WOCKY_NAMESPACES_H__
#define __WOCKY_NAMESPACES_H__

#define WOCKY_XMPP_NS_JABBER_CLIENT \
  "jabber:client"

#define WOCKY_XMPP_NS_STREAM \
  "http://etherx.jabber.org/streams"

#define WOCKY_XMPP_NS_STREAMS \
  "urn:ietf:params:xml:ns:xmpp-streams"

#define WOCKY_XMPP_NS_BIND \
  "urn:ietf:params:xml:ns:xmpp-bind"

#define WOCKY_XMPP_NS_SESSION \
  "urn:ietf:params:xml:ns:xmpp-session"

#define WOCKY_XMPP_NS_TLS \
  "urn:ietf:params:xml:ns:xmpp-tls"

#define WOCKY_XMPP_NS_SASL_AUTH \
  "urn:ietf:params:xml:ns:xmpp-sasl"

#define WOCKY_NS_DISCO_INFO \
  "http://jabber.org/protocol/disco#info"

#define WOCKY_NS_DISCO_ITEMS \
  "http://jabber.org/protocol/disco#items"

#define WOCKY_XMPP_NS_XHTML_IM \
  "http://jabber.org/protocol/xhtml-im"

#define WOCKY_XMPP_NS_IBB \
  "http://jabber.org/protocol/ibb"

#define WOCKY_XMPP_NS_AMP \
  "http://jabber.org/protocol/amp"

#define WOCKY_W3C_NS_XHTML \
  "http://www.w3.org/1999/xhtml"

#define WOCKY_TELEPATHY_NS_CAPS \
  "http://telepathy.freedesktop.org/caps"

#define WOCKY_TELEPATHY_NS_TUBES \
  "http://telepathy.freedesktop.org/xmpp/tubes"

#define WOCKY_TELEPATHY_NS_OLPC_ACTIVITY_PROPS \
  "http://laptop.org/xmpp/activity-properties"

#define WOCKY_XMPP_NS_SI \
  "http://jabber.org/protocol/si"

#define WOCKY_XMPP_NS_FEATURENEG \
  "http://jabber.org/protocol/feature-neg"

#define WOCKY_XMPP_NS_DATA \
  "jabber:x:data"

#define WOCKY_XMPP_NS_EVENT \
  "jabber:x:event"

#define WOCKY_XMPP_NS_DELAY \
  "jabber:x:delay"

#define WOCKY_XMPP_NS_STANZAS \
  "urn:ietf:params:xml:ns:xmpp-stanzas"

#define WOCKY_XMPP_NS_IQ_OOB \
  "jabber:iq:oob"

#define WOCKY_XMPP_NS_X_OOB \
  "jabber:x:oob"


#define WOCKY_TELEPATHY_NS_CLIQUE \
  "http://telepathy.freedesktop.org/xmpp/clique"

#define WOCKY_XEP77_NS_REGISTER \
  "jabber:iq:register"

/* Namespaces for XEP-0166 draft v0.15, the most capable Jingle dialect
 * supported by telepathy-gabble < 0.7.16, including the versions shipped with
 * Maemo Chinook and Diablo.
 */
#define WOCKY_XMPP_NS_JINGLE015 \
  "http://jabber.org/protocol/jingle"

/* RTP audio capability in Jingle v0.15 (obsoleted by WOCKY_XMPP_NS_JINGLE_RTP) */
#define WOCKY_XMPP_NS_JINGLE_DESCRIPTION_AUDIO \
  "http://jabber.org/protocol/jingle/description/audio"
/* RTP video capability in Jingle v0.15 (obsoleted by WOCKY_XMPP_NS_JINGLE_RTP) */
#define WOCKY_XMPP_NS_JINGLE_DESCRIPTION_VIDEO \
  "http://jabber.org/protocol/jingle/description/video"

/* XEP-0166 Jingle */
#define WOCKY_XMPP_NS_JINGLE \
  "urn:xmpp:jingle:1"
#define WOCKY_XMPP_NS_JINGLE_ERRORS \
  "urn:xmpp:jingle:errors:1"

/* XEP-0167 (Jingle RTP) */
#define WOCKY_XMPP_NS_JINGLE_RTP \
  "urn:xmpp:jingle:apps:rtp:1"
#define WOCKY_XMPP_NS_JINGLE_RTP_ERRORS \
  "urn:xmpp:jingle:apps:rtp:errors:1"
#define WOCKY_XMPP_NS_JINGLE_RTP_INFO \
  "urn:xmpp:jingle:apps:rtp:info:1"
#define WOCKY_XMPP_NS_JINGLE_RTP_AUDIO \
  "urn:xmpp:jingle:apps:rtp:audio"
#define WOCKY_XMPP_NS_JINGLE_RTP_VIDEO \
  "urn:xmpp:jingle:apps:rtp:video"

/* ProtoXEPs for rtcp-fb and rtp-hdrext */
#define WOCKY_XMPP_NS_JINGLE_RTCP_FB       "urn:xmpp:jingle:apps:rtp:rtcp-fb:0"
#define WOCKY_XMPP_NS_JINGLE_RTP_HDREXT    "urn:xmpp:jingle:apps:rtp:rtp-hdrext:0"

/* Google's Jingle dialect */
#define WOCKY_XMPP_NS_GOOGLE_SESSION       "http://www.google.com/session"
/* Audio capability in Google Jingle dialect */
#define WOCKY_XMPP_NS_GOOGLE_SESSION_PHONE "http://www.google.com/session/phone"
/* Video capability in Google's Jingle dialect */
#define WOCKY_XMPP_NS_GOOGLE_SESSION_VIDEO "http://www.google.com/session/video"
/* File transfer capability in Google's Jingle dialect */
#define WOCKY_XMPP_NS_GOOGLE_SESSION_SHARE "http://www.google.com/session/share"

/* google-p2p transport */
#define WOCKY_XMPP_NS_GOOGLE_TRANSPORT_P2P "http://www.google.com/transport/p2p"
/* Jingle RAW-UDP transport */
#define WOCKY_XMPP_NS_JINGLE_TRANSPORT_RAWUDP "urn:xmpp:jingle:transports:raw-udp:1"
/* Jingle ICE-UDP transport */
#define WOCKY_XMPP_NS_JINGLE_TRANSPORT_ICEUDP "urn:xmpp:jingle:transports:ice-udp:1"

/* Here are some "quirk" pseudo-namespaces imported from Gabble with the Jingle
 * code and used to work around known bugs in particular Jingle
 * implementations. ASCII BEL is illegal in XML, so these cannot appear in real
 * XMPP capabilities.
 */
/* Gabble 0.7.x with 16 <= x < 29 omits @creator on <content/> */
#define WOCKY_QUIRK_OMITS_CONTENT_CREATORS "\x07omits-content-creators"
/* The Google Webmail client doesn't support some features */
#define WOCKY_QUIRK_GOOGLE_WEBMAIL_CLIENT "\x07google-webmail-client"
/* The Android GTalk client needs a quirk for component names */
#define WOCKY_QUIRK_ANDROID_GTALK_CLIENT "\x07android-gtalk-client"

/* Google's extension for retrieving STUN and relay information from the
 * server. */
#define WOCKY_XMPP_NS_GOOGLE_JINGLE_INFO   "google:jingleinfo"

/* legacy namespaces */
#define WOCKY_JABBER_NS_AUTH \
  "jabber:iq:auth"

#define WOCKY_JABBER_NS_AUTH_FEATURE \
  "http://jabber.org/features/iq-auth"

#define WOCKY_GOOGLE_NS_AUTH \
  "http://www.google.com/talk/protocol/auth"

#define WOCKY_XMPP_NS_ROSTER \
  "jabber:iq:roster"

#define WOCKY_XMPP_NS_PUBSUB \
  "http://jabber.org/protocol/pubsub"

#define WOCKY_XMPP_NS_PUBSUB_EVENT \
  WOCKY_XMPP_NS_PUBSUB "#event"

#define WOCKY_XMPP_NS_PUBSUB_OWNER \
  WOCKY_XMPP_NS_PUBSUB "#owner"

#define WOCKY_XMPP_NS_PUBSUB_NODE_CONFIG \
  WOCKY_XMPP_NS_PUBSUB "#node_config"

#define WOCKY_XMPP_NS_PUBSUB_ERRORS \
  WOCKY_XMPP_NS_PUBSUB "#errors"

#define WOCKY_XMPP_NS_PING \
  "urn:xmpp:ping"

#define WOCKY_NS_MUC \
  "http://jabber.org/protocol/muc"

#define WOCKY_NS_MUC_USER \
  WOCKY_NS_MUC "#user"

#define WOCKY_NS_MUC_ADMIN \
  WOCKY_NS_MUC "#admin"

#define WOCKY_NS_MUC_OWNER \
  WOCKY_NS_MUC "#owner"

#define WOCKY_NS_MUC_UNIQUE \
  WOCKY_NS_MUC "#unique"

#define WOCKY_NS_CHATSTATE \
  "http://jabber.org/protocol/chatstates"

#define WOCKY_NS_GOOGLE_SESSION_PHONE \
  "http://www.google.com/session/phone"

#define WOCKY_NS_GOOGLE_SESSION_VIDEO \
  "http://www.google.com/session/video"

#define WOCKY_NS_VCARD_TEMP           "vcard-temp"
#define WOCKY_NS_VCARD_TEMP_UPDATE    "vcard-temp:x:update"

#endif /* #ifndef __WOCKY_NAMESPACES_H__ */
