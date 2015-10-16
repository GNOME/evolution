/*
 * e-mail-browser.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-mail-browser.h"

#include <string.h>
#include <glib/gi18n.h>

#include <shell/e-shell.h>
#include <shell/e-shell-utils.h>

#include <em-format/e-mail-formatter-enumtypes.h>

#include "e-mail-reader.h"
#include "e-mail-reader-utils.h"
#include "em-folder-tree-model.h"
#include "message-list.h"

#define E_MAIL_BROWSER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_BROWSER, EMailBrowserPrivate))

#define ACTION_GROUP_STANDARD		"action-group-standard"
#define ACTION_GROUP_SEARCH_FOLDERS	"action-group-search-folders"

struct _EMailBrowserPrivate {
	EMailBackend *backend;
	GtkUIManager *ui_manager;
	EFocusTracker *focus_tracker;

	EMailFormatterMode display_mode;
	EAutomaticActionPolicy close_on_reply_policy;

	GtkWidget *main_menu;
	GtkWidget *main_toolbar;
	GtkWidget *message_list;
	GtkWidget *preview_pane;
	GtkWidget *statusbar;

	EAlert *close_on_reply_alert;
	gulong close_on_reply_response_handler_id;

	guint show_deleted : 1;
	guint show_junk : 1;
};

enum {
	PROP_0,
	PROP_BACKEND,
	PROP_CLOSE_ON_REPLY_POLICY,
	PROP_DISPLAY_MODE,
	PROP_FOCUS_TRACKER,
	PROP_FORWARD_STYLE,
	PROP_GROUP_BY_THREADS,
	PROP_REPLY_STYLE,
	PROP_MARK_SEEN_ALWAYS,
	PROP_SHOW_DELETED,
	PROP_SHOW_JUNK,
	PROP_UI_MANAGER
};

/* This is too trivial to put in a file.
 * It gets merged with the EMailReader UI. */
static const gchar *ui =
"<ui>"
"  <menubar name='main-menu'>"
"    <menu action='file-menu'>"
"      <placeholder name='file-actions'/>"
"      <placeholder name='print-actions'/>"
"      <separator/>"
"      <menuitem action='close'/>"
"    </menu>"
"    <menu action='edit-menu'>"
"      <placeholder name='selection-actions'>"
"        <menuitem action='cut-clipboard'/>"
"        <menuitem action='copy-clipboard'/>"
"        <menuitem action='paste-clipboard'/>"
"        <separator/>"
"        <menuitem action='select-all'/>"
"      </placeholder>"
"    </menu>"
"  </menubar>"
"</ui>";

static void	e_mail_browser_reader_init
					(EMailReaderInterface *iface);

G_DEFINE_TYPE_WITH_CODE (
	EMailBrowser,
	e_mail_browser,
	GTK_TYPE_WINDOW,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_READER, e_mail_browser_reader_init)
	G_IMPLEMENT_INTERFACE (
		E_TYPE_EXTENSIBLE, NULL))

static void
action_close_cb (GtkAction *action,
                 EMailBrowser *browser)
{
	e_mail_browser_close (browser);
}

static GtkActionEntry mail_browser_entries[] = {

	{ "close",
	  "window-close",
	  N_("_Close"),
	  "<Control>w",
	  N_("Close this window"),
	  G_CALLBACK (action_close_cb) },

	{ "copy-clipboard",
	  "edit-copy",
	  N_("_Copy"),
	  "<Control>c",
	  N_("Copy the selection"),
	  NULL },  /* Handled by EFocusTracker */

	{ "cut-clipboard",
	  "edit-cut",
	  N_("Cu_t"),
	  "<Control>x",
	  N_("Cut the selection"),
	  NULL },  /* Handled by EFocusTracker */

	{ "paste-clipboard",
	  "edit-paste",
	  N_("_Paste"),
	  "<Control>v",
	  N_("Paste the clipboard"),
	  NULL },  /* Handled by EFocusTracker */

	{ "select-all",
	  "edit-select-all",
	  N_("Select _All"),
	  NULL,
	  N_("Select all text"),
	  NULL },  /* Handled by EFocusTracker */

	/*** Menus ***/

	{ "file-menu",
	  NULL,
	  N_("_File"),
	  NULL,
	  NULL,
	  NULL },

	{ "edit-menu",
	  NULL,
	  N_("_Edit"),
	  NULL,
	  NULL,
	  NULL },

	{ "view-menu",
	  NULL,
	  N_("_View"),
	  NULL,
	  NULL,
	  NULL }
};

