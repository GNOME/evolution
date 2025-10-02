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
#include "addressbook/util/eab-book-util.h"

#include "autocompletion-config.h"

#include "e-book-shell-content.h"
#include "e-book-shell-migrate.h"
#include "e-book-shell-view.h"

#ifdef ENABLE_SMIME
#include "smime/gui/component.h"
#include "smime/gui/certificate-manager.h"
#endif

struct _EBookShellBackendPrivate {
	gint placeholder;
};

G_DEFINE_DYNAMIC_TYPE_EXTENDED (EBookShellBackend, e_book_shell_backend, E_TYPE_SHELL_BACKEND, 0,
	G_ADD_PRIVATE_DYNAMIC (EBookShellBackend))

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
	EBookClient *book_client;
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

	book_client = E_BOOK_CLIENT (client);
	contact = eab_new_contact_for_book (book_client);

	editor = e_contact_editor_new (e_shell_window_get_shell (shell_window), book_client, contact, TRUE, TRUE);
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

	e_book_shell_view_open_list_editor_with_prefill (
		e_shell_window_get_shell_view (shell_window, e_shell_window_get_active_view (shell_window)),
		E_BOOK_CLIENT (client));

	g_object_unref (client);

exit:
	g_object_unref (shell_window);
}

static void
action_contact_new_cb (EUIAction *action,
		       GVariant *parameter,
		       gpointer user_data)
{
	EShellWindow *shell_window = user_data;
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
			EBookClient *book_client;

			book_shell_content = NULL;
			g_object_get (G_OBJECT (shell_view), "shell-content", &book_shell_content, NULL);
			g_return_if_fail (book_shell_content != NULL);

			view = e_book_shell_content_get_current_view (book_shell_content);
			g_return_if_fail (view != NULL);

			book_client = e_addressbook_view_get_client (view);
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
	action_name = g_action_get_name (G_ACTION (action));
	if (g_strcmp0 (action_name, "contact-new") == 0 ||
	    g_strcmp0 (action_name, "new-menu-contact-new") == 0)
		e_client_cache_get_client (
			client_cache, source,
			E_SOURCE_EXTENSION_ADDRESS_BOOK, (guint32) -1,
			NULL,
			book_shell_backend_new_contact_cb,
			g_object_ref (shell_window));
	if (g_strcmp0 (action_name, "contact-new-list") == 0 ||
	    g_strcmp0 (action_name, "new-menu-contact-new-list") == 0)
		e_client_cache_get_client (
			client_cache, source,
			E_SOURCE_EXTENSION_ADDRESS_BOOK, (guint32) -1,
			NULL,
			book_shell_backend_new_contact_list_cb,
			g_object_ref (shell_window));

	g_object_unref (source);
}

static void
action_address_book_new_cb (EUIAction *action,
			    GVariant *parameter,
			    gpointer user_data)
{
	EShellWindow *shell_window = user_data;
	EShell *shell;
	ESourceRegistry *registry;
	GtkWidget *config;
	GtkWidget *dialog;
	const gchar *icon_name;

	shell = e_shell_window_get_shell (shell_window);

	registry = e_shell_get_registry (shell);
	config = e_book_source_config_new (registry, NULL);

	if (g_strcmp0 (e_shell_window_get_active_view (shell_window), "addressbook") == 0) {
		EShellView *shell_view = e_shell_window_peek_shell_view (shell_window, "addressbook");

		if (shell_view)
			e_book_shell_view_preselect_source_config (shell_view, config);
	}

	dialog = e_source_config_dialog_new (E_SOURCE_CONFIG (config));

	gtk_window_set_transient_for (
		GTK_WINDOW (dialog), GTK_WINDOW (shell_window));

	icon_name = e_ui_action_get_icon_name (action);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), icon_name);

	gtk_window_set_title (GTK_WINDOW (dialog), _("New Address Book"));

	gtk_widget_show (dialog);
}

