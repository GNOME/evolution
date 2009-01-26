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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

#include "e-signature-utils.h"

#include <gconf/gconf-client.h>

static ESignatureList *global_signature_list;

ESignatureList *
e_get_signature_list (void)
{
	if (G_UNLIKELY (global_signature_list == NULL)) {
		GConfClient *client;

		client = gconf_client_get_default ();
		global_signature_list = e_signature_list_new (client);
		g_object_unref (client);
	}

	g_return_val_if_fail (global_signature_list != NULL, NULL);

	return global_signature_list;
}

ESignature *
e_get_signature_by_name (const gchar *name)
{
	ESignatureList *signature_list;
	const ESignature *signature;
	e_signature_find_t find;

	g_return_val_if_fail (name != NULL, NULL);

	find = E_SIGNATURE_FIND_NAME;
	signature_list = e_get_signature_list ();
	signature = e_signature_list_find (signature_list, find, name);

	/* XXX ESignatureList misuses const. */
	return (ESignature *) signature;
}

ESignature *
e_get_signature_by_uid (const gchar *uid)
{
	ESignatureList *signature_list;
	const ESignature *signature;
	e_signature_find_t find;

	g_return_val_if_fail (uid != NULL, NULL);

	find = E_SIGNATURE_FIND_UID;
	signature_list = e_get_signature_list ();
	signature = e_signature_list_find (signature_list, find, uid);

	/* XXX ESignatureList misuses const. */
	return (ESignature *) signature;
}