static EPopupActionEntry mail_browser_popup_entries[] = {

	{ "popup-copy-clipboard",
	  NULL,
	  "copy-clipboard" }
};

static void
mail_browser_menu_item_select_cb (EMailBrowser *browser,
                                  GtkWidget *widget)
{
	GtkAction *action;
	GtkActivatable *activatable;
	GtkStatusbar *statusbar;
	const gchar *tooltip;
	guint context_id;
	gpointer data;

	activatable = GTK_ACTIVATABLE (widget);
	action = gtk_activatable_get_related_action (activatable);
	tooltip = gtk_action_get_tooltip (action);

	data = g_object_get_data (G_OBJECT (widget), "context-id");
	context_id = GPOINTER_TO_UINT (data);

	if (tooltip == NULL)
		return;

	statusbar = GTK_STATUSBAR (browser->priv->statusbar);
	gtk_statusbar_push (statusbar, context_id, tooltip);
}

static void
mail_browser_menu_item_deselect_cb (EMailBrowser *browser,
                                    GtkWidget *menu_item)
{
	GtkStatusbar *statusbar;
	guint context_id;
	gpointer data;

	data = g_object_get_data (G_OBJECT (menu_item), "context-id");
	context_id = GPOINTER_TO_UINT (data);

	statusbar = GTK_STATUSBAR (browser->priv->statusbar);
	gtk_statusbar_pop (statusbar, context_id);
}

static void
mail_browser_connect_proxy_cb (EMailBrowser *browser,
                               GtkAction *action,
                               GtkWidget *proxy)
{
	GtkStatusbar *statusbar;
	guint context_id;

	if (!GTK_IS_MENU_ITEM (proxy))
		return;

	statusbar = GTK_STATUSBAR (browser->priv->statusbar);
	context_id = gtk_statusbar_get_context_id (statusbar, G_STRFUNC);

	g_object_set_data (
		G_OBJECT (proxy), "context-id",
		GUINT_TO_POINTER (context_id));

	g_signal_connect_swapped (
		proxy, "select",
		G_CALLBACK (mail_browser_menu_item_select_cb), browser);

	g_signal_connect_swapped (
		proxy, "deselect",
		G_CALLBACK (mail_browser_menu_item_deselect_cb), browser);
}

static void
mail_browser_message_selected_cb (EMailBrowser *browser,
                                  const gchar *uid)
{
	CamelMessageInfo *info;
	CamelFolder *folder;
	EMailReader *reader;
	guint32 state;

	reader = E_MAIL_READER (browser);
	state = e_mail_reader_check_state (reader);
	e_mail_reader_update_actions (reader, state);

	if (uid == NULL)
		return;

	folder = e_mail_reader_ref_folder (reader);

	info = camel_folder_get_message_info (folder, uid);

	if (info != NULL) {
		EMailDisplay *display;
		const gchar *title;

		display = e_mail_reader_get_mail_display (reader);

		title = camel_message_info_subject (info);
		if (title == NULL || *title == '\0')
			title = _("(No Subject)");

		gtk_window_set_title (GTK_WINDOW (browser), title);
		gtk_widget_grab_focus (GTK_WIDGET (display));

		camel_message_info_set_flags (
			info, CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_SEEN);

		camel_message_info_unref (info);
	}

	g_clear_object (&folder);
}

static gboolean
close_on_idle_cb (gpointer browser)
{
	e_mail_browser_close (browser);
	return FALSE;
}

static void
mail_browser_message_list_built_cb (EMailBrowser *browser,
                                    MessageList *message_list)
{
	g_return_if_fail (E_IS_MAIL_BROWSER (browser));
	g_return_if_fail (IS_MESSAGE_LIST (message_list));

	if (message_list_count (message_list) == 0)
		/* Prioritize ahead of GTK+ redraws. */
		g_idle_add_full (
			G_PRIORITY_HIGH_IDLE,
			close_on_idle_cb, browser, NULL);
}

static gboolean
mail_browser_popup_event_cb (EMailBrowser *browser,
                             const gchar *uri)
{
	EMailReader *reader;
	EWebView *web_view;
	GtkMenu *menu;
	guint32 state;

	if (uri != NULL)
		return FALSE;

	reader = E_MAIL_READER (browser);
	web_view = E_WEB_VIEW (e_mail_reader_get_mail_display (reader));

	if (e_web_view_get_cursor_image_src (web_view) != NULL)
		return FALSE;

	menu = e_mail_reader_get_popup_menu (reader);

	state = e_mail_reader_check_state (reader);
	e_mail_reader_update_actions (reader, state);

	gtk_menu_popup (
		menu, NULL, NULL, NULL, NULL,
		0, gtk_get_current_event_time ());

	return TRUE;
}

