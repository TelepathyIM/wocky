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

#include "wocky-data-form.h"

#include "wocky-data-form-enumtypes.h"
#include "wocky-namespaces.h"
#include "wocky-utils.h"

#define DEBUG_FLAG DEBUG_DATA_FORM
#include "wocky-debug.h"

G_DEFINE_TYPE (WockyDataForm, wocky_data_form, G_TYPE_OBJECT)

/* properties */
enum
{
  PROP_TITLE = 1,
  PROP_INSTRUCTIONS,
};

/* private structure */
typedef struct _WockyDataFormPrivate WockyDataFormPrivate;

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

#define WOCKY_DATA_FORM_GET_PRIVATE(o)  \
    (G_TYPE_INSTANCE_GET_PRIVATE ((o), WOCKY_TYPE_DATA_FORM, \
    WockyDataFormPrivate))

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

/* pass ownership of the default_value, the value and the options list */
static WockyDataFormField *
wocky_data_form_field_new (
  WockyDataFormFieldType type,
  const gchar *var,
  const gchar *label,
  const gchar *desc,
  gboolean required,
  GValue *default_value,
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
wocky_data_form_init (WockyDataForm *obj)
{
  WockyDataForm *self = WOCKY_DATA_FORM (obj);
  WockyDataFormPrivate *priv = WOCKY_DATA_FORM_GET_PRIVATE (self);

  self->fields = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, NULL);
  self->fields_list = NULL;

  priv->reported = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
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
  WockyDataFormPrivate *priv = WOCKY_DATA_FORM_GET_PRIVATE (self);

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
  WockyDataFormPrivate *priv = WOCKY_DATA_FORM_GET_PRIVATE (self);

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
  WockyDataFormPrivate *priv = WOCKY_DATA_FORM_GET_PRIVATE (self);

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
  WockyDataFormPrivate *priv = WOCKY_DATA_FORM_GET_PRIVATE (self);

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

/**
 * extract_options_list:
 * @node: a <field/> node
 *
 * Returns: a list of (WockyDataFormFieldOption *) containing all the
 *          <option/>s defined in the node
 */
static GSList *
extract_options_list (WockyXmppNode *node)
{
  GSList *result = NULL;
  WockyXmppNodeIter iter;
  WockyXmppNode *option_node;

  wocky_xmpp_node_iter_init (&iter, node, "option", NULL);

  while (wocky_xmpp_node_iter_next (&iter, &option_node))
    {
      WockyDataFormFieldOption *option;
      const gchar *value, *label;

      value = wocky_xmpp_node_get_content_from_child (option_node, "value");
      label = wocky_xmpp_node_get_attribute (option_node, "label");

      if (value == NULL)
        continue;

      /* the label is optional */

      DEBUG ("Add option: %s", value);
      option = wocky_data_form_field_option_new (label, value);
      result = g_slist_append (result, option);
    }

  return result;
}

/**
 * extract_value_list:
 * @node: a <field/> element
 *
 * Returns: a newly allocated array of strings containing the content of all
 *          the 'value' children nodes of the node
 */
static GStrv
extract_value_list (WockyXmppNode *node)
{
  GPtrArray *tmp = g_ptr_array_new ();
  WockyXmppNodeIter iter;
  WockyXmppNode *value;

  wocky_xmpp_node_iter_init (&iter, node, "value", NULL);

  while (wocky_xmpp_node_iter_next (&iter, &value))
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
    WockyXmppNode *field)
{
  WockyXmppNode *node;
  const gchar *value;

  if (type == WOCKY_DATA_FORM_FIELD_TYPE_UNSPECIFIED)
    {
      /* While parsing a form, we shouldn't get this far without having treated
       * the absence of type='' to mean text-single.
       */
      g_warn_if_reached ();
      return NULL;
    }

  node = wocky_xmpp_node_get_child (field, "value");
  if (node == NULL)
    /* no default value */
    return NULL;

  value = node->content;

  switch (type)
    {
      case WOCKY_DATA_FORM_FIELD_TYPE_BOOLEAN:
        if (!wocky_strdiff (value, "true") || !wocky_strdiff (value, "1"))
          return wocky_g_value_slice_new_boolean (TRUE);
        else if (!wocky_strdiff (value, "false") || !wocky_strdiff (value, "0"))
          return wocky_g_value_slice_new_boolean (FALSE);
        else
          DEBUG ("Invalid boolean value: %s", value);
        break;

      case WOCKY_DATA_FORM_FIELD_TYPE_FIXED:
      case WOCKY_DATA_FORM_FIELD_TYPE_HIDDEN:
      case WOCKY_DATA_FORM_FIELD_TYPE_JID_SINGLE:
      case WOCKY_DATA_FORM_FIELD_TYPE_TEXT_PRIVATE:
      case WOCKY_DATA_FORM_FIELD_TYPE_TEXT_SINGLE:
      case WOCKY_DATA_FORM_FIELD_TYPE_LIST_SINGLE:
        return wocky_g_value_slice_new_string (value);

      case WOCKY_DATA_FORM_FIELD_TYPE_JID_MULTI:
      case WOCKY_DATA_FORM_FIELD_TYPE_TEXT_MULTI:
      case WOCKY_DATA_FORM_FIELD_TYPE_LIST_MULTI:
        return wocky_g_value_slice_new_take_boxed (G_TYPE_STRV,
            extract_value_list (field));

      default:
        g_assert_not_reached ();
    }

  return NULL;
}

static WockyDataFormField *
create_field (WockyXmppNode *field_node,
    const gchar *var,
    WockyDataFormFieldType type,
    const gchar *label,
    const gchar *desc,
    gboolean required)
{
  GValue *default_value = NULL;
  GSList *options = NULL;
  WockyDataFormField *field;

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
  default_value = get_field_value (type, field_node);

  field = wocky_data_form_field_new (type, var, label, desc, required,
      default_value, NULL, options);

  return field;
}

static gboolean
extract_var_type_label (WockyXmppNode *node,
    const gchar **_var,
    WockyDataFormFieldType *_type,
    const gchar **_label)
{
  const gchar *tmp, *var, *label;
  gint type;

  if (wocky_strdiff (node->name, "field"))
    return FALSE;

  /* For data forms of type "form", each <field/> element SHOULD possess a
   * 'type' attribute that defines the data "type" of the field data (if no
   * 'type' is specified, the default is "text-single")
   */
  tmp = wocky_xmpp_node_get_attribute (node, "type");
  if (tmp == NULL)
    {
      type = WOCKY_DATA_FORM_FIELD_TYPE_TEXT_SINGLE;
    }
  else if (!wocky_enum_from_nick (WOCKY_TYPE_DATA_FORM_FIELD_TYPE,
                tmp, &type))
    {
      DEBUG ("Invalid field type: %s", tmp);
      return FALSE;
    }

  var = wocky_xmpp_node_get_attribute (node, "var");
  if (var == NULL && type != WOCKY_DATA_FORM_FIELD_TYPE_FIXED)
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

static WockyDataFormField *
data_form_parse_form_field (WockyXmppNode *field_node)
{
  WockyDataFormField *field;
  const gchar *var, *label, *desc;
  WockyDataFormFieldType type;
  gboolean required;

  if (!extract_var_type_label (field_node, &var, &type, &label))
    return NULL;

  desc = wocky_xmpp_node_get_content_from_child (field_node, "desc");
  required = (wocky_xmpp_node_get_child (field_node, "required") != NULL);
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
wocky_data_form_new_from_form (WockyXmppNode *root,
    GError **error)
{
  WockyXmppNode *x, *node;
  WockyXmppNodeIter iter;
  const gchar *type, *title, *instructions;
  WockyDataForm *form;

  x = wocky_xmpp_node_get_child_ns (root, "x", WOCKY_XMPP_NS_DATA);
  if (x == NULL)
    {
      DEBUG ("No 'x' node");
      g_set_error (error, WOCKY_DATA_FORM_ERROR,
          WOCKY_DATA_FORM_ERROR_NOT_FORM, "No 'x' node");
      return NULL;
    }

  type = wocky_xmpp_node_get_attribute (x, "type");
  if (wocky_strdiff (type, "form"))
    {
      DEBUG ("'type' attribute is not 'form': %s", type);
      g_set_error (error, WOCKY_DATA_FORM_ERROR,
          WOCKY_DATA_FORM_ERROR_WRONG_TYPE,
          "'type' attribute is not 'form': %s", type);
      return NULL;
    }

  title = wocky_xmpp_node_get_content_from_child (x, "title");
  instructions = wocky_xmpp_node_get_content_from_child (x, "instructions");

  form = g_object_new (WOCKY_TYPE_DATA_FORM,
      "title", title,
      "instructions", instructions,
      NULL);

  /* add fields */
  wocky_xmpp_node_iter_init (&iter, x, "field", NULL);

  while (wocky_xmpp_node_iter_next (&iter, &node))
    {
      WockyDataFormField *field = data_form_parse_form_field (node);

      if (field != NULL)
        data_form_add_field (form, field, TRUE);
    }

  form->fields_list = g_slist_reverse (form->fields_list);

  return form;
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

  g_return_val_if_fail (form_type != NULL, FALSE);

  field = g_hash_table_lookup (self->fields, "FORM_TYPE");

  if (field != NULL)
    {
      DEBUG ("form already has a FORM_TYPE");
      return FALSE;
    }

  field = wocky_data_form_field_new (WOCKY_DATA_FORM_FIELD_TYPE_HIDDEN,
      "FORM_TYPE", NULL, NULL, FALSE, NULL,
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

  g_return_val_if_fail (field_name != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  field = g_hash_table_lookup (self->fields, field_name);

  if (field == NULL)
    {
      if (create_if_missing)
        {
          field = wocky_data_form_field_new (
              WOCKY_DATA_FORM_FIELD_TYPE_UNSPECIFIED, field_name, NULL, NULL,
              FALSE, NULL, NULL, NULL);
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
    WockyXmppNode *node)
{
  const GValue *value = field->value;
  GType t;
  WockyXmppNode *field_node;

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

  field_node = wocky_xmpp_node_add_child (node, "field");
  wocky_xmpp_node_set_attribute (field_node, "var", field->var);

  if (field->type != WOCKY_DATA_FORM_FIELD_TYPE_UNSPECIFIED)
    wocky_xmpp_node_set_attribute (field_node, "type",
        type_to_str (field->type));

  t = G_VALUE_TYPE (value);

  if (t == G_TYPE_BOOLEAN)
    {
      wocky_xmpp_node_add_child_with_content (field_node, "value",
          g_value_get_boolean (value) ? "1" : "0");
    }
  else if (t == G_TYPE_STRING)
    {
      wocky_xmpp_node_add_child_with_content (field_node, "value",
          g_value_get_string (value));
    }
  else if (t == G_TYPE_STRV)
    {
      GStrv tmp = g_value_get_boxed (value);
      GStrv s;

      for (s = tmp; *s != NULL; s++)
        wocky_xmpp_node_add_child_with_content (field_node, "value", *s);
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
    WockyXmppNode *node)
{
  WockyXmppNode *x;

  x = wocky_xmpp_node_add_child_ns (node, "x", WOCKY_XMPP_NS_DATA);
  wocky_xmpp_node_set_attribute (x, "type", "submit");

  g_slist_foreach (self->fields_list, (GFunc) add_field_to_node, x);
}

static void
data_form_parse_reported (WockyDataForm *self,
    WockyXmppNode *reported_node)
{
  WockyDataFormPrivate *priv = WOCKY_DATA_FORM_GET_PRIVATE (self);
  GSList *l;

  for (l = reported_node->children; l != NULL; l = g_slist_next (l))
    {
      WockyXmppNode *node = l->data;
      const gchar *var, *label;
      WockyDataFormField *field;
      WockyDataFormFieldType type;

      if (!extract_var_type_label (node, &var, &type, &label))
        continue;

      field = wocky_data_form_field_new (type, var, label, NULL, FALSE, NULL,
          NULL, NULL);

      DEBUG ("Add '%s'", field->var);
      g_hash_table_insert (priv->reported, field->var, field);
    }
}

static void
data_form_parse_item (WockyDataForm *self,
    WockyXmppNode *item_node)
{
  WockyDataFormPrivate *priv = WOCKY_DATA_FORM_GET_PRIVATE (self);
  WockyXmppNodeIter iter;
  WockyXmppNode *field_node;
  GSList *item = NULL;

  wocky_xmpp_node_iter_init (&iter, item_node, "field", NULL);
  while (wocky_xmpp_node_iter_next (&iter, &field_node))
    {
      const gchar *var;
      WockyDataFormField *field, *result;
      GValue *value;

      var = wocky_xmpp_node_get_attribute (field_node, "var");
      if (var == NULL)
        continue;

      field = g_hash_table_lookup (priv->reported, var);
      if (field == NULL)
        {
          DEBUG ("Field '%s' wasn't in the reported fields; ignoring", var);
          continue;
        }

      value = get_field_value (field->type, field_node);
      if (value == NULL)
        continue;

      result = wocky_data_form_field_new (field->type, var, field->label,
          field->desc, field->required, field->default_value, value, NULL);

      item = g_slist_prepend (item, result);
    }

  item = g_slist_reverse (item);
  self->results = g_slist_prepend (self->results, item);
}

static void
parse_unique_result (WockyDataForm *self,
    WockyXmppNode *x)
{
  GSList *l, *item = NULL;

  for (l = x->children; l != NULL; l = g_slist_next (l))
    {
      WockyXmppNode *node = l->data;
      const gchar *var;
      WockyDataFormFieldType type;
      WockyDataFormField *result;
      GValue *value;

      if (!extract_var_type_label (node, &var, &type, NULL))
        continue;

      value = get_field_value (type, node);
      if (value == NULL)
        continue;

      result = wocky_data_form_field_new (type, var, NULL,
          NULL, FALSE, NULL, value, NULL);

      item = g_slist_prepend (item, result);
    }

  self->results = g_slist_prepend (self->results, item);
}

gboolean
wocky_data_form_parse_result (WockyDataForm *self,
    WockyXmppNode *node,
    GError **error)
{
  WockyXmppNode *x, *reported;
  const gchar *type;

  x = wocky_xmpp_node_get_child_ns (node, "x", WOCKY_XMPP_NS_DATA);
  if (x == NULL)
    {
      DEBUG ("No 'x' node");
      g_set_error (error, WOCKY_DATA_FORM_ERROR,
          WOCKY_DATA_FORM_ERROR_NOT_FORM, "No 'x' node");
      return FALSE;
    }

  type = wocky_xmpp_node_get_attribute (x, "type");
  if (wocky_strdiff (type, "result"))
    {
      DEBUG ("'type' attribute is not 'result': %s", type);
      g_set_error (error, WOCKY_DATA_FORM_ERROR,
          WOCKY_DATA_FORM_ERROR_WRONG_TYPE,
          "'type' attribute is not 'result': %s", type);
      return FALSE;
    }

  reported = wocky_xmpp_node_get_child (x, "reported");

  if (reported != NULL)
    {
      WockyXmppNodeIter iter;
      WockyXmppNode *item;

      /* The field definitions are in a <reported/> header, and a series of
       * <item/> nodes contain sets of results.
       */
      data_form_parse_reported (self, reported);

      wocky_xmpp_node_iter_init (&iter, x, "item", NULL);
      while (wocky_xmpp_node_iter_next (&iter, &item))
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
  WockyDataFormPrivate *priv = WOCKY_DATA_FORM_GET_PRIVATE (self);

  return priv->title;
}

const gchar *
wocky_data_form_get_instructions (WockyDataForm *self)
{
  WockyDataFormPrivate *priv = WOCKY_DATA_FORM_GET_PRIVATE (self);

  return priv->instructions;
}
