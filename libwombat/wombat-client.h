/* Wombat client library
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Author: Rodrigo Moya <rodrigo@ximian.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef WOMBAT_CLIENT_H
#define WOMBAT_CLIENT_H

#include <libgnome/gnome-defs.h>
#include <bonobo/bonobo-xobject.h>
#include "wombat.h"

BEGIN_GNOME_DECLS

#define WOMBAT_TYPE_CLIENT            (wombat_client_get_type())
#define WOMBAT_CLIENT(obj)            GTK_CHECK_CAST(obj, WOMBAT_TYPE_CLIENT, WombatClient)
#define WOMBAT_CLIENT_CLASS(klass)    GTK_CHECK_CLASS_CAST(klass, WOMBAT_TYPE_CLIENT, WombatClientClass)
#define WOMBAT_IS_CLIENT(obj)         GTK_CHECK_TYPE(obj, WOMBAT_TYPE_CLIENT)
#define WOMBAT_IS_CLIENT_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), WOMBAT_TYPE_CLIENT))

typedef struct _WombatClient        WombatClient;
typedef struct _WombatClientClass   WombatClientClass;
typedef struct _WombatClientPrivate WombatClientPrivate;

struct _WombatClient {
	BonoboXObject        object;
	WombatClientPrivate *priv;
};

struct _WombatClientClass {
	BonoboXObjectClass parent_class;

	POA_GNOME_Evolution_WombatClient__epv epv;
};

typedef gchar * (* WombatClientGetPasswordFn) (WombatClient *client,
					       const gchar *prompt,
					       const gchar *key,
					       gpointer user_data);
typedef void    (* WombatClientForgetPasswordFn) (WombatClient *client,
						  const gchar *key,
						  gpointer user_data);

GtkType       wombat_client_get_type  (void);

WombatClient *wombat_client_construct (WombatClient *client,
				       WombatClientGetPasswordFn get_password_fn,
				       WombatClientForgetPasswordFn forget_password_fn,
				       gpointer fn_data);
WombatClient *wombat_client_new       (WombatClientGetPasswordFn get_password_fn,
				       WombatClientForgetPasswordFn forget_password_fn,
				       gpointer fn_data);


END_GNOME_DECLS

#endif
