/*
 * wocky-ll-contact.c - Source for WockyLLContact
 * Copyright (C) 2011 Collabora Ltd.
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
 * SECTION: wocky-ll-contact
 * @title: WockyLLContact
 * @short_description: Wrapper around a link-local contact.
 * @include: wocky/wocky-ll-contact.h
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "wocky-ll-contact.h"

#include <gio/gio.h>

#include "wocky-utils.h"

G_DEFINE_TYPE (WockyLLContact, wocky_ll_contact, WOCKY_TYPE_CONTACT)

/* properties */
enum
{
  PROP_JID = 1,
};

/* signal enum */
enum
{
  LAST_SIGNAL,
};

/* private structure */
struct _WockyLLContactPrivate
{
  gboolean dispose_has_run;

  gchar *jid;
};

static void
wocky_ll_contact_init (WockyLLContact *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      WOCKY_TYPE_LL_CONTACT, WockyLLContactPrivate);
}

static void
wocky_ll_contact_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  WockyLLContact *self = WOCKY_LL_CONTACT (object);
  WockyLLContactPrivate *priv = self->priv;

  switch (property_id)
    {
    case PROP_JID:
      priv->jid = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
wocky_ll_contact_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  WockyLLContact *self = WOCKY_LL_CONTACT (object);
  WockyLLContactPrivate *priv = self->priv;

  switch (property_id)
    {
    case PROP_JID:
      g_value_set_string (value, priv->jid);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
wocky_ll_contact_constructed (GObject *object)
{
  WockyLLContact *self = WOCKY_LL_CONTACT (object);

  g_assert (self->priv->jid != NULL);
}

static void
wocky_ll_contact_finalize (GObject *object)
{
  WockyLLContact *self = WOCKY_LL_CONTACT (object);
  WockyLLContactPrivate *priv = self->priv;

  if (priv->jid != NULL)
    g_free (priv->jid);

  G_OBJECT_CLASS (wocky_ll_contact_parent_class)->finalize (object);
}

static gchar *
ll_contact_dup_jid (WockyContact *contact)
{
  return g_strdup (wocky_ll_contact_get_jid (WOCKY_LL_CONTACT (contact)));
}

static void
wocky_ll_contact_class_init (WockyLLContactClass *wocky_ll_contact_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (wocky_ll_contact_class);
  WockyContactClass *contact_class = WOCKY_CONTACT_CLASS (wocky_ll_contact_class);
  GParamSpec *spec;

  g_type_class_add_private (wocky_ll_contact_class,
      sizeof (WockyLLContactPrivate));

  object_class->constructed = wocky_ll_contact_constructed;
  object_class->set_property = wocky_ll_contact_set_property;
  object_class->get_property = wocky_ll_contact_get_property;
  object_class->finalize = wocky_ll_contact_finalize;

  contact_class->dup_jid = ll_contact_dup_jid;

  /**
   * WockyLLContact:jid:
   *
   * The contact's link-local JID.
   */
  spec = g_param_spec_string ("jid", "Contact JID",
      "Contact JID",
      "",
      G_PARAM_READWRITE |
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_JID, spec);
}

/**
 * wocky_ll_contact_new:
 * @jid: the JID of the contact to create
 *
 * Creates a new #WockyLLContact for a given JID.
 *
 * Returns: a newly constructed #WockyLLContact
 */

WockyLLContact *
wocky_ll_contact_new (const gchar *jid)
{
  return g_object_new (WOCKY_TYPE_LL_CONTACT,
      "jid", jid,
      NULL);
}

/**
 * wocky_ll_contact_get_jid:
 * @contact: a #WockyLLContact instance
 *
 * Returns the JID of the contact wrapped by @contact.
 *
 * Returns: @contact's JID.
 */
const gchar *
wocky_ll_contact_get_jid (WockyLLContact *contact)
{
  WockyLLContactPrivate *priv;

  g_return_val_if_fail (WOCKY_IS_LL_CONTACT (contact), NULL);

  priv = contact->priv;

  return priv->jid;
}

/**
 * wocky_ll_contact_equal:
 * @a: a #WockyLLContact instance
 * @b: a #WockyLLContact instance to compare with @a
 *
 * Compares whether two #WockyLLContact instances refer to the same
 * link-local contact.
 *
 * Returns: #TRUE if the two contacts match.
 */
gboolean
wocky_ll_contact_equal (WockyLLContact *a,
    WockyLLContact *b)
{
  if (a == NULL || b == NULL)
    return FALSE;

  if (wocky_strdiff (wocky_ll_contact_get_jid (a),
        wocky_ll_contact_get_jid (b)))
    return FALSE;

  return TRUE;
}

/**
 * wocky_ll_contact_get_addresses:
 * @self: a #WockyLLContact
 *
 * Returns a #GList of #GInetSocketAddress<!-- -->es which are
 * advertised by the contact @self as addresses to connect on. Note
 * that the #GInetSocketAddresses should be unreffed by calling
 * g_object_unref() on each list member and the list freed using
 * g_list_free() when the caller is finished.
 *
 * Returns: (element-type GInetSocketAddress) (transfer full): a new
 *   #GList of #GInetSocketAddress<!-- -->es.
 */
GList *
wocky_ll_contact_get_addresses (WockyLLContact *self)
{
  WockyLLContactClass *cls;

  g_return_val_if_fail (WOCKY_IS_LL_CONTACT (self), NULL);

  cls = WOCKY_LL_CONTACT_GET_CLASS (self);

  if (cls->get_addresses != NULL)
    return cls->get_addresses (self);

  return NULL;
}

/**
 * wocky_ll_contact_has_address:
 * @self: a #WockyLLContact
 * @address: a #GInetAddress
 *
 * Checks whether @address relates to the contact @self.
 *
 * Returns: %TRUE if @address relates to the contact @self, otherwise
 *   %FALSE
 */
gboolean
wocky_ll_contact_has_address (WockyLLContact *self,
    GInetAddress *address)
{
  gchar *s = g_inet_address_to_string (address);
  gboolean ret = FALSE;
  GList *l, *addresses = wocky_ll_contact_get_addresses (self);

  for (l = addresses; l != NULL; l = l->next)
    {
      GInetAddress *a = g_inet_socket_address_get_address (
          G_INET_SOCKET_ADDRESS (l->data));
      gchar *tmp = g_inet_address_to_string (a);

      if (!wocky_strdiff (tmp, s))
        ret = TRUE;

      g_free (tmp);

      if (ret)
        break;
    }

  g_list_foreach (addresses, (GFunc) g_object_unref, NULL);
  g_list_free (addresses);
  g_free (s);

  return ret;
}
