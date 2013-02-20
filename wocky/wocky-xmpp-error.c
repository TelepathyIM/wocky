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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "wocky-xmpp-error.h"

#include <stdlib.h>
#include <stdio.h>

#include "wocky-namespaces.h"
#include "wocky-utils.h"

/* Definitions of XMPP core stanza errors, as per RFC 3920 §9.3; plus the
 * corresponding legacy error codes as described by XEP-0086.
 */
#define MAX_LEGACY_ERRORS 3

typedef struct {
    const gchar *description;
    WockyXmppErrorType type;
    const guint16 legacy_errors[MAX_LEGACY_ERRORS];
} XmppErrorSpec;

static const XmppErrorSpec xmpp_errors[NUM_WOCKY_XMPP_ERRORS] =
{
    /* undefined-condition */
    {
      "application-specific condition",
      WOCKY_XMPP_ERROR_TYPE_CANCEL,
      { 500, 0, },
    },

    /* redirect */
    {
      "the recipient or server is redirecting requests for this information "
      "to another entity",
      WOCKY_XMPP_ERROR_TYPE_MODIFY,
      { 302, 0, },
    },

    /* gone */
    {
      "the recipient or server can no longer be contacted at this address",
      WOCKY_XMPP_ERROR_TYPE_MODIFY,
      { 302, 0, },
    },

    /* bad-request */
    {
      "the sender has sent XML that is malformed or that cannot be processed",
      WOCKY_XMPP_ERROR_TYPE_MODIFY,
      { 400, 0, },
    },

    /* unexpected-request */
    {
      "the recipient or server understood the request but was not expecting "
      "it at this time",
      WOCKY_XMPP_ERROR_TYPE_WAIT,
      { 400, 0, },
    },

    /* jid-malformed */
    {
      "the sending entity has provided or communicated an XMPP address or "
      "aspect thereof (e.g., a resource identifier) that does not adhere "
      "to the syntax defined in Addressing Scheme (Section 3)",
      WOCKY_XMPP_ERROR_TYPE_MODIFY,
      { 400, 0, },
    },

    /* not-authorized */
    {
      "the sender must provide proper credentials before being allowed to "
      "perform the action, or has provided improper credentials",
      WOCKY_XMPP_ERROR_TYPE_AUTH,
      { 401, 0, },
    },

    /* payment-required */
    {
      "the requesting entity is not authorized to access the requested "
      "service because payment is required",
      WOCKY_XMPP_ERROR_TYPE_AUTH,
      { 402, 0, },
    },

    /* forbidden */
    {
      "the requesting entity does not possess the required permissions to "
      "perform the action",
      WOCKY_XMPP_ERROR_TYPE_AUTH,
      { 403, 0, },
    },

    /* item-not-found */
    {
      "the addressed JID or item requested cannot be found",
      WOCKY_XMPP_ERROR_TYPE_CANCEL,
      { 404, 0, },
    },

    /* recipient-unavailable */
    {
      "the intended recipient is temporarily unavailable",
      WOCKY_XMPP_ERROR_TYPE_WAIT,
      { 404, 0, },
    },

    /* remote-server-not-found */
    {
      "a remote server or service specified as part or all of the JID of the "
      "intended recipient (or required to fulfill a request) could not be "
      "contacted within a reasonable amount of time",
      WOCKY_XMPP_ERROR_TYPE_CANCEL,
      { 404, 0, },
    },

    /* not-allowed */
    {
      "the recipient or server does not allow any entity to perform the action",
      WOCKY_XMPP_ERROR_TYPE_CANCEL,
      { 405, 0, },
    },

    /* not-acceptable */
    {
      "the recipient or server understands the request but is refusing to "
      "process it because it does not meet criteria defined by the recipient "
      "or server (e.g., a local policy regarding acceptable words in messages)",
      WOCKY_XMPP_ERROR_TYPE_MODIFY,
      { 406, 0, },
    },

    /* registration-required */
    {
      "the requesting entity is not authorized to access the requested service "
      "because registration is required",
      WOCKY_XMPP_ERROR_TYPE_AUTH,
      { 407, 0, },
    },
    /* subscription-required */
    {
      "the requesting entity is not authorized to access the requested service "
      "because a subscription is required",
      WOCKY_XMPP_ERROR_TYPE_AUTH,
      { 407, 0, },
    },

    /* remote-server-timeout */
    {
      "a remote server or service specified as part or all of the JID of the "
      "intended recipient (or required to fulfill a request) could not be "
      "contacted within a reasonable amount of time",
      WOCKY_XMPP_ERROR_TYPE_WAIT,
      { 504, 408, 0, },
    },

    /* conflict */
    {
      "access cannot be granted because an existing resource or session exists "
      "with the same name or address",
      WOCKY_XMPP_ERROR_TYPE_CANCEL,
      { 409, 0, },
    },

    /* internal-server-error */
    {
      "the server could not process the stanza because of a misconfiguration "
      "or an otherwise-undefined internal server error",
      WOCKY_XMPP_ERROR_TYPE_WAIT,
      { 500, 0, },
    },

    /* resource-constraint */
    {
      "the server or recipient lacks the system resources necessary to service "
      "the request",
      WOCKY_XMPP_ERROR_TYPE_WAIT,
      { 500, 0, },
    },

    /* feature-not-implemented */
    {
      "the feature requested is not implemented by the recipient or server and "
      "therefore cannot be processed",
      WOCKY_XMPP_ERROR_TYPE_CANCEL,
      { 501, 0, },
    },

    /* service-unavailable */
    {
      "the server or recipient does not currently provide the requested "
      "service",
      WOCKY_XMPP_ERROR_TYPE_CANCEL,
      { 503, 502, 510, },
    },

    /* policy-violation */
    {
      "the entity has violated some local service policy (e.g., a message "
      "contains words that are prohibited by the service)",
      /* TODO: should support either MODIFY or WAIT depending on the policy
       * being violated */
      WOCKY_XMPP_ERROR_TYPE_MODIFY,
      { 406, 0, },
    },
};

