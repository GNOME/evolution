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
#include <camel/camel-folder.h>

#include "e-util/e-util.h"
#include "e-util/gconf-bridge.h"
#include "shell/e-shell.h"

#include "mail/e-mail-reader.h"
#include "mail/e-mail-reader-utils.h"
#include "mail/e-mail-search-bar.h"
#include "mail/e-mail-shell-backend.h"
#include "mail/em-folder-tree-model.h"
#include "mail/em-format-html-display.h"
#include "mail/message-list.h"

#define E_MAIL_BROWSER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_BROWSER, EMailBrowserPrivate))

#define MAIL_BROWSER_GCONF_PREFIX "/apps/evolution/mail/mail_browser"

struct _EMailBrowserPrivate {
	GtkUIManager *ui_manager;
	EShellBackend *shell_backend;
	GtkActionGroup *action_group;
	EMFormatHTMLDisplay *html_display;

	GtkWidget *main_menu;
	GtkWidget *main_toolbar;
	GtkWidget *message_list;
	GtkWidget *search_bar;
	GtkWidget *statusbar;

	guint show_deleted : 1;
};

enum {
	PROP_0,
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

static void
mail_browser_menu_item_select_cb (EMailBrowser *browser,
                                  GtkWidget *menu_item)
{
	GtkAction *action;
	GtkStatusbar *statusbar;
	gchar *tooltip = NULL;
	guint context_id;
	gpointer data;

	action = g_object_get_data (G_OBJECT (menu_item), "action");
	g_return_if_fail (GTK_IS_ACTION (action));

	data = g_object_get_data (G_OBJECT (menu_item), "context-id");
	context_id = GPOINTER_TO_UINT (data);

	g_object_get (action, "tooltip", &tooltip, NULL);

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

	g_object_set_data_full (
		G_OBJECT (proxy),
		"action", g_object_ref (action),
		(GDestroyNotify) g_object_unref);

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
	EMFormatHTMLDisplay *html_display;
	MessageList *message_list;
	CamelMessageInfo *info;
	EMailReader *reader;

	if (uid == NULL)
		return;

	reader = E_MAIL_READER (browser);
	html_display = e_mail_reader_get_html_display (reader);
	message_list = e_mail_reader_get_message_list (reader);
	info = camel_folder_get_message_info (message_list->folder, uid);

	if (info == NULL)
		return;

	gtk_window_set_title (
		GTK_WINDOW (browser),
		camel_message_info_subject (info));
	gtk_widget_grab_focus (
		GTK_WIDGET (((EMFormatHTML *) html_display)->html));

	camel_folder_free_message_info (message_list->folder, info);
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

	if (priv->shell_backend != NULL) {
		g_object_unref (priv->shell_backend);
		priv->shell_backend = NULL;
	}

	if (priv->action_group != NULL) {
		g_object_unref (priv->action_group);
		priv->action_group = NULL;
	}

	if (priv->html_display != NULL) {
		g_object_unref (priv->html_display);
		priv->html_display = NULL;
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
		g_object_unref (priv->message_list);
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
	EMFormatHTMLDisplay *html_display;
	EMailBrowserPrivate *priv;
	EMailReader *reader;
	EShellBackend *shell_backend;
	EShell *shell;
	GConfBridge *bridge;
	GtkAccelGroup *accel_group;
	GtkActionGroup *action_group;
	GtkUIManager *ui_manager;
	GtkWidget *container;
	GtkWidget *widget;
	GtkHTML *html;
	const gchar *domain;
	const gchar *key;
	guint merge_id;

	priv = E_MAIL_BROWSER_GET_PRIVATE (object);

	reader = E_MAIL_READER (object);
	ui_manager = priv->ui_manager;
	domain = GETTEXT_PACKAGE;

	html_display = e_mail_reader_get_html_display (reader);
	shell_backend = e_mail_reader_get_shell_backend (reader);

	shell = e_shell_backend_get_shell (shell_backend);
	e_shell_watch_window (shell, GTK_WINDOW (object));

	html = EM_FORMAT_HTML (html_display)->html;

	/* The message list is a widget, but it is not shown in the browser.
	 * Unfortunately, the widget is inseparable from its model, and the
	 * model is all we need. */
	priv->message_list = message_list_new (shell_backend);
	g_object_ref_sink (priv->message_list);

	g_signal_connect_swapped (
		priv->message_list, "message-selected",
		G_CALLBACK (mail_browser_message_selected_cb), object);

	g_signal_connect_swapped (
		html, "status-message",
		G_CALLBACK (mail_browser_status_message_cb), object);

	e_mail_reader_init (reader);

	action_group = priv->action_group;
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_action_group_add_actions (
		action_group, mail_browser_entries,
		G_N_ELEMENTS (mail_browser_entries), object);
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);

	e_load_ui_definition (ui_manager, E_MAIL_READER_UI_DEFINITION);
	gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, NULL);

	merge_id = gtk_ui_manager_new_merge_id (ui_manager);
	e_mail_reader_create_charset_menu (reader, ui_manager, merge_id);

