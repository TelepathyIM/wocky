/*
 * wocky-data-forms.c - WockyDataForms
 * Copyright (C) 2009 Collabora Ltd.
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

#include "wocky-data-forms.h"

#include <wocky/wocky-utils.h>
#include <wocky/wocky-namespaces.h>

#define DEBUG_FLAG DEBUG_DATA_FORMS
#include "wocky-debug.h"

G_DEFINE_TYPE (WockyDataForms, wocky_data_forms, G_TYPE_OBJECT)

/* properties */
enum
{
  PROP_TITLE = 1,
  PROP_INSTRUCTIONS,
};

/* private structure */
typedef struct _WockyDataFormsPrivate WockyDataFormsPrivate;

struct _WockyDataFormsPrivate
{
  gchar *title;
  gchar *instructions;

  /* (gchar *) => owned (WockyDataFormsField *) */
  GHashTable *reported;

  gboolean dispose_has_run;
};

GQuark
wocky_data_forms_error_quark (void)
{
  static GQuark quark = 0;

  if (quark == 0)
    quark = g_quark_from_static_string ("wocky-data-forms-error");

  return quark;
}

#define WOCKY_DATA_FORMS_GET_PRIVATE(o)  \
    (G_TYPE_INSTANCE_GET_PRIVATE ((o), WOCKY_TYPE_DATA_FORMS, \
    WockyDataFormsPrivate))

static WockyDataFormsFieldOption *
wocky_data_forms_field_option_new (const gchar *label,
    const gchar *value)
{
  WockyDataFormsFieldOption *option;

  g_assert (value != NULL);

  option = g_slice_new0 (WockyDataFormsFieldOption);
  option->label = g_strdup (label);
  option->value = g_strdup (value);
  return option;
}

static void
wocky_data_forms_field_option_free (WockyDataFormsFieldOption *option)
{
  g_free (option->label);
  g_free (option->value);
  g_slice_free (WockyDataFormsFieldOption, option);
}

/* pass ownership of the default_value, the value and the options list */
static WockyDataFormsField *
wocky_data_forms_field_new (wocky_data_forms_field_type type,
  const gchar *var,
  const gchar *label,
  const gchar *desc,
  gboolean required,
  GValue *default_value,
  GValue *value,
  GSList *options)
{
  WockyDataFormsField *field;

  field = g_slice_new0 (WockyDataFormsField);
  field->type = type;
  field->var = g_strdup (var);
  field->label = g_strdup (label);
  field->desc = g_strdup (desc);
  field->required = required;
  field->default_value = default_value;
  field->value = value;
  field->options = options;
  return field;
}

static void
wocky_data_forms_field_free (WockyDataFormsField *field)
{
  GSList *l;
  if (field == NULL)
    return;

  g_free (field->var);
  g_free (field->label);
  g_free (field->desc);

  if (field->default_value != NULL)
    wocky_g_value_slice_free (field->default_value);
  if (field->value != NULL)
    wocky_g_value_slice_free (field->value);

  for (l = field->options; l != NULL; l = g_slist_next (l))
    {
      WockyDataFormsFieldOption *option = l->data;
      wocky_data_forms_field_option_free (option);
    }
  g_slist_free (field->options);
  g_slice_free (WockyDataFormsField, field);
}

static void
wocky_data_forms_init (WockyDataForms *obj)
{
  WockyDataForms *self = WOCKY_DATA_FORMS (obj);
  WockyDataFormsPrivate *priv = WOCKY_DATA_FORMS_GET_PRIVATE (self);

  self->fields = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, NULL);
  self->fields_list = NULL;

  priv->reported = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
      (GDestroyNotify) wocky_data_forms_field_free);
  self->results = NULL;
}