GQuark
wocky_xmpp_error_quark (void)
{
  static GQuark quark = 0;

  if (quark == 0)
    quark = g_quark_from_static_string (WOCKY_XMPP_NS_STANZAS);

  return quark;
}

/**
 * wocky_xmpp_error_string:
 * @error: a core stanza error
 *
 * <!-- -->
 *
 * Returns: the name of the tag corresponding to @error
 */
const gchar *
wocky_xmpp_error_string (WockyXmppError error)
{
  return wocky_enum_to_nick (WOCKY_TYPE_XMPP_ERROR, error);
}

/**
 * wocky_xmpp_error_description:
 * @error: a core stanza error
 *
 * <!-- -->
 *
 * Returns: a description of the error, in English, as specified in XMPP Core
 */
const gchar *
wocky_xmpp_error_description (WockyXmppError error)
{
  if (error < NUM_WOCKY_XMPP_ERRORS)
    return xmpp_errors[error].description;
  else
    return NULL;
}

static GList *error_domains = NULL;

/**
 * wocky_xmpp_error_register_domain
 * @domain: a description of the error domain
 *
 * Registers a new set of application-specific stanza errors. This allows
 * GErrors in that domain to be passed to wocky_stanza_error_to_node(), and to
 * be recognized and returned by wocky_xmpp_error_extract() (and
 * wocky_stanza_extract_errors(), by extension).
 */
void
wocky_xmpp_error_register_domain (WockyXmppErrorDomain *domain)
{
  error_domains = g_list_prepend (error_domains, domain);
}

static WockyXmppErrorDomain *
xmpp_error_find_domain (GQuark domain)
{
  GList *l;

  for (l = error_domains; l != NULL; l = l->next)
    {
      WockyXmppErrorDomain *d = l->data;

      if (d->domain == domain)
        return d;
    }

  return NULL;
}

/**
 * wocky_xmpp_stanza_error_to_string:
 * @error: an error in the domain %WOCKY_XMPP_ERROR, or another domain
 *  registered with wocky_xmpp_error_register_domain() (such as
 *  %WOCKY_JINGLE_ERROR).
 *
 * Returns the name of the XMPP stanza error element represented by @error.
 * This is intended for use in debugging messages, with %GErrors returned by
 * wocky_stanza_extract_errors().
 *
 * Returns: the error code as a string, or %NULL if
 *  <code>error-&gt;domain</code> is not known to Wocky.
 */
