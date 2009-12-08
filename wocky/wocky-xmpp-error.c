/*
 * wocky-xmpp-error.c - Source for Wocky's XMPP error handling API
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

#include "wocky-xmpp-error.h"
#include "wocky-utils.h"

#include <stdlib.h>
#include <stdio.h>

#include "wocky-namespaces.h"

#define MAX_LEGACY_ERRORS 3

typedef struct {
    const gchar *name;
    const gchar *description;
    const gchar *type;
    guint specialises;
    const gchar *namespace;
    const guint16 legacy_errors[MAX_LEGACY_ERRORS];
} XmppErrorSpec;

static const XmppErrorSpec xmpp_errors[NUM_WOCKY_XMPP_ERRORS] =
{
    {
      "undefined-condition",
      "application-specific condition",
      NULL,
      0,
      WOCKY_XMPP_NS_STANZAS,
      { 500, 0, },
    },
    {
      "redirect",
      "the recipient or server is redirecting requests for this information "
      "to another entity",
      "modify",
      0,
      WOCKY_XMPP_NS_STANZAS,
      { 302, 0, },
    },

    {
      "gone",
      "the recipient or server can no longer be contacted at this address",
      "modify",
      0,
      WOCKY_XMPP_NS_STANZAS,
      { 302, 0, },
    },

    {
      "bad-request",
      "the sender has sent XML that is malformed or that cannot be processed",
      "modify",
      0,
      WOCKY_XMPP_NS_STANZAS,
      { 400, 0, },
    },
    {
      "unexpected-request",
      "the recipient or server understood the request but was not expecting "
      "it at this time",
      "wait",
      0,
      WOCKY_XMPP_NS_STANZAS,
      { 400, 0, },
    },
    {
      "jid-malformed",
      "the sending entity has provided or communicated an XMPP address or "
      "aspect thereof (e.g., a resource identifier) that does not adhere "
      "to the syntax defined in Addressing Scheme (Section 3)",
      "modify",
      0,
      WOCKY_XMPP_NS_STANZAS,
      { 400, 0, },
    },

    {
      "not-authorized",
      "the sender must provide proper credentials before being allowed to "
      "perform the action, or has provided improper credentials",
      "auth",
      0,
      WOCKY_XMPP_NS_STANZAS,
      { 401, 0, },
    },

    {
      "payment-required",
      "the requesting entity is not authorized to access the requested "
      "service because payment is required",
      "auth",
      0,
      WOCKY_XMPP_NS_STANZAS,
      { 402, 0, },
    },

    {
      "forbidden",
      "the requesting entity does not possess the required permissions to "
      "perform the action",
      "auth",
      0,
      WOCKY_XMPP_NS_STANZAS,
      { 403, 0, },
    },

    {
      "item-not-found",
      "the addressed JID or item requested cannot be found",
      "cancel",
      0,
      WOCKY_XMPP_NS_STANZAS,
      { 404, 0, },
    },
    {
      "recipient-unavailable",
      "the intended recipient is temporarily unavailable",
      "wait",
      0,
      WOCKY_XMPP_NS_STANZAS,
      { 404, 0, },
    },
    {
      "remote-server-not-found",
      "a remote server or service specified as part or all of the JID of the "
      "intended recipient (or required to fulfill a request) could not be "
      "contacted within a reasonable amount of time",
      "cancel",
      0,
      WOCKY_XMPP_NS_STANZAS,
      { 404, 0, },
    },

    {
      "not-allowed",
      "the recipient or server does not allow any entity to perform the action",
      "cancel",
      0,
      WOCKY_XMPP_NS_STANZAS,
      { 405, 0, },
    },

    {
      "not-acceptable",
      "the recipient or server understands the request but is refusing to "
      "process it because it does not meet criteria defined by the recipient "
      "or server (e.g., a local policy regarding acceptable words in messages)",
      "modify",
      0,
      WOCKY_XMPP_NS_STANZAS,
      { 406, 0, },
    },

    {
      "registration-required",
      "the requesting entity is not authorized to access the requested service "
      "because registration is required",
      "auth",
      0,
      WOCKY_XMPP_NS_STANZAS,
      { 407, 0, },
    },
    {
      "subscription-required",
      "the requesting entity is not authorized to access the requested service "
      "because a subscription is required",
      "auth",
      0,
      WOCKY_XMPP_NS_STANZAS,
      { 407, 0, },
    },

    {
      "remote-server-timeout",
      "a remote server or service specified as part or all of the JID of the "
      "intended recipient (or required to fulfill a request) could not be "
      "contacted within a reasonable amount of time",
      "wait",
      0,
      WOCKY_XMPP_NS_STANZAS,
      { 408, 504, 0, },
    },

    {
      "conflict",
      "access cannot be granted because an existing resource or session exists "
      "with the same name or address",
      "cancel",
      0,
      WOCKY_XMPP_NS_STANZAS,
      { 409, 0, },
    },

    {
      "internal-server-error",
      "the server could not process the stanza because of a misconfiguration "
      "or an otherwise-undefined internal server error",
      "wait",
      0,
      WOCKY_XMPP_NS_STANZAS,
      { 500, 0, },
    },
    {
      "resource-constraint",
      "the server or recipient lacks the system resources necessary to service "
      "the request",
      "wait",
      0,
      WOCKY_XMPP_NS_STANZAS,
      { 500, 0, },
    },

    {
      "feature-not-implemented",
      "the feature requested is not implemented by the recipient or server and "
      "therefore cannot be processed",
      "cancel",
      0,
      WOCKY_XMPP_NS_STANZAS,
      { 501, 0, },
    },

    {
      "service-unavailable",
      "the server or recipient does not currently provide the requested "
      "service",
      "cancel",
      0,
      WOCKY_XMPP_NS_STANZAS,
      { 502, 503, 510, },
    },

    {
      "out-of-order",
      "the request cannot occur at this point in the state machine",
      "cancel",
      WOCKY_XMPP_ERROR_UNEXPECTED_REQUEST,
      WOCKY_XMPP_NS_JINGLE_ERRORS,
      { 0, },
    },

    {
      "unknown-session",
      "the 'sid' attribute specifies a session that is unknown to the "
      "recipient",
      "cancel",
      WOCKY_XMPP_ERROR_BAD_REQUEST,
      WOCKY_XMPP_NS_JINGLE_ERRORS,
      { 0, },
    },

    {
      "unsupported-transports",
      "the recipient does not support any of the desired content transport "
      "methods",
      "cancel",
      WOCKY_XMPP_ERROR_FEATURE_NOT_IMPLEMENTED,
      WOCKY_XMPP_NS_JINGLE_ERRORS,
      { 0, },
    },

    {
      "unsupported-content",
      "the recipient does not support any of the desired content description"
      "formats",
      "cancel",
      WOCKY_XMPP_ERROR_FEATURE_NOT_IMPLEMENTED,
      WOCKY_XMPP_NS_JINGLE_ERRORS,
      { 0, },
    },

    {
      "no-valid-streams",
      "None of the available streams are acceptable.",
      "cancel",
      WOCKY_XMPP_ERROR_BAD_REQUEST,
      WOCKY_XMPP_NS_SI,
      { 400, 0 },
    },

    {
      "bad-profile",
      "The profile is not understood or invalid.",
      "modify",
      WOCKY_XMPP_ERROR_BAD_REQUEST,
      WOCKY_XMPP_NS_SI,
      { 400, 0 },
    },
};

GQuark
wocky_xmpp_error_quark (void)
{
  static GQuark quark = 0;
  if (!quark)
    quark = g_quark_from_static_string ("wocky-xmpp-error");
  return quark;
}

WockyXmppError
wocky_xmpp_error_from_node (WockyXmppNode *error_node)
{
  gint i, j;
  const gchar *error_code_str;

  g_return_val_if_fail (error_node != NULL,
      WOCKY_XMPP_ERROR_UNDEFINED_CONDITION);

  /* First, try to look it up the modern way */
  if (error_node->children)
    {
      /* we loop backwards because the most specific errors are the larger
       * numbers; the >= 0 test is OK because i is signed */
      for (i = NUM_WOCKY_XMPP_ERRORS - 1; i >= 0; i--)
        {
          if (wocky_xmpp_node_get_child_ns (error_node, xmpp_errors[i].name,
                xmpp_errors[i].namespace))
            {
              return i;
            }
        }
    }

  /* Ok, do it the legacy way */
  error_code_str = wocky_xmpp_node_get_attribute (error_node, "code");
  if (error_code_str)
    {
      gint error_code;

      error_code = atoi (error_code_str);

      /* skip UNDEFINED_CONDITION, we want code 500 to be translated
       * to INTERNAL_SERVER_ERROR */
      for (i = 1; i < NUM_WOCKY_XMPP_ERRORS; i++)
        {
          const XmppErrorSpec *spec = &xmpp_errors[i];

          for (j = 0; j < MAX_LEGACY_ERRORS; j++)
            {
              gint cur_code = spec->legacy_errors[j];
              if (cur_code == 0)
                break;

              if (cur_code == error_code)
                return i;
            }
        }
    }

  return WOCKY_XMPP_ERROR_UNDEFINED_CONDITION;
}

