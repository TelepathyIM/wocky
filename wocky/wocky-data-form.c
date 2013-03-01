/*
 * wocky-data-form.c - WockyDataForm
 * Copyright © 2009–2010 Collabora Ltd.
 * Copyright © 2010 Nokia Corporation
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
 * SECTION: wocky-data-form
 * @title: WockyDataForm
 * @short_description: An object to represent an XMPP data form
 * @include: wocky/wocky-data-form.h
 *
 * An object that represents an XMPP data form as described in
 * XEP-0004.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "wocky-data-form.h"

#include <string.h>

#include "wocky-namespaces.h"
#include "wocky-utils.h"

#define WOCKY_DEBUG_FLAG WOCKY_DEBUG_DATA_FORM
#include "wocky-debug-internal.h"

G_DEFINE_TYPE (WockyDataForm, wocky_data_form, G_TYPE_OBJECT)

/* properties */
enum
{
  PROP_TITLE = 1,
  PROP_INSTRUCTIONS,
};

/* private structure */
struct _WockyDataFormPrivate
{
  gchar *title;
  gchar *instructions;

  /* (gchar *) => owned (WockyDataFormField *) */
  GHashTable *reported;

  gboolean dispose_has_run;
};

GQuark
wocky_data_form_error_quark (void)
{
  static GQuark quark = 0;

  if (quark == 0)
    quark = g_quark_from_static_string ("wocky-data-form-error");

  return quark;
}

static WockyDataFormFieldOption *
wocky_data_form_field_option_new (const gchar *label,
    const gchar *value)
{
  WockyDataFormFieldOption *option;

  g_assert (value != NULL);

  option = g_slice_new0 (WockyDataFormFieldOption);
  option->label = g_strdup (label);
  option->value = g_strdup (value);
  return option;
}

static void
wocky_data_form_field_option_free (WockyDataFormFieldOption *option)
{
  g_free (option->label);
  g_free (option->value);
  g_slice_free (WockyDataFormFieldOption, option);
}

/* pass ownership of the default_value, raw_value_contents, the value
 * and the options list */
static WockyDataFormField *
wocky_data_form_field_new (
  WockyDataFormFieldType type,
  const gchar *var,
  const gchar *label,
  const gchar *desc,
  gboolean required,
  GValue *default_value,
  gchar **raw_value_contents,
  GValue *value,
  GSList *options)
{
  WockyDataFormField *field;

  field = g_slice_new0 (WockyDataFormField);
  field->type = type;
  field->var = g_strdup (var);
  field->label = g_strdup (label);
  field->desc = g_strdup (desc);
  field->required = required;
  field->default_value = default_value;
  field->raw_value_contents = raw_value_contents;
  field->value = value;
  field->options = options;
  return field;
}

static void
wocky_data_form_field_free (WockyDataFormField *field)
{
  if (field == NULL)
    return;

  g_free (field->var);
  g_free (field->label);
  g_free (field->desc);
  g_strfreev (field->raw_value_contents);

  if (field->default_value != NULL)
    wocky_g_value_slice_free (field->default_value);
  if (field->value != NULL)
    wocky_g_value_slice_free (field->value);

  g_slist_foreach (field->options, (GFunc) wocky_data_form_field_option_free,
      NULL);
  g_slist_free (field->options);
  g_slice_free (WockyDataFormField, field);
}

static void
wocky_data_form_init (WockyDataForm *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, WOCKY_TYPE_DATA_FORM,
      WockyDataFormPrivate);

  self->fields = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, NULL);
  self->fields_list = NULL;

  self->priv->reported = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
      (GDestroyNotify) wocky_data_form_field_free);
  self->results = NULL;
}