static void
mail_browser_status_message_cb (EMailBrowser *browser,
                                const gchar *status_message)
{
	GtkStatusbar *statusbar;
	guint context_id;

	statusbar = GTK_STATUSBAR (browser->priv->statusbar);
	context_id = gtk_statusbar_get_context_id (statusbar, G_STRFUNC);

	/* Always pop first.  This prevents messages from piling up. */
	gtk_statusbar_pop (statusbar, context_id);

	if (status_message != NULL && *status_message != '\0')
		gtk_statusbar_push (statusbar, context_id, status_message);
}

static void
mail_browser_close_on_reply_response_cb (EAlert *alert,
                                         gint response_id,
                                         EMailBrowser *browser)
{
	/* Disconnect the signal handler, but leave the EAlert
	 * itself in place so we know it's already been presented. */
	g_signal_handler_disconnect (
		browser->priv->close_on_reply_alert,
		browser->priv->close_on_reply_response_handler_id);
	browser->priv->close_on_reply_response_handler_id = 0;

	if (response_id == GTK_RESPONSE_YES) {
		e_mail_browser_set_close_on_reply_policy (
			browser, E_AUTOMATIC_ACTION_POLICY_ALWAYS);
		e_mail_browser_close (browser);
	}

	if (response_id == GTK_RESPONSE_NO) {
		e_mail_browser_set_close_on_reply_policy (
			browser, E_AUTOMATIC_ACTION_POLICY_NEVER);
	}
}

static void
mail_browser_set_backend (EMailBrowser *browser,
                          EMailBackend *backend)
{
	g_return_if_fail (E_IS_MAIL_BACKEND (backend));
	g_return_if_fail (browser->priv->backend == NULL);

	browser->priv->backend = g_object_ref (backend);
}

static void
mail_browser_set_display_mode (EMailBrowser *browser,
                               EMailFormatterMode display_mode)
{
	browser->priv->display_mode = display_mode;
}

