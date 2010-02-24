/*
 * wocky-data-forms.h - Header of WockyDataForms
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

#ifndef __WOCKY_DATA_FORMS_H__
#define __WOCKY_DATA_FORMS_H__

#include <glib-object.h>

#include "wocky-xmpp-node.h"

G_BEGIN_DECLS

/*< prefix=WOCKY_DATA_FORMS_FIELD_TYPE >*/
typedef enum
{
  WOCKY_DATA_FORMS_FIELD_TYPE_BOOLEAN,
  WOCKY_DATA_FORMS_FIELD_TYPE_FIXED,
  WOCKY_DATA_FORMS_FIELD_TYPE_HIDDEN,
  WOCKY_DATA_FORMS_FIELD_TYPE_JID_MULTI,
  WOCKY_DATA_FORMS_FIELD_TYPE_JID_SINGLE,
  WOCKY_DATA_FORMS_FIELD_TYPE_LIST_MULTI,
  WOCKY_DATA_FORMS_FIELD_TYPE_LIST_SINGLE,
  WOCKY_DATA_FORMS_FIELD_TYPE_TEXT_MULTI,
  WOCKY_DATA_FORMS_FIELD_TYPE_TEXT_PRIVATE,
  WOCKY_DATA_FORMS_FIELD_TYPE_TEXT_SINGLE
} WockyDataFormsFieldType;

typedef struct
{
  gchar *label;
  gchar *value;
} WockyDataFormsFieldOption;

typedef struct
{
  WockyDataFormsFieldType type;
  gchar *var;
  gchar *label;
  gchar *desc;
  gboolean required;
  GValue *default_value;
  GValue *value;
  /* for LIST_MULTI and LIST_SINGLE only.
   * List of (WockyDataFormsFieldOption *)*/
  GSList *options;
} WockyDataFormsField;

typedef struct _WockyDataForms WockyDataForms;
typedef struct _WockyDataFormsClass WockyDataFormsClass;

typedef enum {
  WOCKY_DATA_FORMS_ERROR_NOT_FORM,
  WOCKY_DATA_FORMS_ERROR_WRONG_TYPE,
} WockyDataFormsError;

GQuark wocky_data_forms_error_quark (void);

#define WOCKY_DATA_FORMS_ERROR (wocky_data_forms_error_quark ())

struct _WockyDataFormsClass {
  GObjectClass parent_class;
};

struct _WockyDataForms {
  GObject parent;

  /* (gchar *) owned by the WockyDataFormsField =>
   * borrowed (WockyDataFormsField *) */
  GHashTable *fields;
  /* list containing owned (WockyDataFormsField *) in the order they
   * have been presented in the form */
  GSList *fields_list;

  /* list of GSList * of (WockyDataFormsField *), representing one or more sets
   * of results */
  GSList *results;
};

GType wocky_data_forms_get_type (void);

#define WOCKY_TYPE_DATA_FORMS \
  (wocky_data_forms_get_type ())
#define WOCKY_DATA_FORMS(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), WOCKY_TYPE_DATA_FORMS, \
   WockyDataForms))
#define WOCKY_DATA_FORMS_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), WOCKY_TYPE_DATA_FORMS, \
   WockyDataFormsClass))
#define WOCKY_IS_DATA_FORMS(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), WOCKY_TYPE_DATA_FORMS))
#define WOCKY_IS_DATA_FORMS_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), WOCKY_TYPE_DATA_FORMS))
#define WOCKY_DATA_FORMS_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_DATA_FORMS, \
   WockyDataFormsClass))

WockyDataForms * wocky_data_forms_new_from_form (WockyXmppNode *node,
    GError **error);

void wocky_data_forms_submit (WockyDataForms *forms,
    WockyXmppNode *node);

gboolean wocky_data_forms_parse_result (WockyDataForms *forms,
    WockyXmppNode *node,
    GError **error);

const gchar *wocky_data_forms_get_title (WockyDataForms *forms);

const gchar *wocky_data_forms_get_instructions (WockyDataForms *forms);

G_END_DECLS

#endif /* __WOCKY_DATA_FORMS_H__ */
