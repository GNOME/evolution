/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-book-shell-module.c
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <string.h>
#include <glib/gi18n.h>
#include <libebook/e-book.h>
#include <libedataserver/e-url.h>
#include <libedataserver/e-source.h>
#include <libedataserver/e-source-list.h>
#include <libedataserver/e-source-group.h>

#include <e-shell.h>
#include <e-shell-module.h>
#include <e-shell-window.h>

#include <gal-view-collection.h>
#include <gal-view-factory-etable.h>
#include <gal-view-factory-minicard.h>

#include <eab-gui-util.h>
#include <e-book-shell-view.h>
#include <addressbook-config.h>
#include <autocompletion-config.h>

#define MODULE_NAME		"addressbook"
#define MODULE_ALIASES		""
#define MODULE_SCHEMES		""
#define MODULE_SEARCHES		"addresstypes.xml"
#define MODULE_SORT_ORDER	300

#define LDAP_BASE_URI		"ldap://"
#define PERSONAL_RELATIVE_URI	"system"
#define ETSPEC_FILENAME		"e-addressbook-view.etspec"

/* Module Entry Point */
void e_shell_module_init (GTypeModule *type_module);

GalViewCollection *e_book_shell_module_view_collection = NULL;

static void
book_module_ensure_sources (EShellModule *shell_module)
{
	ESourceList *source_list;
	ESourceGroup *on_this_computer;
	ESourceGroup *on_ldap_servers;
	ESource *personal_source;
	GSList *groups, *iter;
	const gchar *base_dir;
	gchar *base_uri;
	gchar *base_uri_proto;

	on_this_computer = NULL;
	on_ldap_servers = NULL;
	personal_source = NULL;

	if (!e_book_get_addressbooks (&source_list, NULL)) {
		g_warning ("Could not get addressbook sources from GConf!");
		return;
	}

	base_dir = e_shell_module_get_data_dir (shell_module);
	base_uri = g_build_filename (base_dir, "local", NULL);

	base_uri_proto = g_filename_to_uri (base_uri, NULL, NULL);

	groups = e_source_list_peek_groups (source_list);
	for (iter = groups; iter != NULL; iter = iter->next) {
		ESourceGroup *source_group = iter->data;
		const gchar *group_base_uri;

		group_base_uri = e_source_group_peek_base_uri (source_group);

		/* Compare only "file://" part.  if user home directory
		 * changes, we do not want to create one more group. */
		if (on_this_computer == NULL &&
			strncmp (base_uri_proto, group_base_uri, 7) == 0)
			on_this_computer = source_group;

		else if (on_ldap_servers == NULL &&
			g_str_equal (LDAP_BASE_URI, group_base_uri))
			on_ldap_servers = source_group;
	}

	if (on_this_computer != NULL) {
		GSList *sources;
		const gchar *group_base_uri;

		sources = e_source_group_peek_sources (on_this_computer);
		group_base_uri = e_source_group_peek_base_uri (on_this_computer);

		/* Make this group includes a "Personal" source. */
		for (iter = sources; iter != NULL; iter = iter->next) {
			ESource *source = iter->data;
			const gchar *relative_uri;

			relative_uri = e_source_peek_relative_uri (source);
			if (relative_uri == NULL)
				continue;

			if (g_str_equal (PERSONAL_RELATIVE_URI, relative_uri)) {
				personal_source = source;
				break;
			}
		}

		/* Make sure we have the correct base URI.  This can
		 * change when the user's home directory changes. */
		if (!g_str_equal (base_uri_proto, group_base_uri)) {
			e_source_group_set_base_uri (
				on_this_computer, base_uri_proto);

			/* XXX We shouldn't need this sync call here as
			 *     set_base_uri() results in synching to GConf,
			 *     but that happens in an idle loop and too late
			 *     to prevent the user from seeing "Cannot
			 *     Open ... because of invalid URI" error. */
			e_source_list_sync (source_list, NULL);
		}

	} else {
		ESourceGroup *source_group;
		const gchar *name;

		/* Create the local source group. */
		name = _("On This Computer");
		source_group = e_source_group_new (name, base_uri_proto);
		e_source_list_add_group (source_list, source_group, -1);
	}

	if (personal_source == NULL) {
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

		/* Create the LDAP source group. */
		name = _("On LDAP Servers");
		source_group = e_source_group_new (name, LDAP_BASE_URI);
		e_source_list_add_group (source_list, source_group, -1);
	}

	g_free (base_uri_proto);
	g_free (base_uri);
}

