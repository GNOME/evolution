/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Ettore Perazzoli <ettore@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_USER_CREATABLE_ITEMS_HANDLER_H_
#define _E_USER_CREATABLE_ITEMS_HANDLER_H_

#include <glib-object.h>
#include <bonobo/bonobo-ui-component.h>
#include <bonobo/bonobo-window.h>

G_BEGIN_DECLS

#define E_TYPE_USER_CREATABLE_ITEMS_HANDLER		(e_user_creatable_items_handler_get_type ())
#define E_USER_CREATABLE_ITEMS_HANDLER(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_USER_CREATABLE_ITEMS_HANDLER, EUserCreatableItemsHandler))
#define E_USER_CREATABLE_ITEMS_HANDLER_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_USER_CREATABLE_ITEMS_HANDLER, EUserCreatableItemsHandlerClass))
#define E_IS_USER_CREATABLE_ITEMS_HANDLER(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_USER_CREATABLE_ITEMS_HANDLER))
#define E_IS_USER_CREATABLE_ITEMS_HANDLER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_USER_CREATABLE_ITEMS_HANDLER))

typedef struct _EUserCreatableItemsHandler        EUserCreatableItemsHandler;
typedef struct _EUserCreatableItemsHandlerPrivate EUserCreatableItemsHandlerPrivate;
typedef struct _EUserCreatableItemsHandlerClass   EUserCreatableItemsHandlerClass;

typedef void (*EUserCreatableItemsHandlerCreate)(EUserCreatableItemsHandler *handler, const gchar *item_type_name, gpointer data);

struct _EUserCreatableItemsHandler {
	GObject parent;

	EUserCreatableItemsHandlerPrivate *priv;
};

struct _EUserCreatableItemsHandlerClass {
	GObjectClass parent_class;
};

GType                       e_user_creatable_items_handler_get_type   (void);
EUserCreatableItemsHandler *e_user_creatable_items_handler_new        (const gchar *component_alias,
								       EUserCreatableItemsHandlerCreate create_local, gpointer data);

void                        e_user_creatable_items_handler_activate   (EUserCreatableItemsHandler *handler,
								       BonoboUIComponent          *ui_component);

G_END_DECLS

#endif /* _E_USER_CREATABLE_ITEMS_HANDLER_H_ */
