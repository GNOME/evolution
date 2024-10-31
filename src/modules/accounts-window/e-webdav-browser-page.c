/*
 * Copyright (C) 2017 Red Hat, Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include "e-util/e-util.h"
#include "shell/e-shell.h"

#include "e-webdav-browser-page.h"

/* Standard GObject macros */
#define E_TYPE_WEBDAV_BROWSER_PAGE \
	(e_webdav_browser_page_get_type ())
#define E_WEBDAV_BROWSER_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_WEBDAV_BROWSER_PAGE, EWebDAVBrowserPage))
#define E_WEBDAV_BROWSER_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_WEBDAV_BROWSER_PAGE, EWebDAVBrowserPageClass))
#define E_IS_WEBDAV_BROWSER_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_WEBDAV_BROWSER_PAGE))
#define E_IS_WEBDAV_BROWSER_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_WEBDAV_BROWSER_PAGE))
#define E_WEBDAV_BROWSER_PAGE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_WEBDAV_BROWSER_PAGE, EWebDAVBrowserPageClass))

typedef struct _EWebDAVBrowserPage EWebDAVBrowserPage;
typedef struct _EWebDAVBrowserPageClass EWebDAVBrowserPageClass;

struct _EWebDAVBrowserPage {
	EExtension parent;

	GtkWidget *browse_button;
	EWebDAVBrowser *webdav_browser;
	gint page_index;
};

struct _EWebDAVBrowserPageClass {
	EExtensionClass parent_class;
};

GType e_webdav_browser_page_get_type (void) G_GNUC_CONST;

G_DEFINE_DYNAMIC_TYPE (EWebDAVBrowserPage, e_webdav_browser_page, E_TYPE_EXTENSION)

static void
webdav_browser_page_selection_changed_cb (EAccountsWindow *accounts_window,
					  ESource *source,
					  gpointer user_data)
{
	EWebDAVBrowserPage *page = user_data;
	gboolean can_use = FALSE;

	g_return_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window));
	g_return_if_fail (E_IS_WEBDAV_BROWSER_PAGE (page));

	if (source && e_source_has_extension (source, E_SOURCE_EXTENSION_WEBDAV_BACKEND)) {
		gchar *path;

		path = e_source_webdav_dup_resource_path (e_source_get_extension (source, E_SOURCE_EXTENSION_WEBDAV_BACKEND));

		can_use = path && *path;

		g_free (path);
	}

	if (source && can_use) {
		ESourceBackend *backend_extension = NULL;

		if (e_source_has_extension (source, E_SOURCE_EXTENSION_ADDRESS_BOOK))
			backend_extension = e_source_get_extension (source, E_SOURCE_EXTENSION_ADDRESS_BOOK);
		else if (e_source_has_extension (source, E_SOURCE_EXTENSION_CALENDAR))
			backend_extension = e_source_get_extension (source, E_SOURCE_EXTENSION_CALENDAR);
		else if (e_source_has_extension (source, E_SOURCE_EXTENSION_TASK_LIST))
			backend_extension = e_source_get_extension (source, E_SOURCE_EXTENSION_TASK_LIST);
		else if (e_source_has_extension (source, E_SOURCE_EXTENSION_MEMO_LIST))
			backend_extension = e_source_get_extension (source, E_SOURCE_EXTENSION_MEMO_LIST);

		can_use = backend_extension && (
			g_strcmp0 (e_source_backend_get_backend_name (backend_extension), "caldav") == 0 ||
			g_strcmp0 (e_source_backend_get_backend_name (backend_extension), "carddav") == 0 ||
			g_strcmp0 (e_source_backend_get_backend_name (backend_extension), "webdav-notes") == 0);
	}

	gtk_widget_set_sensitive (page->browse_button, can_use);
}