static void
mail_browser_set_property (GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_BACKEND:
			mail_browser_set_backend (
				E_MAIL_BROWSER (object),
				g_value_get_object (value));
			return;

		case PROP_CLOSE_ON_REPLY_POLICY:
			e_mail_browser_set_close_on_reply_policy (
				E_MAIL_BROWSER (object),
				g_value_get_enum (value));
			return;

		case PROP_DISPLAY_MODE:
			mail_browser_set_display_mode (
				E_MAIL_BROWSER (object),
				g_value_get_enum (value));
			return;

		case PROP_FORWARD_STYLE:
			e_mail_reader_set_forward_style (
				E_MAIL_READER (object),
				g_value_get_enum (value));
			return;

		case PROP_GROUP_BY_THREADS:
			e_mail_reader_set_group_by_threads (
				E_MAIL_READER (object),
				g_value_get_boolean (value));
			return;

		case PROP_REPLY_STYLE:
			e_mail_reader_set_reply_style (
				E_MAIL_READER (object),
				g_value_get_enum (value));
			return;

		case PROP_SHOW_DELETED:
			e_mail_browser_set_show_deleted (
				E_MAIL_BROWSER (object),
				g_value_get_boolean (value));
			return;

		case PROP_SHOW_JUNK:
			e_mail_browser_set_show_junk (
				E_MAIL_BROWSER (object),
				g_value_get_boolean (value));
			return;

		case PROP_MARK_SEEN_ALWAYS:
			e_mail_reader_set_mark_seen_always (
				E_MAIL_READER (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_browser_get_property (GObject *object,
                           guint property_id,
                           GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_BACKEND:
			g_value_set_object (
				value,
				e_mail_reader_get_backend (
				E_MAIL_READER (object)));
			return;

		case PROP_CLOSE_ON_REPLY_POLICY:
			g_value_set_enum (
				value,
				e_mail_browser_get_close_on_reply_policy (
				E_MAIL_BROWSER (object)));
			return;

		case PROP_DISPLAY_MODE:
			g_value_set_enum (
				value,
				e_mail_browser_get_display_mode (
				E_MAIL_BROWSER (object)));
			return;

		case PROP_FOCUS_TRACKER:
			g_value_set_object (
				value,
				e_mail_browser_get_focus_tracker (
				E_MAIL_BROWSER (object)));
			return;

		case PROP_FORWARD_STYLE:
			g_value_set_enum (
				value,
				e_mail_reader_get_forward_style (
				E_MAIL_READER (object)));
			return;

		case PROP_GROUP_BY_THREADS:
			g_value_set_boolean (
				value,
				e_mail_reader_get_group_by_threads (
				E_MAIL_READER (object)));
			return;

		case PROP_REPLY_STYLE:
			g_value_set_enum (
				value,
				e_mail_reader_get_reply_style (
				E_MAIL_READER (object)));
			return;

		case PROP_SHOW_DELETED:
			g_value_set_boolean (
				value,
				e_mail_browser_get_show_deleted (
				E_MAIL_BROWSER (object)));
			return;

		case PROP_SHOW_JUNK:
			g_value_set_boolean (
				value,
				e_mail_browser_get_show_junk (
				E_MAIL_BROWSER (object)));
			return;

		case PROP_UI_MANAGER:
			g_value_set_object (
				value,
				e_mail_browser_get_ui_manager (
				E_MAIL_BROWSER (object)));
			return;

		case PROP_MARK_SEEN_ALWAYS:
			g_value_set_boolean (
				value,
				e_mail_reader_get_mark_seen_always (
				E_MAIL_READER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_browser_dispose (GObject *object)
{
	EMailBrowserPrivate *priv;

	priv = E_MAIL_BROWSER_GET_PRIVATE (object);

	if (priv->close_on_reply_response_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->close_on_reply_alert,
			priv->close_on_reply_response_handler_id);
		priv->close_on_reply_response_handler_id = 0;
	}

	g_clear_object (&priv->backend);
	g_clear_object (&priv->ui_manager);
	g_clear_object (&priv->focus_tracker);
	g_clear_object (&priv->main_menu);
	g_clear_object (&priv->main_toolbar);
	g_clear_object (&priv->preview_pane);
	g_clear_object (&priv->statusbar);
	g_clear_object (&priv->close_on_reply_alert);

	if (priv->message_list != NULL) {
		/* This will cancel a regen operation. */
		gtk_widget_destroy (priv->message_list);
		priv->message_list = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_browser_parent_class)->dispose (object);
}

static void
mail_browser_constructed (GObject *object)
{
	EMailBrowser *browser;
	EMailReader *reader;
	EMailBackend *backend;
	EMailSession *session;
	EShellBackend *shell_backend;
	EShell *shell;
	EFocusTracker *focus_tracker;
	GtkAccelGroup *accel_group;
	GtkActionGroup *action_group;
	GtkAction *action;
	GtkUIManager *ui_manager;
	GtkWidget *container;
	GtkWidget *display;
	GtkWidget *widget;
	const gchar *domain;
	const gchar *id;
	guint merge_id;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_browser_parent_class)->constructed (object);

	browser = E_MAIL_BROWSER (object);
	reader = E_MAIL_READER (object);
	backend = e_mail_reader_get_backend (reader);
	session = e_mail_backend_get_session (backend);

	shell_backend = E_SHELL_BACKEND (backend);
	shell = e_shell_backend_get_shell (shell_backend);

	ui_manager = gtk_ui_manager_new ();

	browser->priv->ui_manager = ui_manager;
	domain = GETTEXT_PACKAGE;

	gtk_application_add_window (
		GTK_APPLICATION (shell), GTK_WINDOW (object));

	/* The message list is a widget, but it is not shown in the browser.
	 * Unfortunately, the widget is inseparable from its model, and the
	 * model is all we need. */
	browser->priv->message_list = message_list_new (session);
	g_object_ref_sink (browser->priv->message_list);

	g_signal_connect_swapped (
		browser->priv->message_list, "message-selected",
		G_CALLBACK (mail_browser_message_selected_cb), object);

	g_signal_connect_swapped (
		browser->priv->message_list, "message-list-built",
		G_CALLBACK (mail_browser_message_list_built_cb), object);

	display = e_mail_display_new (e_mail_backend_get_remote_content (backend));

	e_mail_display_set_mode (
		E_MAIL_DISPLAY (display),
		browser->priv->display_mode);

	g_signal_connect_swapped (
		display, "popup-event",
		G_CALLBACK (mail_browser_popup_event_cb), object);

	g_signal_connect_swapped (
		display, "status-message",
		G_CALLBACK (mail_browser_status_message_cb), object);

	widget = e_preview_pane_new (E_WEB_VIEW (display));
	browser->priv->preview_pane = g_object_ref (widget);
	gtk_widget_show (widget);

	action_group = gtk_action_group_new (ACTION_GROUP_STANDARD);
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_action_group_add_actions (
		action_group, mail_browser_entries,
		G_N_ELEMENTS (mail_browser_entries), object);
	e_action_group_add_popup_actions (
		action_group, mail_browser_popup_entries,
		G_N_ELEMENTS (mail_browser_popup_entries));
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);

	/* For easy access.  Takes ownership of the reference. */
	g_object_set_data_full (
		object, ACTION_GROUP_STANDARD,
		action_group, (GDestroyNotify) g_object_unref);

	action_group = gtk_action_group_new (ACTION_GROUP_SEARCH_FOLDERS);
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);

	/* For easy access.  Takes ownership of the reference. */
	g_object_set_data_full (
		object, ACTION_GROUP_SEARCH_FOLDERS,
		action_group, (GDestroyNotify) g_object_unref);

	e_mail_reader_init (reader, TRUE, TRUE);

	e_load_ui_manager_definition (ui_manager, E_MAIL_READER_UI_DEFINITION);
	gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, NULL);

	merge_id = gtk_ui_manager_new_merge_id (ui_manager);
	e_mail_reader_create_charset_menu (reader, ui_manager, merge_id);

	accel_group = gtk_ui_manager_get_accel_group (ui_manager);
	gtk_window_add_accel_group (GTK_WINDOW (object), accel_group);

	g_signal_connect_swapped (
		ui_manager, "connect-proxy",
		G_CALLBACK (mail_browser_connect_proxy_cb), object);

	e_mail_reader_connect_remote_content (reader);

	/* Configure an EFocusTracker to manage selection actions. */

	focus_tracker = e_focus_tracker_new (GTK_WINDOW (object));
	action = e_mail_reader_get_action (reader, "cut-clipboard");
	e_focus_tracker_set_cut_clipboard_action (focus_tracker, action);
	action = e_mail_reader_get_action (reader, "copy-clipboard");
	e_focus_tracker_set_copy_clipboard_action (focus_tracker, action);
	action = e_mail_reader_get_action (reader, "paste-clipboard");
	e_focus_tracker_set_paste_clipboard_action (focus_tracker, action);
	action = e_mail_reader_get_action (reader, "select-all");
	e_focus_tracker_set_select_all_action (focus_tracker, action);
	browser->priv->focus_tracker = focus_tracker;

	/* Construct window widgets. */

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add (GTK_CONTAINER (object), widget);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_statusbar_new ();
	gtk_box_pack_end (GTK_BOX (container), widget, FALSE, FALSE, 0);
	browser->priv->statusbar = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_ui_manager_get_widget (ui_manager, "/main-menu");
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	browser->priv->main_menu = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_ui_manager_get_widget (ui_manager, "/main-toolbar");
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	browser->priv->main_toolbar = g_object_ref (widget);
	gtk_widget_show (widget);

	gtk_style_context_add_class (
		gtk_widget_get_style_context (widget),
		GTK_STYLE_CLASS_PRIMARY_TOOLBAR);

	gtk_box_pack_start (
		GTK_BOX (container),
		browser->priv->preview_pane,
		TRUE, TRUE, 0);

	id = "org.gnome.evolution.mail.browser";
	e_plugin_ui_register_manager (ui_manager, id, object);
	e_plugin_ui_enable_manager (ui_manager, id);

	e_extensible_load_extensions (E_EXTENSIBLE (object));
}

static gboolean
mail_browser_key_press_event (GtkWidget *widget,
                              GdkEventKey *event)
{
	EMailDisplay *mail_display;

	g_return_val_if_fail (E_IS_MAIL_BROWSER (widget), FALSE);

	mail_display = e_mail_reader_get_mail_display (E_MAIL_READER (widget));

	switch (event->keyval) {
		case GDK_KEY_Escape:
			e_mail_browser_close (E_MAIL_BROWSER (widget));
			return TRUE;

		case GDK_KEY_Home:
		case GDK_KEY_Left:
		case GDK_KEY_Up:
		case GDK_KEY_Right:
		case GDK_KEY_Down:
		case GDK_KEY_Next:
		case GDK_KEY_End:
		case GDK_KEY_Begin:
			/* If Caret mode is enabled don't try to process these keys */
			if (e_web_view_get_caret_mode (E_WEB_VIEW (mail_display)))
				break;
		case GDK_KEY_Prior:
			if (!e_mail_display_needs_key (mail_display, FALSE) &&
			    webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW (mail_display)) !=
			    webkit_web_view_get_focused_frame (WEBKIT_WEB_VIEW (mail_display))) {
				WebKitDOMDocument *document;
				WebKitDOMDOMWindow *window;

				document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (mail_display));
				window = webkit_dom_document_get_default_view (document);

				/* Workaround WebKit bug for key navigation, when inner IFRAME is focused.
				 * EMailView's inner IFRAMEs have disabled scrolling, but WebKit doesn't post
				 * key navigation events to parent's frame, thus the view doesn't scroll.
				 * This is a poor workaround for this issue, the main frame is focused,
				 * which has scrolling enabled.
				*/
				webkit_dom_dom_window_focus (window);
			}
			break;
	}

	/* Chain up to parent's key_press_event() method. */
	return GTK_WIDGET_CLASS (e_mail_browser_parent_class)->
		key_press_event (widget, event);
}