/**
 * wocky_xmpp_node_unpack_error:
 *
 * @node: a #WockyXmppNode
 * @type: gchar ** into which to write the XMPP Stanza error type
 * @text: #WockyXmppNode ** to hold the node containing the error description
 * @orig: #WockyXmppNode ** to hold the original XMPP Stanza that triggered
 *        the error: XMPP does not require this to be provided in the error
 * @extra: #WockyXmppNode ** to hold any extra domain-specific XML tags
 *         for the error received.
 * @errnum: #WockyXmppError * to hold the value mapping to the error condition
 *
 * Given an XMPP Stanza error #WockyXmppNode see RFC 3920) this function
 * extracts useful error info.
 *
 * The above parameters are all optional, pass NULL to ignore them.
 *
 * The above data are all optional in XMPP, except for @type, which
 * the XMPP spec requires in all stanza errors. See RFC 3920 [9.3.2].
 *
 * None of the above parameters need be freed, they are owned by the
 * parent #WockyXmppNode @node.
 *
 * Returns: a const gchar * indicating the error condition
 */

const gchar *
wocky_xmpp_error_unpack_node (WockyXmppNode *node,
    const gchar **type,
    WockyXmppNode **text,
    WockyXmppNode **orig,
    WockyXmppNode **extra,
    WockyXmppError *errnum)
{
  WockyXmppNode *error = NULL;
  WockyXmppNode *mesg = NULL;
  WockyXmppNode *xtra = NULL;
  const gchar *cond = NULL;
  GSList *child = NULL;
  GQuark stanza = g_quark_from_string (WOCKY_XMPP_NS_STANZAS);

  g_assert (node != NULL);

  error = wocky_xmpp_node_get_child (node, "error");

  /* not an error? weird, in any case */
  if (error == NULL)
    return NULL;

  if (type != NULL)
    *type = wocky_xmpp_node_get_attribute (error, "type");

  for (child = error->children; child != NULL; child = g_slist_next (child))
    {
      WockyXmppNode *c = child->data;
      if (c->ns != stanza)
        xtra = c;
      else if (wocky_strdiff (c->name, "text"))
        {
          cond = c->name;
        }
      else
        mesg = c;
    }

  if (text != NULL)
    *text = mesg;

  if (extra != NULL)
    *extra = xtra;

  if (orig != NULL)
    {
      WockyXmppNode *first = wocky_xmpp_node_get_first_child (node);
      if (first != error)
        *orig = first;
      else
        *orig = NULL;
    }

  if (errnum != NULL)
    *errnum = wocky_xmpp_error_from_node (error);

  return cond;
}

