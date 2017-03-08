/*
 * e-book-shell-backend.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "e-book-shell-backend.h"

#include <string.h>
#include <glib/gi18n.h>
#include <libebook/libebook.h>

#include "shell/e-shell.h"
#include "shell/e-shell-window.h"

#include "addressbook/gui/widgets/eab-gui-util.h"
#include "addressbook/gui/widgets/e-addressbook-view.h"
#include "addressbook/gui/contact-editor/e-contact-editor.h"
#include "addressbook/gui/contact-editor/e-contact-quick-add.h"
#include "addressbook/gui/contact-list-editor/e-contact-list-editor.h"
#include "addressbook/importers/evolution-addressbook-importers.h"

#include "autocompletion-config.h"

#include "e-book-shell-content.h"
#include "e-book-shell-migrate.h"
#include "e-book-shell-view.h"

#ifdef ENABLE_SMIME
#include "smime/gui/component.h"
#include "smime/gui/certificate-manager.h"
#endif

#define E_BOOK_SHELL_BACKEND_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_BOOK_SHELL_BACKEND, EBookShellBackendPrivate))

struct _EBookShellBackendPrivate {
	gint placeholder;
};

G_DEFINE_DYNAMIC_TYPE (
	EBookShellBackend,
	e_book_shell_backend,
	E_TYPE_SHELL_BACKEND)

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
book_shell_backend_new_contact_cb (GObject *source_object,
                                   GAsyncResult *result,
                                   gpointer user_data)
{
	EShellWindow *shell_window = user_data;
	EClient *client;
	EContact *contact;
	EABEditor *editor;
	GError *error = NULL;

	client = e_client_cache_get_client_finish (
		E_CLIENT_CACHE (source_object), result, &error);

	/* Sanity check. */
	g_return_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)));

	/* XXX Handle errors better. */
	if (error != NULL) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
		goto exit;
	}

	contact = e_contact_new ();

	editor = e_contact_editor_new (
		e_shell_window_get_shell (shell_window), E_BOOK_CLIENT (client), contact, TRUE, TRUE);
	gtk_window_set_transient_for (eab_editor_get_window (editor), GTK_WINDOW (shell_window));

	eab_editor_show (editor);

	g_object_unref (contact);
	g_object_unref (client);

exit:
	g_object_unref (shell_window);
}

static void
book_shell_backend_new_contact_list_cb (GObject *source_object,
                                        GAsyncResult *result,
                                        gpointer user_data)
{
	EShellWindow *shell_window = user_data;
	EClient *client;
	EContact *contact;
	EABEditor *editor;
	GError *error = NULL;

	client = e_client_cache_get_client_finish (
		E_CLIENT_CACHE (source_object), result, &error);

	/* Sanity check. */
	g_return_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)));

	/* XXX Handle errors better. */
	if (error != NULL) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
		goto exit;
	}

	contact = e_contact_new ();

	editor = e_contact_list_editor_new (
		e_shell_window_get_shell (shell_window), E_BOOK_CLIENT (client), contact, TRUE, TRUE);
	gtk_window_set_transient_for (eab_editor_get_window (editor), GTK_WINDOW (shell_window));

	eab_editor_show (editor);

	g_object_unref (contact);
	g_object_unref (client);

exit:
	g_object_unref (shell_window);
}

static void
action_contact_new_cb (GtkAction *action,
                       EShellWindow *shell_window)
{
	EShell *shell;
	ESource *source = NULL;
	ESourceRegistry *registry;
	EClientCache *client_cache;
	const gchar *action_name;

	/* This callback is used for both contacts and contact lists. */

	shell = e_shell_window_get_shell (shell_window);
	client_cache = e_shell_get_client_cache (shell);

	if (g_strcmp0 (e_shell_window_get_active_view (shell_window), "addressbook") == 0) {
		EShellView *shell_view = e_shell_window_get_shell_view (shell_window, "addressbook");

		if (shell_view && E_IS_BOOK_SHELL_VIEW (shell_view)) {
			EBookShellContent *book_shell_content;
			EAddressbookView *view;
			EAddressbookModel *model;
			EBookClient *book_client;

			book_shell_content = NULL;
			g_object_get (G_OBJECT (shell_view), "shell-content", &book_shell_content, NULL);
			g_return_if_fail (book_shell_content != NULL);

			view = e_book_shell_content_get_current_view (book_shell_content);
			g_return_if_fail (view != NULL);

			model = e_addressbook_view_get_model (view);
			book_client = e_addressbook_model_get_client (model);
			g_return_if_fail (book_client != NULL);

			source = g_object_ref (e_client_get_source (E_CLIENT (book_client)));

			g_object_unref (book_shell_content);
		}
	}

	if (!source) {
		registry = e_shell_get_registry (shell);
		source = e_source_registry_ref_default_address_book (registry);
	}

	/* Use a callback function appropriate for the action. */
	action_name = gtk_action_get_name (action);
	if (strcmp (action_name, "contact-new") == 0)
		e_client_cache_get_client (
			client_cache, source,
			E_SOURCE_EXTENSION_ADDRESS_BOOK, 30,
			NULL,
			book_shell_backend_new_contact_cb,
			g_object_ref (shell_window));
	if (strcmp (action_name, "contact-new-list") == 0)
		e_client_cache_get_client (
			client_cache, source,
			E_SOURCE_EXTENSION_ADDRESS_BOOK, 30,
			NULL,
			book_shell_backend_new_contact_list_cb,
			g_object_ref (shell_window));

	g_object_unref (source);
}