const gchar *
wocky_xmpp_stanza_error_to_string (GError *error)
{
  g_return_val_if_fail (error != NULL, NULL);

  if (error->domain == WOCKY_XMPP_ERROR)
    {
      return wocky_enum_to_nick (WOCKY_TYPE_XMPP_ERROR, error->code);
    }
  else
    {
      WockyXmppErrorDomain *domain = xmpp_error_find_domain (error->domain);

      if (domain != NULL)
        return wocky_enum_to_nick (domain->enum_type, error->code);
      else
        return NULL;
    }
}

/* Static, but bears documenting.
 *
 * xmpp_error_from_node_for_ns:
 * @node: a node believed to contain an error child
 * @ns: the namespace for errors corresponding to @enum_type
 * @enum_type: a GEnum of error codes
 * @code: location at which to store an error code
 *
 * Scans @node's children for nodes in @ns whose name corresponds to a nickname
 * of a value of @enum_type, storing the value in @code if found.
 *
 * Returns: %TRUE if an error code was retrieved.
 */
static gboolean
xmpp_error_from_node_for_ns (
    WockyNode *node,
    GQuark ns,
    GType enum_type,
    gint *code)
{
  GSList *l;

  for (l = node->children; l != NULL; l = l->next)
    {
      WockyNode *child = l->data;

      if (wocky_node_has_ns_q (child, ns) &&
          wocky_enum_from_nick (enum_type, child->name, code))
        return TRUE;
    }

  return FALSE;
}

/* Attempts to divine a WockyXmppError from a legacy numeric code='' attribute
 */
static WockyXmppError
xmpp_error_from_code (WockyNode *error_node,
    WockyXmppErrorType *type)
{
  const gchar *code = wocky_node_get_attribute (error_node, "code");
  gint error_code, i, j;

  if (code == NULL)
    goto out;

  error_code = atoi (code);

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
            {
              if (type != NULL)
                *type = spec->type;

              return i;
            }
        }
    }

out:
  if (type != NULL)
    *type = WOCKY_XMPP_ERROR_TYPE_CANCEL;

  return WOCKY_XMPP_ERROR_UNDEFINED_CONDITION;
}

/**
 * wocky_xmpp_error_extract:
 * @error: the &lt;error/> child of a stanza with type='error'
 * @type: location at which to store the error type
 * @core: location at which to store an error in the domain #WOCKY_XMPP_ERROR
 * @specialized: location at which to store an error in an application-specific
 *               domain, if one is found
 * @specialized_node: location at which to store the node representing an
 *                    application-specific error, if one is found
 *
 * Given an &lt;error/> node, breaks it down into values describing the error.
 * @type and @core are guaranteed to be set; @specialized and @specialized_node
 * will be set if a recognised application-specific error is found, and the
 * latter will be set to %NULL if no application-specific error is found.
 *
 * Any or all of the out parameters may be %NULL to ignore the value.  The
 * value stored in @specialized_node is borrowed from @stanza, and is only
 * valid as long as the latter is alive.
 */
void
wocky_xmpp_error_extract (WockyNode *error,
    WockyXmppErrorType *type,
    GError **core,
    GError **specialized,
    WockyNode **specialized_node)
{
  gboolean found_core_error = FALSE;
  gint core_code = WOCKY_XMPP_ERROR_UNDEFINED_CONDITION;
  GQuark specialized_domain = 0;
  gint specialized_code;
  gboolean have_specialized = FALSE;
  WockyNode *specialized_node_tmp = NULL;
  const gchar *message = NULL;
  GSList *l;

  g_return_if_fail (!wocky_strdiff (error->name, "error"));

  /* The type='' attributes being present and one of the defined five is a
   * MUST; if the other party is getting XMPP *that* wrong, 'cancel' seems like
   * a sensible default. (If the other party only uses legacy error codes, the
   * call to xmpp_error_from_code() below will try to improve on that default.)
   */
  if (type != NULL)
    {
      const gchar *type_attr = wocky_node_get_attribute (error, "type");
      gint type_i;

      if (type_attr != NULL &&
          wocky_enum_from_nick (WOCKY_TYPE_XMPP_ERROR_TYPE, type_attr, &type_i))
        {
          *type = type_i;
          /* Don't let the xmpp_error_from_code() path below clobber the valid
           * type we found.
           */
          type = NULL;
        }
      else
        {
          *type = WOCKY_XMPP_ERROR_TYPE_CANCEL;
        }
    }

  for (l = error->children; l != NULL; l = g_slist_next (l))
    {
      WockyNode *child = l->data;

      if (child->ns == WOCKY_XMPP_ERROR)
        {
          if (!wocky_strdiff (child->name, "text"))
            {
              message = child->content;
            }
          else if (!found_core_error)
            {
              /* See if the element is a XMPP Core stanza error we know about,
               * given that we haven't found one yet.
               */
              found_core_error = wocky_enum_from_nick (WOCKY_TYPE_XMPP_ERROR,
                  child->name, &core_code);
            }
        }
      else if (specialized_node_tmp == NULL)
        {
          WockyXmppErrorDomain *domain;

          specialized_node_tmp = child;

          /* This could be a specialized error; let's check if it's in a
           * namespace we know about, and if so that it's an element name we
           * know.
           */
          domain = xmpp_error_find_domain (child->ns);
          if (domain != NULL)
            {
              specialized_domain = child->ns;
              if (wocky_enum_from_nick (domain->enum_type, child->name,
                    &specialized_code))
                {
                  have_specialized = TRUE;
                }
            }
        }
    }

  /* If we don't have an XMPP Core stanza error yet, maybe the peer uses Þe
   * Olde Numeric Error Codes.
   */
  if (!found_core_error)
    core_code = xmpp_error_from_code (error, type);

  /* okay, time to make some errors */
  if (message == NULL)
    message = "";

  g_set_error_literal (core, WOCKY_XMPP_ERROR, core_code, message);

  if (have_specialized)
    g_set_error_literal (specialized, specialized_domain, specialized_code,
        message);

  if (specialized_node != NULL)
    *specialized_node = specialized_node_tmp;
}

