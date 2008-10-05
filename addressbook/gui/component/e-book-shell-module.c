/*
 * e-book-shell-module.c
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

#include <string.h>
#include <glib/gi18n.h>
#include <libebook/e-book.h>
#include <libedataserver/e-url.h>
#include <libedataserver/e-source.h>
#include <libedataserver/e-source-list.h>
#include <libedataserver/e-source-group.h>

#include "shell/e-shell.h"
#include "shell/e-shell-module.h"
#include "shell/e-shell-window.h"

#include "e-util/e-import.h"
#include "addressbook/importers/evolution-addressbook-importers.h"

#include <eab-config.h>
#include <eab-gui-util.h>
#include <addressbook-config.h>
#include <autocompletion-config.h>

#include "e-book-shell-view.h"
#include "e-book-shell-module-migrate.h"

#define MODULE_NAME		"addressbook"
#define MODULE_ALIASES		"contacts"
#define MODULE_SCHEMES		""
#define MODULE_SORT_ORDER	300

#define LDAP_BASE_URI		"ldap://"
#define PERSONAL_RELATIVE_URI	"system"

/* Module Entry Point */
void e_shell_module_init (GTypeModule *type_module);

static void
book_shell_module_ensure_sources (EShellModule *shell_module)
{
	/* XXX This is basically the same algorithm across all modules.
	 *     Maybe we could somehow integrate this into EShellModule? */

	ESourceList *source_list;
	ESourceGroup *on_this_computer;
	ESourceGroup *on_ldap_servers;
	ESource *personal;
	GSList *groups, *iter;
	const gchar *data_dir;
	gchar *base_uri;
	gchar *filename;

	on_this_computer = NULL;
	on_ldap_servers = NULL;
	personal = NULL;

	if (!e_book_get_addressbooks (&source_list, NULL)) {
		g_warning ("Could not get addressbook sources from GConf!");
		return;
	}

	/* Share the source list with all address book views.  This
	 * is accessible via e_book_shell_view_get_source_list().
	 * Note: EShellModule takes ownership of the reference.
	 *
	 * XXX I haven't yet decided if I want to add a proper
	 *     EShellModule API for this.  The mail module would
	 *     not use it. */
	g_object_set_data_full (
		G_OBJECT (shell_module), "source-list",
		source_list, (GDestroyNotify) g_object_unref);

	data_dir = e_shell_module_get_data_dir (shell_module);
	filename = g_build_filename (data_dir, "local", NULL);
	base_uri = g_filename_to_uri (filename, NULL, NULL);
	g_free (filename);

	groups = e_source_list_peek_groups (source_list);
	for (iter = groups; iter != NULL; iter = iter->next) {
		ESourceGroup *source_group = iter->data;
		const gchar *group_base_uri;

		group_base_uri = e_source_group_peek_base_uri (source_group);

		/* Compare only "file://" part.  If the user's home
		 * changes, we do not want to create another group. */
		if (on_this_computer == NULL &&
			strncmp (base_uri, group_base_uri, 7) == 0)
			on_this_computer = source_group;

		else if (on_ldap_servers == NULL &&
			strcmp (LDAP_BASE_URI, group_base_uri) == 0)
			on_ldap_servers = source_group;
	}

	if (on_this_computer != NULL) {
		GSList *sources;
		const gchar *group_base_uri;

		sources = e_source_group_peek_sources (on_this_computer);
		group_base_uri = e_source_group_peek_base_uri (on_this_computer);

		/* Make sure this group includes a "Personal" source. */
		for (iter = sources; iter != NULL; iter = iter->next) {
			ESource *source = iter->data;
			const gchar *relative_uri;

			relative_uri = e_source_peek_relative_uri (source);
			if (relative_uri == NULL)
				continue;

			if (strcmp (PERSONAL_RELATIVE_URI, relative_uri) != 0)
				continue;

			personal = source;
			break;
		}

		/* Make sure we have the correct base URI.  This can
		 * change when the user's home directory changes. */
		if (strcmp (base_uri, group_base_uri) != 0) {
			e_source_group_set_base_uri (
				on_this_computer, base_uri);

			/* XXX We shouldn't need this sync call here as
			 *     set_base_uri() results in synching to GConf,
			 *     but that happens in an idle loop and too late
			 *     to prevent the user from seeing a "Cannot
			 *     Open ... because of invalid URI" error. */
			e_source_list_sync (source_list, NULL);
		}

	} else {
		ESourceGroup *source_group;
		const gchar *name;

		name = _("On This Computer");
		source_group = e_source_group_new (name, base_uri);
		e_source_list_add_group (source_list, source_group, -1);
		g_object_unref (source_group);
	}

	if (personal == NULL) {
		ESource *source;
		const gchar *name;

		/* Create the default Personal address book. */
		name = _("Personal");
		source = e_source_new (name, PERSONAL_RELATIVE_URI);
		e_source_group_add_source (on_this_computer, source, -1);
		e_source_set_property (source, "completion", "true");
		g_object_unref (source);
	}

	if (on_ldap_servers == NULL) {
		ESourceGroup *source_group;
		const gchar *name;

		name = _("On LDAP Servers");
		source_group = e_source_group_new (name, LDAP_BASE_URI);
		e_source_list_add_group (source_list, source_group, -1);
		g_object_unref (source_group);
	}

	g_free (base_uri);
}