static gboolean
book_shell_backend_init_preferences (EShell *shell)
{
	GtkWidget *preferences_window;

	/* This is a main loop idle callback. */

	preferences_window = e_shell_get_preferences_window (shell);

	e_preferences_window_add_page (
		E_PREFERENCES_WINDOW (preferences_window),
		"contacts",
		"preferences-contact",
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
	GUri *guri;
	const gchar *cp;
	gchar *source_uid = NULL;
	gchar *contact_uid = NULL;

	#ifdef HAVE_LDAP
	if (g_str_has_prefix (uri, "ldap://") || g_str_has_prefix (uri, "ldaps://")) {
		/* RFC 4516 - "ldap://" [host [COLON port]]
                       [SLASH dn [QUESTION [attributes]
                       [QUESTION [scope] [QUESTION [filter]
                       [QUESTION extensions]]]]] */
		ESourceRegistry *registry;
		ESource *scratch_source;
		ESourceAuthentication *auth_extension;
		ESourceBackend *backend_extension;
		ESourceLDAP *ldap_extension;
		EShell *shell;
		EShellWindow *shell_window = NULL;
		ESourceLDAPScope scope = E_SOURCE_LDAP_SCOPE_SUBTREE;
		GtkWidget *config;
		GtkWidget *dialog;
		GList *link;
		const gchar *ptr, *end;
		gchar *dn = NULL, *filter = NULL;
		guint pos = 0;

		ptr = strchr (strstr (uri, "://") + 3, '/');
		if (!ptr || !ptr[1])
			return FALSE;

		ptr++;

		guri = g_uri_parse (uri, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);

		if (!guri)
			return FALSE;

		if (!g_uri_get_host (guri)) {
			g_uri_unref (guri);
			return FALSE;
		}

		while (ptr && *ptr) {
			end = strchr (ptr, '?');

			switch (pos) {
			case 0: /* dn */
				if (end) {
					gchar *tmp = g_strndup (ptr, end - ptr);
					dn = g_uri_unescape_string (tmp, NULL);
					g_free (tmp);
				} else {
					dn = g_uri_unescape_string (ptr, NULL);
				}
				break;
			case 1: /* attributes - skip */
				break;
			case 2: /* scope */
				if (end) {
					if (g_ascii_strncasecmp (ptr, "base", end - ptr) == 0) {
						/* nothing for it in ESourceLDAPScope */
					} else if (g_ascii_strncasecmp (ptr, "one", end - ptr) == 0) {
						scope = E_SOURCE_LDAP_SCOPE_ONELEVEL;
					} else if (g_ascii_strncasecmp (ptr, "sub", end - ptr) == 0) {
						scope = E_SOURCE_LDAP_SCOPE_SUBTREE;
					}
				} else {
					if (g_ascii_strcasecmp (ptr, "base") == 0) {
						/* nothing for it in ESourceLDAPScope */
					} else if (g_ascii_strcasecmp (ptr, "one") == 0) {
						scope = E_SOURCE_LDAP_SCOPE_ONELEVEL;
					} else if (g_ascii_strcasecmp (ptr, "sub") == 0) {
						scope = E_SOURCE_LDAP_SCOPE_SUBTREE;
					}
				}
				break;
			case 3: /* filter */
				if (end) {
					gchar *tmp = g_strndup (ptr, end - ptr);
					filter = g_uri_unescape_string (tmp, NULL);
					g_free (tmp);
				} else {
					filter = g_uri_unescape_string (ptr, NULL);
				}
				break;
			case 4: /* extensions - skip */
				break;
			}

			pos++;
			ptr = end;
			if (ptr)
				ptr++;
		}

		shell = e_shell_backend_get_shell (shell_backend);

		for (link = gtk_application_get_windows (GTK_APPLICATION (shell)); link; link = g_list_next (link)) {
			GtkWindow *window = link->data;

			if (E_IS_SHELL_WINDOW (window)) {
				shell_window = E_SHELL_WINDOW (window);
				break;
			}
		}

		scratch_source = e_source_new (NULL, NULL, NULL);
		e_source_set_parent (scratch_source, "ldap-stub");
		e_source_set_display_name (scratch_source, g_uri_get_host (guri));

		backend_extension = e_source_get_extension (scratch_source, E_SOURCE_EXTENSION_ADDRESS_BOOK);
		e_source_backend_set_backend_name (backend_extension, "ldap");

		ldap_extension = e_source_get_extension (scratch_source, E_SOURCE_EXTENSION_LDAP_BACKEND);
		e_source_ldap_set_scope (ldap_extension, scope);
		if (g_str_has_prefix (uri, "ldaps://"))
			e_source_ldap_set_security (ldap_extension, E_SOURCE_LDAP_SECURITY_LDAPS);
		else
			e_source_ldap_set_security (ldap_extension, E_SOURCE_LDAP_SECURITY_STARTTLS);
		if (filter)
			e_source_ldap_set_filter (ldap_extension, filter);
		if (dn)
			e_source_ldap_set_root_dn (ldap_extension, dn);
		if (g_uri_get_user (guri)) {
			if (strchr (g_uri_get_user (guri), '='))
				e_source_ldap_set_authentication (ldap_extension, E_SOURCE_LDAP_AUTHENTICATION_BINDDN);
			else if (strchr (g_uri_get_user (guri), '@'))
				e_source_ldap_set_authentication (ldap_extension, E_SOURCE_LDAP_AUTHENTICATION_EMAIL);
		}

		auth_extension = e_source_get_extension (scratch_source, E_SOURCE_EXTENSION_AUTHENTICATION);
		e_source_authentication_set_host (auth_extension, g_uri_get_host (guri));
		e_source_authentication_set_port (auth_extension, g_uri_get_port (guri) > 0 ? g_uri_get_port (guri) : 389);
		e_source_authentication_set_user (auth_extension, g_uri_get_user (guri));

		registry = e_shell_get_registry (shell);
		config = e_book_source_config_new (registry, scratch_source);

		g_clear_object (&scratch_source);

		dialog = e_source_config_dialog_new (E_SOURCE_CONFIG (config));

		gtk_application_add_window (GTK_APPLICATION (shell), GTK_WINDOW (dialog));

		if (shell_window)
			gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (shell_window));
		gtk_window_set_icon_name (GTK_WINDOW (dialog), "address-book-new");
		gtk_window_set_title (GTK_WINDOW (dialog), _("New Address Book"));

		gtk_widget_show (dialog);

		g_uri_unref (guri);
		g_free (filter);
		g_free (dn);

		return TRUE;
	}
	#endif /* HAVE_LDAP */

	if (!g_str_has_prefix (uri, "contacts:"))
		return FALSE;

	guri = g_uri_parse (uri, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);

	if (!guri)
		return FALSE;

	cp = g_uri_get_query (guri);

	if (cp == NULL) {
		g_uri_unref (guri);
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

	g_uri_unref (guri);

	return TRUE;
}

static void
book_shell_backend_window_added_cb (EShellBackend *shell_backend,
                                    GtkWindow *window)
{
	static const EUIActionEntry item_entries[] = {
		{ "new-menu-contact-new",
		  "contact-new",
		  NC_("New", "_Contact"),
		  "<Shift><Control>c",
		  N_("Create a new contact"),
		  action_contact_new_cb, NULL, NULL, NULL },

		{ "new-menu-contact-new-list",
		  "stock_contact-list",
		  NC_("New", "Contact _List"),
		  "<Shift><Control>l",
		  N_("Create a new contact list"),
		  action_contact_new_cb, NULL, NULL, NULL }
	};

	static const EUIActionEntry source_entries[] = {
		{ "new-menu-address-book-new",
		  "address-book-new",
		  NC_("New", "Address _Book"),
		  NULL,
		  N_("Create a new address book"),
		  action_address_book_new_cb, NULL, NULL, NULL }
	};

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
	book_shell_backend->priv = e_book_shell_backend_get_instance_private (book_shell_backend);
}

void
e_book_shell_backend_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_book_shell_backend_register_type (type_module);
}
