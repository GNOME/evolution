/*
 * e-mail-browser.c
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

#include "e-mail-browser.h"

#include <string.h>
#include <glib/gi18n.h>

#include "e-util/e-util.h"
#include "e-util/e-plugin-ui.h"
#include "e-util/gconf-bridge.h"
#include "shell/e-shell.h"
#include "shell/e-shell-utils.h"
#include "widgets/misc/e-popup-action.h"
#include "widgets/misc/e-preview-pane.h"

#include "mail/e-mail-reader.h"
#include "mail/e-mail-reader-utils.h"
#include "mail/em-folder-tree-model.h"
#include "mail/em-format-html-display.h"
#include "mail/message-list.h"

#define E_MAIL_BROWSER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_BROWSER, EMailBrowserPrivate))

#define MAIL_BROWSER_GCONF_PREFIX "/apps/evolution/mail/mail_browser"

struct _EMailBrowserPrivate {
	GtkUIManager *ui_manager;
	EFocusTracker *focus_tracker;
	EShellBackend *shell_backend;
	GtkActionGroup *action_group;
	EMFormatHTMLDisplay *formatter;

	GtkWidget *main_menu;
	GtkWidget *main_toolbar;
	GtkWidget *message_list;
	GtkWidget *search_bar;
	GtkWidget *statusbar;

	guint show_deleted : 1;
};

enum {
	PROP_0,
	PROP_FOCUS_TRACKER,
	PROP_GROUP_BY_THREADS,
	PROP_SHELL_BACKEND,
	PROP_SHOW_DELETED,
	PROP_UI_MANAGER
};

static gpointer parent_class;

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

static void
action_close_cb (GtkAction *action,
                 EMailBrowser *browser)
{
	e_mail_browser_close (browser);
}

static GtkActionEntry mail_browser_entries[] = {

	{ "close",
	  GTK_STOCK_CLOSE,
	  NULL,
	  NULL,
	  N_("Close this window"),
	  G_CALLBACK (action_close_cb) },

	{ "copy-clipboard",
	  GTK_STOCK_COPY,
	  NULL,
	  NULL,
	  N_("Copy the selection"),
	  NULL },  /* Handled by EFocusTracker */

	{ "cut-clipboard",
	  GTK_STOCK_CUT,
	  NULL,
	  NULL,
	  N_("Cut the selection"),
	  NULL },  /* Handled by EFocusTracker */

	{ "paste-clipboard",
	  GTK_STOCK_PASTE,
	  NULL,
	  NULL,
	  N_("Paste the clipboard"),
	  NULL },  /* Handled by EFocusTracker */

	{ "select-all",
	  GTK_STOCK_SELECT_ALL,
	  NULL,
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
	EMFormatHTML *formatter;
	CamelMessageInfo *info;
	CamelFolder *folder;
	EMailReader *reader;
	EWebView *web_view;
	const gchar *title;

	reader = E_MAIL_READER (browser);
	e_mail_reader_update_actions (reader, e_mail_reader_check_state (reader));

	if (uid == NULL)
		return;

	folder = e_mail_reader_get_folder (reader);
	formatter = e_mail_reader_get_formatter (reader);
	web_view = em_format_html_get_web_view (formatter);

	info = camel_folder_get_message_info (folder, uid);

	if (info == NULL)
		return;

	title = camel_message_info_subject (info);
	if (title == NULL || *title == '\0')
		title = _("(No Subject)");

	gtk_window_set_title (GTK_WINDOW (browser), title);
	gtk_widget_grab_focus (GTK_WIDGET (web_view));

	camel_folder_free_message_info (folder, info);
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

	if (!message_list_count (message_list))
		g_idle_add (close_on_idle_cb, browser);
}

