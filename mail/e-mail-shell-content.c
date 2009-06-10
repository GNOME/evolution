/*
 * e-mail-shell-content.c
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

#include "e-mail-shell-content.h"

#include <glib/gi18n.h>
#include <camel/camel-store.h>
#include <libedataserver/e-data-server-util.h>

#include "e-util/gconf-bridge.h"
#include "widgets/menus/gal-view-etable.h"
#include "widgets/menus/gal-view-instance.h"

#include "em-folder-view.h"
#include "em-search-context.h"
#include "em-utils.h"
#include "mail-config.h"
#include "mail-ops.h"

#include "e-mail-reader.h"
#include "e-mail-search-bar.h"
#include "e-mail-shell-backend.h"
#include "e-mail-shell-view-actions.h"

#define E_MAIL_SHELL_CONTENT_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_SHELL_CONTENT, EMailShellContentPrivate))

#define STATE_KEY_SCROLLBAR_POSITION	"ScrollbarPosition"
#define STATE_KEY_SELECTED_MESSAGE	"SelectedMessage"

struct _EMailShellContentPrivate {
	GtkWidget *paned;
	GtkWidget *message_list;
	GtkWidget *search_bar;

	EMFormatHTMLDisplay *html_display;
	GalViewInstance *view_instance;

	/* ETable scrolling hack */
	gdouble default_scrollbar_position;

	guint paned_binding_id;
	guint scroll_timeout_id;

	/* Signal handler IDs */
	guint message_list_built_id;
	guint message_list_scrolled_id;

	guint preview_visible			: 1;
	guint suppress_message_selection	: 1;
	guint vertical_view			: 1;
	guint show_deleted			: 1;
};

enum {
	PROP_0,
	PROP_PREVIEW_VISIBLE,
	PROP_SHOW_DELETED,
	PROP_VERTICAL_VIEW
};

static gpointer parent_class;
static GType mail_shell_content_type;

static void
mail_shell_content_etree_unfreeze (MessageList *message_list,
                                   GdkEvent *event)
{
	ETableItem *item;
	GObject *object;

	item = e_tree_get_item (message_list->tree);
	object = G_OBJECT (((GnomeCanvasItem *) item)->canvas);

	g_object_set_data (object, "freeze-cursor", 0);
}

static void
mail_shell_content_message_list_scrolled_cb (EMailShellContent *mail_shell_content,
                                             MessageList *message_list)
{
	EShellContent *shell_content;
	EShellView *shell_view;
	GKeyFile *key_file;
	const gchar *folder_uri;
	const gchar *key;
	gchar *group_name;
	gdouble position;

	/* Save the scrollbar position for the current folder. */

	folder_uri = message_list->folder_uri;

	if (folder_uri == NULL)
		return;

	shell_content = E_SHELL_CONTENT (mail_shell_content);
	shell_view = e_shell_content_get_shell_view (shell_content);
	key_file = e_shell_view_get_state_key_file (shell_view);

	key = STATE_KEY_SCROLLBAR_POSITION;
	group_name = g_strdup_printf ("Folder %s", folder_uri);
	position = message_list_get_scrollbar_position (message_list);

	g_key_file_set_double (key_file, group_name, key, position);
	e_shell_view_set_state_dirty (shell_view);

	g_free (group_name);
}

static gboolean
mail_shell_content_scroll_timeout_cb (EMailShellContent *mail_shell_content)
{
	EMailShellContentPrivate *priv = mail_shell_content->priv;
	EShellContent *shell_content;
	EShellView *shell_view;
	MessageList *message_list;
	EMailReader *reader;
	GKeyFile *key_file;
	const gchar *folder_uri;
	const gchar *key;
	gchar *group_name;

	/* Initialize the scrollbar position for the current folder
	 * and setup a callback to handle scrollbar position changes. */

	shell_content = E_SHELL_CONTENT (mail_shell_content);
	shell_view = e_shell_content_get_shell_view (shell_content);
	key_file = e_shell_view_get_state_key_file (shell_view);

	reader = E_MAIL_READER (mail_shell_content);
	message_list = e_mail_reader_get_message_list (reader);
	folder_uri = message_list->folder_uri;

	if (folder_uri == NULL)
		goto skip;

	/* Restore the message list scrollbar position. */

	key = STATE_KEY_SCROLLBAR_POSITION;
	group_name = g_strdup_printf ("Folder %s", folder_uri);

	if (g_key_file_has_key (key_file, group_name, key, NULL)) {
		gdouble position;

		position = g_key_file_get_double (
			key_file, group_name, key, NULL);
		message_list_set_scrollbar_position (message_list, position);
	}

	g_free (group_name);

skip:
	priv->message_list_scrolled_id = g_signal_connect_swapped (
		message_list, "message-list-scrolled",
		G_CALLBACK (mail_shell_content_message_list_scrolled_cb),
		mail_shell_content);

	priv->scroll_timeout_id = 0;

	return FALSE;
}

