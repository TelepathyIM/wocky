/*
 * wocky.h - Header for general functions
 * Copyright (C) 2009 Collabora Ltd.
 * @author Guillaume Desmottes <guillaume.desmottes@collabora.co.uk>
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

#ifndef __WOCKY_H__
#define __WOCKY_H__

#include <glib.h>

#define WOCKY_H_INSIDE
#include "wocky-auth-handler.h"
#include "wocky-auth-registry.h"
#include "wocky-bare-contact.h"
#include "wocky-c2s-porter.h"
#include "wocky-caps-cache.h"
#include "wocky-caps-hash.h"
#include "wocky-connector.h"
#include "wocky-contact-factory.h"
#include "wocky-contact.h"
#include "wocky-data-form.h"
#include "wocky-debug.h"
#include "wocky-disco-identity.h"
#include "wocky-enumtypes.h"
#include "wocky-jabber-auth-digest.h"
#include "wocky-jabber-auth.h"
#include "wocky-jabber-auth-password.h"
#include "wocky-jingle-content.h"
#include "wocky-jingle-factory.h"
#include "wocky-jingle-info.h"
#include "wocky-jingle-media-rtp.h"
#include "wocky-jingle-session.h"
#include "wocky-jingle-transport-google.h"
#include "wocky-jingle-transport-iceudp.h"
#include "wocky-jingle-transport-iface.h"
#include "wocky-jingle-transport-rawudp.h"
#include "wocky-jingle-types.h"
#include "wocky-ll-connection-factory.h"
#include "wocky-ll-connector.h"
#include "wocky-ll-contact.h"
#include "wocky-loopback-stream.h"
#include "wocky-meta-porter.h"
#include "wocky-muc.h"
#include "wocky-namespaces.h"
#include "wocky-node.h"
#include "wocky-node-tree.h"
#include "wocky-pep-service.h"
#include "wocky-ping.h"
#include "wocky-porter.h"
#include "wocky-pubsub-helpers.h"
#include "wocky-pubsub-node.h"
#include "wocky-pubsub-node-protected.h"
#include "wocky-pubsub-service.h"
#include "wocky-pubsub-service-protected.h"
#include "wocky-resource-contact.h"
#include "wocky-roster.h"
#include "wocky-sasl-auth.h"
#include "wocky-sasl-digest-md5.h"
#include "wocky-sasl-plain.h"
#include "wocky-sasl-scram.h"
#include "wocky-sasl-utils.h"
#include "wocky-session.h"
#include "wocky-stanza.h"
#include "wocky-tls-connector.h"
#include "wocky-tls.h"
#include "wocky-tls-handler.h"
#include "wocky-types.h"
#include "wocky-utils.h"
#include "wocky-xep-0115-capabilities.h"
#include "wocky-xmpp-connection.h"
#include "wocky-xmpp-error.h"
#include "wocky-xmpp-reader.h"
#include "wocky-xmpp-writer.h"
#undef WOCKY_H_INSIDE

G_BEGIN_DECLS

void wocky_init (void);

void wocky_deinit (void);

G_END_DECLS

#endif /* #ifndef __WOCKY_H__*/