static gboolean
mail_browser_popup_event_cb (EMailBrowser *browser,
                             GdkEventButton *event,
                             const gchar *uri)
{
	EMailReader *reader;
	GtkMenu *menu;
	guint32 state;

	if (uri != NULL)
		return FALSE;

	reader = E_MAIL_READER (browser);
	menu = e_mail_reader_get_popup_menu (reader);

	state = e_mail_reader_check_state (reader);
	e_mail_reader_update_actions (reader, state);

	if (event == NULL)
		gtk_menu_popup (
			menu, NULL, NULL, NULL, NULL,
			0, gtk_get_current_event_time ());
	else
		gtk_menu_popup (
			menu, NULL, NULL, NULL, NULL,
			event->button, event->time);

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
mail_browser_set_shell_backend (EMailBrowser *browser,
                                EShellBackend *shell_backend)
{
	g_return_if_fail (browser->priv->shell_backend == NULL);

	browser->priv->shell_backend = g_object_ref (shell_backend);
}

static void
mail_browser_set_property (GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_GROUP_BY_THREADS:
			e_mail_reader_set_group_by_threads (
				E_MAIL_READER (object),
				g_value_get_boolean (value));
			return;

		case PROP_SHELL_BACKEND:
			mail_browser_set_shell_backend (
				E_MAIL_BROWSER (object),
				g_value_get_object (value));
			return;

		case PROP_SHOW_DELETED:
			e_mail_browser_set_show_deleted (
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
		case PROP_FOCUS_TRACKER:
			g_value_set_object (
				value, e_mail_browser_get_focus_tracker (
				E_MAIL_BROWSER (object)));
			return;

		case PROP_GROUP_BY_THREADS:
			g_value_set_boolean (
				value, e_mail_reader_get_group_by_threads (
				E_MAIL_READER (object)));
			return;

		case PROP_SHELL_BACKEND:
			g_value_set_object (
				value, e_mail_reader_get_shell_backend (
				E_MAIL_READER (object)));
			return;

		case PROP_SHOW_DELETED:
			g_value_set_boolean (
				value, e_mail_browser_get_show_deleted (
				E_MAIL_BROWSER (object)));
			return;

		case PROP_UI_MANAGER:
			g_value_set_object (
				value, e_mail_browser_get_ui_manager (
				E_MAIL_BROWSER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_browser_dispose (GObject *object)
{
	EMailBrowserPrivate *priv;

	priv = E_MAIL_BROWSER_GET_PRIVATE (object);

	if (priv->ui_manager != NULL) {
		g_object_unref (priv->ui_manager);
		priv->ui_manager = NULL;
	}

	if (priv->focus_tracker != NULL) {
		g_object_unref (priv->focus_tracker);
		priv->focus_tracker = NULL;
	}

	if (priv->shell_backend != NULL) {
		g_object_unref (priv->shell_backend);
		priv->shell_backend = NULL;
	}

	if (priv->action_group != NULL) {
		g_object_unref (priv->action_group);
		priv->action_group = NULL;
	}

	if (priv->formatter != NULL) {
		g_object_unref (priv->formatter);
		priv->formatter = NULL;
	}

	if (priv->main_menu != NULL) {
		g_object_unref (priv->main_menu);
		priv->main_menu = NULL;
	}

	if (priv->main_toolbar != NULL) {
		g_object_unref (priv->main_toolbar);
		priv->main_toolbar = NULL;
	}

	if (priv->message_list != NULL) {
		/* This will cancel a regen operation. */
		gtk_widget_destroy (priv->message_list);
		priv->message_list = NULL;
	}

	if (priv->search_bar != NULL) {
		g_object_unref (priv->search_bar);
		priv->search_bar = NULL;
	}

	if (priv->statusbar != NULL) {
		g_object_unref (priv->statusbar);
		priv->statusbar = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
mail_browser_constructed (GObject *object)
{
	EMailBrowserPrivate *priv;
	EMFormatHTML *formatter;
	EMailReader *reader;
	EShellBackend *shell_backend;
	EShell *shell;
	EFocusTracker *focus_tracker;
	ESearchBar *search_bar;
	GConfBridge *bridge;
	GtkAccelGroup *accel_group;
	GtkActionGroup *action_group;
	GtkAction *action;
	GtkUIManager *ui_manager;
	GtkWidget *container;
	GtkWidget *widget;
	EWebView *web_view;
	const gchar *domain;
	const gchar *key;
	const gchar *id;
	guint merge_id;

	/* Chain up to parent's constructed() method. */
	if (G_OBJECT_CLASS (parent_class)->constructed)
		G_OBJECT_CLASS (parent_class)->constructed (object);

	priv = E_MAIL_BROWSER_GET_PRIVATE (object);

	reader = E_MAIL_READER (object);

	shell_backend = e_mail_reader_get_shell_backend (reader);
	shell = e_shell_backend_get_shell (shell_backend);

	ui_manager = e_ui_manager_new ();
	e_shell_configure_ui_manager (shell, E_UI_MANAGER (ui_manager));

	priv->ui_manager = ui_manager;
	domain = GETTEXT_PACKAGE;

	formatter = e_mail_reader_get_formatter (reader);
	e_shell_watch_window (shell, GTK_WINDOW (object));

	web_view = em_format_html_get_web_view (formatter);

	/* The message list is a widget, but it is not shown in the browser.
	 * Unfortunately, the widget is inseparable from its model, and the
	 * model is all we need. */
	priv->message_list = message_list_new (shell_backend);
	g_object_ref_sink (priv->message_list);

	g_signal_connect_swapped (
		priv->message_list, "message-selected",
		G_CALLBACK (mail_browser_message_selected_cb), object);

	g_signal_connect_swapped (
		priv->message_list, "message-list-built",
		G_CALLBACK (mail_browser_message_list_built_cb), object);

	g_signal_connect_swapped (
		web_view, "popup-event",
		G_CALLBACK (mail_browser_popup_event_cb), object);

	g_signal_connect_swapped (
		web_view, "status-message",
		G_CALLBACK (mail_browser_status_message_cb), object);

	e_mail_reader_init (reader, TRUE, TRUE);

	action_group = priv->action_group;
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_action_group_add_actions (
		action_group, mail_browser_entries,
		G_N_ELEMENTS (mail_browser_entries), object);
	e_action_group_add_popup_actions (
		action_group, mail_browser_popup_entries,
		G_N_ELEMENTS (mail_browser_popup_entries));
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);

	e_ui_manager_add_ui_from_file (
		E_UI_MANAGER (ui_manager), E_MAIL_READER_UI_DEFINITION);
	e_ui_manager_add_ui_from_string (
		E_UI_MANAGER (ui_manager), ui, NULL);

	merge_id = gtk_ui_manager_new_merge_id (GTK_UI_MANAGER (ui_manager));
	e_mail_reader_create_charset_menu (reader, ui_manager, merge_id);

	accel_group = gtk_ui_manager_get_accel_group (ui_manager);
	gtk_window_add_accel_group (GTK_WINDOW (object), accel_group);

	g_signal_connect_swapped (
		ui_manager, "connect-proxy",
		G_CALLBACK (mail_browser_connect_proxy_cb), object);

	/* Configure an EFocusTracker to manage selection actions. */

	focus_tracker = e_focus_tracker_new (GTK_WINDOW (object));
	action = gtk_action_group_get_action (action_group, "cut-clipboard");
	e_focus_tracker_set_cut_clipboard_action (focus_tracker, action);
	action = gtk_action_group_get_action (action_group, "copy-clipboard");
	e_focus_tracker_set_copy_clipboard_action (focus_tracker, action);
	action = gtk_action_group_get_action (action_group, "paste-clipboard");
	e_focus_tracker_set_paste_clipboard_action (focus_tracker, action);
	action = gtk_action_group_get_action (action_group, "select-all");
	e_focus_tracker_set_select_all_action (focus_tracker, action);
	priv->focus_tracker = focus_tracker;

	/* Construct window widgets. */

	widget = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (object), widget);
	gtk_widget_show (widget);

	container = widget;

	/* Create the status bar before connecting proxy widgets. */
	widget = gtk_statusbar_new ();
	gtk_box_pack_end (GTK_BOX (container), widget, FALSE, FALSE, 0);
	priv->statusbar = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_ui_manager_get_widget (ui_manager, "/main-menu");
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	priv->main_menu = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_ui_manager_get_widget (ui_manager, "/main-toolbar");
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	priv->main_toolbar = g_object_ref (widget);
	gtk_widget_show (widget);

	gtk_widget_show (GTK_WIDGET (web_view));

	widget = e_preview_pane_new (web_view);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	search_bar = e_preview_pane_get_search_bar (E_PREVIEW_PANE (widget));
	priv->search_bar = g_object_ref (search_bar);

	g_signal_connect_swapped (
		search_bar, "changed",
		G_CALLBACK (em_format_queue_redraw), priv->formatter);

	/* Bind GObject properties to GConf keys. */

	bridge = gconf_bridge_get ();

	object = G_OBJECT (reader);
	key = "/apps/evolution/mail/display/show_deleted";
	gconf_bridge_bind_property (bridge, key, object, "show-deleted");

	id = "org.gnome.evolution.mail.browser";
	e_plugin_ui_register_manager (ui_manager, id, object);
	e_plugin_ui_enable_manager (ui_manager, id);

	e_mail_reader_connect_headers (E_MAIL_READER (reader));
}

static gboolean
mail_browser_key_press_event (GtkWidget *widget,
                              GdkEventKey *event)
{
	if (event->keyval == GDK_Escape) {
		e_mail_browser_close (E_MAIL_BROWSER (widget));
		return TRUE;
	}

	/* Chain up to parent's key_press_event() method. */
	return GTK_WIDGET_CLASS (parent_class)->
		key_press_event (widget, event);
}

static GtkActionGroup *
mail_browser_get_action_group (EMailReader *reader)
{
	EMailBrowserPrivate *priv;

	priv = E_MAIL_BROWSER_GET_PRIVATE (reader);

	return priv->action_group;
}

static gboolean
mail_browser_get_hide_deleted (EMailReader *reader)
{
	EMailBrowser *browser;

	browser = E_MAIL_BROWSER (reader);

	return !e_mail_browser_get_show_deleted (browser);
}

static EMFormatHTML *
mail_browser_get_formatter (EMailReader *reader)
{
	EMailBrowserPrivate *priv;

	priv = E_MAIL_BROWSER_GET_PRIVATE (reader);

	return EM_FORMAT_HTML (priv->formatter);
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

static EShellBackend *
mail_browser_get_shell_backend (EMailReader *reader)
{
	EMailBrowserPrivate *priv;

	priv = E_MAIL_BROWSER_GET_PRIVATE (reader);

	return priv->shell_backend;
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
	EMailReaderInterface *interface;
	CamelMessageInfo *info;
	CamelFolder *folder;

	/* Chain up to parent's set_message() method. */
	interface = g_type_default_interface_peek (E_TYPE_MAIL_READER);
	interface->set_message (reader, uid);

	if (uid == NULL) {
		e_mail_browser_close (E_MAIL_BROWSER (reader));
		return;
	}

	folder = e_mail_reader_get_folder (reader);
	info = camel_folder_get_message_info (folder, uid);

	if (info != NULL) {
		gtk_window_set_title (
			GTK_WINDOW (reader),
			camel_message_info_subject (info));
		camel_folder_free_message_info (folder, info);
	}
}

static void
mail_browser_show_search_bar (EMailReader *reader)
{
	EMailBrowserPrivate *priv;

	priv = E_MAIL_BROWSER_GET_PRIVATE (reader);

	gtk_widget_show (priv->search_bar);
}

static void
mail_browser_class_init (EMailBrowserClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	parent_class = g_type_class_peek_parent (class);
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
		PROP_FOCUS_TRACKER,
		g_param_spec_object (
			"focus-tracker",
			"Focus Tracker",
			NULL,
			E_TYPE_FOCUS_TRACKER,
			G_PARAM_READABLE));

	/* Inherited from EMailReader */
	g_object_class_override_property (
		object_class,
		PROP_GROUP_BY_THREADS,
		"group-by-threads");

	g_object_class_install_property (
		object_class,
		PROP_SHELL_BACKEND,
		g_param_spec_object (
			"shell-backend",
			"Shell Module",
			"The mail shell backend",
			E_TYPE_SHELL_BACKEND,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_DELETED,
		g_param_spec_boolean (
			"show-deleted",
			"Show Deleted",
			"Show deleted messages",
			FALSE,
			G_PARAM_READWRITE));
}

static void
mail_browser_interface_init (EMailReaderInterface *interface)
{
	interface->get_action_group = mail_browser_get_action_group;
	interface->get_formatter = mail_browser_get_formatter;
	interface->get_hide_deleted = mail_browser_get_hide_deleted;
	interface->get_message_list = mail_browser_get_message_list;
	interface->get_popup_menu = mail_browser_get_popup_menu;
	interface->get_shell_backend = mail_browser_get_shell_backend;
	interface->get_window = mail_browser_get_window;
	interface->set_message = mail_browser_set_message;
	interface->show_search_bar = mail_browser_show_search_bar;
}

static void
mail_browser_init (EMailBrowser *browser)
{
	GConfBridge *bridge;
	const gchar *prefix;

	browser->priv = E_MAIL_BROWSER_GET_PRIVATE (browser);

	browser->priv->action_group = gtk_action_group_new ("mail-browser");
	browser->priv->formatter = em_format_html_display_new ();

	bridge = gconf_bridge_get ();
	prefix = "/apps/evolution/mail/mail_browser";
	gconf_bridge_bind_window_size (bridge, prefix, GTK_WINDOW (browser));

	gtk_window_set_title (GTK_WINDOW (browser), _("Evolution"));
}

GType
e_mail_browser_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EMailBrowserClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) mail_browser_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EMailBrowser),
			0,     /* n_preallocs */
			(GInstanceInitFunc) mail_browser_init,
			NULL   /* value_table */
		};

		static const GInterfaceInfo interface_info = {
			(GInterfaceInitFunc) mail_browser_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL   /* interface_data */
		};

		type = g_type_register_static (
			GTK_TYPE_WINDOW, "EMailBrowser", &type_info, 0);

		g_type_add_interface_static (
			type, E_TYPE_MAIL_READER, &interface_info);
	}

	return type;
}

GtkWidget *
e_mail_browser_new (EShellBackend *shell_backend)
{
	g_return_val_if_fail (E_IS_SHELL_BACKEND (shell_backend), NULL);

	return g_object_new (
		E_TYPE_MAIL_BROWSER,
		"shell-backend", shell_backend, NULL);
}

void
e_mail_browser_close (EMailBrowser *browser)
{
	g_return_if_fail (E_IS_MAIL_BROWSER (browser));

	gtk_widget_destroy (GTK_WIDGET (browser));
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

	browser->priv->show_deleted = show_deleted;

	g_object_notify (G_OBJECT (browser), "show-deleted");
}

EFocusTracker *
e_mail_browser_get_focus_tracker (EMailBrowser *browser)
{
	g_return_val_if_fail (E_IS_MAIL_BROWSER (browser), NULL);

	return browser->priv->focus_tracker;
}

GtkUIManager *
e_mail_browser_get_ui_manager (EMailBrowser *browser)
{
	g_return_val_if_fail (E_IS_MAIL_BROWSER (browser), NULL);

	return browser->priv->ui_manager;
}