static void
mail_shell_content_message_list_built_cb (EMailShellContent *mail_shell_content,
                                          MessageList *message_list)
{
	EMailShellContentPrivate *priv = mail_shell_content->priv;
	EShellContent *shell_content;
	EShellView *shell_view;
	GtkScrolledWindow *scrolled_window;
	GtkWidget *vscrollbar;
	GKeyFile *key_file;
	gchar *uid;

	g_signal_handler_disconnect (
		message_list, priv->message_list_built_id);
	priv->message_list_built_id = 0;

	shell_content = E_SHELL_CONTENT (mail_shell_content);
	shell_view = e_shell_content_get_shell_view (shell_content);
	key_file = e_shell_view_get_state_key_file (shell_view);

	if (message_list->cursor_uid != NULL)
		uid = NULL;

	else if (message_list->folder_uri == NULL)
		uid = NULL;

	else if (mail_shell_content->priv->suppress_message_selection)
		uid = NULL;

	else {
		const gchar *folder_uri;
		const gchar *key;
		gchar *group_name;

		key = STATE_KEY_SELECTED_MESSAGE;
		folder_uri = message_list->folder_uri;
		group_name = g_strdup_printf ("Folder %s", folder_uri);
		uid = g_key_file_get_string (key_file, group_name, key, NULL);
		g_free (group_name);
	}

	if (uid != NULL) {
		CamelFolder *folder;
		CamelMessageInfo *info;

		folder = message_list->folder;
		info = camel_folder_get_message_info (folder, uid);
		if (info != NULL) {
			EMailReader *reader;

			reader = E_MAIL_READER (mail_shell_content);
			e_mail_reader_set_message (reader, uid, TRUE);
			camel_folder_free_message_info (folder, info);
		}

		g_free (uid);
	}

	/* FIXME This is a gross workaround for an ETable bug that I can't
	 *       fix (Ximian bug #55303).
	 *
	 *       Since e_canvas_item_region_show_relay() uses a timeout,
	 *       we have to use a timeout of the same interval but a lower
	 *       priority. */
	priv->scroll_timeout_id = g_timeout_add_full (
		G_PRIORITY_LOW, 250, (GSourceFunc)
		mail_shell_content_scroll_timeout_cb,
		mail_shell_content, NULL);

	/* FIXME This is another ugly hack to hide a side-effect of the
	 *       previous workaround. */
	scrolled_window = GTK_SCROLLED_WINDOW (message_list);
	vscrollbar = gtk_scrolled_window_get_vscrollbar (scrolled_window);
	g_signal_connect_swapped (
		vscrollbar, "button-press-event",
		G_CALLBACK (mail_shell_content_etree_unfreeze),
		message_list);
}

static void
mail_shell_content_display_view_cb (EMailShellContent *mail_shell_content,
                                    GalView *gal_view)
{
	EMailReader *reader;
	MessageList *message_list;

	reader = E_MAIL_READER (mail_shell_content);
	message_list = e_mail_reader_get_message_list (reader);

	if (GAL_IS_VIEW_ETABLE (gal_view))
		gal_view_etable_attach_tree (
			GAL_VIEW_ETABLE (gal_view), message_list->tree);
}

static void
mail_shell_content_message_selected_cb (EMailShellContent *mail_shell_content,
                                        const gchar *message_uid,
                                        MessageList *message_list)
{
	EShellContent *shell_content;
	EShellView *shell_view;
	GKeyFile *key_file;
	const gchar *folder_uri;
	const gchar *key;
	gchar *group_name;

	folder_uri = message_list->folder_uri;

	/* This also gets triggered when selecting a store name on
	 * the sidebar such as "On This Computer", in which case
	 * 'folder_uri' will be NULL. */
	if (folder_uri == NULL)
		return;

	shell_content = E_SHELL_CONTENT (mail_shell_content);
	shell_view = e_shell_content_get_shell_view (shell_content);
	key_file = e_shell_view_get_state_key_file (shell_view);

	key = STATE_KEY_SELECTED_MESSAGE;
	group_name = g_strdup_printf ("Folder %s", folder_uri);

	if (message_uid != NULL)
		g_key_file_set_string (key_file, group_name, key, message_uid);
	else
		g_key_file_remove_key (key_file, group_name, key, NULL);
	e_shell_view_set_state_dirty (shell_view);

	g_free (group_name);
}