static void
wocky_data_form_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  WockyDataForm *self = WOCKY_DATA_FORM (object);
  WockyDataFormPrivate *priv = self->priv;

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
wocky_data_form_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  WockyDataForm *self = WOCKY_DATA_FORM (object);
  WockyDataFormPrivate *priv = self->priv;

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
wocky_data_form_dispose (GObject *object)
{
  WockyDataForm *self = WOCKY_DATA_FORM (object);
  WockyDataFormPrivate *priv = self->priv;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  if (G_OBJECT_CLASS (wocky_data_form_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_data_form_parent_class)->dispose (object);
}

static void
data_form_field_list_free (GSList *fields)
{
  g_slist_foreach (fields, (GFunc) wocky_data_form_field_free, NULL);
  g_slist_free (fields);
}

static void
wocky_data_form_finalize (GObject *object)
{
  WockyDataForm *self = WOCKY_DATA_FORM (object);
  WockyDataFormPrivate *priv = self->priv;

  g_free (priv->title);
  g_free (priv->instructions);
  g_hash_table_unref (self->fields);

  data_form_field_list_free (self->fields_list);

  g_slist_foreach (self->results, (GFunc) data_form_field_list_free, NULL);
  g_slist_free (self->results);

  g_hash_table_unref (priv->reported);

  G_OBJECT_CLASS (wocky_data_form_parent_class)->finalize (object);
}

static void
wocky_data_form_class_init (
    WockyDataFormClass *wocky_data_form_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (wocky_data_form_class);
  GParamSpec *param_spec;

  g_type_class_add_private (wocky_data_form_class,
      sizeof (WockyDataFormPrivate));

  object_class->set_property = wocky_data_form_set_property;
  object_class->get_property = wocky_data_form_get_property;
  object_class->dispose = wocky_data_form_dispose;
  object_class->finalize = wocky_data_form_finalize;

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

static const gchar *
type_to_str (WockyDataFormFieldType type)
{
  return wocky_enum_to_nick (WOCKY_TYPE_DATA_FORM_FIELD_TYPE, type);
}

/*
 * extract_options_list:
 * @node: a <field/> node
 *
 * Returns: a list of (WockyDataFormFieldOption *) containing all the
 *          <option/>s defined in the node
 */
static GSList *
extract_options_list (WockyNode *node)
{
  GSList *result = NULL;
  WockyNodeIter iter;
  WockyNode *option_node;

  wocky_node_iter_init (&iter, node, "option", NULL);

  while (wocky_node_iter_next (&iter, &option_node))
    {
      WockyDataFormFieldOption *option;
      const gchar *value, *label;

      value = wocky_node_get_content_from_child (option_node, "value");
      label = wocky_node_get_attribute (option_node, "label");

      if (value == NULL)
        continue;

      /* the label is optional */

      DEBUG ("Add option: %s", value);
      option = wocky_data_form_field_option_new (label, value);
      result = g_slist_append (result, option);
    }

  return result;
}

/*
 * extract_value_list:
 * @node: a <field/> element
 *
 * Returns: a newly allocated array of strings containing the content of all
 *          the 'value' children nodes of the node
 */
static GStrv
extract_value_list (WockyNode *node)
{
  GPtrArray *tmp = g_ptr_array_new ();
  WockyNodeIter iter;
  WockyNode *value;

  wocky_node_iter_init (&iter, node, "value", NULL);

  while (wocky_node_iter_next (&iter, &value))
    {
      if (value->content != NULL)
        g_ptr_array_add (tmp, g_strdup (value->content));
    }

  /* Add trailing NULL */
  g_ptr_array_add (tmp, NULL);

  return (GStrv) g_ptr_array_free (tmp, FALSE);
}

static GValue *
get_field_value (
    WockyDataFormFieldType type,
    WockyNode *field,
    gchar ***raw_value_contents)
{
  WockyNode *node;
  const gchar *value;
  GValue *ret = NULL;

  if (type == WOCKY_DATA_FORM_FIELD_TYPE_UNSPECIFIED)
    {
      /* While parsing a form, we shouldn't get this far without having treated
       * the absence of type='' to mean text-single.
       */
      g_warn_if_reached ();
      return NULL;
    }

  node = wocky_node_get_child (field, "value");
  if (node == NULL)
    /* no default value */
    return NULL;

  value = node->content;

  switch (type)
    {
      case WOCKY_DATA_FORM_FIELD_TYPE_BOOLEAN:
        {
          if (!wocky_strdiff (value, "true") || !wocky_strdiff (value, "1"))
            ret = wocky_g_value_slice_new_boolean (TRUE);
          else if (!wocky_strdiff (value, "false") || !wocky_strdiff (value, "0"))
            ret = wocky_g_value_slice_new_boolean (FALSE);
          else
            DEBUG ("Invalid boolean value: %s", value);

          if (ret != NULL)
            {
              const gchar const *value_str[] = { value, NULL };

              if (raw_value_contents != NULL)
                *raw_value_contents = g_strdupv ((GStrv) value_str);

              return ret;
            }
        }
        break;

      case WOCKY_DATA_FORM_FIELD_TYPE_FIXED:
      case WOCKY_DATA_FORM_FIELD_TYPE_HIDDEN:
      case WOCKY_DATA_FORM_FIELD_TYPE_JID_SINGLE:
      case WOCKY_DATA_FORM_FIELD_TYPE_TEXT_PRIVATE:
      case WOCKY_DATA_FORM_FIELD_TYPE_TEXT_SINGLE:
      case WOCKY_DATA_FORM_FIELD_TYPE_LIST_SINGLE:
        {
          const gchar const *value_str[] = { value, NULL };

          if (raw_value_contents != NULL)
            *raw_value_contents = g_strdupv ((GStrv) value_str);

          return wocky_g_value_slice_new_string (value);
        }

      case WOCKY_DATA_FORM_FIELD_TYPE_JID_MULTI:
      case WOCKY_DATA_FORM_FIELD_TYPE_TEXT_MULTI:
      case WOCKY_DATA_FORM_FIELD_TYPE_LIST_MULTI:
        {
          gchar **value_str = extract_value_list (field);

          if (raw_value_contents != NULL)
            *raw_value_contents = g_strdupv (value_str);

          return wocky_g_value_slice_new_take_boxed (G_TYPE_STRV,
              value_str);
        }

      default:
        g_assert_not_reached ();
    }

  return NULL;
}

static WockyDataFormField *
create_field (WockyNode *field_node,
    const gchar *var,
    WockyDataFormFieldType type,
    const gchar *label,
    const gchar *desc,
    gboolean required)
{
  GValue *default_value = NULL;
  GSList *options = NULL;
  WockyDataFormField *field;
  gchar **raw_value_contents = NULL;

  if (type == WOCKY_DATA_FORM_FIELD_TYPE_LIST_MULTI ||
      type == WOCKY_DATA_FORM_FIELD_TYPE_LIST_SINGLE)
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
  default_value = get_field_value (type, field_node, &raw_value_contents);

  field = wocky_data_form_field_new (type, var, label, desc, required,
      default_value, raw_value_contents, NULL, options);

  return field;
}

static gboolean
extract_var_type_label (WockyNode *node,
    const gchar **_var,
    WockyDataFormFieldType *_type,
    const gchar **_label)
{
  const gchar *tmp, *var, *label;
  gint type = 0;

  if (wocky_strdiff (node->name, "field"))
    return FALSE;

  /* For data forms of type "form", each <field/> element SHOULD possess a
   * 'type' attribute that defines the data "type" of the field data (if no
   * 'type' is specified, the default is "text-single")
   */
  tmp = wocky_node_get_attribute (node, "type");
  if (tmp == NULL)
    {
      WockyNodeIter iter;

      type = WOCKY_DATA_FORM_FIELD_TYPE_TEXT_SINGLE;

      wocky_node_iter_init (&iter, node, "value", NULL);

      if (wocky_node_iter_next (&iter, NULL) &&
          wocky_node_iter_next (&iter, NULL))
        {
          type = WOCKY_DATA_FORM_FIELD_TYPE_TEXT_MULTI;
        }
    }
  else if (!wocky_enum_from_nick (WOCKY_TYPE_DATA_FORM_FIELD_TYPE,
                tmp, &type))
    {
      DEBUG ("Invalid field type: %s", tmp);
      return FALSE;
    }

  var = wocky_node_get_attribute (node, "var");
  if (var == NULL && type != WOCKY_DATA_FORM_FIELD_TYPE_FIXED)
    {
      DEBUG ("field node doesn't have a 'var' attribute; ignoring");
      return FALSE;
    }

  label = wocky_node_get_attribute (node, "label");

  if (_var != NULL)
    *_var = var;
  if (_type != NULL)
    *_type = type;
  if (_label != NULL)
    *_label = label;

  return TRUE;
}

static WockyDataFormField *
data_form_parse_form_field (WockyNode *field_node)
{
  WockyDataFormField *field;
  const gchar *var, *label, *desc;
  WockyDataFormFieldType type;
  gboolean required;

  if (!extract_var_type_label (field_node, &var, &type, &label))
    return NULL;

  desc = wocky_node_get_content_from_child (field_node, "desc");
  required = (wocky_node_get_child (field_node, "required") != NULL);
  field = create_field (field_node, var, type, label, desc, required);

  if (field == NULL)
    return NULL;

  if (field->var != NULL)
    DEBUG ("parsed field '%s' of type %s", field->var, type_to_str (type));
  else
    DEBUG ("parsed anonymous field of type %s", type_to_str (type));

  return field;
}

static void
data_form_add_field (WockyDataForm *self,
    WockyDataFormField *field,
    gboolean prepend)
{

  self->fields_list =
      (prepend ? g_slist_prepend : g_slist_append) (self->fields_list, field);

  /* Fixed fields can be used for instructions to the user. They have
   * no var='' attribute and hence shouldn't be included in the form
   * submission.*/
  if (field->var != NULL)
    g_hash_table_insert (self->fields, field->var, field);
}

WockyDataForm *
wocky_data_form_new_from_node (WockyNode *x,
    GError **error)
{
  WockyNode *node;
  WockyNodeIter iter;
  const gchar *type, *title, *instructions;
  WockyDataForm *form;

  if (!wocky_node_matches (x, "x", WOCKY_XMPP_NS_DATA))
    {
      DEBUG ("Invalid 'x' node");
      g_set_error (error, WOCKY_DATA_FORM_ERROR,
          WOCKY_DATA_FORM_ERROR_NOT_FORM, "Invalid 'x' node");
      return NULL;
    }

  type = wocky_node_get_attribute (x, "type");
  if (wocky_strdiff (type, "form") && wocky_strdiff (type, "result"))
    {
      DEBUG ("'type' attribute is not 'form' or 'result': %s", type);
      g_set_error (error, WOCKY_DATA_FORM_ERROR,
          WOCKY_DATA_FORM_ERROR_WRONG_TYPE,
          "'type' attribute is not 'form' or 'result': %s", type);
      return NULL;
    }

  title = wocky_node_get_content_from_child (x, "title");
  instructions = wocky_node_get_content_from_child (x, "instructions");

  form = g_object_new (WOCKY_TYPE_DATA_FORM,
      "title", title,
      "instructions", instructions,
      NULL);

  /* add fields */
  wocky_node_iter_init (&iter, x, "field", NULL);

  while (wocky_node_iter_next (&iter, &node))
    {
      WockyDataFormField *field = data_form_parse_form_field (node);

      if (field != NULL)
        data_form_add_field (form, field, TRUE);
    }

  form->fields_list = g_slist_reverse (form->fields_list);

  return form;
}

WockyDataForm *
wocky_data_form_new_from_form (WockyNode *root,
    GError **error)
{
  WockyNode *x;

  x = wocky_node_get_child_ns (root, "x", WOCKY_XMPP_NS_DATA);
  if (x == NULL)
    {
      DEBUG ("No 'x' node");
      g_set_error (error, WOCKY_DATA_FORM_ERROR,
          WOCKY_DATA_FORM_ERROR_NOT_FORM, "No 'x' node");
      return NULL;
    }

  return wocky_data_form_new_from_node (x, error);
}

/**
 * wocky_data_form_set_type:
 * @self: a #WockyDataForm
 * @form_type: the URI to use as the FORM_TYPE field; may not be %NULL
 *
 * Creates a hidden FORM_TYPE field in @self and sets its value to @form_type.
 * This is intended only to be used on empty forms created for blind
 * submission.
 *
 * Returns: %TRUE if the form's type was set; %FALSE if the form already had a
 *          type.
 */
gboolean
wocky_data_form_set_type (WockyDataForm *self,
    const gchar *form_type)
{
  WockyDataFormField *field;
  const gchar const *raw_value_contents[] =
      { form_type, NULL };

  g_return_val_if_fail (form_type != NULL, FALSE);

  field = g_hash_table_lookup (self->fields, "FORM_TYPE");

  if (field != NULL)
    {
      DEBUG ("form already has a FORM_TYPE");
      return FALSE;
    }

  field = wocky_data_form_field_new (WOCKY_DATA_FORM_FIELD_TYPE_HIDDEN,
      "FORM_TYPE", NULL, NULL, FALSE,
      wocky_g_value_slice_new_string (form_type),
      g_strdupv ((GStrv) raw_value_contents),
      wocky_g_value_slice_new_string (form_type),
      NULL);
  data_form_add_field (self, field, FALSE);

  return TRUE;
}

/*
 * data_form_set_value:
 * @self: a data form
 * @field_name: the name of a field of @self
 * @value: a slice-allocated value to fill in for @field_name (stolen by this
 *         function)
 * @create_if_missing: if no field named @field_name exists, create it
 *
 * Returns: %TRUE if the field was successfully filled in; %FALSE if the field
 *          did not exist or has a different type to @value.
 */
static gboolean
data_form_set_value (WockyDataForm *self,
    const gchar *field_name,
    GValue *value,
    gboolean create_if_missing)
{
  WockyDataFormField *field;
  GType t;

  g_return_val_if_fail (field_name != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  field = g_hash_table_lookup (self->fields, field_name);

  if (field == NULL)
    {
      if (create_if_missing)
        {
          field = wocky_data_form_field_new (
              WOCKY_DATA_FORM_FIELD_TYPE_UNSPECIFIED, field_name, NULL, NULL,
              FALSE, NULL, NULL, NULL, NULL);
          data_form_add_field (self, field, FALSE);
        }
      else
        {
          DEBUG ("field %s not found", field_name);
          wocky_g_value_slice_free (value);
          return FALSE;
        }
    }

  if (field->value != NULL)
    wocky_g_value_slice_free (field->value);

  field->value = value;

  g_strfreev (field->raw_value_contents);

  t = G_VALUE_TYPE (field->value);
  if (t == G_TYPE_STRING)
    {
      const gchar const *value_str[] =
          { g_value_get_string (field->value), NULL };

      field->raw_value_contents = g_strdupv ((GStrv) value_str);
    }
  else if (t == G_TYPE_BOOLEAN)
    {
      const gchar const *value_str[] =
          { g_value_get_boolean (field->value) ? "1" : "0", NULL };

      field->raw_value_contents = g_strdupv ((GStrv) value_str);
    }
  else if (t == G_TYPE_STRV)
    {
      const GStrv value_str = g_value_get_boxed (field->value);

      field->raw_value_contents = g_strdupv (value_str);
    }
  else
    {
      g_assert_not_reached ();
    }

  return TRUE;
}

/**
 * wocky_data_form_set_boolean:
 * @self: a data form
 * @field_name: the name of a boolean field of @self
 * @field_value: the value to fill in for @field_name
 * @create_if_missing: if no field named @field_name exists, create it
 *
 * Returns: %TRUE if the field was successfully filled in; %FALSE if the field
 *          did not exist or does not accept a boolean
 */
gboolean
wocky_data_form_set_boolean (WockyDataForm *self,
    const gchar *field_name,
    gboolean field_value,
    gboolean create_if_missing)
{
  return data_form_set_value (self, field_name,
      wocky_g_value_slice_new_boolean (field_value), create_if_missing);
}

/**
 * wocky_data_form_set_string:
 * @self: a data form
 * @field_name: the name of a string field of @self
 * @field_value: the value to fill in for @field_name
 * @create_if_missing: if no field named @field_name exists, create it
 *
 * Returns: %TRUE if the field was successfully filled in; %FALSE if the field
 *          did not exist or does not accept a string
 */
gboolean
wocky_data_form_set_string (WockyDataForm *self,
    const gchar *field_name,
    const gchar *field_value,
    gboolean create_if_missing)
{
  return data_form_set_value (self, field_name,
      wocky_g_value_slice_new_string (field_value), create_if_missing);
}

gboolean
wocky_data_form_set_strv (WockyDataForm *self,
    const gchar *field_name,
    const gchar * const *field_values,
    gboolean create_if_missing)
{
  return data_form_set_value (self, field_name,
      wocky_g_value_slice_new_boxed (G_TYPE_STRV, field_values),
      create_if_missing);
}

static void
add_field_to_node (WockyDataFormField *field,
    WockyNode *node)
{
  const GValue *value = field->value;
  GType t;
  WockyNode *field_node;

  /* Skip anonymous fields, which are used for instructions to the user. */
  if (field->var == NULL)
    return;

  /* Hidden fields shouldn't have their values modified, but should be returned
   * along with the form.
   */
  if (value == NULL && field->type == WOCKY_DATA_FORM_FIELD_TYPE_HIDDEN)
    value = field->default_value;

  /* Skip fields which don't have a value. */
  if (value == NULL)
    return;

  field_node = wocky_node_add_child (node, "field");
  wocky_node_set_attribute (field_node, "var", field->var);

  if (field->type != WOCKY_DATA_FORM_FIELD_TYPE_UNSPECIFIED)
    wocky_node_set_attribute (field_node, "type",
        type_to_str (field->type));

  t = G_VALUE_TYPE (value);

  if (t == G_TYPE_BOOLEAN)
    {
      wocky_node_add_child_with_content (field_node, "value",
          g_value_get_boolean (value) ? "1" : "0");
    }
  else if (t == G_TYPE_STRING)
    {
      wocky_node_add_child_with_content (field_node, "value",
          g_value_get_string (value));
    }
  else if (t == G_TYPE_STRV)
    {
      GStrv tmp = g_value_get_boxed (value);
      GStrv s;

      for (s = tmp; *s != NULL; s++)
        wocky_node_add_child_with_content (field_node, "value", *s);
    }
  else
    {
      g_assert_not_reached ();
    }
}

/**
 * wocky_data_form_submit:
 * @self: a data form
 * @node: a node to which to add a form submission
 *
 * Adds a node tree which submits @self based on the current values set on
 * @self's fields.
 */
void
wocky_data_form_submit (WockyDataForm *self,
    WockyNode *node)
{
  WockyNode *x;

  x = wocky_node_add_child_ns (node, "x", WOCKY_XMPP_NS_DATA);
  wocky_node_set_attribute (x, "type", "submit");

  g_slist_foreach (self->fields_list, (GFunc) add_field_to_node, x);
}

static void
data_form_parse_reported (WockyDataForm *self,
    WockyNode *reported_node)
{
  WockyDataFormPrivate *priv = self->priv;
  GSList *l;

  for (l = reported_node->children; l != NULL; l = g_slist_next (l))
    {
      WockyNode *node = l->data;
      const gchar *var, *label;
      WockyDataFormField *field;
      WockyDataFormFieldType type;

      if (!extract_var_type_label (node, &var, &type, &label))
        continue;

      field = wocky_data_form_field_new (type, var, label, NULL, FALSE, NULL,
          NULL, NULL, NULL);

      DEBUG ("Add '%s'", field->var);
      g_hash_table_insert (priv->reported, field->var, field);
    }
}

static void
data_form_parse_item (WockyDataForm *self,
    WockyNode *item_node)
{
  WockyDataFormPrivate *priv = self->priv;
  WockyNodeIter iter;
  WockyNode *field_node;
  GSList *item = NULL;

  wocky_node_iter_init (&iter, item_node, "field", NULL);
  while (wocky_node_iter_next (&iter, &field_node))
    {
      const gchar *var;
      WockyDataFormField *field, *result;
      GValue *value;

      var = wocky_node_get_attribute (field_node, "var");
      if (var == NULL)
        continue;

      field = g_hash_table_lookup (priv->reported, var);
      if (field == NULL)
        {
          DEBUG ("Field '%s' wasn't in the reported fields; ignoring", var);
          continue;
        }

      value = get_field_value (field->type, field_node, NULL);
      if (value == NULL)
        continue;

      result = wocky_data_form_field_new (field->type, var, field->label,
          field->desc, field->required, field->default_value,
          field->raw_value_contents, value, NULL);

      item = g_slist_prepend (item, result);
    }

  item = g_slist_reverse (item);
  self->results = g_slist_prepend (self->results, item);
}

static void
parse_unique_result (WockyDataForm *self,
    WockyNode *x)
{
  GSList *l, *item = NULL;

  for (l = x->children; l != NULL; l = g_slist_next (l))
    {
      WockyNode *node = l->data;
      const gchar *var;
      WockyDataFormFieldType type;
      WockyDataFormField *result;
      GValue *value;

      if (!extract_var_type_label (node, &var, &type, NULL))
        continue;

      value = get_field_value (type, node, NULL);
      if (value == NULL)
        continue;

      result = wocky_data_form_field_new (type, var, NULL,
          NULL, FALSE, NULL, NULL, value, NULL);

      item = g_slist_prepend (item, result);
    }

  self->results = g_slist_prepend (self->results, item);
}

gboolean
wocky_data_form_parse_result (WockyDataForm *self,
    WockyNode *node,
    GError **error)
{
  WockyNode *x, *reported;
  const gchar *type;

  x = wocky_node_get_child_ns (node, "x", WOCKY_XMPP_NS_DATA);
  if (x == NULL)
    {
      DEBUG ("No 'x' node");
      g_set_error (error, WOCKY_DATA_FORM_ERROR,
          WOCKY_DATA_FORM_ERROR_NOT_FORM, "No 'x' node");
      return FALSE;
    }

  type = wocky_node_get_attribute (x, "type");
  if (wocky_strdiff (type, "result"))
    {
      DEBUG ("'type' attribute is not 'result': %s", type);
      g_set_error (error, WOCKY_DATA_FORM_ERROR,
          WOCKY_DATA_FORM_ERROR_WRONG_TYPE,
          "'type' attribute is not 'result': %s", type);
      return FALSE;
    }

  reported = wocky_node_get_child (x, "reported");

  if (reported != NULL)
    {
      WockyNodeIter iter;
      WockyNode *item;

      /* The field definitions are in a <reported/> header, and a series of
       * <item/> nodes contain sets of results.
       */
      data_form_parse_reported (self, reported);

      wocky_node_iter_init (&iter, x, "item", NULL);
      while (wocky_node_iter_next (&iter, &item))
        data_form_parse_item (self, item);
    }
  else
    {
      /* no <reported/> header; so there must be only one result. */
      parse_unique_result (self, x);
    }

  self->results = g_slist_reverse (self->results);
  return TRUE;
}

const gchar *
wocky_data_form_get_title (WockyDataForm *self)
{
  WockyDataFormPrivate *priv = self->priv;

  return priv->title;
}

const gchar *
wocky_data_form_get_instructions (WockyDataForm *self)
{
  WockyDataFormPrivate *priv = self->priv;

  return priv->instructions;
}

gint
wocky_data_form_field_cmp (const WockyDataFormField *left,
    const WockyDataFormField *right)
{
  return g_strcmp0 (left->var, right->var);
}

static void
add_field_to_node_using_default (WockyDataFormField *field,
    WockyNode *node)
{
  WockyNode *field_node;
  GStrv s;

  /* Skip anonymous fields, which are used for instructions to the user. */
  if (field->var == NULL)
    return;

  field_node = wocky_node_add_child (node, "field");
  wocky_node_set_attribute (field_node, "var", field->var);

  if (field->type != WOCKY_DATA_FORM_FIELD_TYPE_UNSPECIFIED)
    wocky_node_set_attribute (field_node, "type",
        type_to_str (field->type));

  g_assert (field->raw_value_contents != NULL);
  for (s = field->raw_value_contents; *s != NULL; s++)
    wocky_node_add_child_with_content (field_node, "value", *s);
}

/**
 * wocky_data_form_add_to_node:
 * @self: the #WockyDataForm object
 * @node: a node to which to add the form
 *
 * Adds a node tree with default values of @self based on the defaults
 * set on each field when first created.
 *
 * This function is for adding a data form to an existing node, like
 * the query node of a disco response. wocky_data_form_submit(), in
 * contrast, is for adding a node tree which submits the data form
 * based on the current values set on its fields.
 */
void
wocky_data_form_add_to_node (WockyDataForm *self,
    WockyNode *node)
{
  WockyNode *x;

  x = wocky_node_add_child_ns (node, "x", WOCKY_XMPP_NS_DATA);
  wocky_node_set_attribute (x, "type", "result");

  g_slist_foreach (self->fields_list,
      (GFunc) add_field_to_node_using_default, x);
}
