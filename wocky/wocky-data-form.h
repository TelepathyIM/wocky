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

#ifndef WOCKY_DATA_FORM_H
#define WOCKY_DATA_FORM_H

#include <glib-object.h>

#include "wocky-node.h"

G_BEGIN_DECLS

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

typedef struct
{
  gchar *label;
  gchar *value;
} WockyDataFormFieldOption;

typedef struct
{
  WockyDataFormFieldType type;
  gchar *var;
  gchar *label;
  gchar *desc;
  gboolean required;
  GValue *default_value;
  GValue *value;
  /* for LIST_MULTI and LIST_SINGLE only.
   * List of (WockyDataFormFieldOption *)*/
  GSList *options;
} WockyDataFormField;

typedef struct _WockyDataForm WockyDataForm;
typedef struct _WockyDataFormClass WockyDataFormClass;
typedef struct _WockyDataFormPrivate WockyDataFormPrivate;

typedef enum {
  WOCKY_DATA_FORM_ERROR_NOT_FORM,
  WOCKY_DATA_FORM_ERROR_WRONG_TYPE,
} WockyDataFormError;

GQuark wocky_data_form_error_quark (void);

#define WOCKY_DATA_FORM_ERROR (wocky_data_form_error_quark ())

struct _WockyDataFormClass {
  GObjectClass parent_class;
};

struct _WockyDataForm {
  GObject parent;

  /* (gchar *) owned by the WockyDataFormField =>
   * borrowed (WockyDataFormField *) */
  GHashTable *fields;
  /* list containing owned (WockyDataFormField *) in the order they
   * have been presented in the form */
  GSList *fields_list;

  /* list of GSList * of (WockyDataFormField *), representing one or more sets
   * of results */
  GSList *results;

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

G_END_DECLS

#endif /* WOCKY_DATA_FORM_H */