static void
mail_shell_content_set_property (GObject *object,
                                 guint property_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_PREVIEW_VISIBLE:
			e_mail_shell_content_set_preview_visible (
				E_MAIL_SHELL_CONTENT (object),
				g_value_get_boolean (value));
			return;

		case PROP_SHOW_DELETED:
			e_mail_shell_content_set_show_deleted (
				E_MAIL_SHELL_CONTENT (object),
				g_value_get_boolean (value));
			return;

		case PROP_VERTICAL_VIEW:
			e_mail_shell_content_set_vertical_view (
				E_MAIL_SHELL_CONTENT (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_shell_content_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_PREVIEW_VISIBLE:
			g_value_set_boolean (
				value,
				e_mail_shell_content_get_preview_visible (
				E_MAIL_SHELL_CONTENT (object)));
			return;

		case PROP_SHOW_DELETED:
			g_value_set_boolean (
				value,
				e_mail_shell_content_get_show_deleted (
				E_MAIL_SHELL_CONTENT (object)));
			return;

		case PROP_VERTICAL_VIEW:
			g_value_set_boolean (
				value,
				e_mail_shell_content_get_vertical_view (
				E_MAIL_SHELL_CONTENT (object)));
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_shell_content_dispose (GObject *object)
{
	EMailShellContentPrivate *priv;

	priv = E_MAIL_SHELL_CONTENT_GET_PRIVATE (object);

	if (priv->paned != NULL) {
		g_object_unref (priv->paned);
		priv->paned = NULL;
	}

	if (priv->message_list != NULL) {
		g_object_unref (priv->message_list);
		priv->message_list = NULL;
	}

	if (priv->search_bar != NULL) {
		g_object_unref (priv->search_bar);
		priv->search_bar = NULL;
	}

	if (priv->html_display != NULL) {
		g_object_unref (priv->html_display);
		priv->html_display = NULL;
	}

	if (priv->view_instance != NULL) {
		g_object_unref (priv->view_instance);
		priv->view_instance = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
mail_shell_content_constructed (GObject *object)
{
	EMailShellContentPrivate *priv;
	EShellContent *shell_content;
	EShellBackend *shell_backend;
	EShellView *shell_view;
	EShellViewClass *shell_view_class;
	EMailReader *reader;
	MessageList *message_list;
	GConfBridge *bridge;
	GtkWidget *container;
	GtkWidget *widget;
	GtkHTML *html;
	GalViewCollection *view_collection;
	const gchar *key;

	priv = E_MAIL_SHELL_CONTENT_GET_PRIVATE (object);
	priv->html_display = em_format_html_display_new ();

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (parent_class)->constructed (object);

	shell_content = E_SHELL_CONTENT (object);
	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_view_class = E_SHELL_VIEW_GET_CLASS (shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	view_collection = shell_view_class->view_collection;

	html = EM_FORMAT_HTML (priv->html_display)->html;

	/* Build content widgets. */

	container = GTK_WIDGET (object);

	widget = gtk_vpaned_new ();
	gtk_container_add (GTK_CONTAINER (container), widget);
	priv->paned = g_object_ref (widget);
	gtk_widget_show (widget);

	container = widget;

	widget = message_list_new (shell_backend);
	gtk_paned_add1 (GTK_PANED (container), widget);
	priv->message_list = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_vbox_new (FALSE, 1);
	gtk_paned_add2 (GTK_PANED (container), widget);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (widget), GTK_WIDGET (html));
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (GTK_WIDGET (html));
	gtk_widget_show (widget);

	widget = e_mail_search_bar_new (html);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	priv->search_bar = g_object_ref (widget);
	gtk_widget_hide (widget);

	g_signal_connect_swapped (
		widget, "changed",
		G_CALLBACK (em_format_redraw), priv->html_display);

	/* Load the view instance. */

	e_mail_shell_content_update_view_instance (
		E_MAIL_SHELL_CONTENT (shell_content));

	/* Bind GObject properties to GConf keys. */

	bridge = gconf_bridge_get ();

	object = G_OBJECT (priv->paned);
	key = "/apps/evolution/mail/display/paned_size";
	gconf_bridge_bind_property_delayed (bridge, key, object, "position");

	object = G_OBJECT (shell_content);
	key = "/apps/evolution/mail/display/show_deleted";
	gconf_bridge_bind_property (bridge, key, object, "show-deleted");

	/* Message list customizations. */

	reader = E_MAIL_READER (shell_content);
	message_list = e_mail_reader_get_message_list (reader);

	g_signal_connect_swapped (
		message_list, "message-selected",
		G_CALLBACK (mail_shell_content_message_selected_cb),
		shell_content);
}

static guint32
mail_shell_content_check_state (EShellContent *shell_content)
{
	return e_mail_reader_check_state (E_MAIL_READER (shell_content));
}

static GtkActionGroup *
mail_shell_content_get_action_group (EMailReader *reader)
{
	EShellContent *shell_content;
	EShellWindow *shell_window;
	EShellView *shell_view;

	shell_content = E_SHELL_CONTENT (reader);
	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_window = e_shell_view_get_shell_window (shell_view);

	return E_SHELL_WINDOW_ACTION_GROUP_MAIL (shell_window);
}

static gboolean
mail_shell_content_get_hide_deleted (EMailReader *reader)
{
	EMailShellContent *mail_shell_content;

	mail_shell_content = E_MAIL_SHELL_CONTENT (reader);

	return !e_mail_shell_content_get_show_deleted (mail_shell_content);
}

static EMFormatHTMLDisplay *
mail_shell_content_get_html_display (EMailReader *reader)
{
	EMailShellContentPrivate *priv;

	priv = E_MAIL_SHELL_CONTENT_GET_PRIVATE (reader);

	return priv->html_display;
}

static MessageList *
mail_shell_content_get_message_list (EMailReader *reader)
{
	EMailShellContentPrivate *priv;

	priv = E_MAIL_SHELL_CONTENT_GET_PRIVATE (reader);

	return MESSAGE_LIST (priv->message_list);
}

static EShellBackend *
mail_shell_content_get_shell_backend (EMailReader *reader)
{
	EShellContent *shell_content;
	EShellView *shell_view;

	shell_content = E_SHELL_CONTENT (reader);
	shell_view = e_shell_content_get_shell_view (shell_content);

	return e_shell_view_get_shell_backend (shell_view);
}

static GtkWindow *
mail_shell_content_get_window (EMailReader *reader)
{
	EShellContent *shell_content;
	EShellWindow *shell_window;
	EShellView *shell_view;

	shell_content = E_SHELL_CONTENT (reader);
	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_window = e_shell_view_get_shell_window (shell_view);

	return GTK_WINDOW (shell_window);
}

static void
mail_shell_content_set_folder (EMailReader *reader,
                               CamelFolder *folder,
                               const gchar *folder_uri)
{
	EMailShellContentPrivate *priv;
	EMailReaderIface *default_iface;
	MessageList *message_list;
	gboolean different_folder;

	priv = E_MAIL_SHELL_CONTENT_GET_PRIVATE (reader);

	message_list = e_mail_reader_get_message_list (reader);

	message_list_freeze (message_list);

	different_folder =
		message_list->folder != NULL &&
		folder != message_list->folder;

	/* Chain up to interface's default set_folder() method. */
	default_iface = g_type_default_interface_peek (E_TYPE_MAIL_READER);
	default_iface->set_folder (reader, folder, folder_uri);

	if (folder == NULL)
		goto exit;

	mail_refresh_folder (folder, NULL, NULL);

	/* This function gets triggered several times at startup,
	 * so we don't want to reset the message suppression state
	 * unless we're actually switching to a different folder. */
	if (different_folder)
		priv->suppress_message_selection = FALSE;

	/* This is a one-time-only callback. */
	if (message_list->cursor_uid == NULL && priv->message_list_built_id == 0)
		priv->message_list_built_id = g_signal_connect_swapped (
			message_list, "message-list-built",
			G_CALLBACK (mail_shell_content_message_list_built_cb),
			reader);

exit:
	message_list_thaw (message_list);
}

static void
mail_shell_content_show_search_bar (EMailReader *reader)
{
	EMailShellContentPrivate *priv;

	priv = E_MAIL_SHELL_CONTENT_GET_PRIVATE (reader);

	gtk_widget_show (priv->search_bar);
}

static void
mail_shell_content_class_init (EMailShellContentClass *class)
{
	GObjectClass *object_class;
	EShellContentClass *shell_content_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EMailShellContentPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_shell_content_set_property;
	object_class->get_property = mail_shell_content_get_property;
	object_class->dispose = mail_shell_content_dispose;
	object_class->constructed = mail_shell_content_constructed;

	shell_content_class = E_SHELL_CONTENT_CLASS (class);
	shell_content_class->new_search_context = em_search_context_new;
	shell_content_class->check_state = mail_shell_content_check_state;

	g_object_class_install_property (
		object_class,
		PROP_PREVIEW_VISIBLE,
		g_param_spec_boolean (
			"preview-visible",
			_("Preview is Visible"),
			_("Whether the preview pane is visible"),
			TRUE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_DELETED,
		g_param_spec_boolean (
			"show-deleted",
			"Show Deleted",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_VERTICAL_VIEW,
		g_param_spec_boolean (
			"vertical-view",
			_("Vertical View"),
			_("Whether vertical view is enabled"),
			FALSE,
			G_PARAM_READWRITE));
}

static void
mail_shell_content_iface_init (EMailReaderIface *iface)
{
	iface->get_action_group = mail_shell_content_get_action_group;
	iface->get_hide_deleted = mail_shell_content_get_hide_deleted;
	iface->get_html_display = mail_shell_content_get_html_display;
	iface->get_message_list = mail_shell_content_get_message_list;
	iface->get_shell_backend = mail_shell_content_get_shell_backend;
	iface->get_window = mail_shell_content_get_window;
	iface->set_folder = mail_shell_content_set_folder;
	iface->show_search_bar = mail_shell_content_show_search_bar;
}

static void
mail_shell_content_init (EMailShellContent *mail_shell_content)
{
	mail_shell_content->priv =
		E_MAIL_SHELL_CONTENT_GET_PRIVATE (mail_shell_content);

	mail_shell_content->priv->preview_visible = TRUE;

	/* Postpone widget construction until we have a shell view. */
}

GType
e_mail_shell_content_get_type (void)
{
	return mail_shell_content_type;
}

void
e_mail_shell_content_register_type (GTypeModule *type_module)
{
	static const GTypeInfo type_info = {
		sizeof (EMailShellContentClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) mail_shell_content_class_init,
		(GClassFinalizeFunc) NULL,
		NULL,  /* class_data */
		sizeof (EMailShellContent),
		0,     /* n_preallocs */
		(GInstanceInitFunc) mail_shell_content_init,
		NULL   /* value_table */
	};

	static const GInterfaceInfo iface_info = {
		(GInterfaceInitFunc) mail_shell_content_iface_init,
		(GInterfaceFinalizeFunc) NULL,
		NULL  /* interface_data */
	};

	mail_shell_content_type = g_type_module_register_type (
		type_module, E_TYPE_SHELL_CONTENT,
		"EMailShellContent", &type_info, 0);

	g_type_module_add_interface (
		type_module, mail_shell_content_type,
		E_TYPE_MAIL_READER, &iface_info);
}

GtkWidget *
e_mail_shell_content_new (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return g_object_new (
		E_TYPE_MAIL_SHELL_CONTENT,
		"shell-view", shell_view, NULL);
}

gboolean
e_mail_shell_content_get_preview_visible (EMailShellContent *mail_shell_content)
{
	g_return_val_if_fail (
		E_IS_MAIL_SHELL_CONTENT (mail_shell_content), FALSE);

	return mail_shell_content->priv->preview_visible;
}

void
e_mail_shell_content_set_preview_visible (EMailShellContent *mail_shell_content,
                                          gboolean preview_visible)
{
	GtkPaned *paned;
	GtkWidget *child;

	g_return_if_fail (E_IS_MAIL_SHELL_CONTENT (mail_shell_content));

	if (preview_visible == mail_shell_content->priv->preview_visible)
		return;

	paned = GTK_PANED (mail_shell_content->priv->paned);
	child = gtk_paned_get_child2 (paned);

	if (preview_visible)
		gtk_widget_show (child);
	else
		gtk_widget_hide (child);

	mail_shell_content->priv->preview_visible = preview_visible;

	g_object_notify (G_OBJECT (mail_shell_content), "preview-visible");
}

gboolean
e_mail_shell_content_get_show_deleted (EMailShellContent *mail_shell_content)
{
	g_return_val_if_fail (
		E_IS_MAIL_SHELL_CONTENT (mail_shell_content), FALSE);

	return mail_shell_content->priv->show_deleted;
}

void
e_mail_shell_content_set_show_deleted (EMailShellContent *mail_shell_content,
                                       gboolean show_deleted)
{
	g_return_if_fail (E_IS_MAIL_SHELL_CONTENT (mail_shell_content));

	mail_shell_content->priv->show_deleted = show_deleted;

	g_object_notify (G_OBJECT (mail_shell_content), "show-deleted");
}

gboolean
e_mail_shell_content_get_vertical_view (EMailShellContent *mail_shell_content)
{
	g_return_val_if_fail (
		E_IS_MAIL_SHELL_CONTENT (mail_shell_content), FALSE);

	return mail_shell_content->priv->vertical_view;
}

void
e_mail_shell_content_set_vertical_view (EMailShellContent *mail_shell_content,
                                        gboolean vertical_view)
{
	GConfBridge *bridge;
	GtkWidget *old_paned;
	GtkWidget *new_paned;
	GtkWidget *child1;
	GtkWidget *child2;
	guint binding_id;
	const gchar *key;

	g_return_if_fail (E_IS_MAIL_SHELL_CONTENT (mail_shell_content));

	if (vertical_view == mail_shell_content->priv->vertical_view)
		return;

	bridge = gconf_bridge_get ();
	old_paned = mail_shell_content->priv->paned;
	binding_id = mail_shell_content->priv->paned_binding_id;

	child1 = gtk_paned_get_child1 (GTK_PANED (old_paned));
	child2 = gtk_paned_get_child2 (GTK_PANED (old_paned));

	if (binding_id > 0)
		gconf_bridge_unbind (bridge, binding_id);

	if (vertical_view) {
		new_paned = gtk_hpaned_new ();
		key = "/apps/evolution/mail/display/hpaned_size";
	} else {
		new_paned = gtk_vpaned_new ();
		key = "/apps/evolution/mail/display/paned_size";
	}

	gtk_widget_reparent (child1, new_paned);
	gtk_widget_reparent (child2, new_paned);
	gtk_widget_show (new_paned);

	gtk_widget_destroy (old_paned);
	gtk_container_add (GTK_CONTAINER (mail_shell_content), new_paned);

	binding_id = gconf_bridge_bind_property_delayed (
		bridge, key, G_OBJECT (new_paned), "position");

	mail_shell_content->priv->vertical_view = vertical_view;
	mail_shell_content->priv->paned_binding_id = binding_id;
	mail_shell_content->priv->paned = g_object_ref (new_paned);

	e_mail_shell_content_update_view_instance (mail_shell_content);

	g_object_notify (G_OBJECT (mail_shell_content), "vertical-view");
}

GalViewInstance *
e_mail_shell_content_get_view_instance (EMailShellContent *mail_shell_content)
{
	g_return_val_if_fail (
		E_IS_MAIL_SHELL_CONTENT (mail_shell_content), NULL);

	return mail_shell_content->priv->view_instance;
}

void
e_mail_shell_content_set_search_strings (EMailShellContent *mail_shell_content,
                                         GSList *search_strings)
{
	EMailSearchBar *search_bar;
	ESearchingTokenizer *tokenizer;

	g_return_if_fail (E_IS_MAIL_SHELL_CONTENT (mail_shell_content));

	search_bar = E_MAIL_SEARCH_BAR (mail_shell_content->priv->search_bar);
	tokenizer = e_mail_search_bar_get_tokenizer (search_bar);

	e_searching_tokenizer_set_secondary_case_sensitivity (tokenizer, FALSE);
	e_searching_tokenizer_set_secondary_search_string (tokenizer, NULL);

	while (search_strings != NULL) {
		e_searching_tokenizer_add_secondary_search_string (
			tokenizer, search_strings->data);
		search_strings = g_slist_next (search_strings);
	}

	e_mail_search_bar_changed (search_bar);
}

void
e_mail_shell_content_update_view_instance (EMailShellContent *mail_shell_content)
{
	EMailReader *reader;
	EShellContent *shell_content;
	EShellView *shell_view;
	EShellViewClass *shell_view_class;
	GalViewCollection *view_collection;
	GalViewInstance *view_instance;
	MessageList *message_list;
	gboolean outgoing_folder;
	gboolean show_vertical_view;
	gchar *view_id;

	g_return_if_fail (E_IS_MAIL_SHELL_CONTENT (mail_shell_content));

	shell_content = E_SHELL_CONTENT (mail_shell_content);
	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_view_class = E_SHELL_VIEW_GET_CLASS (shell_view);
	view_collection = shell_view_class->view_collection;

	reader = E_MAIL_READER (mail_shell_content);
	message_list = e_mail_reader_get_message_list (reader);

	/* If no folder is selected, return silently. */
	if (message_list->folder == NULL)
		return;

	/* If we have a folder, we should also have a URI. */
	g_return_if_fail (message_list->folder_uri != NULL);

	if (mail_shell_content->priv->view_instance != NULL) {
		g_object_unref (mail_shell_content->priv->view_instance);
		mail_shell_content->priv->view_instance = NULL;
	}

	view_id = mail_config_folder_to_safe_url (message_list->folder);
	view_instance = e_shell_view_new_view_instance (shell_view, view_id);
	mail_shell_content->priv->view_instance = view_instance;

	show_vertical_view =
		e_mail_shell_content_get_vertical_view (mail_shell_content);

	if (show_vertical_view) {
		gchar *filename;
		gchar *safe_view_id;

		/* Force the view instance into vertical view. */

		g_free (view_instance->custom_filename);
		g_free (view_instance->current_view_filename);

		safe_view_id = g_strdup (view_id);
		e_filename_make_safe (safe_view_id);

		filename = g_strdup_printf (
			"custom_wide_view-%s.xml", safe_view_id);
		view_instance->custom_filename = g_build_filename (
			view_collection->local_dir, filename, NULL);
		g_free (filename);

		filename = g_strdup_printf (
			"current_wide_view-%s.xml", safe_view_id);
		view_instance->current_view_filename = g_build_filename (
			view_collection->local_dir, filename, NULL);
		g_free (filename);

		g_free (safe_view_id);
	}

	g_free (view_id);

	outgoing_folder =
		em_utils_folder_is_drafts (
			message_list->folder, message_list->folder_uri) ||
		em_utils_folder_is_outbox (
			message_list->folder, message_list->folder_uri) ||
		em_utils_folder_is_sent (
			message_list->folder, message_list->folder_uri);

	if (outgoing_folder) {
		if (show_vertical_view)
			gal_view_instance_set_default_view (
				view_instance, "Wide_View_Sent");
		else
			gal_view_instance_set_default_view (
				view_instance, "As_Sent_Folder");
	} else if (show_vertical_view) {
		gal_view_instance_set_default_view (
			view_instance, "Wide_View_Normal");
	}

	gal_view_instance_load (view_instance);

	if (!gal_view_instance_exists (view_instance)) {
		gchar *state_filename;

		state_filename = mail_config_folder_to_cachename (
			message_list->folder, "et-header-");

		if (g_file_test (state_filename, G_FILE_TEST_IS_REGULAR)) {
			ETableSpecification *spec;
			ETableState *state;
			GalView *view;
			gchar *spec_filename;

			spec = e_table_specification_new ();
			spec_filename = g_build_filename (
				EVOLUTION_ETSPECDIR,
				"message-list.etspec",
				NULL);
			e_table_specification_load_from_file (
				spec, spec_filename);
			g_free (spec_filename);

			state = e_table_state_new ();
			view = gal_view_etable_new (spec, "");

			e_table_state_load_from_file (
				state, state_filename);
			gal_view_etable_set_state (
				GAL_VIEW_ETABLE (view), state);
			gal_view_instance_set_custom_view (
				view_instance, view);

			g_object_unref (state);
			g_object_unref (view);
			g_object_unref (spec);
		}

		g_free (state_filename);
	}

	g_signal_connect (
		view_instance, "display-view",
		G_CALLBACK (mail_shell_content_display_view_cb),
		mail_shell_content);

	mail_shell_content_display_view_cb (
		mail_shell_content,
		gal_view_instance_get_current_view (view_instance));
}
