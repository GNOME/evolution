/*
 * e-mail-local.c
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-mail-local.h"

#include <glib/gi18n.h>

#include "e-mail-folder-utils.h"

#define CHECK_LOCAL_FOLDER_TYPE(type) \
	((type) < G_N_ELEMENTS (default_local_folders))

/* The array elements correspond to EMailLocalFolder. */
static struct {
	const gchar *display_name;
	CamelFolder *folder;
	gchar *folder_uri;
} default_local_folders[] = {
	{ N_("Inbox") },
	{ N_("Drafts") },
	{ N_("Outbox") },
	{ N_("Sent") },
	{ N_("Templates") },
	{ "Inbox" }  /* "always local" inbox */
};

static CamelStore *local_store;
static gboolean mail_local_initialized = FALSE;

void
e_mail_local_init (EMailSession *session,
                   const gchar *data_dir)
{
	CamelService *service;
	CamelURL *url;
	gchar *temp;
	gint ii;
	GError *error = NULL;

	if (mail_local_initialized)
		return;

	g_return_if_fail (E_IS_MAIL_SESSION (session));
	g_return_if_fail (data_dir != NULL);

	mail_local_initialized = TRUE;

	url = camel_url_new ("maildir:", NULL);
	temp = g_build_filename (data_dir, "local", NULL);
	camel_url_set_path (url, temp);
	g_free (temp);

	temp = camel_url_to_string (url, 0);
	service = camel_session_add_service (
		CAMEL_SESSION (session), "local", temp,
		CAMEL_PROVIDER_STORE, &error);
	g_free (temp);

	camel_service_set_display_name (service, _("On This Computer"));

	/* Shouldn't need to worry about other mail applications
	 * altering files in our local mail store. */
	g_object_set (service, "need-summary-check", FALSE, NULL);

	if (error != NULL)
		goto fail;

	/* Populate the rest of the default_local_folders array. */
	for (ii = 0; ii < G_N_ELEMENTS (default_local_folders); ii++) {
		const gchar *display_name;

		display_name = default_local_folders[ii].display_name;

		default_local_folders[ii].folder_uri =
			e_mail_folder_uri_build (
			CAMEL_STORE (service), display_name);

		/* FIXME camel_store_get_folder() may block. */
		if (!strcmp (display_name, "Inbox"))
			default_local_folders[ii].folder =
				camel_store_get_inbox_folder_sync (
				CAMEL_STORE (service), NULL, &error);
		else
			default_local_folders[ii].folder =
				camel_store_get_folder_sync (
				CAMEL_STORE (service), display_name,
				CAMEL_STORE_FOLDER_CREATE, NULL, &error);

		if (error != NULL) {
			g_critical ("%s", error->message);
			g_clear_error (&error);
		}
	}

	camel_url_free (url);

	local_store = g_object_ref (service);

	return;

fail:
	g_critical (
		"Could not initialize local store/folder: %s",
		error->message);

	g_error_free (error);
	camel_url_free (url);
}

CamelFolder *
e_mail_local_get_folder (EMailLocalFolder type)
{
	g_return_val_if_fail (mail_local_initialized, NULL);
	g_return_val_if_fail (CHECK_LOCAL_FOLDER_TYPE (type), NULL);

	return default_local_folders[type].folder;
}

const gchar *
e_mail_local_get_folder_uri (EMailLocalFolder type)
{
	g_return_val_if_fail (mail_local_initialized, NULL);
	g_return_val_if_fail (CHECK_LOCAL_FOLDER_TYPE (type), NULL);

	return default_local_folders[type].folder_uri;
}

CamelStore *
e_mail_local_get_store (void)
{
	g_return_val_if_fail (mail_local_initialized, NULL);
	g_return_val_if_fail (CAMEL_IS_STORE (local_store), NULL);

	return local_store;
}
