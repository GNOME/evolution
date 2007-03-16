/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-user-creatable-items-handler.h
 *
 * Copyright (C) 2001-2004 Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifndef _E_USER_CREATABLE_ITEMS_HANDLER_H_
#define _E_USER_CREATABLE_ITEMS_HANDLER_H_

#include <glib-object.h>
#include <bonobo/bonobo-ui-component.h>
#include <bonobo/bonobo-window.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_USER_CREATABLE_ITEMS_HANDLER		(e_user_creatable_items_handler_get_type ())
#define E_USER_CREATABLE_ITEMS_HANDLER(obj)		(GTK_CHECK_CAST ((obj), E_TYPE_USER_CREATABLE_ITEMS_HANDLER, EUserCreatableItemsHandler))
#define E_USER_CREATABLE_ITEMS_HANDLER_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_USER_CREATABLE_ITEMS_HANDLER, EUserCreatableItemsHandlerClass))
#define E_IS_USER_CREATABLE_ITEMS_HANDLER(obj)		(GTK_CHECK_TYPE ((obj), E_TYPE_USER_CREATABLE_ITEMS_HANDLER))
#define E_IS_USER_CREATABLE_ITEMS_HANDLER_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_USER_CREATABLE_ITEMS_HANDLER))


typedef struct _EUserCreatableItemsHandler        EUserCreatableItemsHandler;
typedef struct _EUserCreatableItemsHandlerPrivate EUserCreatableItemsHandlerPrivate;
typedef struct _EUserCreatableItemsHandlerClass   EUserCreatableItemsHandlerClass;

typedef void (*EUserCreatableItemsHandlerCreate)(EUserCreatableItemsHandler *handler, const char *item_type_name, void *data);

struct _EUserCreatableItemsHandler {
	GObject parent;

	EUserCreatableItemsHandlerPrivate *priv;
};

struct _EUserCreatableItemsHandlerClass {
	GObjectClass parent_class;
};


GType                       e_user_creatable_items_handler_get_type   (void);
EUserCreatableItemsHandler *e_user_creatable_items_handler_new        (const char *component_alias,
								       EUserCreatableItemsHandlerCreate create_local, void *data);

void                        e_user_creatable_items_handler_activate   (EUserCreatableItemsHandler *handler,
								       BonoboUIComponent          *ui_component);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_USER_CREATABLE_ITEMS_HANDLER_H_ */