/**
 * wocky_g_error_to_node:
 * @error: an error in the domain #WOCKY_XMPP_ERROR, or in an
 *         application-specific domain registered with
 *         wocky_xmpp_error_register_domain()
 * @parent_node: the node to which to add an error (such as an IQ error)
 *
 * Adds an <code>&lt;error/&gt;</code> node to a stanza corresponding
 * to the error described by @error. If @error is in a domain other
 * than #WOCKY_XMPP_ERROR, both the application-specific error name
 * and the error from #WOCKY_XMPP_ERROR will be created. See RFC 3902
 * (XMPP Core) §9.3, “Stanza Errors”.
 *
 * There is currently no way to override the type='' of an XMPP Core stanza
 * error without creating an application-specific error code which does so.
 *
 * Returns: the newly-created <error/> node
 */
WockyNode *
wocky_stanza_error_to_node (const GError *error,
    WockyNode *parent_node)
{
  WockyNode *error_node;
  WockyXmppErrorDomain *domain = NULL;
  WockyXmppError core_error;
  const XmppErrorSpec *spec;
  WockyXmppErrorType type;
  gchar str[6];

  g_return_val_if_fail (parent_node != NULL, NULL);

  error_node = wocky_node_add_child (parent_node, "error");

  g_return_val_if_fail (error != NULL, error_node);

  if (error->domain == WOCKY_XMPP_ERROR)
    {
      core_error = error->code;
      spec = &(xmpp_errors[core_error]);
      type = spec->type;
    }
  else
    {
      WockyXmppErrorSpecialization *s;

      domain = xmpp_error_find_domain (error->domain);
      g_return_val_if_fail (domain != NULL, error_node);

      /* This will crash if you mess up and pass a code that's not in the
       * domain. */
      s = &(domain->codes[error->code]);
      core_error = s->specializes;
      spec = &(xmpp_errors[core_error]);

      if (s->override_type)
        type = s->type;
      else
        type = spec->type;
    }

  sprintf (str, "%d", spec->legacy_errors[0]);
  wocky_node_set_attribute (error_node, "code", str);

  wocky_node_set_attribute (error_node, "type",
      wocky_enum_to_nick (WOCKY_TYPE_XMPP_ERROR_TYPE, type));

  wocky_node_add_child_ns (error_node, wocky_xmpp_error_string (core_error),
      WOCKY_XMPP_NS_STANZAS);

  if (domain != NULL)
    {
      const gchar *name = wocky_enum_to_nick (domain->enum_type, error->code);

      wocky_node_add_child_ns_q (error_node, name, domain->domain);
    }

  if (error->message != NULL && *error->message != '\0')
    wocky_node_add_child_with_content_ns (error_node, "text",
        error->message, WOCKY_XMPP_NS_STANZAS);

  return error_node;
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
    quark = g_quark_from_static_string (WOCKY_XMPP_NS_STREAMS);

  return quark;
}

