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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-mail-display-popup-text-highlight.h"
#include "mail/e-mail-display-popup-extension.h"
#include "mail/e-mail-display.h"
#include <shell/e-shell.h>
#include <shell/e-shell-window.h>
#include "mail/e-mail-browser.h"

#include "web-extension/module-text-highlight-web-extension.h"

#include <libebackend/libebackend.h>

#include <glib/gi18n-lib.h>

#include "languages.h"

#define d(x)

typedef struct _EMailDisplayPopupTextHighlight {
	EExtension parent;

	GtkActionGroup *action_group;

	GDBusProxy *web_extension;
	gint web_extension_watch_name_id;
} EMailDisplayPopupTextHighlight;

typedef struct _EMailDisplayPopupTextHighlightClass {
	EExtensionClass parent_class;
} EMailDisplayPopupTextHighlightClass;

#define E_MAIL_DISPLAY_POPUP_TEXT_HIGHLIGHT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), e_mail_display_popup_text_highlight_get_type (), EMailDisplayPopupTextHighlight))

GType e_mail_display_popup_text_highlight_get_type (void);
static void e_mail_display_popup_extension_interface_init (EMailDisplayPopupExtensionInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (
	EMailDisplayPopupTextHighlight,
	e_mail_display_popup_text_highlight,
	E_TYPE_EXTENSION,
	0,
	G_IMPLEMENT_INTERFACE_DYNAMIC (
		E_TYPE_MAIL_DISPLAY_POPUP_EXTENSION,
		e_mail_display_popup_extension_interface_init));

static const gchar *ui =
"<ui>"
"  <popup name='context'>"
"    <placeholder name='custom-actions-2'>"
"      <separator />"
"      <menu action='format-as-menu'>"
"	 <placeholder name='format-as-actions' />"
"	 <menu action='format-as-other-menu'>"
"	 </menu>"
"      </menu>"
"    </placeholder>"
"  </popup>"
"</ui>";

static const gchar *ui_reader =
"<ui>"
"  <popup name='mail-preview-popup'>"
"    <placeholder name='mail-preview-popup-actions'>"
"      <separator />"
"      <menu action='format-as-menu'>"
"	 <placeholder name='format-as-actions' />"
"	 <menu action='format-as-other-menu'>"
"	 </menu>"
"      </menu>"
"    </placeholder>"
"  </popup>"
"</ui>";

static GtkActionEntry entries[] = {

	{ "format-as-menu",
	  NULL,
	  N_("_Format as..."),
	  NULL,
	  NULL,
	  NULL
	},

	{ "format-as-other-menu",
	  NULL,
	  N_("_Other languages"),
	  NULL,
	  NULL,
	  NULL
	}
};

static void
web_extension_proxy_created_cb (GDBusProxy *proxy,
                                GAsyncResult *result,
                                EMailDisplayPopupTextHighlight *th_extension)
{
	GError *error = NULL;

	th_extension->web_extension = g_dbus_proxy_new_finish (result, &error);
	if (!th_extension->web_extension) {
		g_warning ("Error creating web extension proxy: %s\n", error->message);
		g_error_free (error);
	}
}

static void
web_extension_appeared_cb (GDBusConnection *connection,
                           const gchar *name,
                           const gchar *name_owner,
                           EMailDisplayPopupTextHighlight *th_extension)
{
	g_dbus_proxy_new (
		connection,
		G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START |
		G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
		G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
		NULL,
		name,
		MODULE_TEXT_HIGHLIGHT_WEB_EXTENSION_OBJECT_PATH,
		MODULE_TEXT_HIGHLIGHT_WEB_EXTENSION_INTERFACE,
		NULL,
		(GAsyncReadyCallback)web_extension_proxy_created_cb,
		th_extension);
}

static void
web_extension_vanished_cb (GDBusConnection *connection,
                           const gchar *name,
                           EMailDisplayPopupTextHighlight *th_extension)
{
	g_clear_object (&th_extension->web_extension);
}

static void
mail_display_popup_prefer_plain_watch_web_extension (EMailDisplayPopupTextHighlight *th_extension)
{
	th_extension->web_extension_watch_name_id =
		g_bus_watch_name (
			G_BUS_TYPE_SESSION,
			MODULE_TEXT_HIGHLIGHT_WEB_EXTENSION_SERVICE_NAME,
			G_BUS_NAME_WATCHER_FLAGS_NONE,
			(GBusNameAppearedCallback) web_extension_appeared_cb,
			(GBusNameVanishedCallback) web_extension_vanished_cb,
			th_extension, NULL);
}

static void
reformat (GtkAction *old,
          GtkAction *action,
          gpointer user_data)
{
	EMailDisplayPopupTextHighlight *th_extension;
	GVariant *result;
	SoupURI *soup_uri;
	GHashTable *query;
	const gchar *document_uri;
	gchar *uri;

	th_extension = E_MAIL_DISPLAY_POPUP_TEXT_HIGHLIGHT (user_data);

	if (!th_extension->web_extension)
		return;

	/* Get URI from saved document */
	result = g_dbus_proxy_call_sync (
			th_extension->web_extension,
			"GetDocumentURI",
			NULL,
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			NULL,
			NULL);
	if (result) {
		document_uri = g_variant_get_string (result, NULL);
		g_variant_unref (result);
	}

	soup_uri = soup_uri_new (document_uri);

	if (!soup_uri)
		return;

	if (!soup_uri->query) {
		soup_uri_free (soup_uri);
		return;
	}

	query = soup_form_decode (soup_uri->query);
	g_hash_table_replace (
		query, g_strdup ("__formatas"), (gpointer) gtk_action_get_name (action));
	g_hash_table_replace (
		query, g_strdup ("mime_type"), (gpointer) "text/plain");

	soup_uri_set_query_from_form (soup_uri, query);
	g_hash_table_destroy (query);

	uri = soup_uri_to_string (soup_uri, FALSE);
	soup_uri_free (soup_uri);

	/* Get frame's window and from the window the actual <iframe> element */
	result = g_dbus_proxy_call_sync (
			th_extension->web_extension,
			"ChangeIFrameSource",
			g_variant_new ("(s)", uri),
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			NULL,
			NULL);
	if (result)
		g_variant_unref (result);

	g_free (uri);
}

static GtkActionGroup *
create_group (EMailDisplayPopupExtension *extension)
{
	EExtensible *extensible;
	EWebView *web_view;
	GtkUIManager *ui_manager, *shell_ui_manager;
	GtkActionGroup *group;
	EShell *shell;
	GtkWindow *shell_window;
	gint i;
	gsize len;
	guint merge_id, shell_merge_id;
	Language *languages;
	GSList *radio_group;
	gint action_index;

	extensible = e_extension_get_extensible (E_EXTENSION (extension));
	web_view = E_WEB_VIEW (extensible);

	ui_manager = e_web_view_get_ui_manager (web_view);
	shell = e_shell_get_default ();
	shell_window = e_shell_get_active_window (shell);
	if (E_IS_SHELL_WINDOW (shell_window)) {
		shell_ui_manager = e_shell_window_get_ui_manager (E_SHELL_WINDOW (shell_window));
	} else if (E_IS_MAIL_BROWSER (shell_window)) {
		shell_ui_manager = e_mail_browser_get_ui_manager (E_MAIL_BROWSER (shell_window));
	} else {
		return NULL;
	}

	group = gtk_action_group_new ("format-as");
	gtk_action_group_add_actions (group, entries, G_N_ELEMENTS (entries), NULL);

	gtk_ui_manager_insert_action_group (ui_manager, group, 0);
	gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, NULL);

	gtk_ui_manager_insert_action_group (shell_ui_manager, group, 0);
	gtk_ui_manager_add_ui_from_string (shell_ui_manager, ui_reader, -1, NULL);

	merge_id = gtk_ui_manager_new_merge_id (ui_manager);
	shell_merge_id = gtk_ui_manager_new_merge_id (shell_ui_manager);

	languages = get_default_langauges (&len);
	radio_group = NULL;
	action_index = 0;
	for (i = 0; i < len; i++) {

		GtkRadioAction *action;
		action = gtk_radio_action_new (
				languages[i].action_name,
				languages[i].action_label,
				NULL, NULL, action_index);
		action_index++;
		gtk_action_group_add_action (group, GTK_ACTION (action));
		g_signal_connect (
			action, "changed",
			G_CALLBACK (reformat), extension);
		gtk_radio_action_set_group (action, radio_group);
		radio_group = gtk_radio_action_get_group (action);

		g_object_unref (action);

		gtk_ui_manager_add_ui (
			ui_manager, merge_id,
			"/context/custom-actions-2/format-as-menu/format-as-actions",
			languages[i].action_name, languages[i].action_name,
			GTK_UI_MANAGER_AUTO, FALSE);

		gtk_ui_manager_add_ui (
			shell_ui_manager, shell_merge_id,
			"/mail-preview-popup/mail-preview-popup-actions/format-as-menu/format-as-actions",
			languages[i].action_name, languages[i].action_name,
			GTK_UI_MANAGER_AUTO, FALSE);
	}

	languages = get_additinal_languages (&len);
	for (i = 0; i < len; i++) {
		GtkRadioAction *action;

		action = gtk_radio_action_new (
				languages[i].action_name,
				languages[i].action_label,
				NULL, NULL, action_index);
		action_index++;
		gtk_action_group_add_action (group, GTK_ACTION (action));
		g_signal_connect (
			action, "changed",
			G_CALLBACK (reformat), extension);

		gtk_radio_action_set_group (action, radio_group);
		radio_group = gtk_radio_action_get_group (action);

		g_object_unref (action);

		gtk_ui_manager_add_ui (
			ui_manager, merge_id,
			"/context/custom-actions-2/format-as-menu/format-as-other-menu",
			languages[i].action_name, languages[i].action_name,
			GTK_UI_MANAGER_AUTO, FALSE);

		gtk_ui_manager_add_ui (
			shell_ui_manager, shell_merge_id,
			"/mail-preview-popup/mail-preview-popup-actions/format-as-menu/format-as-other-menu",
			languages[i].action_name, languages[i].action_name,
			GTK_UI_MANAGER_AUTO, FALSE);
	}

	return group;
}

