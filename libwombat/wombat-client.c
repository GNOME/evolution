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

#include "wombat-client.h"

#define PARENT_TYPE BONOBO_X_OBJECT_TYPE

struct _WombatClientPrivate {
	WombatClientGetPasswordFn    get_password;
	WombatClientForgetPasswordFn forget_password;
	gpointer                     fn_data;
};

static void wombat_client_class_init (WombatClientClass *klass);
static void wombat_client_init       (WombatClient *client);
static void wombat_client_destroy    (GtkObject *objct);

/*
 * CORBA interface implementation
 */
static CORBA_char *
impl_GNOME_Evolution_WombatClient_getPassword (PortableServer_Servant servant,
					       const CORBA_char *prompt,
					       const CORBA_char *key,
					       CORBA_Environment *ev)
{
	WombatClient *client;

	client = WOMBAT_CLIENT (bonobo_x_object (servant));
	g_return_val_if_fail (WOMBAT_IS_CLIENT (client), NULL);
	g_return_val_if_fail (client->priv != NULL, NULL);

	if (client->priv->get_password != NULL)
		return client->priv->get_password (client, prompt, key, client->priv->fn_data);

	return NULL;
}

static void
impl_GNOME_Evolution_WombatClient_forgetPassword (PortableServer_Servant servant,
						  const CORBA_char *key,
						  CORBA_Environment *ev)
{
	WombatClient *client;

	client = WOMBAT_CLIENT (bonobo_x_object (servant));
	g_return_if_fail (WOMBAT_IS_CLIENT (client));
	g_return_if_fail (client->priv != NULL);

	if (client->priv->forget_password != NULL)
		client->priv->forget_password (client, key, client->priv->fn_data);
}

/*
 * WombatClient class implementation
 */
static void
wombat_client_class_init (WombatClientClass *klass)
{
	GtkObjectClass *object_class = GTK_OBJECT_CLASS(klass);
	POA_GNOME_Evolution_WombatClient__epv *epv = &klass->epv;

	object_class->destroy = wombat_client_destroy;

	epv->getPassword = impl_GNOME_Evolution_WombatClient_getPassword;
	epv->forgetPassword = impl_GNOME_Evolution_WombatClient_forgetPassword;
}

static void
wombat_client_init (WombatClient *client)
{
	client->priv = g_new0 (WombatClientPrivate, 1);
}

static void
wombat_client_destroy (GtkObject *object)
{
	GtkObjectClass *parent_class;
	WombatClient *client = (WombatClient *) object;

	g_return_if_fail (WOMBAT_IS_CLIENT (client));

	/* free memory */
	if (client->priv != NULL) {
		g_free (client->priv);
	}

	/* call parent class' destroy handler */
	parent_class = GTK_OBJECT_CLASS (gtk_type_class (PARENT_TYPE));
	if (parent_class->destroy != NULL)
		parent_class->destroy (GTK_OBJECT(client));
}

/**
 * wombat_client_get_type
 */
GtkType
wombat_client_get_type (void)
{
	static GtkType type = 0;

	if (!type) {
		GtkTypeInfo info = {
                        "WombatClient",
                        sizeof (WombatClient),
                        sizeof (WombatClientClass),
                        (GtkClassInitFunc) wombat_client_class_init,
                        (GtkObjectInitFunc) wombat_client_init,
                        (GtkArgSetFunc) NULL,
                        (GtkArgSetFunc) NULL
                };
                type = bonobo_x_type_unique(
                        PARENT_TYPE,
                        POA_GNOME_Evolution_WombatClient__init, NULL,
                        GTK_STRUCT_OFFSET (WombatClientClass, epv),
                        &info);
	}
	return type;
}

/**
 * wombat_client_construct
 */
WombatClient *
wombat_client_construct (WombatClient *client,
			 WombatClientGetPasswordFn get_password_fn,
			 WombatClientForgetPasswordFn forget_password_fn,
			 gpointer fn_data)
{
	g_return_val_if_fail (WOMBAT_IS_CLIENT (client), NULL);
	g_return_val_if_fail (client->priv != NULL, NULL);

	client->priv->get_password = get_password_fn;
	client->priv->forget_password = forget_password_fn;
	client->priv->fn_data = fn_data;

	return client;
}

/**
 * wombat_client_new
 */
WombatClient *
wombat_client_new (WombatClientGetPasswordFn get_password_fn,
		   WombatClientForgetPasswordFn forget_password_fn,
		   gpointer fn_data)
{
	WombatClient *client;

	client = WOMBAT_CLIENT (gtk_type_new (WOMBAT_TYPE_CLIENT));
	return wombat_client_construct (client,
					get_password_fn,
					forget_password_fn,
					fn_data);
}
