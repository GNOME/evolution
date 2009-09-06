/*
 * e-book-shell-backend.c
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

#include "e-book-shell-backend.h"

#include <config.h>

#include <string.h>
#include <glib/gi18n.h>
#include <libebook/e-book.h>
#include <libedataserver/e-url.h>
#include <libedataserver/e-source.h>
#include <libedataserver/e-source-group.h>

#include "e-util/e-import.h"
#include "shell/e-shell.h"
#include "shell/e-shell-window.h"
#include "widgets/misc/e-preferences-window.h"

#include "addressbook/gui/widgets/eab-gui-util.h"
#include "addressbook/gui/contact-editor/e-contact-editor.h"
#include "addressbook/gui/contact-editor/e-contact-quick-add.h"
#include "addressbook/gui/contact-list-editor/e-contact-list-editor.h"
#include "addressbook/importers/evolution-addressbook-importers.h"

#include "addressbook-config.h"
#include "autocompletion-config.h"

#include "e-book-shell-migrate.h"
#include "e-book-shell-settings.h"
#include "e-book-shell-view.h"

#ifdef ENABLE_SMIME
#include "smime/gui/component.h"
#include "smime/gui/certificate-manager.h"
#endif

#define E_BOOK_SHELL_BACKEND_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_BOOK_SHELL_BACKEND, EBookShellBackendPrivate))

#define LDAP_BASE_URI		"ldap://"
#define PERSONAL_RELATIVE_URI	"system"

struct _EBookShellBackendPrivate {
	ESourceList *source_list;
};

enum {
	PROP_0,
	PROP_SOURCE_LIST
};

static gpointer parent_class;
static GType book_shell_backend_type;

static void
book_shell_backend_ensure_sources (EShellBackend *shell_backend)
{
	/* XXX This is basically the same algorithm across all backends.
	 *     Maybe we could somehow integrate this into EShellBackend? */

	EBookShellBackendPrivate *priv;
	ESourceGroup *on_this_computer;
	ESourceGroup *on_ldap_servers;
	ESource *personal;
	GSList *groups, *iter;
	const gchar *data_dir;
	const gchar *name;
	gchar *base_uri;
	gchar *filename;

	on_this_computer = NULL;
	on_ldap_servers = NULL;
	personal = NULL;

	priv = E_BOOK_SHELL_BACKEND_GET_PRIVATE (shell_backend);

	if (!e_book_get_addressbooks (&priv->source_list, NULL)) {
		g_warning ("Could not get addressbook sources from GConf!");
		return;
	}

	data_dir = e_shell_backend_get_data_dir (shell_backend);
	filename = g_build_filename (data_dir, "local", NULL);
	base_uri = g_filename_to_uri (filename, NULL, NULL);
	g_free (filename);

	groups = e_source_list_peek_groups (priv->source_list);
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

	name = _("On This Computer");

	if (on_this_computer != NULL) {
		GSList *sources;
		const gchar *group_base_uri;

		/* Force the group name to the current locale. */
		e_source_group_set_name (on_this_computer, name);

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
			e_source_list_sync (priv->source_list, NULL);
		}

	} else {
		ESourceGroup *source_group;

		source_group = e_source_group_new (name, base_uri);
		e_source_list_add_group (priv->source_list, source_group, -1);
		g_object_unref (source_group);
	}

	name = _("Personal");

	if (personal == NULL) {
		ESource *source;

		/* Create the default Personal address book. */
		source = e_source_new (name, PERSONAL_RELATIVE_URI);
		e_source_group_add_source (on_this_computer, source, -1);
		e_source_set_property (source, "completion", "true");
		g_object_unref (source);
	} else {
		/* Force the source name to the current locale. */
		e_source_set_name (personal, name);
	}

	name = _("On LDAP Servers");

	if (on_ldap_servers == NULL) {
		ESourceGroup *source_group;

		source_group = e_source_group_new (name, LDAP_BASE_URI);
		e_source_list_add_group (priv->source_list, source_group, -1);
		g_object_unref (source_group);
	} else {
		/* Force the group name to the current locale. */
		e_source_group_set_name (on_ldap_servers, name);
	}

	g_free (base_uri);
}

