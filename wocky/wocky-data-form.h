/*
 * wocky-data-form.h - Header of WockyDataForm
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
#if !defined (WOCKY_H_INSIDE) && !defined (WOCKY_COMPILATION)
# error "Only <wocky/wocky.h> can be included directly."
#endif

#ifndef WOCKY_DATA_FORM_H
#define WOCKY_DATA_FORM_H

#include <glib-object.h>

#include "wocky-enumtypes.h"
#include "wocky-node.h"

G_BEGIN_DECLS

/**
 * WockyDataFormFieldType:
 * @WOCKY_DATA_FORM_FIELD_TYPE_UNSPECIFIED: Unspecified field type
 * @WOCKY_DATA_FORM_FIELD_TYPE_BOOLEAN: Boolean field type
 * @WOCKY_DATA_FORM_FIELD_TYPE_FIXED: Fixed description field type
 * @WOCKY_DATA_FORM_FIELD_TYPE_HIDDEN: Hidden field type
 * @WOCKY_DATA_FORM_FIELD_TYPE_JID_MULTI: A list of multiple JIDs
 * @WOCKY_DATA_FORM_FIELD_TYPE_JID_SINGLE: A single JID
 * @WOCKY_DATA_FORM_FIELD_TYPE_LIST_MULTI: Many options to choose one
 *   or more from
 * @WOCKY_DATA_FORM_FIELD_TYPE_LIST_SINGLE: Many options to choose one
 *   from
 * @WOCKY_DATA_FORM_FIELD_TYPE_TEXT_MULTI: Multiple lines of text
 * @WOCKY_DATA_FORM_FIELD_TYPE_TEXT_PRIVATE: A single line of text
 *   that should be obscured (by, say, asterisks)
 * @WOCKY_DATA_FORM_FIELD_TYPE_TEXT_SINGLE: A single line of text
 *
 * Data form field types, as documented in XEP-0004 §3.3.
 */
/*< prefix=WOCKY_DATA_FORM_FIELD_TYPE >*/
typedef enum
{
  WOCKY_DATA_FORM_FIELD_TYPE_UNSPECIFIED, /*< skip >*/
  WOCKY_DATA_FORM_FIELD_TYPE_BOOLEAN,
  WOCKY_DATA_FORM_FIELD_TYPE_FIXED,
  WOCKY_DATA_FORM_FIELD_TYPE_HIDDEN,
  WOCKY_DATA_FORM_FIELD_TYPE_JID_MULTI,
  WOCKY_DATA_FORM_FIELD_TYPE_JID_SINGLE,
  WOCKY_DATA_FORM_FIELD_TYPE_LIST_MULTI,
  WOCKY_DATA_FORM_FIELD_TYPE_LIST_SINGLE,
  WOCKY_DATA_FORM_FIELD_TYPE_TEXT_MULTI,
  WOCKY_DATA_FORM_FIELD_TYPE_TEXT_PRIVATE,
  WOCKY_DATA_FORM_FIELD_TYPE_TEXT_SINGLE
} WockyDataFormFieldType;

/**
 * WockyDataFormFieldOption:
 * @label: the option label
 * @value: the option value
 *
 * A single data form field option.
 */
typedef struct _WockyDataFormFieldOption WockyDataFormFieldOption;
struct _WockyDataFormFieldOption
{
  gchar *label;
  gchar *value;
};

/**
 * WockyDataFormField:
 * @type: the type of the field
 * @var: the field name
 * @label: the label of the field
 * @desc: the description of the field
 * @required: %TRUE if the field is required, otherwise %FALSE
 * @default_value: the default of the field
 * @raw_value_contents: a %NULL-terminated array holding the literal value(s) as
 *  specified in the original XML. For example, this might be something like
 *  <code language="c">{ "1", NULL }</code> or <code language="c">{ "false",
 *  NULL }</code> for a #WOCKY_DATA_FORM_FIELD_TYPE_BOOLEAN field, or
 *  <code language="c">{ "hi", "there", NULL }</code> for a
 *  #WOCKY_DATA_FORM_FIELD_TYPE_TEXT_MULTI field.
 * @value: the field value
 * @options: a #GSList of #WockyDataFormFieldOption<!-- -->s if @type
 *   if %WOCKY_DATA_FORM_FIELD_TYPE_LIST_MULTI or
 *   %WOCKY_DATA_FORM_FIELD_TYPE_LIST_SINGLE
 *
 * Details about a single data form field in a #WockyDataForm.
 */
