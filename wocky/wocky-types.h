/*
 * wocky-contact.h - Header for Wocky type definitions
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
#if !defined (WOCKY_H_INSIDE) && !defined (WOCKY_COMPILATION)
# error "Only <wocky/wocky.h> can be included directly."
#endif

#ifndef __WOCKY_TYPES_H__
#define __WOCKY_TYPES_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _WockyBareContact WockyBareContact;
typedef struct _WockyLLContact WockyLLContact;
typedef struct _WockyNodeTree WockyNodeTree;
typedef struct _WockyResourceContact WockyResourceContact;
typedef struct _WockySession WockySession;
typedef struct _WockyPubsubNode WockyPubsubNode;

G_END_DECLS

#endif /* #ifndef __WOCKY_TYPES_H__*/