static void
book_shell_backend_init_importers (void)
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
book_shell_backend_new_contact_cb (EBook *book,
                                   EBookStatus status,
                                   gpointer user_data)
{
	EShell *shell;
	EContact *contact;
	EABEditor *editor;

	/* XXX Handle errors better. */
	if (status != E_BOOK_ERROR_OK)
		return;

	contact = e_contact_new ();
	shell = E_SHELL (user_data);

	editor = e_contact_editor_new (
		shell, book, contact, TRUE, TRUE);

	eab_editor_show (editor);

	g_object_unref (contact);
	g_object_unref (book);
}

static void
book_shell_backend_new_contact_list_cb (EBook *book,
                                        EBookStatus status,
                                        gpointer user_data)
{
	EShell *shell;
	EContact *contact;
	EABEditor *editor;

	/* XXX Handle errors better. */
	if (status != E_BOOK_ERROR_OK)
		return;

	contact = e_contact_new ();
	shell = E_SHELL (user_data);

	editor = e_contact_list_editor_new (
		shell, book, contact, TRUE, TRUE);

	eab_editor_show (editor);

	g_object_unref (contact);
	g_object_unref (book);
}

static void
action_contact_new_cb (GtkAction *action,
                       EShellWindow *shell_window)
{
	EShell *shell;
	EBook *book = NULL;
	GConfClient *client;
	ESourceList *source_list;
	const gchar *action_name;
	const gchar *key;
	gchar *uid;

	/* This callback is used for both contacts and contact lists. */

	if (!e_book_get_addressbooks (&source_list, NULL)) {
		g_warning ("Could not get addressbook sources from GConf!");
		return;
	}

	shell = e_shell_window_get_shell (shell_window);
	client = e_shell_get_gconf_client (shell);
	action_name = gtk_action_get_name (action);

	key = "/apps/evolution/addressbook/display/primary_addressbook";
	uid = gconf_client_get_string (client, key, NULL);

	if (uid != NULL) {
		ESource *source;

		source = e_source_list_peek_source_by_uid (source_list, uid);
		if (source != NULL)
			book = e_book_new (source, NULL);
		g_free (uid);
	}

	if (book == NULL)
		book = e_book_new_default_addressbook (NULL);

	if (strcmp (action_name, "contact-new") == 0)
		e_book_async_open (
			book, FALSE,
			book_shell_backend_new_contact_cb, shell);

	if (strcmp (action_name, "contact-new-list") == 0)
		e_book_async_open (
			book, FALSE,
			book_shell_backend_new_contact_list_cb, shell);
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
	  NC_("New", "_Contact"),
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
	  NC_("New", "Address _Book"),
	  NULL,
	  N_("Create a new address book"),
	  G_CALLBACK (action_address_book_new_cb) }
};

static gboolean
book_shell_backend_init_preferences (EShell *shell)
{
	GtkWidget *preferences_window;

	/* This is a main loop idle callback. */

	preferences_window = e_shell_get_preferences_window (shell);

	e_preferences_window_add_page (
		E_PREFERENCES_WINDOW (preferences_window),
		"contacts",
		"preferences-autocompletion",
		_("Contacts"),
		autocompletion_config_new (shell),
		200);

	return FALSE;
}

static void
book_shell_backend_quick_add_email_cb (EShell *shell,
                                       const gchar *email)
{
	/* XXX This is an ugly hack but it's the only way I could think
	 *     of to integrate this feature with other shell modules. */

	e_contact_quick_add_free_form (email, NULL, NULL);
}

static void
book_shell_backend_quick_add_vcard_cb (EShell *shell,
                                       const gchar *vcard)
{
	/* XXX This is an ugly hack but it's the only way I could think
	 *     of to integrate this feature with other shell modules. */

	e_contact_quick_add_vcard (vcard, NULL, NULL);
}

