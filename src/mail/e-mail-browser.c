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

#include "evolution-config.h"

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

#define ACTION_GROUP_STANDARD		"action-group-standard"
#define ACTION_GROUP_SEARCH_FOLDERS	"action-group-search-folders"
#define ACTION_GROUP_LABELS		"action-group-labels"

struct _EMailBrowserPrivate {
	EMailBackend *backend;
	EUIManager *ui_manager;
	EFocusTracker *focus_tracker;

	EMailFormatterMode display_mode;
	EAutomaticActionPolicy close_on_reply_policy;

	EMenuBar *menu_bar;
	GtkWidget *menu_button; /* owned by menu_bar */
	GtkWidget *main_toolbar;
	GtkWidget *message_list;
	GtkWidget *preview_pane;
	GtkWidget *statusbar;

	EAlert *close_on_reply_alert;
	gulong close_on_reply_response_handler_id;

	guint show_deleted : 1;
	guint show_junk : 1;
	guint close_on_delete_or_junk : 1;
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
	PROP_DELETE_SELECTS_PREVIOUS,
	PROP_CLOSE_ON_DELETE_OR_JUNK
};


static void	e_mail_browser_reader_init
					(EMailReaderInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EMailBrowser, e_mail_browser, GTK_TYPE_WINDOW,
	G_ADD_PRIVATE (EMailBrowser)
	G_IMPLEMENT_INTERFACE (E_TYPE_MAIL_READER, e_mail_browser_reader_init)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL))

static void
action_close_cb (EUIAction *action,
		 GVariant *parameter,
		 gpointer user_data)
{
	EMailBrowser *browser = user_data;

	e_mail_browser_close (browser);
}

static void
action_search_web_cb (EUIAction *action,
		      GVariant *parameter,
		      gpointer user_data)
{
	EMailReader *reader = user_data;
	EMailDisplay *display;
	EUIAction *wv_action;

	display = e_mail_reader_get_mail_display (reader);
	wv_action = e_web_view_get_action (E_WEB_VIEW (display), "search-web");

	g_action_activate (G_ACTION (wv_action), NULL);
}

static void
action_mail_smart_backward_cb (EUIAction *action,
			       GVariant *parameter,
			       gpointer user_data)
{
	EMailReader *reader = user_data;
	EMailDisplay *mail_display;

	mail_display = e_mail_reader_get_mail_display (reader);

	e_mail_display_process_magic_spacebar (mail_display, FALSE);
}

static void
action_mail_smart_forward_cb (EUIAction *action,
			      GVariant *parameter,
			      gpointer user_data)
{
	EMailReader *reader = user_data;
	EMailDisplay *mail_display;

	mail_display = e_mail_reader_get_mail_display (reader);

	e_mail_display_process_magic_spacebar (mail_display, TRUE);
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

		title = camel_message_info_get_subject (info);
		if (title == NULL || *title == '\0')
			title = _("(No Subject)");

		gtk_window_set_title (GTK_WINDOW (browser), title);

		if (gtk_widget_get_mapped (GTK_WIDGET (browser)))
			gtk_widget_grab_focus (GTK_WIDGET (display));

		if (e_mail_reader_utils_get_mark_seen_setting (reader, NULL))
			camel_message_info_set_flags (info, CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_SEEN);

		g_clear_object (&info);
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
                             const gchar *uri,
                             GdkEvent *event)
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

	state = e_mail_reader_check_state (reader);
	e_mail_reader_update_actions (reader, state);

	menu = e_mail_reader_get_popup_menu (reader);
	gtk_menu_popup_at_pointer (menu, event);

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

static gboolean
mail_browser_ui_manager_create_item_cb (EUIManager *manager,
					EUIElement *elem,
					EUIAction *action,
					EUIElementKind for_kind,
					GObject **out_item,
					gpointer user_data)
{
	EMailBrowser *self = user_data;

	g_return_val_if_fail (E_IS_MAIL_BROWSER (self), FALSE);

	if (for_kind != E_UI_ELEMENT_KIND_HEADERBAR ||
	    g_strcmp0 (g_action_get_name (G_ACTION (action)), "menu-button") != 0)
		return FALSE;

	if (self->priv->menu_button)
		*out_item = G_OBJECT (g_object_ref (self->priv->menu_button));
	else
		*out_item = NULL;

	return TRUE;
}

