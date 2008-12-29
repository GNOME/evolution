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

#include "e-mail-reader.h"
#include "e-mail-shell-module.h"

#define E_MAIL_SHELL_CONTENT_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_SHELL_CONTENT, EMailShellContentPrivate))

struct _EMailShellContentPrivate {
	GtkWidget *paned;
	GtkWidget *folder_view;

	EMFormatHTMLDisplay *preview;
	GalViewInstance *view_instance;

	guint paned_binding_id;

	guint preview_visible	: 1;
	guint vertical_view	: 1;
};

enum {
	PROP_0,
	PROP_PREVIEW_VISIBLE,
	PROP_VERTICAL_VIEW
};

static gpointer parent_class;

static void
mail_shell_content_display_view_cb (EMailShellContent *mail_shell_content,
                                    GalView *gal_view)
{
	EMFolderView *folder_view;

	folder_view = e_mail_shell_content_get_folder_view (mail_shell_content);

	if (GAL_IS_VIEW_ETABLE (gal_view))
		gal_view_etable_attach_tree (
			GAL_VIEW_ETABLE (gal_view),
			folder_view->list->tree);
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

	if (priv->folder_view != NULL) {
		g_object_unref (priv->folder_view);
		priv->folder_view = NULL;
	}

	if (priv->preview != NULL) {
		g_object_unref (priv->preview);
		priv->preview = NULL;
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

	/* XXX Kill EMFolderView? */
	priv->folder_view = em_folder_view_new ();
	g_object_ref_sink (priv->folder_view);
	widget = GTK_WIDGET (((EMFolderView *) priv->folder_view)->list);
	gtk_paned_add1 (GTK_PANED (container), widget);
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

	/* XXX Kill EMFolderView? */
	priv->preview = ((EMFolderView *) priv->folder_view)->preview;
	widget = GTK_WIDGET (((EMFormatHTML *) priv->preview)->html);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	/* Load the view instance. */

	e_mail_shell_content_update_view_instance (
		E_MAIL_SHELL_CONTENT (object));

	/* Bind GObject properties to GConf keys. */

	bridge = gconf_bridge_get ();

	object = G_OBJECT (priv->paned);
	key = "/apps/evolution/mail/display/paned_size";
	gconf_bridge_bind_property_delayed (bridge, key, object, "position");
}

static guint32
mail_shell_content_check_state (EShellContent *shell_content)
{
	EMailShellContent *mail_shell_content;
	EMFolderView *folder_view;
	GPtrArray *uids;
	CamelFolder *folder;
	CamelStore *store;
	const gchar *folder_uri;
	const gchar *tag;
	gboolean can_clear_flags = FALSE;
	gboolean can_flag_completed = FALSE;
	gboolean can_flag_for_followup = FALSE;
	gboolean has_deleted = FALSE;
	gboolean has_important = FALSE;
	gboolean has_junk = FALSE;
	gboolean has_not_junk = FALSE;
	gboolean has_read = FALSE;
	gboolean has_undeleted = FALSE;
	gboolean has_unimportant = FALSE;
	gboolean has_unread = FALSE;
	gboolean draft_or_outbox;
	guint32 state = 0;
	guint ii;

	mail_shell_content = E_MAIL_SHELL_CONTENT (shell_content);
	folder_view = e_mail_shell_content_get_folder_view (mail_shell_content);
	uids = message_list_get_selected (folder_view->list);
	folder_uri = folder_view->folder_uri;
	folder = folder_view->folder;
	store = CAMEL_STORE (folder->parent_store);

	draft_or_outbox =
		em_utils_folder_is_drafts (folder, folder_uri) ||
		em_utils_folder_is_outbox (folder, folder_uri);
	if (!draft_or_outbox) {
		has_junk = !(store->flags & CAMEL_STORE_VJUNK);
		has_not_junk = TRUE;
	}

	for (ii = 0; ii < uids->len; ii++) {
		CamelMessageInfo *info;
		guint32 flags;

		info = camel_folder_get_message_info (
			folder, uids->pdata[ii]);
		if (info == NULL)
			continue;

		flags = camel_message_info_flags (info);

		if (flags & CAMEL_MESSAGE_SEEN)
			has_read = TRUE;
		else
			has_unread = TRUE;

		if (flags & CAMEL_MESSAGE_DELETED)
			has_deleted = TRUE;
		else
			has_undeleted = TRUE;

		if (flags & CAMEL_MESSAGE_FLAGGED)
			has_important = TRUE;
		else
			has_unimportant = TRUE;

		tag = camel_message_info_user_tag (info, "follow-up");
		if (tag != NULL && *tag != '\0') {
			can_clear_flags = TRUE;
			tag = camel_message_info_user_tag (
				info, "completed-on");
			if (tag != NULL && *tag != '\0')
				can_flag_completed = TRUE;
		} else
			can_flag_for_followup = TRUE;
	}

	if (uids->len == 1)
		state |= E_MAIL_SHELL_CONTENT_SELECTION_SINGLE;
	if (uids->len > 1)
		state |= E_MAIL_SHELL_CONTENT_SELECTION_MULTIPLE;
	if (!draft_or_outbox && uids->len == 1)
		state |= E_MAIL_SHELL_CONTENT_SELECTION_CAN_ADD_SENDER;
#if 0  /* FIXME */
	if (can_edit)
		state |= E_MAIL_SHELL_CONTENT_SELECTION_CAN_EDIT;
#endif
	if (can_clear_flags)
		state |= E_MAIL_SHELL_CONTENT_SELECTION_FLAG_CLEAR;
	if (can_flag_completed)
		state |= E_MAIL_SHELL_CONTENT_SELECTION_FLAG_COMPLETED;
	if (can_flag_for_followup)
		state |= E_MAIL_SHELL_CONTENT_SELECTION_FLAG_FOLLOWUP;
	if (has_deleted)
		state |= E_MAIL_SHELL_CONTENT_SELECTION_HAS_DELETED;
	if (has_important)
		state |= E_MAIL_SHELL_CONTENT_SELECTION_HAS_IMPORTANT;
	if (has_junk)
		state |= E_MAIL_SHELL_CONTENT_SELECTION_HAS_JUNK;
	if (has_not_junk)
		state |= E_MAIL_SHELL_CONTENT_SELECTION_HAS_NOT_JUNK;
	if (has_read)
		state |= E_MAIL_SHELL_CONTENT_SELECTION_HAS_READ;
	if (has_undeleted)
		state |= E_MAIL_SHELL_CONTENT_SELECTION_HAS_UNDELETED;
	if (has_unimportant)
		state |= E_MAIL_SHELL_CONTENT_SELECTION_HAS_UNIMPORTANT;
	if (has_unread)
		state |= E_MAIL_SHELL_CONTENT_SELECTION_HAS_UNREAD;
#if 0  /* FIXME */
	if (has_callto_uri)
		state |= E_MAIL_SHELL_CONTENT_SELECTION_HAS_URI_CALLTO;
	if (has_http_uri)
		state |= E_MAIL_SHELL_CONTENT_SELECTION_HAS_URI_HTTP;
	if (has_mailto_uri)
		state |= E_MAIL_SHELL_CONTENT_SELECTION_HAS_URI_MAILTO;
	if (is_mailing_list)
		state |= E_MAIL_SHELL_CONTENT_SELECTION_IS_MAILING_LIST;
#endif

	em_utils_uids_free (uids);

	return state;
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

static CamelFolder *
mail_shell_content_get_folder (EMailReader *reader)
{
	EMailShellContent *mail_shell_content;
	EMFolderView *folder_view;

	mail_shell_content = E_MAIL_SHELL_CONTENT (reader);
	folder_view = e_mail_shell_content_get_folder_view (mail_shell_content);

	return folder_view->folder;
}

static const gchar *
mail_shell_content_get_folder_uri (EMailReader *reader)
{
	EMailShellContent *mail_shell_content;
	EMFolderView *folder_view;

	mail_shell_content = E_MAIL_SHELL_CONTENT (reader);
	folder_view = e_mail_shell_content_get_folder_view (mail_shell_content);

	return folder_view->folder_uri;
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
	EMailShellContent *mail_shell_content;

	mail_shell_content = E_MAIL_SHELL_CONTENT (reader);

	return e_mail_shell_content_get_preview_format (mail_shell_content);
}

static MessageList *
mail_shell_content_get_message_list (EMailReader *reader)
{
	EMailShellContent *mail_shell_content;
	EMFolderView *folder_view;

	mail_shell_content = E_MAIL_SHELL_CONTENT (reader);
	folder_view = e_mail_shell_content_get_folder_view (mail_shell_content);

	return folder_view->list;
}

static EMFolderTreeModel *
mail_shell_content_get_tree_model (EMailReader *reader)
{
	EShellContent *shell_content;
	EShellModule *shell_module;
	EShellView *shell_view;

	shell_content = E_SHELL_CONTENT (reader);
	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_module = e_shell_view_get_shell_module (shell_view);

	return e_mail_shell_module_get_folder_tree_model (shell_module);
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
	iface->get_folder = mail_shell_content_get_folder;
	iface->get_folder_uri = mail_shell_content_get_folder_uri;
	iface->get_hide_deleted = mail_shell_content_get_hide_deleted;
	iface->get_html_display = mail_shell_content_get_html_display;
	iface->get_message_list = mail_shell_content_get_message_list;
	iface->get_tree_model = mail_shell_content_get_tree_model;
	iface->get_window = mail_shell_content_get_window;
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

EMFolderView *
e_mail_shell_content_get_folder_view (EMailShellContent *mail_shell_content)
{
	g_return_val_if_fail (
		E_IS_MAIL_SHELL_CONTENT (mail_shell_content), NULL);

	return EM_FOLDER_VIEW (mail_shell_content->priv->folder_view);
}

EMFormatHTMLDisplay *
e_mail_shell_content_get_preview_format (EMailShellContent *mail_shell_content)
{
	g_return_val_if_fail (
		E_IS_MAIL_SHELL_CONTENT (mail_shell_content), NULL);

	return mail_shell_content->priv->preview;
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

GtkWidget *
e_mail_shell_content_get_preview_widget (EMailShellContent *mail_shell_content)
{
	EMFormatHTML *format;

	g_return_val_if_fail (
		E_IS_MAIL_SHELL_CONTENT (mail_shell_content), NULL);

	format = (EMFormatHTML *) mail_shell_content->priv->preview;

	return GTK_WIDGET (format->html);
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

	g_object_notify (G_OBJECT (mail_shell_content), "vertical-view");
}

void
e_mail_shell_content_update_view_instance (EMailShellContent *mail_shell_content)
{
	EShellContent *shell_content;
	EShellView *shell_view;
	EShellViewClass *shell_view_class;
	GalViewCollection *view_collection;
	GalViewInstance *view_instance;
	EMFolderView *folder_view;
	gboolean outgoing_folder;
	gboolean show_vertical_view;
	gchar *view_id;

	g_return_if_fail (E_IS_MAIL_SHELL_CONTENT (mail_shell_content));

	shell_content = E_SHELL_CONTENT (mail_shell_content);
	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_view_class = E_SHELL_VIEW_GET_CLASS (shell_view);
	view_collection = shell_view_class->view_collection;

	folder_view = e_mail_shell_content_get_folder_view (mail_shell_content);

	/* If no folder is selected, return silently. */
	if (folder_view->folder == NULL)
		return;

	/* If we have a folder, we should also have a URI. */
	g_return_if_fail (folder_view->folder_uri != NULL);

	if (mail_shell_content->priv->view_instance != NULL) {
		g_object_unref (mail_shell_content->priv->view_instance);
		mail_shell_content->priv->view_instance = NULL;
	}

	/* TODO: Should this go through the mail-config API? */
	view_id = mail_config_folder_to_safe_url (folder_view->folder);
	view_instance = e_shell_view_new_view_instance (shell_view, view_id);
	mail_shell_content->priv->view_instance = view_instance;

	show_vertical_view = folder_view->list_active &&
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
			folder_view->folder, folder_view->folder_uri) ||
		em_utils_folder_is_outbox (
			folder_view->folder, folder_view->folder_uri) ||
		em_utils_folder_is_sent (
			folder_view->folder, folder_view->folder_uri);

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
			folder_view->folder, "et-header-");

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