static gboolean
book_shell_backend_handle_uri_cb (EShellBackend *shell_backend,
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
book_shell_backend_window_created_cb (EShellBackend *shell_backend,
                                      GtkWindow *window)
{
	const gchar *backend_name;

	if (!E_IS_SHELL_WINDOW (window))
		return;

	backend_name = E_SHELL_BACKEND_GET_CLASS (shell_backend)->name;

	e_shell_window_register_new_item_actions (
		E_SHELL_WINDOW (window), backend_name,
		item_entries, G_N_ELEMENTS (item_entries));

	e_shell_window_register_new_source_actions (
		E_SHELL_WINDOW (window), backend_name,
		source_entries, G_N_ELEMENTS (source_entries));
}

static void
book_shell_backend_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SOURCE_LIST:
			g_value_set_object (
				value,
				e_book_shell_backend_get_source_list (
				E_BOOK_SHELL_BACKEND (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
book_shell_backend_dispose (GObject *object)
{
	EBookShellBackendPrivate *priv;

	priv = E_BOOK_SHELL_BACKEND_GET_PRIVATE (object);

	if (priv->source_list != NULL) {
		g_object_unref (priv->source_list);
		priv->source_list = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
book_shell_backend_constructed (GObject *object)
{
	EShell *shell;
	EShellBackend *shell_backend;

	shell_backend = E_SHELL_BACKEND (object);
	shell = e_shell_backend_get_shell (shell_backend);

	/* XXX Why is this here?  Address books aren't the only
	 *     things that use S/MIME.  Maybe put it in EShell? */
#ifdef ENABLE_SMIME
	smime_component_init ();
	certificate_manager_config_init (shell);
#endif

	book_shell_backend_init_importers ();
	book_shell_backend_ensure_sources (shell_backend);

	g_signal_connect (
		shell, "event::contact-quick-add-email",
		G_CALLBACK (book_shell_backend_quick_add_email_cb), NULL);

	g_signal_connect_swapped (
		shell, "event::contact-quick-add-vcard",
		G_CALLBACK (book_shell_backend_quick_add_vcard_cb), NULL);

	g_signal_connect_swapped (
		shell, "handle-uri",
		G_CALLBACK (book_shell_backend_handle_uri_cb),
		shell_backend);

	g_signal_connect_swapped (
		shell, "window-created",
		G_CALLBACK (book_shell_backend_window_created_cb),
		shell_backend);

	e_book_shell_backend_init_settings (shell);

	/* Initialize preferences after the main loop starts so
	 * that all EPlugins and EPluginHooks are loaded first. */
	g_idle_add ((GSourceFunc) book_shell_backend_init_preferences, shell);
}

static void
book_shell_backend_class_init (EBookShellBackendClass *class)
{
	GObjectClass *object_class;
	EShellBackendClass *shell_backend_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EBookShellBackendPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = book_shell_backend_get_property;
	object_class->dispose = book_shell_backend_dispose;
	object_class->constructed = book_shell_backend_constructed;

	shell_backend_class = E_SHELL_BACKEND_CLASS (class);
	shell_backend_class->shell_view_type = E_TYPE_BOOK_SHELL_VIEW;
	shell_backend_class->name = "addressbook";
	shell_backend_class->aliases = "contacts";
	shell_backend_class->schemes = "";
	shell_backend_class->sort_order = 300;
	shell_backend_class->start = NULL;
	shell_backend_class->migrate = e_book_shell_backend_migrate;

	g_object_class_install_property (
		object_class,
		PROP_SOURCE_LIST,
		g_param_spec_object (
			"source-list",
			_("Source List"),
			_("The registry of address books"),
			E_TYPE_SOURCE_LIST,
			G_PARAM_READABLE));
}

static void
book_shell_backend_init (EBookShellBackend *book_shell_backend)
{
	book_shell_backend->priv =
		E_BOOK_SHELL_BACKEND_GET_PRIVATE (book_shell_backend);
}

GType
e_book_shell_backend_get_type (void)
{
	return book_shell_backend_type;
}

void
e_book_shell_backend_register_type (GTypeModule *type_module)
{
	const GTypeInfo type_info = {
		sizeof (EBookShellBackendClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) book_shell_backend_class_init,
		(GClassFinalizeFunc) NULL,
		NULL,  /* class_data */
		sizeof (EBookShellBackend),
		0,     /* n_preallocs */
		(GInstanceInitFunc) book_shell_backend_init,
		NULL   /* value_table */
	};

	book_shell_backend_type = g_type_module_register_type (
		type_module, E_TYPE_SHELL_BACKEND,
		"EBookShellBackend", &type_info, 0);
}

ESourceList *
e_book_shell_backend_get_source_list (EBookShellBackend *book_shell_backend)
{
	g_return_val_if_fail (
		E_IS_BOOK_SHELL_BACKEND (book_shell_backend), NULL);

	return book_shell_backend->priv->source_list;
}
