/* Wombat client library
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Author: Rodrigo Moya <rodrigo@ximian.com>
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
 */

#include "wombat-client.h"

#define PARENT_TYPE BONOBO_TYPE_OBJECT

struct _WombatClientPrivate {
	WombatClientGetPasswordFn    get_password;
	WombatClientForgetPasswordFn forget_password;
	gpointer                     fn_data;
};

static void wombat_client_class_init (WombatClientClass *klass);
static void wombat_client_init       (WombatClient *client, WombatClientClass *klass);
static void wombat_client_finalize   (GObject *objct);

static GObjectClass *parent_class = NULL;

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

	client = WOMBAT_CLIENT (bonobo_object_from_servant (servant));
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

	client = WOMBAT_CLIENT (bonobo_object_from_servant (servant));
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
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	POA_GNOME_Evolution_WombatClient__epv *epv = &klass->epv;

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = wombat_client_finalize;

	epv->getPassword = impl_GNOME_Evolution_WombatClient_getPassword;
	epv->forgetPassword = impl_GNOME_Evolution_WombatClient_forgetPassword;
}

static void
wombat_client_init (WombatClient *client, WombatClientClass *klass)
{
	client->priv = g_new0 (WombatClientPrivate, 1);
}

static void
wombat_client_finalize (GObject *object)
{
	WombatClient *client = (WombatClient *) object;

	g_return_if_fail (WOMBAT_IS_CLIENT (client));

	/* free memory */
	if (client->priv != NULL) {
		g_free (client->priv);
	}

	/* call parent class' destroy handler */
	if (parent_class->finalize != NULL)
		parent_class->finalize (object);
}

BONOBO_TYPE_FUNC_FULL(WombatClient, GNOME_Evolution_WombatClient, PARENT_TYPE, wombat_client)

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

	client = WOMBAT_CLIENT (g_object_new (WOMBAT_TYPE_CLIENT, NULL));
	return wombat_client_construct (client,
					get_password_fn,
					forget_password_fn,
					fn_data);
}