static GtkActionGroup *
mail_browser_get_action_group (EMailReader *reader,
                               EMailReaderActionGroup group)
{
	const gchar *group_name;

	switch (group) {
		case E_MAIL_READER_ACTION_GROUP_STANDARD:
			group_name = ACTION_GROUP_STANDARD;
			break;
		case E_MAIL_READER_ACTION_GROUP_SEARCH_FOLDERS:
			group_name = ACTION_GROUP_SEARCH_FOLDERS;
			break;
		default:
			g_return_val_if_reached (NULL);
	}

	return g_object_get_data (G_OBJECT (reader), group_name);
}

static EMailBackend *
mail_browser_get_backend (EMailReader *reader)
{
	EMailBrowser *browser;

	browser = E_MAIL_BROWSER (reader);

	return browser->priv->backend;
}

static gboolean
mail_browser_get_hide_deleted (EMailReader *reader)
{
	EMailBrowser *browser;

	browser = E_MAIL_BROWSER (reader);

	return !e_mail_browser_get_show_deleted (browser);
}

static EMailDisplay *
mail_browser_get_mail_display (EMailReader *reader)
{
	EPreviewPane *preview_pane;
	EWebView *web_view;

	preview_pane = e_mail_reader_get_preview_pane (reader);
	web_view = e_preview_pane_get_web_view (preview_pane);

	return E_MAIL_DISPLAY (web_view);
}