	accel_group = gtk_ui_manager_get_accel_group (ui_manager);
	gtk_window_add_accel_group (GTK_WINDOW (object), accel_group);

	g_signal_connect_swapped (
		ui_manager, "connect-proxy",
		G_CALLBACK (mail_browser_connect_proxy_cb), object);

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

	widget = e_mail_search_bar_new (EM_FORMAT_HTML (html_display)->html);
	gtk_box_pack_end (GTK_BOX (container), widget, FALSE, FALSE, 0);
	priv->search_bar = g_object_ref (widget);
	gtk_widget_hide (widget);

	g_signal_connect_swapped (
		widget, "changed",
		G_CALLBACK (em_format_redraw), html_display);

	widget = gtk_ui_manager_get_widget (ui_manager, "/main-menu");
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	priv->main_menu = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_ui_manager_get_widget (ui_manager, "/main-toolbar");
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	priv->main_toolbar = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = GTK_WIDGET (EM_FORMAT_HTML (html_display)->html);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	/* Bind GObject properties to GConf keys. */

	bridge = gconf_bridge_get ();

	object = G_OBJECT (reader);
	key = "/apps/evolution/mail/display/show_deleted";
	gconf_bridge_bind_property (bridge, key, object, "show-deleted");
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

static EMFormatHTMLDisplay *
mail_browser_get_html_display (EMailReader *reader)
{
	EMailBrowserPrivate *priv;

	priv = E_MAIL_BROWSER_GET_PRIVATE (reader);

	return priv->html_display;
}

static MessageList *
mail_browser_get_message_list (EMailReader *reader)
{
	EMailBrowserPrivate *priv;

	priv = E_MAIL_BROWSER_GET_PRIVATE (reader);

	return MESSAGE_LIST (priv->message_list);
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
                          const gchar *uid,
                          gboolean mark_read)
{
	EMailReaderIface *iface;
	MessageList *message_list;
	CamelMessageInfo *info;
	CamelFolder *folder;

	/* Chain up to parent's set_message() method. */
	iface = g_type_default_interface_peek (E_TYPE_MAIL_READER);
	iface->set_message (reader, uid, mark_read);

	if (uid == NULL) {
		e_mail_browser_close (E_MAIL_BROWSER (reader));
		return;
	}

	message_list = e_mail_reader_get_message_list (reader);

	folder = message_list->folder;
	info = camel_folder_get_message_info (folder, uid);

	if (info != NULL) {
		gtk_window_set_title (
			GTK_WINDOW (reader),
			camel_message_info_subject (info));
		camel_folder_free_message_info (folder, info);
	}

	if (mark_read)
		e_mail_reader_mark_as_read (reader, uid);
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
		PROP_SHELL_BACKEND,
		g_param_spec_object (
			"shell-backend",
			_("Shell Module"),
			_("The mail shell backend"),
			E_TYPE_SHELL_BACKEND,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_DELETED,
		g_param_spec_boolean (
			"show-deleted",
			_("Show Deleted"),
			_("Show deleted messages"),
			FALSE,
			G_PARAM_READWRITE));
}

static void
mail_browser_iface_init (EMailReaderIface *iface)
{
	iface->get_action_group = mail_browser_get_action_group;
	iface->get_hide_deleted = mail_browser_get_hide_deleted;
	iface->get_html_display = mail_browser_get_html_display;
	iface->get_message_list = mail_browser_get_message_list;
	iface->get_shell_backend = mail_browser_get_shell_backend;
	iface->get_window = mail_browser_get_window;
	iface->set_message = mail_browser_set_message;
	iface->show_search_bar = mail_browser_show_search_bar;
}

static void
mail_browser_init (EMailBrowser *browser)
{
	GConfBridge *bridge;
	const gchar *prefix;

	browser->priv = E_MAIL_BROWSER_GET_PRIVATE (browser);

	browser->priv->ui_manager = gtk_ui_manager_new ();
	browser->priv->action_group = gtk_action_group_new ("mail-browser");
	browser->priv->html_display = em_format_html_display_new ();

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

		static const GInterfaceInfo iface_info = {
			(GInterfaceInitFunc) mail_browser_iface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL   /* interface_data */
		};

		type = g_type_register_static (
			GTK_TYPE_WINDOW, "EMailBrowser", &type_info, 0);

		g_type_add_interface_static (
			type, E_TYPE_MAIL_READER, &iface_info);
	}

	return type;
}

GtkWidget *
e_mail_browser_new (EMailShellBackend *mail_shell_backend)
{
	g_return_val_if_fail (
		E_IS_MAIL_SHELL_BACKEND (mail_shell_backend), NULL);

	return g_object_new (
		E_TYPE_MAIL_BROWSER,
		"shell-backend", mail_shell_backend, NULL);
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

GtkUIManager *
e_mail_browser_get_ui_manager (EMailBrowser *browser)
{
	g_return_val_if_fail (E_IS_MAIL_BROWSER (browser), NULL);

	return browser->priv->ui_manager;
}