static void
wocky_data_forms_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  WockyDataForms *self = WOCKY_DATA_FORMS (object);
  WockyDataFormsPrivate *priv = WOCKY_DATA_FORMS_GET_PRIVATE (self);

  switch (property_id)
    {
      case PROP_TITLE:
        priv->title = g_value_dup_string (value);
        break;
      case PROP_INSTRUCTIONS:
        priv->instructions = g_value_dup_string (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
wocky_data_forms_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  WockyDataForms *self = WOCKY_DATA_FORMS (object);
  WockyDataFormsPrivate *priv = WOCKY_DATA_FORMS_GET_PRIVATE (self);

  switch (property_id)
    {
      case PROP_TITLE:
        g_value_set_string (value, priv->title);
        break;
      case PROP_INSTRUCTIONS:
        g_value_set_string (value, priv->instructions);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}


static void
wocky_data_forms_dispose (GObject *object)
{
  WockyDataForms *self = WOCKY_DATA_FORMS (object);
  WockyDataFormsPrivate *priv = WOCKY_DATA_FORMS_GET_PRIVATE (self);

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  if (G_OBJECT_CLASS (wocky_data_forms_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_data_forms_parent_class)->dispose (object);
}

static void
wocky_data_forms_finalize (GObject *object)
{
  WockyDataForms *self = WOCKY_DATA_FORMS (object);
  WockyDataFormsPrivate *priv = WOCKY_DATA_FORMS_GET_PRIVATE (self);
  GSList *l;

  g_free (priv->title);
  g_free (priv->instructions);
  g_hash_table_unref (self->fields);
  g_slist_foreach (self->fields_list, (GFunc) wocky_data_forms_field_free,
      NULL);
  g_slist_free (self->fields_list);

  for (l = self->results; l != NULL; l = g_slist_next (l))
    {
      GSList *item = l->data;
      GSList *i;

      for (i = item; i != NULL; i = g_slist_next (i))
        {
          WockyDataFormsField *field = i->data;
          wocky_data_forms_field_free (field);
        }
      g_slist_free (item);
    }
  g_slist_free (self->results);

  g_hash_table_unref (priv->reported);

  G_OBJECT_CLASS (wocky_data_forms_parent_class)->finalize (object);
}

static void
wocky_data_forms_class_init (
    WockyDataFormsClass *wocky_data_forms_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (wocky_data_forms_class);
  GParamSpec *param_spec;

  g_type_class_add_private (wocky_data_forms_class,
      sizeof (WockyDataFormsPrivate));

  object_class->set_property = wocky_data_forms_set_property;
  object_class->get_property = wocky_data_forms_get_property;
  object_class->dispose = wocky_data_forms_dispose;
  object_class->finalize = wocky_data_forms_finalize;

  param_spec = g_param_spec_string ("title", "title",
      "Title",
      NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_TITLE, param_spec);

  param_spec = g_param_spec_string ("instructions", "instructions",
      "Instructions",
      NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_INSTRUCTIONS, param_spec);
}

static wocky_data_forms_field_type
str_to_type (const gchar *str)
{
  if (!wocky_strdiff (str, "boolean"))
    return WOCKY_DATA_FORMS_FIELD_TYPE_BOOLEAN;
  else if (!wocky_strdiff (str, "fixed"))
    return WOCKY_DATA_FORMS_FIELD_TYPE_FIXED;
  else if (!wocky_strdiff (str, "hidden"))
    return WOCKY_DATA_FORMS_FIELD_TYPE_HIDDEN;
  else if (!wocky_strdiff (str, "jid-multi"))
    return WOCKY_DATA_FORMS_FIELD_TYPE_JID_MULTI;
  else if (!wocky_strdiff (str, "jid-single"))
    return WOCKY_DATA_FORMS_FIELD_TYPE_JID_SINGLE;
  else if (!wocky_strdiff (str, "list-multi"))
    return WOCKY_DATA_FORMS_FIELD_TYPE_LIST_MULTI;
  else if (!wocky_strdiff (str, "list-single"))
    return WOCKY_DATA_FORMS_FIELD_TYPE_LIST_SINGLE;
  else if (!wocky_strdiff (str, "text-multi"))
    return WOCKY_DATA_FORMS_FIELD_TYPE_TEXT_MULTI;
  else if (!wocky_strdiff (str, "text-private"))
    return WOCKY_DATA_FORMS_FIELD_TYPE_TEXT_PRIVATE;
  else if (!wocky_strdiff (str, "text-single"))
    return WOCKY_DATA_FORMS_FIELD_TYPE_TEXT_SINGLE;

  return WOCKY_DATA_FORMS_FIELD_TYPE_INVALID;
}

static const gchar *
type_to_str (wocky_data_forms_field_type type)
{
  switch (type)
    {
      case WOCKY_DATA_FORMS_FIELD_TYPE_BOOLEAN:
        return "boolean";
      case WOCKY_DATA_FORMS_FIELD_TYPE_FIXED:
        return "fixed";
      case WOCKY_DATA_FORMS_FIELD_TYPE_HIDDEN:
        return "hidden";
      case WOCKY_DATA_FORMS_FIELD_TYPE_JID_MULTI:
        return "jid-multi";
      case WOCKY_DATA_FORMS_FIELD_TYPE_JID_SINGLE:
        return "jid-single";
      case WOCKY_DATA_FORMS_FIELD_TYPE_LIST_MULTI:
        return "list-multi";
      case WOCKY_DATA_FORMS_FIELD_TYPE_LIST_SINGLE:
        return "list-single";
      case WOCKY_DATA_FORMS_FIELD_TYPE_TEXT_MULTI:
        return "text-multi";
      case WOCKY_DATA_FORMS_FIELD_TYPE_TEXT_PRIVATE:
        return "text-private";
      case WOCKY_DATA_FORMS_FIELD_TYPE_TEXT_SINGLE:
        return "text-single";
      default:
        g_assert_not_reached ();
    }

  return NULL;
}

/* Return a list of (WockyDataFormsFieldOption *) containing all the
 * options defined in the node */
static GSList *
extract_options_list (WockyXmppNode *node)
{
  GSList *result = NULL, *l;

  for (l = node->children; l != NULL; l = g_slist_next (l))
    {
      WockyXmppNode *option_node = (WockyXmppNode *) l->data;
      WockyXmppNode *value;
      WockyDataFormsFieldOption *option;
      const gchar *label;

      if (wocky_strdiff (option_node->name, "option"))
        /* wrong name */
        continue;

      label = wocky_xmpp_node_get_attribute (option_node, "label");
      /* the label is optionnal */

      value = wocky_xmpp_node_get_child (option_node, "value");
      if (value == NULL)
        continue;

      if (value->content == NULL)
        continue;

      DEBUG ("Add option: %s", value->content);
      option = wocky_data_forms_field_option_new (label, value->content);
      result = g_slist_append (result, option);
    }

  return result;
}

/* Return a newly allocated array of strings containing the content of all the
 * 'value' children nodes of the node */
static GStrv
extract_value_list (WockyXmppNode *node)
{
  GPtrArray *tmp = g_ptr_array_new ();
  GSList *l;

  for (l = node->children; l != NULL; l = g_slist_next (l))
    {
      WockyXmppNode *value = (WockyXmppNode *) l->data;

      if (wocky_strdiff (value->name, "value"))
        /* wrong name */
        continue;

      if (value->content == NULL)
        continue;

      g_ptr_array_add (tmp, g_strdup (value->content));
    }

  /* Add trailing NULL */
  g_ptr_array_add (tmp, NULL);

  return (GStrv) g_ptr_array_free (tmp, FALSE);
}

static GValue *
get_field_value (wocky_data_forms_field_type type,
    WockyXmppNode *field)
{
  WockyXmppNode *node;
  const gchar *value;

  node = wocky_xmpp_node_get_child (field, "value");
  if (node == NULL)
    /* no default value */
    return NULL;

  value = node->content;

  switch (type)
    {
      case WOCKY_DATA_FORMS_FIELD_TYPE_BOOLEAN:
        if (!wocky_strdiff (value, "true") || !wocky_strdiff (value, "1"))
          return wocky_g_value_slice_new_boolean (TRUE);
        else if (!wocky_strdiff (value, "false") || !wocky_strdiff (value, "0"))
          return wocky_g_value_slice_new_boolean (FALSE);
        else
          DEBUG ("Invalid boolean value: %s", value);
        break;

      case WOCKY_DATA_FORMS_FIELD_TYPE_FIXED:
      case WOCKY_DATA_FORMS_FIELD_TYPE_HIDDEN:
      case WOCKY_DATA_FORMS_FIELD_TYPE_JID_SINGLE:
      case WOCKY_DATA_FORMS_FIELD_TYPE_TEXT_PRIVATE:
      case WOCKY_DATA_FORMS_FIELD_TYPE_TEXT_SINGLE:
      case WOCKY_DATA_FORMS_FIELD_TYPE_LIST_SINGLE:
        return wocky_g_value_slice_new_string (value);

      case WOCKY_DATA_FORMS_FIELD_TYPE_JID_MULTI:
      case WOCKY_DATA_FORMS_FIELD_TYPE_TEXT_MULTI:
      case WOCKY_DATA_FORMS_FIELD_TYPE_LIST_MULTI:
        return wocky_g_value_slice_new_take_boxed (G_TYPE_STRV,
            extract_value_list (field));

      default:
        g_assert_not_reached ();
    }

  return NULL;
}

static WockyDataFormsField *
create_field (WockyXmppNode *field_node,
    const gchar *var,
    wocky_data_forms_field_type type,
    const gchar *label,
    const gchar *desc,
    gboolean required)
{
  GValue *default_value = NULL;
  GSList *options = NULL;
  WockyDataFormsField *field;

  if (type == WOCKY_DATA_FORMS_FIELD_TYPE_LIST_MULTI ||
      type == WOCKY_DATA_FORMS_FIELD_TYPE_LIST_SINGLE)
    {
      /* get the list of options (accepted values) */
      options = extract_options_list (field_node);
      if (options == NULL)
        {
          DEBUG ("No options provided for '%s'", var);
          return NULL;
        }
    }

  /* get default value (if any) */
  default_value = get_field_value (type, field_node);

  field = wocky_data_forms_field_new (type, var, label, desc, required,
      default_value, NULL, options);

  return field;
}

static gboolean
extract_var_type_label (WockyXmppNode *node,
    const gchar **_var,
    wocky_data_forms_field_type *_type,
    const gchar **_label)
{
  const gchar *tmp, *var, *label;
  wocky_data_forms_field_type type;

  if (wocky_strdiff (node->name, "field"))
    return FALSE;

  tmp = wocky_xmpp_node_get_attribute (node, "type");
  if (tmp == NULL)
    {
      DEBUG ("field doesn't have a 'type' attribute; ignoring");
      return FALSE;
    }

  type = str_to_type (tmp);
  if (type == WOCKY_DATA_FORMS_FIELD_TYPE_INVALID)
    {
      DEBUG ("Invalid field type for: %s", tmp);
      return FALSE;
    }

  var = wocky_xmpp_node_get_attribute (node, "var");
  if (var == NULL && type != WOCKY_DATA_FORMS_FIELD_TYPE_FIXED)
    {
      DEBUG ("field node doesn't have a 'var' attribute; ignoring");
      return FALSE;
    }

  label = wocky_xmpp_node_get_attribute (node, "label");

  if (_var != NULL)
    *_var = var;
  if (_type != NULL)
    *_type = type;
  if (_label != NULL)
    *_label = label;

  return TRUE;
}

static gboolean
foreach_x_child (WockyXmppNode *field_node,
    gpointer user_data)
{
  WockyDataForms *self = WOCKY_DATA_FORMS (user_data);
  WockyXmppNode *node;
  const gchar *var, *label;
  wocky_data_forms_field_type type;
  const gchar *desc = NULL;
  gboolean required = FALSE;
  WockyDataFormsField *field;

  if (!extract_var_type_label (field_node, &var, &type, &label))
    return TRUE;

  /* get desc */
  node = wocky_xmpp_node_get_child (field_node, "desc");
  if (node != NULL)
    desc = node->content;

  /* check required */
  if (wocky_xmpp_node_get_child (field_node, "required") != NULL)
    required = TRUE;

  field = create_field (field_node, var, type, label, desc, required);
  if (field == NULL)
    return TRUE;

  DEBUG ("add field '%s of type %s'", field->var, type_to_str (type));
  if (field->var != NULL)
    /* Fixed fields don't have a 'var' attribute and so are not added to the
     * hash table */
    g_hash_table_insert (self->fields, field->var, field);

  /* list will be reversed */
  self->fields_list = g_slist_prepend (self->fields_list, field);
  return TRUE;
}

WockyDataForms *
wocky_data_forms_new_from_form (WockyXmppNode *root)
{
  WockyXmppNode *x, *node;
  const gchar *type, *title = NULL, *instructions = NULL;
  WockyDataForms *forms;

  x = wocky_xmpp_node_get_child_ns (root, "x", WOCKY_XMPP_NS_DATA);
  if (x == NULL)
    {
      DEBUG ("No 'x' node");
      return NULL;
    }

  type = wocky_xmpp_node_get_attribute (x, "type");
  if (wocky_strdiff (type, "form"))
    {
      DEBUG ("'type' attribute is not 'form': %s", type);
      return NULL;
    }

  /* get title */
  node = wocky_xmpp_node_get_child (x, "title");
  if (node != NULL)
    title = node->content;

  /* get instructions */
  node = wocky_xmpp_node_get_child (x, "instructions");
  if (node != NULL)
    instructions = node->content;

  forms = g_object_new (WOCKY_TYPE_DATA_FORMS,
      "title", title,
      "instructions", instructions,
      NULL);

  /* add fields */
  wocky_xmpp_node_each_child (x, foreach_x_child, forms);
  forms->fields_list = g_slist_reverse (forms->fields_list);

  return forms;
}

static void
add_field_to_node (WockyDataFormsField *field,
    WockyXmppNode *node)
{
  WockyXmppNode *field_node, *value_node;

  if (field->value == NULL && field->type != WOCKY_DATA_FORMS_FIELD_TYPE_HIDDEN)
    /* no value, skip */
    return;

  DEBUG ("add field '%s'", field->var);

  field_node = wocky_xmpp_node_add_child (node, "field");
  wocky_xmpp_node_set_attribute (field_node, "var", field->var);
  wocky_xmpp_node_set_attribute (field_node, "type", type_to_str (field->type));

  switch (field->type)
    {
      case WOCKY_DATA_FORMS_FIELD_TYPE_BOOLEAN:
        {
          value_node = wocky_xmpp_node_add_child (field_node, "value");

          if (g_value_get_boolean (field->value))
            wocky_xmpp_node_set_content (value_node, "1");
          else
            wocky_xmpp_node_set_content (value_node, "0");
        }
        break;

      case WOCKY_DATA_FORMS_FIELD_TYPE_HIDDEN:
        {
          value_node = wocky_xmpp_node_add_child (field_node, "value");

          /* hidden fields are not supposed to be modified; set the default
           * value */
          wocky_xmpp_node_set_content (value_node,
              g_value_get_string (field->default_value));
        }
        break;

      case WOCKY_DATA_FORMS_FIELD_TYPE_JID_SINGLE:
      case WOCKY_DATA_FORMS_FIELD_TYPE_LIST_SINGLE:
      case WOCKY_DATA_FORMS_FIELD_TYPE_TEXT_PRIVATE:
      case WOCKY_DATA_FORMS_FIELD_TYPE_TEXT_SINGLE:
        {
          value_node = wocky_xmpp_node_add_child (field_node, "value");

          wocky_xmpp_node_set_content (value_node,
              g_value_get_string (field->value));
        }
        break;

      case WOCKY_DATA_FORMS_FIELD_TYPE_JID_MULTI:
      case WOCKY_DATA_FORMS_FIELD_TYPE_LIST_MULTI:
      case WOCKY_DATA_FORMS_FIELD_TYPE_TEXT_MULTI:
        {
          GStrv tmp;
          guint i;

          tmp = g_value_get_boxed (field->value);
          for (i = 0; tmp[i] != NULL; i++)
            {
              value_node = wocky_xmpp_node_add_child (field_node, "value");

              wocky_xmpp_node_set_content (value_node, tmp[i]);
            }
        }
        break;
      default:
        g_assert_not_reached ();
    }
}

void
wocky_data_forms_submit (WockyDataForms *self,
    WockyXmppNode *node)
{
  WockyXmppNode *x;
  GSList *l;

  x = wocky_xmpp_node_add_child_ns (node, "x", WOCKY_XMPP_NS_DATA);
  wocky_xmpp_node_set_attribute (x, "type", "submit");

  for (l = self->fields_list; l != NULL; l = g_slist_next (l))
    {
      WockyDataFormsField *field = l->data;

      add_field_to_node (field, x);
    }
}

static gboolean
foreach_reported (WockyXmppNode *reported_node,
    gpointer user_data)
{
  WockyDataForms *self = WOCKY_DATA_FORMS (user_data);
  WockyDataFormsPrivate *priv = WOCKY_DATA_FORMS_GET_PRIVATE (self);
  GSList *l;

  if (wocky_strdiff (reported_node->name, "reported"))
    return TRUE;

  for (l = reported_node->children; l != NULL; l = g_slist_next (l))
    {
      WockyXmppNode *node = l->data;
      const gchar *var, *label;
      WockyDataFormsField *field;
      wocky_data_forms_field_type type;

      if (!extract_var_type_label (node, &var, &type, &label))
        continue;

      field = wocky_data_forms_field_new (type, var, label, NULL, FALSE, NULL,
          NULL, NULL);

      DEBUG ("Add '%s'", field->var);
      g_hash_table_insert (priv->reported, field->var, field);
    }

  return TRUE;
}

static gboolean
foreach_item (WockyXmppNode *item_node,
    gpointer user_data)
{
  WockyDataForms *self = WOCKY_DATA_FORMS (user_data);
  WockyDataFormsPrivate *priv = WOCKY_DATA_FORMS_GET_PRIVATE (self);
  GSList *l, *item = NULL;

  if (wocky_strdiff (item_node->name, "item"))
    return TRUE;

  for (l = item_node->children; l != NULL; l = g_slist_next (l))
    {
      WockyXmppNode *node = l->data;
      const gchar *var;
      WockyDataFormsField *field, *result;
      GValue *value;

      if (wocky_strdiff (node->name, "field"))
        continue;

      var = wocky_xmpp_node_get_attribute (node, "var");
      if (var == NULL)
        continue;

      field = g_hash_table_lookup (priv->reported, var);
      if (field == NULL)
        {
          DEBUG ("Field '%s' wasn't in the reported fields; ignoring", var);
          continue;
        }

      value = get_field_value (field->type, node);
      if (value == NULL)
        continue;

      result = wocky_data_forms_field_new (field->type, var, field->label,
          field->desc, field->required, field->default_value, value, NULL);

      item = g_slist_prepend (item, result);
    }

  item = g_slist_reverse (item);
  self->results = g_slist_prepend (self->results, item);

  return TRUE;
}

static void
parse_unique_result (WockyDataForms *self,
    WockyXmppNode *x)
{
  GSList *l, *item = NULL;

  for (l = x->children; l != NULL; l = g_slist_next (l))
    {
      WockyXmppNode *node = l->data;
      const gchar *var;
      wocky_data_forms_field_type type;
      WockyDataFormsField *result;
      GValue *value;

      if (!extract_var_type_label (node, &var, &type, NULL))
        continue;

      value = get_field_value (type, node);
      if (value == NULL)
        continue;

      result = wocky_data_forms_field_new (type, var, NULL,
          NULL, FALSE, NULL, value, NULL);

      item = g_slist_prepend (item, result);
    }

  self->results = g_slist_prepend (self->results, item);
}

gboolean
wocky_data_forms_parse_result (WockyDataForms *self,
    WockyXmppNode *node,
    GError **error)
{
  WockyDataFormsPrivate *priv = WOCKY_DATA_FORMS_GET_PRIVATE (self);
  WockyXmppNode *x;
  const gchar *type;

  x = wocky_xmpp_node_get_child_ns (node, "x", WOCKY_XMPP_NS_DATA);
  if (x == NULL)
    {
      DEBUG ("No 'x' node");
      g_set_error (error, WOCKY_DATA_FORMS_ERROR,
          WOCKY_DATA_FORMS_ERROR_NOT_FORM, "No 'x' node");
      return FALSE;
    }

  type = wocky_xmpp_node_get_attribute (x, "type");
  if (wocky_strdiff (type, "result"))
    {
      DEBUG ("'type' attribute is not 'result': %s", type);
      g_set_error (error, WOCKY_DATA_FORMS_ERROR,
          WOCKY_DATA_FORMS_ERROR_NOT_RESULT,
          "'type' attribute is not 'result': %s", type);
      return FALSE;
    }

  /* first parse the reported elements so we'll know the type of the different
   * fields */
  wocky_xmpp_node_each_child (x, foreach_reported, self);

  if (g_hash_table_size (priv->reported) > 0)
    /* stanza contains reported fields; results are in item nodes */
    wocky_xmpp_node_each_child (x, foreach_item, self);
  else
    /* no reporter fields, there is only one result */
    parse_unique_result (self, x);

  self->results = g_slist_reverse (self->results);
  return TRUE;
}

const gchar *
wocky_data_forms_get_title (WockyDataForms *self)
{
  WockyDataFormsPrivate *priv = WOCKY_DATA_FORMS_GET_PRIVATE (self);

  return priv->title;
}

const gchar *
wocky_data_forms_get_instructions (WockyDataForms *self)
{
  WockyDataFormsPrivate *priv = WOCKY_DATA_FORMS_GET_PRIVATE (self);

  return priv->instructions;
}