/**
 * wocky_xmpp_stream_error_from_node:
 * @error: the root node of a #WOCKY_STANZA_TYPE_STREAM_ERROR stanza
 *
 * Returns: a GError in the #WOCKY_XMPP_STREAM_ERROR domain.
 */
GError *
wocky_xmpp_stream_error_from_node (WockyNode *error)
{
  gint code = WOCKY_XMPP_STREAM_ERROR_UNKNOWN;
  const gchar *message = NULL;

  /* Ignore the return value; we have a default. */
  xmpp_error_from_node_for_ns (error, WOCKY_XMPP_STREAM_ERROR,
      WOCKY_TYPE_XMPP_STREAM_ERROR, &code);

  message = wocky_node_get_content_from_child_ns (error, "text",
      WOCKY_XMPP_NS_STREAMS);

  if (message == NULL)
    message = "";

  return g_error_new_literal (WOCKY_XMPP_STREAM_ERROR, code, message);
}


/* Built-in specialized error domains */

GQuark
wocky_jingle_error_quark (void)
{
  static GQuark quark = 0;

  if (quark == 0)
    quark = g_quark_from_static_string (WOCKY_XMPP_NS_JINGLE_ERRORS);

  return quark;
}

static WockyXmppErrorDomain *
jingle_error_get_domain (void)
{
  static WockyXmppErrorSpecialization codes[] = {
      /* out-of-order */
      { "The request cannot occur at this point in the state machine (e.g., "
        "session-initiate after session-accept).",
        WOCKY_XMPP_ERROR_UNEXPECTED_REQUEST,
        FALSE
      },

      /* tie-break */
      { "The request is rejected because it was sent while the initiator was "
        "awaiting a reply on a similar request.",
        WOCKY_XMPP_ERROR_CONFLICT,
        FALSE
      },

      /* unknown-session */
      { "The 'sid' attribute specifies a session that is unknown to the "
        "recipient (e.g., no longer live according to the recipient's state "
        "machine because the recipient previously terminated the session).",
        WOCKY_XMPP_ERROR_ITEM_NOT_FOUND,
        FALSE
      },

      /* unsupported-info */
      { "The recipient does not support the informational payload of a "
        "session-info action.",
        WOCKY_XMPP_ERROR_FEATURE_NOT_IMPLEMENTED,
        FALSE
      }
  };
  static WockyXmppErrorDomain jingle_errors = { 0, };

  if (G_UNLIKELY (jingle_errors.domain == 0))
    {
      jingle_errors.domain = WOCKY_JINGLE_ERROR;
      jingle_errors.enum_type = WOCKY_TYPE_JINGLE_ERROR;
      jingle_errors.codes = codes;
    }

  return &jingle_errors;
}

GQuark
wocky_si_error_quark (void)
{
  static GQuark quark = 0;

  if (quark == 0)
    quark = g_quark_from_static_string (WOCKY_XMPP_NS_SI);

  return quark;
}

static WockyXmppErrorDomain *
si_error_get_domain (void)
{
  static WockyXmppErrorSpecialization codes[] = {
      /* no-valid-streams */
      { "None of the available streams are acceptable.",
        WOCKY_XMPP_ERROR_BAD_REQUEST,
        TRUE,
        WOCKY_XMPP_ERROR_TYPE_CANCEL
      },

      /* bad-profile */
      { "The profile is not understood or invalid. The profile MAY supply a "
        "profile-specific error condition.",
        WOCKY_XMPP_ERROR_BAD_REQUEST,
        TRUE,
        WOCKY_XMPP_ERROR_TYPE_MODIFY
      }
  };
  static WockyXmppErrorDomain si_errors = { 0, };

  if (G_UNLIKELY (si_errors.domain == 0))
    {
      si_errors.domain = WOCKY_SI_ERROR;
      si_errors.enum_type = WOCKY_TYPE_SI_ERROR;
      si_errors.codes = codes;
    }

  return &si_errors;
}

void
wocky_xmpp_error_init ()
{
  if (error_domains == NULL)
    {
      /* Register standard domains */
      wocky_xmpp_error_register_domain (jingle_error_get_domain ());
      wocky_xmpp_error_register_domain (si_error_get_domain ());
    }
}

void
wocky_xmpp_error_deinit ()
{
  g_list_free (error_domains);
  error_domains = NULL;
}