static void
book_shell_module_init_importers (void)
{
	EImportClass *import_class;
	EImportImporter *importer;

	import_class = g_type_class_ref (e_import_get_type ());

	importer = evolution_ldif_importer_peek ();
	e_import_class_add_importer (import_class, importer, NULL, NULL);

	importer = evolution_vcard_importer_peek ();
	e_import_class_add_importer (import_class, importer, NULL, NULL);

	importer = evolution_csv_outlook_importer_peek ();
	e_import_class_add_importer (import_class, importer, NULL, NULL);

	importer = evolution_csv_mozilla_importer_peek ();
	e_import_class_add_importer (import_class, importer, NULL, NULL);

	importer = evolution_csv_evolution_importer_peek ();
	e_import_class_add_importer (import_class, importer, NULL, NULL);
}

static void
book_shell_module_book_loaded_cb (EBook *book,
                                  EBookStatus status,
                                  gpointer user_data)
{
	EContact *contact;
	GtkAction *action;
	const gchar *action_name;

	/* XXX Handle errors better. */
	if (status != E_BOOK_ERROR_OK)
		return;

	contact = e_contact_new ();
	action = GTK_ACTION (user_data);
	action_name = gtk_action_get_name (action);

	if (strcmp (action_name, "contact-new") == 0)
		eab_show_contact_editor (book, contact, TRUE, TRUE);

	if (strcmp (action_name, "contact-new-list") == 0)
		eab_show_contact_list_editor (book, contact, TRUE, TRUE);

	g_object_unref (contact);
	g_object_unref (book);
}

static void
action_contact_new_cb (GtkAction *action,
                       EShellWindow *shell_window)
{
	EBook *book = NULL;
	GConfClient *client;
	ESourceList *source_list;
	const gchar *key;
	gchar *uid;

	/* This callback is used for both contacts and contact lists. */

	if (!e_book_get_addressbooks (&source_list, NULL)) {
		g_warning ("Could not get addressbook sources from GConf!");
		return;
	}

	client = gconf_client_get_default ();
	key = "/apps/evolution/addressbook/display/primary_addressbook";
	uid = gconf_client_get_string (client, key, NULL);
	g_object_unref (client);

	if (uid != NULL) {
		ESource *source;

		source = e_source_list_peek_source_by_uid (source_list, uid);
		if (source != NULL)
			book = e_book_new (source, NULL);
		g_free (uid);
	}

	if (book == NULL)
		book = e_book_new_default_addressbook (NULL);

	e_book_async_open (
		book, FALSE, book_shell_module_book_loaded_cb, action);
}

static void
action_address_book_new_cb (GtkAction *action,
                            EShellWindow *shell_window)
{
	addressbook_config_create_new_source (NULL);
}