static void
book_module_init_view_collection (EShellModule *shell_module)
{
	GalViewCollection *collection;
	GalViewFactory *factory;
	ETableSpecification *spec;
	const gchar *base_dir;
	gchar *filename;
	gchar *system_dir;
	gchar *local_dir;

	collection = gal_view_collection_new ();
	gal_view_collection_set_title (collection, _("Address Book"));

	base_dir = EVOLUTION_GALVIEWSDIR;
	system_dir = g_build_filename (base_dir, "addressbook", NULL);

	base_dir = e_shell_module_get_data_dir (shell_module);
	local_dir = g_build_filename (base_dir, "views", NULL);

	gal_view_collection_set_storage_directories (
		collection, system_dir, local_dir);

	g_free (system_dir);
	g_free (local_dir);

	base_dir = EVOLUTION_ETSPECDIR;
	spec = e_table_specification_new ();
	filename = g_build_filename (base_dir, ETSPEC_FILENAME, NULL);
	if (!e_table_specification_load_from_file (spec, filename))
		g_error ("Unable to load ETable specification file "
			 "for address book");
	g_free (filename);

	factory = gal_view_factory_etable_new (spec);
	gal_view_collection_add_factory (collection, factory);
	g_object_unref (factory);
	g_object_unref (spec);

	factory = gal_view_factory_minicard_new ();
	gal_view_collection_add_factory (collection, factory);
	g_object_unref (factory);

	gal_view_collection_load (collection);
}

static void
book_module_book_loaded_cb (EBook *book,
                            EBookStatus status,
                            gpointer user_data)
{
	EContact *contact;
	GtkAction *action;
	const gchar *action_name;

	if (status != E_BOOK_ERROR_OK) {
		/* XXX We really need a dialog here, but we don't
		 *     have access to the ESource so we can't use
		 *     eab_load_error_dialog.  Fun! */
		return;
	}

	contact = e_contact_new ();
	action = GTK_ACTION (user_data);
	action_name = gtk_action_get_name (action);

	if (g_str_equal (action_name, "contact-new"))
		eab_show_contact_editor (book, contact, TRUE, TRUE);

	if (g_str_equal (action_name, "contact-list-new"))
		eab_show_contact_list_editor (book, contact, TRUE, TRUE);

	g_object_unref (contact);
	g_object_unref (book);
}

static void
action_contact_new_cb (GtkAction *action,
                       EShellWindow *shell_window)
{
	EBook *book;
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

	e_book_async_open (book, FALSE, book_module_book_loaded_cb, action);
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
	  "<Control>c",
	  N_("Create a new contact"),
	  G_CALLBACK (action_contact_new_cb) },

	{ "contact-new-list",
	  "stock_contact-list",
	  N_("Contact _List"),
	  "<Control>l",
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
book_module_is_busy (EShellModule *shell_module)
{
	return !eab_editor_request_close_all ();
}

static gboolean
book_module_shutdown (EShellModule *shell_module)
{
	/* FIXME */
	return TRUE;
}

static gboolean
book_module_handle_uri (EShellModule *shell_module,
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
book_module_window_created (EShellModule *shell_module,
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
	MODULE_SEARCHES,
	MODULE_SORT_ORDER,

	/* Methods */
	book_module_is_busy,
	book_module_shutdown
};

void
e_shell_module_init (GTypeModule *type_module)
{
	EShell *shell;
	EShellModule *shell_module;

	shell_module = E_SHELL_MODULE (type_module);
	shell = e_shell_module_get_shell (shell_module);

	e_book_shell_view_get_type (type_module);
	e_shell_module_set_info (shell_module, &module_info);

	book_module_ensure_sources (shell_module);
	book_module_init_view_collection (shell_module);

	g_signal_connect_swapped (
		shell, "handle-uri",
		G_CALLBACK (book_module_handle_uri), shell_module);

	g_signal_connect_swapped (
		shell, "window-created",
		G_CALLBACK (book_module_window_created), shell_module);

	autocompletion_config_init ();
}
