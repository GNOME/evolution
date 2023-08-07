/*
 * Copyright (C) 2015 SUSE (www.suse.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 * Authors:
 *           David Liang <dliang@suse.com>
 *
 */

#include "evolution-config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include <libebook/libebook.h>

#include "shell/e-shell.h"
#include "mail/e-mail-backend.h"

#include "kmail-libs.h"

static GSList  *kmail_read_folder (const gchar *path, GSList *list);

const CamelStore *
evolution_get_local_store (void)
{
	EShell *shell;
	EShellBackend *shell_backend;
	EMailBackend *backend;
	EMailSession *session;
	static CamelStore *local_store = NULL;

	if (local_store)
		return local_store;
	/* XXX Dig up the mail backend from the default EShell.
	 *     Since the EImport framework doesn't allow for user
	 *     data, I don't see how else to get to it. */
	shell = e_shell_get_default ();
	shell_backend = e_shell_get_backend_by_name (shell, "mail");

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);

	local_store = e_mail_session_get_local_store (session);

	return local_store;
}

static gboolean
is_kmail_box (const gchar *k_folder)
{
	const gchar *special_folders []= {"cur", "tmp", "new", NULL};
	gchar *source_dir;
	GDir *dir;
	gint i;

	for (i = 0; special_folders[i]; i++) {
		source_dir = g_build_filename (k_folder, special_folders[i], NULL);
		dir = g_dir_open (source_dir, 0, NULL);
		if (!dir) {
			/* If we did not find the subdir with 'cur' 'tmp' and 'new',
			   we don't take it as the kmail box. */
			g_free (source_dir);
			return FALSE;
		}
		g_dir_close (dir);
		g_free (source_dir);
	}

	/* No matter whether the folder was empty, we return it to the importer */
	return TRUE;
}

static gboolean
is_kmail_directory (const gchar *folder)
{
	if (g_str_has_prefix (folder, ".") && g_str_has_suffix (folder, ".directory"))
		return TRUE;
	else
		return FALSE;
}

gchar *
kmail_get_base_dir (void)
{
	gchar *base_dir;

	base_dir = g_build_filename (g_get_home_dir (), KMAIL_4_3_DIR, NULL);

	return base_dir;
}

gchar *
kuri_to_euri (const gchar *k_uri)
{
	gchar *base_dir;
	gchar *p;
	gchar **folders;
	GString *e_folder = NULL;
	gint i;
	gboolean dropped = FALSE;

	e_folder = g_string_new (EVOLUTION_LOCAL_BASE);
	base_dir = g_build_filename (g_get_home_dir (), KMAIL_4_3_DIR, NULL);
	p = (gchar *) k_uri + strlen (base_dir) + 1;
	folders = g_strsplit (p, "/", -1);

	for (i = 0; folders[i]; i++) {
		gchar *folder = folders[i];
		if (g_str_has_prefix (folder, ".") && g_str_has_suffix (folder, ".directory")) {
			folder ++;
			p = g_strrstr (folder, ".directory");
			*p = '\0';
		}
		if (i == 0) {
			/* Some local folders */
			if ((strcasecmp (folder, "Inbox") == 0) || (strcmp (folder, _("Inbox")) == 0)) {
				folder = (gchar *)"Inbox";
			} else if ((strcasecmp (folder, "Outbox") == 0) || (strcmp (folder, _("Outbox")) == 0)) {
				folder = (gchar *)"Outbox";
			} else if ((strcasecmp (folder, "sent-mail") == 0) || (strcmp (folder, _("Sent")) == 0)) {
				folder = (gchar *)"Sent";
			} else if ((strcasecmp (folder, "drafts") == 0) || (strcmp (folder, _("Drafts")) == 0)) {
				folder = (gchar *)"Drafts";
			} else if ((strcasecmp (folder, "templates") == 0) || (strcmp (folder, _("Templates")) == 0)) {
				folder = (gchar *)"Templates";
			} else if ((strcasecmp (folder, "trash") == 0) || (strcmp (folder, _("Trash")) == 0)) {
				dropped = TRUE;
				break;
			}
		}
		g_string_append_printf (e_folder, "/%s", folder);
	}

	g_strfreev (folders);
	return g_string_free (e_folder, dropped);
}

static GSList *
kmail_read_folder (const gchar *path, GSList *kmail_list)
{
	GDir *dir;
	gchar *filename;
	const gchar *d;
	struct stat st;

	dir = g_dir_open (path, 0, NULL);

	while ((d = g_dir_read_name (dir))) {
		if ((strcmp (d, ".") == 0) || (strcmp (d, "..") == 0)) {
			continue;
		}

		filename = g_build_filename (path, d, NULL);
		/* skip non files and directories, and skip directories in mozilla mode */
		if (g_stat (filename, &st) == -1) {
			g_free (filename);
			continue;
		}
		if (S_ISDIR (st.st_mode)) {
			if (is_kmail_directory (d)) {
				kmail_list = kmail_read_folder (filename, kmail_list);
			} else if (is_kmail_box (filename)) {
				kmail_list = g_slist_prepend (kmail_list, g_strdup (filename));
			}
		}
		g_free (filename);
	}
	g_dir_close (dir);

	return kmail_list;
}

GSList *
kmail_get_folders (gchar *path)
{
	GSList *list = NULL;

	list = kmail_read_folder (path, list);

	return list;
}