static GtkWidget *
mail_browser_get_message_list (EMailReader *reader)
{
	EMailBrowserPrivate *priv;

	priv = E_MAIL_BROWSER_GET_PRIVATE (reader);

	return priv->message_list;
}

static GtkMenu *
mail_browser_get_popup_menu (EMailReader *reader)
{
	EMailBrowser *browser;
	GtkUIManager *ui_manager;
	GtkWidget *widget;

	browser = E_MAIL_BROWSER (reader);
	ui_manager = e_mail_browser_get_ui_manager (browser);
	widget = gtk_ui_manager_get_widget (ui_manager, "/mail-preview-popup");

	return GTK_MENU (widget);
}

static EPreviewPane *
mail_browser_get_preview_pane (EMailReader *reader)
{
	EMailBrowserPrivate *priv;

	priv = E_MAIL_BROWSER_GET_PRIVATE (reader);

	return E_PREVIEW_PANE (priv->preview_pane);
}

static GtkWindow *
mail_browser_get_window (EMailReader *reader)
{
	return GTK_WINDOW (reader);
}

static void
mail_browser_set_message (EMailReader *reader,
                          const gchar *uid)
{
	EMailReaderInterface *iface;
	EMailBrowser *browser;
	CamelMessageInfo *info;
	CamelFolder *folder;

	browser = E_MAIL_BROWSER (reader);

	/* Chain up to parent's set_message() method. */
	iface = g_type_default_interface_peek (E_TYPE_MAIL_READER);
	iface->set_message (reader, uid);

	if (uid == NULL) {
		e_mail_browser_close (browser);
		return;
	}

	folder = e_mail_reader_ref_folder (reader);

	info = camel_folder_get_message_info (folder, uid);

	if (info != NULL) {
		gtk_window_set_title (
			GTK_WINDOW (reader),
			camel_message_info_subject (info));
		camel_message_info_unref (info);
	}

	g_clear_object (&folder);
}

