/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-user-creatable-items-handler.h
 *
 * Copyright (C) 2001  Ximian, Inc.
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

#ifndef _E_SHELL_USER_CREATABLE_ITEMS_HANDLER_H_
#define _E_SHELL_USER_CREATABLE_ITEMS_HANDLER_H_

#include "evolution-shell-component-client.h"

#include <glib-object.h>
#include <bonobo/bonobo-ui-component.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_SHELL_USER_CREATABLE_ITEMS_HANDLER			(e_shell_user_creatable_items_handler_get_type ())
#define E_SHELL_USER_CREATABLE_ITEMS_HANDLER(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_SHELL_USER_CREATABLE_ITEMS_HANDLER, EShellUserCreatableItemsHandler))
#define E_SHELL_USER_CREATABLE_ITEMS_HANDLER_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_SHELL_USER_CREATABLE_ITEMS_HANDLER, EShellUserCreatableItemsHandlerClass))
#define E_IS_SHELL_USER_CREATABLE_ITEMS_HANDLER(obj)			(GTK_CHECK_TYPE ((obj), E_TYPE_SHELL_USER_CREATABLE_ITEMS_HANDLER))
#define E_IS_SHELL_USER_CREATABLE_ITEMS_HANDLER_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_SHELL_USER_CREATABLE_ITEMS_HANDLER))


typedef struct _EShellUserCreatableItemsHandler        EShellUserCreatableItemsHandler;
typedef struct _EShellUserCreatableItemsHandlerPrivate EShellUserCreatableItemsHandlerPrivate;
typedef struct _EShellUserCreatableItemsHandlerClass   EShellUserCreatableItemsHandlerClass;

#include "e-shell-view.h"

struct _EShellUserCreatableItemsHandler {
	GObject parent;

	EShellUserCreatableItemsHandlerPrivate *priv;
};

struct _EShellUserCreatableItemsHandlerClass {
	GObjectClass parent_class;
};


GtkType                          e_shell_user_creatable_items_handler_get_type  (void);
EShellUserCreatableItemsHandler *e_shell_user_creatable_items_handler_new       (void);

void  e_shell_user_creatable_items_handler_add_component  (EShellUserCreatableItemsHandler *handler,
							   const char                      *id,
							   EvolutionShellComponentClient   *shell_component_client);

void  e_shell_user_creatable_items_handler_attach_menus   (EShellUserCreatableItemsHandler *handler,
							   EShellView                      *shell_view);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_SHELL_USER_CREATABLE_ITEMS_HANDLER_H_ */