static void
update_actions (EMailDisplayPopupExtension *extension,
                WebKitHitTestResult *context)
{
	EMailDisplay *display;
	EMailDisplayPopupTextHighlight *th_extension;
	const gchar *document_uri;
	GVariant *result;
	gint32 x, y;
	GdkDeviceManager *device_manager;
	GdkDevice *pointer;

	display = E_MAIL_DISPLAY (e_extension_get_extensible (
			E_EXTENSION (extension)));

	th_extension = E_MAIL_DISPLAY_POPUP_TEXT_HIGHLIGHT (extension);

	if (th_extension->action_group == NULL) {
		th_extension->action_group = create_group (extension);
	}

	/* In WK2 you can't get the node on what WebKitHitTest was performed,
	 * we have to use other way */
	device_manager = gdk_display_get_device_manager (
		gtk_widget_get_display (GTK_WIDGET(display)));
	pointer = gdk_device_manager_get_client_pointer (device_manager);
	gdk_window_get_device_position (
		gtk_widget_get_window (GTK_WIDGET (display)), pointer, &x, &y, NULL);

	if (!th_extension->web_extension)
       		return;

	result = g_dbus_proxy_call_sync (
			th_extension->web_extension,
			"SaveDocumentFromPoint",
			g_variant_new (
				"(tii)",
				webkit_web_view_get_page_id (
					WEBKIT_WEB_VIEW (display)),
				x, y),
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			NULL,
			NULL);
	if (result)
		g_variant_unref (result);

	/* Get URI from saved document */
	result = g_dbus_proxy_call_sync (
			th_extension->web_extension,
			"GetDocumentURI",
			NULL,
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			NULL,
			NULL);
	if (result) {
		document_uri = g_variant_get_string (result, NULL);
		g_variant_unref (result);
	}


	/* If the part below context menu was made by text-highlight formatter,
	 * then try to check what formatter it's using at the moment and set
	 * it as active in the popup menu */
	if (document_uri && strstr (document_uri, ".text-highlight") != NULL) {
		SoupURI *soup_uri;
		gtk_action_group_set_visible (
			th_extension->action_group, TRUE);

		soup_uri = soup_uri_new (document_uri);
		if (soup_uri && soup_uri->query) {
			GHashTable *query = soup_form_decode (soup_uri->query);
			gchar *highlighter;

			highlighter = g_hash_table_lookup (query, "__formatas");
			if (highlighter && *highlighter) {
				GtkAction *action = gtk_action_group_get_action (
					th_extension->action_group, highlighter);
				if (action) {
					gint value;
					g_object_get (
						G_OBJECT (action), "value",
						&value, NULL);
					gtk_radio_action_set_current_value (
						GTK_RADIO_ACTION (action), value);
				}
			}
			g_hash_table_destroy (query);
		}

		if (soup_uri) {
			soup_uri_free (soup_uri);
		}

	} else {
		gtk_action_group_set_visible (
			th_extension->action_group, FALSE);
	}
}

void
e_mail_display_popup_text_highlight_type_register (GTypeModule *type_module)
{
	e_mail_display_popup_text_highlight_register_type (type_module);
}

static void
e_mail_display_popup_text_highlight_class_init (EMailDisplayPopupTextHighlightClass *klass)
{
	EExtensionClass *extension_class;

	e_mail_display_popup_text_highlight_parent_class = g_type_class_peek_parent (klass);

	extension_class = E_EXTENSION_CLASS (klass);
	extension_class->extensible_type = E_TYPE_MAIL_DISPLAY;
}

static void
e_mail_display_popup_extension_interface_init (EMailDisplayPopupExtensionInterface *iface)
{
	iface->update_actions = update_actions;
}

void
e_mail_display_popup_text_highlight_class_finalize (EMailDisplayPopupTextHighlightClass *klass)
{

}

static void
e_mail_display_popup_text_highlight_init (EMailDisplayPopupTextHighlight *extension)
{
	extension->action_group = NULL;

	mail_display_popup_prefer_plain_watch_web_extension (extension);
}