/* Copied from addressbook/util/eab-book-util.c:eab_contact_list_from_string */
static GSList *
get_contact_list_from_string (const gchar *str)
{
	GSList *contacts = NULL;
	GString *gstr = g_string_new (NULL);
	gchar *str_stripped;
	gchar *p = (gchar *) str;
	gchar *q;
	if (!p)
		return NULL;

	if (!strncmp (p, "Book: ", 6)) {
		p = strchr (p, '\n');
		if (!p) {
			g_warning (G_STRLOC ": Got book but no newline!");
			return NULL;
		}
		p++;
	}

	while (*p) {
		if (*p != '\r') g_string_append_c (gstr, *p);

		p++;
	}

	p = str_stripped = g_string_free (gstr, FALSE);

	for (p = camel_strstrcase (p, "BEGIN:VCARD"); p; p = camel_strstrcase (q, "\nBEGIN:VCARD")) {
		gchar *card_str;

		if (*p == '\n')
			p++;

		for (q = camel_strstrcase (p, "END:VCARD"); q; q = camel_strstrcase (q, "END:VCARD")) {
			gchar *temp;

			q += 9;
			temp = q;
			if (*temp)
				temp += strspn (temp, "\r\n\t ");

			if (*temp == '\0' || !g_ascii_strncasecmp (temp, "BEGIN:VCARD", 11))
				break;  /* Found the outer END:VCARD */
		}

		if (!q)
			break;
		card_str = g_strndup (p, q - p);
		contacts = g_slist_prepend (contacts, e_contact_new_from_vcard (card_str));
		g_free (card_str);
	}

	g_free (str_stripped);

	return g_slist_reverse (contacts);
}

static gchar *
get_kcontact_folder (void)
{
	gchar *folder;

	folder = g_build_filename (g_get_home_dir (), KCONTACT_4_3_DIR, NULL);

	return folder;
}

GSList *
kcontact_get_list (void)
{
	GSList *list = NULL;
	gchar *foldername = NULL;
	gchar *filename;
	const gchar *d;
	GDir *dir;
	struct stat st;

	foldername = get_kcontact_folder ();
	if (!foldername)
		return NULL;
	dir = g_dir_open (foldername, 0, NULL);

	while ((d = g_dir_read_name (dir))) {
		if ((strcmp (d, ".") == 0) || (strcmp (d, "..") == 0)) {
			continue;
		}
		if (!g_str_has_suffix (d, ".vcf")) {
			continue;
		}
		filename = g_build_filename (foldername, d, NULL);
		if (g_stat (filename, &st) == -1) {
			g_free (filename);
			continue;
		}
		if (S_ISREG (st.st_mode)) {
			list = g_slist_prepend (list, filename);
		}
	}

	g_free (foldername);
	g_dir_close (dir);

	return list;
}

void
kcontact_load (GSList *files)
{
	GSList *contactlist = NULL;
	GSList *l;

	GError *error = NULL;
	GString *vcards = NULL;
	EBookClient *book_client;
	EClient *client;
	EShell *shell;
	ESourceRegistry *registry;
	EClientCache *client_cache;
	ESource *primary;

	if (!files)
		return;

	shell = e_shell_get_default ();
	registry = e_shell_get_registry (shell);

	primary = e_source_registry_ref_default_address_book (registry);
	if (!primary) {
		printf ("%s: No default address book found\n", G_STRFUNC);
		return;
	}

	client_cache = e_shell_get_client_cache (shell);
	client = e_client_cache_get_client_sync (client_cache, primary, E_SOURCE_EXTENSION_ADDRESS_BOOK, E_DEFAULT_WAIT_FOR_CONNECTED_SECONDS, NULL, &error);

	if (!client) {
		printf ("%s: Failed to open address book '%s': %s\n", G_STRFUNC, e_source_get_display_name (primary), error ? error->message : "Unknown error");
		g_clear_object (&primary);
		g_clear_error (&error);
		return;
	}
	g_clear_object (&primary);

	book_client = E_BOOK_CLIENT (client);

	for (l = files; l; l = l->next) {
		const gchar *filename;
		gchar *contents = NULL;

		filename = (gchar *) l->data;
		if (g_file_get_contents (filename, &contents, NULL, NULL)) {
			if (vcards == NULL) {
				vcards = g_string_new (contents);
			} else {
				g_string_append_c (vcards, '\n');
				g_string_append (vcards, contents);
			}
			g_free (contents);
		}
	}

	if (vcards) {
		contactlist = get_contact_list_from_string (vcards->str);
	}

	if (contactlist) {
		e_book_client_add_contacts_sync (book_client, contactlist, E_BOOK_OPERATION_FLAG_NONE, NULL, NULL, &error);

		if (error) {
			printf ("%s: Failed to add contacts: %s\n", G_STRFUNC, error->message);
			g_error_free (error);
		}
	}

	if (vcards)
		g_string_free (vcards, TRUE);
	if (contactlist)
		g_slist_free_full (contactlist, g_object_unref);
	g_object_unref (book_client);
}

gboolean
kmail_is_supported (void)
{
	gchar *kmaildir;
	gboolean exists;

	kmaildir = g_build_filename (g_get_home_dir (), KMAIL_4_3_DIR, NULL);
	exists = g_file_test (kmaildir, G_FILE_TEST_IS_DIR);
	g_free (kmaildir);

	if (!exists)
		return FALSE;

	return TRUE;
}