static void
webdav_browser_page_browse_button_clicked_cb (GtkButton *button,
					      gpointer user_data)
{
	EWebDAVBrowserPage *page = user_data;
	EAccountsWindow *accounts_window;
	ESource *source;

	g_return_if_fail (E_IS_WEBDAV_BROWSER_PAGE (page));

	accounts_window = E_ACCOUNTS_WINDOW (e_extension_get_extensible (E_EXTENSION (page)));
	g_return_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window));

	source = e_accounts_window_ref_selected_source (accounts_window);
	g_return_if_fail (E_IS_SOURCE (source));

	e_accounts_window_activate_page (accounts_window, page->page_index);
	e_webdav_browser_set_source (page->webdav_browser, source);

	g_object_unref (source);
}

static void
webdav_browser_back_button_clicked_cb (GtkButton *button,
				       gpointer user_data)
{
	EWebDAVBrowserPage *page = user_data;
	EAccountsWindow *accounts_window;

	g_return_if_fail (E_IS_WEBDAV_BROWSER_PAGE (page));

	accounts_window = E_ACCOUNTS_WINDOW (e_extension_get_extensible (E_EXTENSION (page)));
	g_return_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window));

	e_webdav_browser_abort (page->webdav_browser);
	e_webdav_browser_set_source (page->webdav_browser, NULL);

	e_accounts_window_activate_page (accounts_window, -1);
}

static void
webdav_browser_page_constructed (GObject *object)
{
	EAccountsWindow *accounts_window;
	ECredentialsPrompter *credentials_prompter;
	EWebDAVBrowserPage *page;
	EShell *shell;
	GtkButtonBox *button_box;
	GtkWidget *widget;
	GtkWidget *vbox;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_webdav_browser_page_parent_class)->constructed (object);

	page = E_WEBDAV_BROWSER_PAGE (object);
	accounts_window = E_ACCOUNTS_WINDOW (e_extension_get_extensible (E_EXTENSION (page)));

	g_signal_connect (accounts_window, "selection-changed",
		G_CALLBACK (webdav_browser_page_selection_changed_cb), object);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show (vbox);

	shell = e_shell_get_default ();
	/* Can be NULL in test-accounts-window */
	if (shell) {
		credentials_prompter = e_shell_get_credentials_prompter (shell);
		g_object_ref (credentials_prompter);
	} else {
		credentials_prompter = e_credentials_prompter_new (e_accounts_window_get_registry (accounts_window));
	}

	widget = e_webdav_browser_new (credentials_prompter);
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		NULL);
	gtk_widget_show (widget);
	gtk_box_pack_start (GTK_BOX (vbox), widget, TRUE, TRUE, 0);
	page->webdav_browser = E_WEBDAV_BROWSER (widget);

	g_object_unref (credentials_prompter);

	widget = e_dialog_button_new_with_icon ("go-previous", _("_Back"));
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_END,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		NULL);
	gtk_widget_show (widget);
	gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);

	g_signal_connect (widget, "clicked",
		G_CALLBACK (webdav_browser_back_button_clicked_cb), page);

	page->page_index = e_accounts_window_add_page (accounts_window, vbox);

	button_box = e_accounts_window_get_button_box (accounts_window);

	widget = gtk_label_new ("");
	gtk_widget_show (widget);
	gtk_box_pack_start (GTK_BOX (button_box), widget, FALSE, FALSE, 0);

	widget = gtk_button_new_with_mnemonic (_("_Browse"));
	gtk_widget_set_sensitive (widget, FALSE);
	gtk_widget_set_tooltip_text (widget, _("Browse a WebDAV (CalDAV or CardDAV) server and create, edit or delete address books, calendars, memo lists or task lists there"));
	gtk_widget_show (widget);
	gtk_box_pack_start (GTK_BOX (button_box), widget, FALSE, FALSE, 0);
	page->browse_button = widget;

	g_signal_connect (widget, "clicked",
		G_CALLBACK (webdav_browser_page_browse_button_clicked_cb), page);
}

static void
e_webdav_browser_page_class_init (EWebDAVBrowserPageClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = webdav_browser_page_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_ACCOUNTS_WINDOW;
}

static void
e_webdav_browser_page_class_finalize (EWebDAVBrowserPageClass *class)
{
}

static void
e_webdav_browser_page_init (EWebDAVBrowserPage *extension)
{
}

void
e_webdav_browser_page_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_webdav_browser_page_register_type (type_module);
}