/*
 * See RFC 3920: 4.7 Stream Errors, 9.3 Stanza Errors.
 */
WockyXmppNode *
wocky_xmpp_error_to_node (WockyXmppError error,
    WockyXmppNode *parent_node,
    const gchar *errmsg)
{
  const XmppErrorSpec *spec, *extra;
  WockyXmppNode *error_node, *node;
  gchar str[6];

  g_return_val_if_fail (error != WOCKY_XMPP_ERROR_UNDEFINED_CONDITION &&
      error < NUM_WOCKY_XMPP_ERRORS, NULL);

  if (xmpp_errors[error].specialises)
    {
      extra = &xmpp_errors[error];
      spec = &xmpp_errors[extra->specialises];
    }
  else
    {
      extra = NULL;
      spec = &xmpp_errors[error];
    }

  error_node = wocky_xmpp_node_add_child (parent_node, "error");

  sprintf (str, "%d", spec->legacy_errors[0]);
  wocky_xmpp_node_set_attribute (error_node, "code", str);

  if (spec->type)
    {
      wocky_xmpp_node_set_attribute (error_node, "type", spec->type);
    }

  node = wocky_xmpp_node_add_child (error_node, spec->name);
  wocky_xmpp_node_set_ns (node, WOCKY_XMPP_NS_STANZAS);

  if (extra != NULL)
    {
      node = wocky_xmpp_node_add_child (error_node, extra->name);
      wocky_xmpp_node_set_ns (node, extra->namespace);
    }

  if (NULL != errmsg)
    {
      node = wocky_xmpp_node_add_child (error_node, "text");
      wocky_xmpp_node_set_content (node, errmsg);
    }

  return error_node;
}