typedef struct _WockyDataFormField WockyDataFormField;
struct _WockyDataFormField
{
  WockyDataFormFieldType type;
  gchar *var;
  gchar *label;
  gchar *desc;
  gboolean required;
  GValue *default_value;
  gchar **raw_value_contents;
  GValue *value;
  /* for LIST_MULTI and LIST_SINGLE only.
   * List of (WockyDataFormFieldOption *)*/
  GSList *options;
};

/**
 * WockyDataForm:
 * @fields: a #GHashTable of strings to #WockyDataFormField<!-- -->s
 * @fields_list: a list of #WockyDataFormField<!-- -->s in the order
 *   they have been presented in the form
 * @results: a list of #GSList<!-- -->s of #WockyDataFormField<!-- -->s
 *   representing one or more sets of result.
 *
 * An object that represents an XMPP data form as described in
 * XEP-0004.
 */
typedef struct _WockyDataForm WockyDataForm;

/**
 * WockyDataFormClass:
 *
 * The class of a #WockyDataForm.
 */
typedef struct _WockyDataFormClass WockyDataFormClass;
typedef struct _WockyDataFormPrivate WockyDataFormPrivate;

/**
 * WockyDataFormError:
 * @WOCKY_DATA_FORM_ERROR_NOT_FORM: Node is not a data form
 * @WOCKY_DATA_FORM_ERROR_WRONG_TYPE: Data form is of the wrong type
 *
 * #WockyDataForm specific errors.
 */
typedef enum {
  WOCKY_DATA_FORM_ERROR_NOT_FORM,
  WOCKY_DATA_FORM_ERROR_WRONG_TYPE,
} WockyDataFormError;

GQuark wocky_data_form_error_quark (void);

#define WOCKY_DATA_FORM_ERROR (wocky_data_form_error_quark ())

struct _WockyDataFormClass {
  /*<private>*/
  GObjectClass parent_class;
};

struct _WockyDataForm {
  /*<private>*/
  GObject parent;

  /*<public>*/

  /* (gchar *) owned by the WockyDataFormField =>
   * borrowed (WockyDataFormField *) */
  GHashTable *fields;
  /* list containing owned (WockyDataFormField *) in the order they
   * have been presented in the form */
  GSList *fields_list;

  /* list of GSList * of (WockyDataFormField *), representing one or more sets
   * of results */
  GSList *results;

  /*<private>*/
  WockyDataFormPrivate *priv;
};

GType wocky_data_form_get_type (void);

#define WOCKY_TYPE_DATA_FORM \
  (wocky_data_form_get_type ())
#define WOCKY_DATA_FORM(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), WOCKY_TYPE_DATA_FORM, \
   WockyDataForm))
#define WOCKY_DATA_FORM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), WOCKY_TYPE_DATA_FORM, \
   WockyDataFormClass))
#define WOCKY_IS_DATA_FORM(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), WOCKY_TYPE_DATA_FORM))
#define WOCKY_IS_DATA_FORM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), WOCKY_TYPE_DATA_FORM))
#define WOCKY_DATA_FORM_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_DATA_FORM, \
   WockyDataFormClass))

WockyDataForm * wocky_data_form_new_from_form (WockyNode *node,
    GError **error);

WockyDataForm * wocky_data_form_new_from_node (WockyNode *x,
    GError **error);

gboolean wocky_data_form_set_type (WockyDataForm *self,
    const gchar *form_type);

gboolean wocky_data_form_set_boolean (WockyDataForm *self,
    const gchar *field_name,
    gboolean field_value,
    gboolean create_if_missing);

gboolean wocky_data_form_set_string (WockyDataForm *self,
    const gchar *field_name,
    const gchar *field_value,
    gboolean create_if_missing);

gboolean wocky_data_form_set_strv (WockyDataForm *self,
    const gchar *field_name,
    const gchar * const *field_values,
    gboolean create_if_missing);

void wocky_data_form_submit (WockyDataForm *self,
    WockyNode *node);

gboolean wocky_data_form_parse_result (WockyDataForm *self,
    WockyNode *node,
    GError **error);

const gchar *wocky_data_form_get_title (WockyDataForm *self);

const gchar *wocky_data_form_get_instructions (WockyDataForm *self);

gint wocky_data_form_field_cmp (const WockyDataFormField *left,
    const WockyDataFormField *right);

void wocky_data_form_add_to_node (WockyDataForm *self, WockyNode *node);

G_END_DECLS

#endif /* WOCKY_DATA_FORM_H */