static void
e_mail_browser_customize_toolbar_activate_cb (GtkWidget *toolbar,
					      const gchar *id,
					      gpointer user_data)
{
	EMailBrowser *self = user_data;
	EUICustomizeDialog *dialog;

	g_return_if_fail (E_IS_MAIL_BROWSER (self));

	dialog = e_ui_customize_dialog_new (GTK_WINDOW (self));

	e_ui_customize_dialog_add_customizer (dialog, e_ui_manager_get_customizer (self->priv->ui_manager));
	e_ui_customize_dialog_run (dialog, id);

	gtk_widget_destroy (GTK_WIDGET (dialog));
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

		case PROP_DELETE_SELECTS_PREVIOUS:
			e_mail_reader_set_delete_selects_previous (
				E_MAIL_READER (object),
				g_value_get_boolean (value));
			return;

		case PROP_CLOSE_ON_DELETE_OR_JUNK:
			e_mail_browser_set_close_on_delete_or_junk (
				E_MAIL_BROWSER (object),
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

		case PROP_MARK_SEEN_ALWAYS:
			g_value_set_boolean (
				value,
				e_mail_reader_get_mark_seen_always (
				E_MAIL_READER (object)));
			return;

		case PROP_DELETE_SELECTS_PREVIOUS:
			g_value_set_boolean (
				value,
				e_mail_reader_get_delete_selects_previous (
				E_MAIL_READER (object)));
			return;

		case PROP_CLOSE_ON_DELETE_OR_JUNK:
			g_value_set_boolean (
				value,
				e_mail_browser_get_close_on_delete_or_junk (
				E_MAIL_BROWSER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_browser_dispose (GObject *object)
{
	EMailBrowser *self = E_MAIL_BROWSER (object);

	e_mail_reader_dispose (E_MAIL_READER (object));

	if (self->priv->close_on_reply_response_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->close_on_reply_alert,
			self->priv->close_on_reply_response_handler_id);
		self->priv->close_on_reply_response_handler_id = 0;
	}

	g_clear_object (&self->priv->backend);
	g_clear_object (&self->priv->ui_manager);
	g_clear_object (&self->priv->focus_tracker);
	g_clear_object (&self->priv->main_toolbar);
	g_clear_object (&self->priv->menu_bar);
	g_clear_object (&self->priv->preview_pane);
	g_clear_object (&self->priv->statusbar);
	g_clear_object (&self->priv->close_on_reply_alert);

	if (self->priv->message_list != NULL) {
		/* This will cancel a regen operation. */
		gtk_widget_destroy (self->priv->message_list);
		g_clear_object (&self->priv->message_list);
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_browser_parent_class)->dispose (object);
}

static void
mail_browser_constructed (GObject *object)
{
	static const gchar *eui =
		"<eui>"
		  "<headerbar id='main-headerbar'>"
		    "<start/>"
		    "<end>"
		      "<item action='mail-reply-sender'/>"
		      "<item action='EMailReader::mail-reply-group'/>"
		      "<item action='EMailReader::mail-forward-as-group'/>"
		      "<item action='menu-button' order='999999'/>"
		    "</end>"
		  "</headerbar>"
		  "<menu id='main-menu'>"
		    "<submenu action='file-menu'>"
		      "<placeholder id='file-actions'/>"
		      "<placeholder id='print-actions'/>"
		      "<separator/>"
		      "<item action='close'/>"
		    "</submenu>"
		    "<submenu action='edit-menu'>"
		      "<placeholder id='selection-actions'>"
			"<item action='cut-clipboard'/>"
			"<item action='copy-clipboard'/>"
			"<item action='paste-clipboard'/>"
			"<separator/>"
			"<item action='select-all'/>"
		      "</placeholder>"
		    "</submenu>"
		  "</menu>"
		"</eui>";

	static const EUIActionEntry mail_browser_entries[] = {

		{ "close",
		  "window-close",
		  N_("_Close"),
		  "<Control>w",
		  N_("Close this window"),
		  action_close_cb, NULL, NULL, NULL },

		{ "copy-clipboard",
		  "edit-copy",
		  N_("_Copy"),
		  "<Control>c",
		  N_("Copy the selection"),
		  NULL, NULL, NULL, NULL },  /* Handled by EFocusTracker */

		{ "cut-clipboard",
		  "edit-cut",
		  N_("Cu_t"),
		  "<Control>x",
		  N_("Cut the selection"),
		  NULL, NULL, NULL, NULL },  /* Handled by EFocusTracker */

		{ "paste-clipboard",
		  "edit-paste",
		  N_("_Paste"),
		  "<Control>v",
		  N_("Paste the clipboard"),
		  NULL, NULL, NULL, NULL },  /* Handled by EFocusTracker */

		{ "select-all",
		  "edit-select-all",
		  N_("Select _All"),
		  NULL,
		  N_("Select all text"),
		  NULL, NULL, NULL, NULL },  /* Handled by EFocusTracker */

		{ "search-web",
		  NULL,
		  N_("Search _Web…"),
		  NULL,
		  N_("Search the Web with the selected text"),
		  action_search_web_cb, NULL, NULL, NULL },

		/*** Menus ***/

		{ "file-menu", NULL, N_("_File"), NULL, NULL, NULL, NULL, NULL, NULL },
		{ "edit-menu", NULL, N_("_Edit"), NULL, NULL, NULL, NULL, NULL, NULL },
		{ "view-menu", NULL, N_("_View"), NULL, NULL, NULL, NULL, NULL, NULL },
		{ "menu-button", NULL, N_("Menu"), NULL, NULL, NULL, NULL, NULL, NULL }
	};

	static const EUIActionEntry mail_entries[] = {

		{ "mail-smart-backward",
		  "go-up",
		  "mail smart backward",
		  "BackSpace",
		  NULL,
		  action_mail_smart_backward_cb, NULL, NULL, NULL },

		{ "mail-smart-forward",
		  "go-down",
		  "mail smart forward",
		  "space",
		  NULL,
		  action_mail_smart_forward_cb, NULL, NULL, NULL },
	};

	EMailBrowser *browser;
	EMailReader *reader;
	EMailBackend *backend;
	EMailSession *session;
	EShellBackend *shell_backend;
	EShell *shell;
	EFocusTracker *focus_tracker;
	EAttachmentStore *attachment_store;
	EUIAction *action, *mail_action;
	EUICustomizer *customizer;
	GtkWidget *container;
	GtkWidget *display;
	GtkWidget *widget;
	GObject *ui_item;
	const gchar *toolbar_id;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_browser_parent_class)->constructed (object);

	browser = E_MAIL_BROWSER (object);
	reader = E_MAIL_READER (object);
	backend = e_mail_reader_get_backend (reader);
	session = e_mail_backend_get_session (backend);

	shell_backend = E_SHELL_BACKEND (backend);
	shell = e_shell_backend_get_shell (shell_backend);

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

	display = e_mail_display_new (e_mail_backend_get_remote_content (backend), E_MAIL_READER (browser));

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

	gtk_window_add_accel_group (GTK_WINDOW (browser), e_ui_manager_get_accel_group (e_web_view_get_ui_manager (E_WEB_VIEW (display))));

	browser->priv->ui_manager = e_ui_manager_new (e_ui_customizer_util_dup_filename_for_component ("mail-browser"));

	g_signal_connect (browser->priv->ui_manager, "create-item",
		G_CALLBACK (mail_browser_ui_manager_create_item_cb), browser);
	g_signal_connect_swapped (browser->priv->ui_manager, "ignore-accel",
		G_CALLBACK (e_mail_reader_ignore_accel), browser);

	e_mail_reader_init (reader);
	e_mail_reader_init_ui_data (reader);

	e_ui_manager_add_actions (browser->priv->ui_manager, "mail", NULL,
		mail_entries, G_N_ELEMENTS (mail_entries), object);

	e_ui_manager_add_actions_with_eui_data (browser->priv->ui_manager, ACTION_GROUP_STANDARD, NULL,
		mail_browser_entries, G_N_ELEMENTS (mail_browser_entries), object, eui);

	action = e_ui_manager_get_action (browser->priv->ui_manager, "close");
	e_ui_action_add_secondary_accel (action, "Escape");

	action = e_ui_manager_get_action (browser->priv->ui_manager, "menu-button");
	e_ui_action_set_usable_for_kinds (action, E_UI_ELEMENT_KIND_HEADERBAR);

	e_ui_manager_set_actions_usable_for_kinds (browser->priv->ui_manager, E_UI_ELEMENT_KIND_MENU,
		"file-menu",
		"edit-menu",
		"view-menu",
		NULL);

	mail_action = e_web_view_get_action (E_WEB_VIEW (display), "search-web");
	action = e_ui_manager_get_action (browser->priv->ui_manager, "search-web");

	e_binding_bind_property (
		mail_action, "sensitive",
		action, "sensitive",
		G_BINDING_SYNC_CREATE);

	e_ui_manager_set_action_groups_widget (browser->priv->ui_manager, GTK_WIDGET (browser));
	gtk_window_add_accel_group (GTK_WINDOW (browser), e_ui_manager_get_accel_group (browser->priv->ui_manager));

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

	customizer = e_ui_manager_get_customizer (browser->priv->ui_manager);

	ui_item = e_ui_manager_create_item (browser->priv->ui_manager, "main-menu");
	widget = gtk_menu_bar_new_from_model (G_MENU_MODEL (ui_item));
	g_clear_object (&ui_item);

	browser->priv->menu_bar = e_menu_bar_new (GTK_MENU_BAR (widget), GTK_WINDOW (browser), &browser->priv->menu_button);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);

	e_ui_customizer_register (customizer, "main-menu", NULL);

	if (e_util_get_use_header_bar ()) {
		ui_item = e_ui_manager_create_item (browser->priv->ui_manager, "main-headerbar");
		widget = GTK_WIDGET (ui_item);
		gtk_window_set_titlebar (GTK_WINDOW (browser), widget);

		e_ui_customizer_register (customizer, "main-headerbar", NULL);

		toolbar_id = "main-toolbar-with-headerbar";
	} else {
		toolbar_id = "main-toolbar-without-headerbar";
	}

	ui_item = e_ui_manager_create_item (browser->priv->ui_manager, toolbar_id);
	widget = GTK_WIDGET (ui_item);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	browser->priv->main_toolbar = g_object_ref (widget);
	gtk_widget_show (widget);

	e_ui_customizer_register (customizer, toolbar_id, _("Main Toolbar"));
	e_ui_customizer_util_attach_toolbar_context_menu (widget, toolbar_id,
		e_mail_browser_customize_toolbar_activate_cb, browser);

	attachment_store = e_mail_display_get_attachment_store (E_MAIL_DISPLAY (display));
	widget = GTK_WIDGET (e_mail_display_get_attachment_view (E_MAIL_DISPLAY (display)));
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	container = e_attachment_bar_get_content_area (E_ATTACHMENT_BAR (widget));

	gtk_box_pack_start (GTK_BOX (container), browser->priv->preview_pane, TRUE, TRUE, 0);

	e_binding_bind_property_full (
		attachment_store, "num-attachments",
		widget, "attachments-visible",
		G_BINDING_SYNC_CREATE,
		e_attachment_store_transform_num_attachments_to_visible_boolean,
		NULL, NULL, NULL);

	e_plugin_ui_register_manager (browser->priv->ui_manager, "org.gnome.evolution.mail.browser", object);

	action = e_mail_reader_get_action (reader, "mail-label-none");
	e_binding_bind_property (
		display, "need-input",
		action, "sensitive",
		G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

	/* WebKitGTK does not support print preview, thus hide the option from the menu;
	   maybe it'll be supported in the future */
	action = e_mail_reader_get_action (reader, "mail-print-preview");
	e_ui_action_set_visible (action, FALSE);

	e_extensible_load_extensions (E_EXTENSIBLE (object));
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
	if (!preview_pane)
		return NULL;

	web_view = e_preview_pane_get_web_view (preview_pane);

	return E_MAIL_DISPLAY (web_view);
}

static GtkWidget *
mail_browser_get_message_list (EMailReader *reader)
{
	EMailBrowser *self = E_MAIL_BROWSER (reader);

	return self->priv->message_list;
}

static EUIManager *
mail_browser_get_ui_manager (EMailReader *reader)
{
	EMailBrowser *self = E_MAIL_BROWSER (reader);

	return self->priv->ui_manager;
}

static EPreviewPane *
mail_browser_get_preview_pane (EMailReader *reader)
{
	EMailBrowser *self = E_MAIL_BROWSER (reader);

	if (!self->priv->preview_pane)
		return NULL;

	return E_PREVIEW_PANE (self->priv->preview_pane);
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
			camel_message_info_get_subject (info));
		g_clear_object (&info);
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

static gboolean
mail_browser_close_on_delete_or_junk (EMailReader *reader)
{
	g_return_val_if_fail (E_IS_MAIL_BROWSER (reader), FALSE);

	if (!e_mail_browser_get_close_on_delete_or_junk (E_MAIL_BROWSER (reader)))
		return FALSE;

	g_idle_add_full (
		G_PRIORITY_HIGH_IDLE,
		close_on_idle_cb, reader, NULL);

	return TRUE;
}

static void
e_mail_browser_class_init (EMailBrowserClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_browser_set_property;
	object_class->get_property = mail_browser_get_property;
	object_class->dispose = mail_browser_dispose;
	object_class->constructed = mail_browser_constructed;

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

	/* Inherited from EMailReader */
	g_object_class_override_property (
		object_class,
		PROP_DELETE_SELECTS_PREVIOUS,
		"delete-selects-previous");

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
		PROP_CLOSE_ON_DELETE_OR_JUNK,
		g_param_spec_boolean (
			"close-on-delete-or-junk",
			"Close On Delete Or Junk",
			"Close on message delete or when marked as Junk",
			FALSE,
			G_PARAM_CONSTRUCT |
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));
}

static void
e_mail_browser_reader_init (EMailReaderInterface *iface)
{
	iface->get_backend = mail_browser_get_backend;
	iface->get_mail_display = mail_browser_get_mail_display;
	iface->get_hide_deleted = mail_browser_get_hide_deleted;
	iface->get_message_list = mail_browser_get_message_list;
	iface->get_ui_manager = mail_browser_get_ui_manager;
	iface->get_preview_pane = mail_browser_get_preview_pane;
	iface->get_window = mail_browser_get_window;
	iface->set_message = mail_browser_set_message;
	iface->composer_created = mail_browser_composer_created;
	iface->close_on_delete_or_junk = mail_browser_close_on_delete_or_junk;
}

static void
e_mail_browser_init (EMailBrowser *browser)
{
	browser->priv = e_mail_browser_get_instance_private (browser);

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

gboolean
e_mail_browser_get_close_on_delete_or_junk (EMailBrowser *browser)
{
	g_return_val_if_fail (E_IS_MAIL_BROWSER (browser), FALSE);

	return browser->priv->close_on_delete_or_junk;
}

void
e_mail_browser_set_close_on_delete_or_junk (EMailBrowser *browser,
					    gboolean close_on_delete_or_junk)
{
	g_return_if_fail (E_IS_MAIL_BROWSER (browser));

	if ((browser->priv->close_on_delete_or_junk ? 1 : 0) == (close_on_delete_or_junk ? 1 : 0))
		return;

	browser->priv->close_on_delete_or_junk = close_on_delete_or_junk;

	g_object_notify (G_OBJECT (browser), "close-on-delete-or-junk");
}