static void
action_address_book_new_cb (GtkAction *action,
                            EShellWindow *shell_window)
{
	EShell *shell;
	ESourceRegistry *registry;
	GtkWidget *config;
	GtkWidget *dialog;
	const gchar *icon_name;

	shell = e_shell_window_get_shell (shell_window);

	registry = e_shell_get_registry (shell);
	config = e_book_source_config_new (registry, NULL);

	dialog = e_source_config_dialog_new (E_SOURCE_CONFIG (config));

	gtk_window_set_transient_for (
		GTK_WINDOW (dialog), GTK_WINDOW (shell_window));

	icon_name = gtk_action_get_icon_name (action);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), icon_name);

	gtk_window_set_title (GTK_WINDOW (dialog), _("New Address Book"));

	gtk_widget_show (dialog);
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
	  NC_("New", "Contact _List"),
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
		"index#contacts",
		autocompletion_config_new,
		200);

#ifdef ENABLE_SMIME
	preferences_window = e_shell_get_preferences_window (shell);
	e_preferences_window_add_page (
		E_PREFERENCES_WINDOW (preferences_window),
		"certificates",
		"preferences-certificates",
		_("Certificates"),
		"mail-encryption-s-mime-manage",
		e_cert_manager_config_new,
		700);
#endif

	return FALSE;
}

static void
book_shell_backend_quick_add_email_cb (EShell *shell,
                                       const gchar *email)
{
	EClientCache *client_cache;

	/* XXX This is an ugly hack but it's the only way I could think
	 *     of to integrate this feature with other shell modules. */

	client_cache = e_shell_get_client_cache (shell);
	e_contact_quick_add_email (client_cache, email, NULL, NULL);
}

static void
book_shell_backend_quick_add_vcard_cb (EShell *shell,
                                       const gchar *vcard)
{
	EClientCache *client_cache;

	/* XXX This is an ugly hack but it's the only way I could think
	 *     of to integrate this feature with other shell modules. */

	client_cache = e_shell_get_client_cache (shell);
	e_contact_quick_add_vcard (client_cache, vcard, NULL, NULL);
}

static gboolean
book_shell_backend_handle_uri_cb (EShellBackend *shell_backend,
                                  const gchar *uri)
{
	SoupURI *soup_uri;
	const gchar *cp;
	gchar *source_uid = NULL;
	gchar *contact_uid = NULL;

	if (!g_str_has_prefix (uri, "contacts:"))
		return FALSE;

	soup_uri = soup_uri_new (uri);

	if (soup_uri == NULL)
		return FALSE;

	cp = soup_uri_get_query (soup_uri);

	if (cp == NULL) {
		soup_uri_free (soup_uri);
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

	soup_uri_free (soup_uri);

	return TRUE;
}

static void
book_shell_backend_window_added_cb (EShellBackend *shell_backend,
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
#endif

	book_shell_backend_init_importers ();

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
		shell, "window-added",
		G_CALLBACK (book_shell_backend_window_added_cb),
		shell_backend);

	/* Initialize preferences after the main loop starts so
	 * that all EPlugins and EPluginHooks are loaded first. */
	g_idle_add ((GSourceFunc) book_shell_backend_init_preferences, shell);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_book_shell_backend_parent_class)->constructed (object);
}

static void
e_book_shell_backend_class_init (EBookShellBackendClass *class)
{
	GObjectClass *object_class;
	EShellBackendClass *shell_backend_class;

	g_type_class_add_private (class, sizeof (EBookShellBackendPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = book_shell_backend_constructed;

	shell_backend_class = E_SHELL_BACKEND_CLASS (class);
	shell_backend_class->shell_view_type = E_TYPE_BOOK_SHELL_VIEW;
	shell_backend_class->name = "addressbook";
	shell_backend_class->aliases = "contacts";
	shell_backend_class->schemes = "";
	shell_backend_class->sort_order = 300;
	shell_backend_class->preferences_page = "contacts";
	shell_backend_class->start = NULL;
	shell_backend_class->migrate = e_book_shell_backend_migrate;
}

static void
e_book_shell_backend_class_finalize (EBookShellBackendClass *class)
{
}

static void
e_book_shell_backend_init (EBookShellBackend *book_shell_backend)
{
	book_shell_backend->priv =
		E_BOOK_SHELL_BACKEND_GET_PRIVATE (book_shell_backend);
}

void
e_book_shell_backend_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_book_shell_backend_register_type (type_module);
}