const gchar *
wocky_xmpp_error_string (WockyXmppError error)
{
  if (error < NUM_WOCKY_XMPP_ERRORS)
    return xmpp_errors[error].name;
  else
    return NULL;
}

const gchar *
wocky_xmpp_error_description (WockyXmppError error)
{
  if (error < NUM_WOCKY_XMPP_ERRORS)
    return xmpp_errors[error].description;
  else
    return NULL;
}

/**
 * wocky_xmpp_stream_error_quark
 *
 * Get the error quark used for stream errors
 *
 * Returns: the quark for stream errors.
 */
GQuark
wocky_xmpp_stream_error_quark (void)
{
  static GQuark quark = 0;

  if (quark == 0)
    quark = g_quark_from_static_string ("wocky-xmpp-stream-error");

  return quark;
}

typedef struct
{
  const gchar *name;
  WockyXmppStreamError error;
} StreamErrorName;

static const StreamErrorName stream_errors[] =
{
    { "bad-format", WOCKY_XMPP_STREAM_ERROR_BAD_FORMAT },
    { "bad-namespace-prefix", WOCKY_XMPP_STREAM_ERROR_BAD_NAMESPACE_PREFIX },
    { "conflict", WOCKY_XMPP_STREAM_ERROR_CONFLICT },
    { "connection-timeout", WOCKY_XMPP_STREAM_ERROR_CONNECTION_TIMEOUT },
    { "host-gone", WOCKY_XMPP_STREAM_ERROR_HOST_GONE },
    { "host-unknown", WOCKY_XMPP_STREAM_ERROR_HOST_UNKNOWN },
    { "improper-addressing", WOCKY_XMPP_STREAM_ERROR_IMPROPER_ADDRESSING },
    { "internal-server-error", WOCKY_XMPP_STREAM_ERROR_INTERNAL_SERVER_ERROR },
    { "invalid-from", WOCKY_XMPP_STREAM_ERROR_INVALID_FROM },
    { "invalid-id", WOCKY_XMPP_STREAM_ERROR_INVALID_ID },
    { "invalid-namespace", WOCKY_XMPP_STREAM_ERROR_INVALID_NAMESPACE },
    { "invalid-xml", WOCKY_XMPP_STREAM_ERROR_INVALID_XML },
    { "not-authorized", WOCKY_XMPP_STREAM_ERROR_NOT_AUTHORIZED },
    { "policy-violation", WOCKY_XMPP_STREAM_ERROR_POLICY_VIOLATION },
    { "remote-connection-failed",
      WOCKY_XMPP_STREAM_ERROR_REMOTE_CONNECTION_FAILED },
    { "resource-constraint", WOCKY_XMPP_STREAM_ERROR_RESOURCE_CONSTRAINT },
    { "restricted-xml", WOCKY_XMPP_STREAM_ERROR_RESTRICTED_XML },
    { "see-other-host", WOCKY_XMPP_STREAM_ERROR_SEE_OTHER_HOST },
    { "system-shutdown", WOCKY_XMPP_STREAM_ERROR_SYSTEM_SHUTDOWN },
    { "undefined-condition", WOCKY_XMPP_STREAM_ERROR_UNDEFINED_CONDITION },
    { "unsupported-encoding", WOCKY_XMPP_STREAM_ERROR_UNSUPPORTED_ENCODING },
    { "unsupported-stanza-type",
      WOCKY_XMPP_STREAM_ERROR_UNSUPPORTED_STANZA_TYPE },
    { "unsupported-version", WOCKY_XMPP_STREAM_ERROR_UNSUPPORTED_VERSION },
    { "xml-not-well-formed", WOCKY_XMPP_STREAM_ERROR_XML_NOT_WELL_FORMED },
    { NULL, WOCKY_XMPP_STREAM_ERROR_UNKNOWN },
};

WockyXmppStreamError
wocky_xmpp_stream_error_from_node (WockyXmppNode *node)
{
  guint i;

  for (i = 0; stream_errors[i].name != NULL; i++)
    {
      if (wocky_xmpp_node_get_child_ns (node, stream_errors[i].name,
            WOCKY_XMPP_NS_STREAMS) != NULL)
        {
          return stream_errors[i].error;
        }
    }

  return WOCKY_XMPP_STREAM_ERROR_UNKNOWN;
}