static GtkActionEntry item_entries[] = {

	{ "contact-new",
	  "contact-new",
	  N_("_Contact"),  /* XXX Need C_() here */
	  "<Shift><Control>c",
	  N_("Create a new contact"),
	  G_CALLBACK (action_contact_new_cb) },

	{ "contact-new-list",
	  "stock_contact-list",
	  N_("Contact _List"),
	  "<Shift><Control>l",
	  N_("Create a new contact list"),
	  G_CALLBACK (action_contact_new_cb) }
};

static GtkActionEntry source_entries[] = {

	{ "address-book-new",
	  "address-book-new",
	  N_("Address _Book"),
	  NULL,
	  N_("Create a new address book"),
	  G_CALLBACK (action_address_book_new_cb) }
};

static gboolean
book_shell_module_is_busy (EShellModule *shell_module)
{
	return !eab_editor_request_close_all ();
}

static gboolean
book_shell_module_shutdown (EShellModule *shell_module)
{
	/* FIXME */
	return TRUE;
}

static gboolean
book_shell_module_handle_uri (EShellModule *shell_module,
                              const gchar *uri)
{
	EUri *euri;
	const gchar *cp;
	gchar *source_uid = NULL;
	gchar *contact_uid = NULL;

	if (!g_str_has_prefix (uri, "contacts:"))
		return FALSE;

	euri = e_uri_new (uri);
	cp = euri->query;

	if (cp == NULL) {
		e_uri_free (euri);
		return FALSE;
	}

	while (*cp != '\0') {
		gchar *header;
		gchar *content;
		gsize length;
		gsize content_length;

		length = strcspn (cp, "=&");

		/* If it's malformed, give up. */
		if (cp[length] != '=')
			break;

		header = (gchar *) cp;
		header[length] = '\0';
		cp += length + 1;

		content_length = strcspn (cp, "&");
		content = g_strndup (cp, content_length);

		if (g_ascii_strcasecmp (header, "source-uid") == 0)
			source_uid = g_strdup (content);

		if (g_ascii_strcasecmp (header, "contact-uid") == 0)
			contact_uid = g_strdup (content);

		g_free (content);

		cp += content_length;
		if (*cp == '&') {
			cp++;
			if (strcmp (cp, "amp;"))
				cp += 4;
		}
	}

	/* FIXME */
	/*addressbook_view_edit_contact (view, source_uid, contact_uid);*/

	g_free (source_uid);
	g_free (contact_uid);

	e_uri_free (euri);

	return TRUE;
}

static void
book_shell_module_window_created (EShellModule *shell_module,
                                  EShellWindow *shell_window)
{
	const gchar *module_name;

	module_name = G_TYPE_MODULE (shell_module)->name;

	e_shell_window_register_new_item_actions (
		shell_window, module_name,
		item_entries, G_N_ELEMENTS (item_entries));

	e_shell_window_register_new_source_actions (
		shell_window, module_name,
		source_entries, G_N_ELEMENTS (source_entries));
}

static EShellModuleInfo module_info = {

	MODULE_NAME,
	MODULE_ALIASES,
	MODULE_SCHEMES,
	MODULE_SORT_ORDER,

	/* Methods */
	book_shell_module_is_busy,
	book_shell_module_shutdown,
	e_book_shell_module_migrate
};

void
e_shell_module_init (GTypeModule *type_module)
{
	EShell *shell;
	EShellModule *shell_module;

	shell_module = E_SHELL_MODULE (type_module);
	shell = e_shell_module_get_shell (shell_module);

	/* Register the GType for EBookShellView. */
	e_book_shell_view_get_type (type_module);

	e_shell_module_set_info (shell_module, &module_info);

	book_shell_module_init_importers ();
	book_shell_module_ensure_sources (shell_module);

	e_plugin_hook_register_type (eab_config_get_type ());

	g_signal_connect_swapped (
		shell, "handle-uri",
		G_CALLBACK (book_shell_module_handle_uri), shell_module);

	g_signal_connect_swapped (
		shell, "window-created",
		G_CALLBACK (book_shell_module_window_created), shell_module);

	autocompletion_config_init ();
}
