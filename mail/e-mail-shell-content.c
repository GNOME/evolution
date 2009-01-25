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
#include "e-mail-shell-module.h"

#define E_MAIL_SHELL_CONTENT_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_SHELL_CONTENT, EMailShellContentPrivate))

struct _EMailShellContentPrivate {
	GtkWidget *paned;
	GtkWidget *message_list;

	EMFormatHTMLDisplay *html_display;
	GalViewInstance *view_instance;

	gchar *selected_uid;

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
};

enum {
	PROP_0,
	PROP_PREVIEW_VISIBLE,
	PROP_VERTICAL_VIEW
};

static gpointer parent_class;

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
	const gchar *key;
	gdouble position;
	gchar *value;

	/* Save the scrollbar position for the current folder. */

	if (message_list->folder == NULL)
		return;

	key = "evolution:list_scroll_position";
	position = message_list_get_scrollbar_position (message_list);
	value = g_strdup_printf ("%f", position);

	if (camel_object_meta_set (message_list->folder, key, value))
		camel_object_state_write (message_list->folder);

	g_free (value);
}

static gboolean
mail_shell_content_scroll_timeout_cb (EMailShellContent *mail_shell_content)
{
	EMailShellContentPrivate *priv = mail_shell_content->priv;
	MessageList *message_list;
	EMailReader *reader;
	const gchar *key;
	gdouble position;
	gchar *value;

	/* Initialize the scrollbar position for the current folder
	 * and setup a callback to handle scrollbar position changes. */

	reader = E_MAIL_READER (mail_shell_content);
	message_list = e_mail_reader_get_message_list (reader);
	position = priv->default_scrollbar_position;

	key = "evolution:list_scroll_position";
	value = camel_object_meta_get (message_list->folder, key);

	if (value != NULL) {
		position = strtod (value, NULL);
		g_free (value);
	}

	message_list_set_scrollbar_position (message_list, position);

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
	GtkScrolledWindow *scrolled_window;
	GtkWidget *vscrollbar;
	gdouble position = 0.0;

	g_signal_handler_disconnect (
		message_list, priv->message_list_built_id);
	priv->message_list_built_id = 0;

	if (message_list->cursor_uid == NULL && priv->selected_uid != NULL) {
		CamelMessageInfo *info;

		/* If the message isn't in the folder yet, keep selected_uid
		 * around, as it could be caught by a set_folder() at some
		 * later date. */
		info = camel_folder_get_message_info (
			message_list->folder, priv->selected_uid);
		if (info != NULL) {
			camel_folder_free_message_info (
				message_list->folder, info);
			e_mail_reader_set_message (
				E_MAIL_READER (mail_shell_content),
				priv->selected_uid, TRUE);
			g_free (priv->selected_uid);
			priv->selected_uid = NULL;
		}

		position = message_list_get_scrollbar_position (message_list);
	}

	priv->default_scrollbar_position = position;

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
                                        const gchar *selected_uid,
                                        MessageList *message_list)
{
	const gchar *key = "evolution:selected_uid";
	CamelFolder *folder;

	folder = message_list->folder;

	/* This also gets triggered when selecting a store name on
	 * the sidebar such as "On This Computer", in which case
	 * 'folder' will be NULL. */
	if (folder == NULL)
		return;

	if (camel_object_meta_set (folder, key, selected_uid))
		camel_object_state_write (folder);

	g_free (mail_shell_content->priv->selected_uid);
	mail_shell_content->priv->selected_uid = NULL;
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
mail_shell_content_finalize (GObject *object)
{
	EMailShellContentPrivate *priv;

	priv = E_MAIL_SHELL_CONTENT_GET_PRIVATE (object);

	g_free (priv->selected_uid);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
mail_shell_content_constructed (GObject *object)
{
	EMailShellContentPrivate *priv;
	EShellContent *shell_content;
	EShellView *shell_view;
	EShellViewClass *shell_view_class;
	EMailReader *reader;
	MessageList *message_list;
	GConfBridge *bridge;
	GtkWidget *container;
	GtkWidget *widget;
	GalViewCollection *view_collection;
	const gchar *key;

	priv = E_MAIL_SHELL_CONTENT_GET_PRIVATE (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (parent_class)->constructed (object);

	shell_content = E_SHELL_CONTENT (object);
	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_view_class = E_SHELL_VIEW_GET_CLASS (shell_view);
	view_collection = shell_view_class->view_collection;

	/* Build content widgets. */

	container = GTK_WIDGET (object);

	widget = gtk_vpaned_new ();
	gtk_container_add (GTK_CONTAINER (container), widget);
	priv->paned = g_object_ref (widget);
	gtk_widget_show (widget);

	container = widget;

	widget = message_list_new ();
	gtk_paned_add1 (GTK_PANED (container), widget);
	priv->message_list = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_paned_add2 (GTK_PANED (container), widget);
	gtk_widget_show (widget);

	container = widget;

	priv->html_display = em_format_html_display_new ();
	widget = GTK_WIDGET (((EMFormatHTML *) priv->html_display)->html);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	/* Load the view instance. */

	e_mail_shell_content_update_view_instance (
		E_MAIL_SHELL_CONTENT (shell_content));

	/* Bind GObject properties to GConf keys. */

	bridge = gconf_bridge_get ();

	object = G_OBJECT (priv->paned);
	key = "/apps/evolution/mail/display/paned_size";
	gconf_bridge_bind_property_delayed (bridge, key, object, "position");

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

	return e_shell_window_get_action_group (shell_window, "mail");
}

static gboolean
mail_shell_content_get_hide_deleted (EMailReader *reader)
{
	/* FIXME */
	return TRUE;
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

static EShellModule *
mail_shell_content_get_shell_module (EMailReader *reader)
{
	EShellContent *shell_content;
	EShellView *shell_view;

	shell_content = E_SHELL_CONTENT (reader);
	shell_view = e_shell_content_get_shell_view (shell_content);

	return e_shell_view_get_shell_module (shell_view);
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
	gchar *meta_data;

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

	if (!priv->suppress_message_selection)
		meta_data = camel_object_meta_get (
			folder, "evolution:selected_uid");
	else
		meta_data = NULL;

	g_free (priv->selected_uid);
	priv->selected_uid = meta_data;

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
	object_class->finalize = mail_shell_content_finalize;
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
	iface->get_shell_module = mail_shell_content_get_shell_module;
	iface->get_window = mail_shell_content_get_window;
	iface->set_folder = mail_shell_content_set_folder;
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
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
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

		type = g_type_register_static (
			E_TYPE_SHELL_CONTENT, "EMailShellContent",
			&type_info, 0);

		g_type_add_interface_static (
			type, E_TYPE_MAIL_READER, &iface_info);
	}

	return type;
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