static void
mail_browser_composer_created (EMailReader *reader,
                               EMsgComposer *composer,
                               CamelMimeMessage *message)
{
	EMailBrowser *browser;
	EAutomaticActionPolicy policy;

	/* Do not prompt if there is no source message.  It means
	 * the user wants to start a brand new message, presumably
	 * unrelated to the message shown in the browser window. */
	if (message == NULL)
		return;

	browser = E_MAIL_BROWSER (reader);
	policy = e_mail_browser_get_close_on_reply_policy (browser);

	switch (policy) {
		case E_AUTOMATIC_ACTION_POLICY_ALWAYS:
			e_mail_browser_close (browser);
			break;

		case E_AUTOMATIC_ACTION_POLICY_NEVER:
			/* do nothing */
			break;

		case E_AUTOMATIC_ACTION_POLICY_ASK:
			e_mail_browser_ask_close_on_reply (browser);
			break;
	}
}

static void
e_mail_browser_class_init (EMailBrowserClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	g_type_class_add_private (class, sizeof (EMailBrowserPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_browser_set_property;
	object_class->get_property = mail_browser_get_property;
	object_class->dispose = mail_browser_dispose;
	object_class->constructed = mail_browser_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->key_press_event = mail_browser_key_press_event;

	g_object_class_install_property (
		object_class,
		PROP_BACKEND,
		g_param_spec_object (
			"backend",
			"Mail Backend",
			"The mail backend",
			E_TYPE_MAIL_BACKEND,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_CLOSE_ON_REPLY_POLICY,
		g_param_spec_enum (
			"close-on-reply-policy",
			"Close on Reply Policy",
			"Policy for automatically closing the message "
			"browser window when forwarding or replying to "
			"the displayed message",
			E_TYPE_AUTOMATIC_ACTION_POLICY,
			E_AUTOMATIC_ACTION_POLICY_ASK,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_DISPLAY_MODE,
		g_param_spec_enum (
			"display-mode",
			"Display Mode",
			NULL,
			E_TYPE_MAIL_FORMATTER_MODE,
			E_MAIL_FORMATTER_MODE_NORMAL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_FOCUS_TRACKER,
		g_param_spec_object (
			"focus-tracker",
			"Focus Tracker",
			NULL,
			E_TYPE_FOCUS_TRACKER,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	/* Inherited from EMailReader */
	g_object_class_override_property (
		object_class,
		PROP_FORWARD_STYLE,
		"forward-style");

	/* Inherited from EMailReader */
	g_object_class_override_property (
		object_class,
		PROP_GROUP_BY_THREADS,
		"group-by-threads");

	/* Inherited from EMailReader */
	g_object_class_override_property (
		object_class,
		PROP_REPLY_STYLE,
		"reply-style");

	/* Inherited from EMailReader */
	g_object_class_override_property (
		object_class,
		PROP_MARK_SEEN_ALWAYS,
		"mark-seen-always");

	g_object_class_install_property (
		object_class,
		PROP_SHOW_DELETED,
		g_param_spec_boolean (
			"show-deleted",
			"Show Deleted",
			"Show deleted messages",
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_JUNK,
		g_param_spec_boolean (
			"show-junk",
			"Show Junk",
			"Show junk messages",
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_UI_MANAGER,
		g_param_spec_object (
			"ui-manager",
			"UI Manager",
			NULL,
			GTK_TYPE_UI_MANAGER,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));
}

static void
e_mail_browser_reader_init (EMailReaderInterface *iface)
{
	iface->get_action_group = mail_browser_get_action_group;
	iface->get_backend = mail_browser_get_backend;
	iface->get_mail_display = mail_browser_get_mail_display;
	iface->get_hide_deleted = mail_browser_get_hide_deleted;
	iface->get_message_list = mail_browser_get_message_list;
	iface->get_popup_menu = mail_browser_get_popup_menu;
	iface->get_preview_pane = mail_browser_get_preview_pane;
	iface->get_window = mail_browser_get_window;
	iface->set_message = mail_browser_set_message;
	iface->composer_created = mail_browser_composer_created;
}

static void
e_mail_browser_init (EMailBrowser *browser)
{
	browser->priv = E_MAIL_BROWSER_GET_PRIVATE (browser);

	gtk_window_set_title (GTK_WINDOW (browser), _("Evolution"));
	gtk_window_set_default_size (GTK_WINDOW (browser), 600, 400);

	e_restore_window (
		GTK_WINDOW (browser),
		"/org/gnome/evolution/mail/browser-window/",
		E_RESTORE_WINDOW_SIZE);
}

GtkWidget *
e_mail_browser_new (EMailBackend *backend,
                    EMailFormatterMode display_mode)
{
	g_return_val_if_fail (E_IS_MAIL_BACKEND (backend), NULL);

	return g_object_new (
		E_TYPE_MAIL_BROWSER,
		"backend", backend,
		"display-mode", display_mode,
		NULL);
}

void
e_mail_browser_close (EMailBrowser *browser)
{
	g_return_if_fail (E_IS_MAIL_BROWSER (browser));

	gtk_widget_destroy (GTK_WIDGET (browser));
}

void
e_mail_browser_ask_close_on_reply (EMailBrowser *browser)
{
	EAlertSink *alert_sink;
	EAlert *alert;
	gulong handler_id;

	g_return_if_fail (E_IS_MAIL_BROWSER (browser));

	/* Do nothing if the question has already been presented, even if
	 * the user dismissed it without answering.  We only present the
	 * question once per browser window, lest it become annoying. */
	if (browser->priv->close_on_reply_alert != NULL)
		return;

	alert = e_alert_new ("mail:browser-close-on-reply", NULL);

	handler_id = g_signal_connect (
		alert, "response",
		G_CALLBACK (mail_browser_close_on_reply_response_cb),
		browser);

	browser->priv->close_on_reply_alert = g_object_ref (alert);
	browser->priv->close_on_reply_response_handler_id = handler_id;

	alert_sink = e_mail_reader_get_alert_sink (E_MAIL_READER (browser));
	e_alert_sink_submit_alert (alert_sink, alert);

	g_object_unref (alert);
}

EAutomaticActionPolicy
e_mail_browser_get_close_on_reply_policy (EMailBrowser *browser)
{
	g_return_val_if_fail (
		E_IS_MAIL_BROWSER (browser),
		E_AUTOMATIC_ACTION_POLICY_ASK);

	return browser->priv->close_on_reply_policy;
}

void
e_mail_browser_set_close_on_reply_policy (EMailBrowser *browser,
                                          EAutomaticActionPolicy policy)
{
	g_return_if_fail (E_IS_MAIL_BROWSER (browser));

	if (policy == browser->priv->close_on_reply_policy)
		return;

	browser->priv->close_on_reply_policy = policy;

	g_object_notify (G_OBJECT (browser), "close-on-reply-policy");
}

EMailFormatterMode
e_mail_browser_get_display_mode (EMailBrowser *browser)
{
	g_return_val_if_fail (
		E_IS_MAIL_BROWSER (browser),
		E_MAIL_FORMATTER_MODE_INVALID);

	return browser->priv->display_mode;
}

EFocusTracker *
e_mail_browser_get_focus_tracker (EMailBrowser *browser)
{
	g_return_val_if_fail (E_IS_MAIL_BROWSER (browser), NULL);

	return browser->priv->focus_tracker;
}

gboolean
e_mail_browser_get_show_deleted (EMailBrowser *browser)
{
	g_return_val_if_fail (E_IS_MAIL_BROWSER (browser), FALSE);

	return browser->priv->show_deleted;
}

void
e_mail_browser_set_show_deleted (EMailBrowser *browser,
                                 gboolean show_deleted)
{
	g_return_if_fail (E_IS_MAIL_BROWSER (browser));

	if (browser->priv->show_deleted == show_deleted)
		return;

	browser->priv->show_deleted = show_deleted;

	g_object_notify (G_OBJECT (browser), "show-deleted");
}

gboolean
e_mail_browser_get_show_junk (EMailBrowser *browser)
{
	g_return_val_if_fail (E_IS_MAIL_BROWSER (browser), FALSE);

	return browser->priv->show_junk;
}

void
e_mail_browser_set_show_junk (EMailBrowser *browser,
			      gboolean show_junk)
{
	g_return_if_fail (E_IS_MAIL_BROWSER (browser));

	if (browser->priv->show_junk == show_junk)
		return;

	browser->priv->show_junk = show_junk;

	g_object_notify (G_OBJECT (browser), "show-junk");
}

GtkUIManager *
e_mail_browser_get_ui_manager (EMailBrowser *browser)
{
	g_return_val_if_fail (E_IS_MAIL_BROWSER (browser), NULL);

	return browser->priv->ui_manager;
}
