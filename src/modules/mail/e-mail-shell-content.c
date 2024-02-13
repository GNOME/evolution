/*
 * e-mail-shell-content.c
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

#include "e-mail-shell-content.h"

#include <glib/gi18n.h>

#include <e-util/e-util-private.h>

#include "calendar/gui/e-to-do-pane.h"

#include <mail/e-mail-paned-view.h>
#include <mail/e-mail-reader.h>
#include <mail/e-mail-reader-utils.h>
#include <mail/em-utils.h>
#include <mail/message-list.h>

#include "e-mail-shell-backend.h"
#include "e-mail-shell-view-actions.h"

struct _EMailShellContentPrivate {
	EMailView *mail_view;
	GtkWidget *to_do_pane; /* not referenced */
};

enum {
	PROP_0,
	PROP_FORWARD_STYLE,
	PROP_GROUP_BY_THREADS,
	PROP_MAIL_VIEW,
	PROP_REPLY_STYLE,
	PROP_MARK_SEEN_ALWAYS,
	PROP_TO_DO_PANE,
	PROP_DELETE_SELECTS_PREVIOUS
};

/* Forward Declarations */
static void	e_mail_shell_content_reader_init
					(EMailReaderInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (EMailShellContent, e_mail_shell_content, E_TYPE_SHELL_CONTENT, 0,
	G_ADD_PRIVATE_DYNAMIC (EMailShellContent)
	G_IMPLEMENT_INTERFACE_DYNAMIC (E_TYPE_MAIL_READER, e_mail_shell_content_reader_init))

static gboolean
mail_shell_content_transform_num_attachments_to_visible_boolean_with_settings (GBinding *binding,
									       const GValue *from_value,
									       GValue *to_value,
									       gpointer user_data)
{
	GSettings *settings;
	gboolean res = TRUE;

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	if (g_settings_get_boolean (settings, "show-attachment-bar"))
		res = e_attachment_store_transform_num_attachments_to_visible_boolean (binding, from_value, to_value, user_data);
	else
		g_value_set_boolean (to_value, FALSE);

	g_clear_object (&settings);

	return res;
}

static void
reconnect_changed_event (EMailReader *child,
                         EMailReader *parent)
{
	g_signal_emit_by_name (parent, "changed");
}

static void
reconnect_folder_loaded_event (EMailReader *child,
               EMailReader *parent)
{
	g_signal_emit_by_name (parent, "folder-loaded");
}

/* To recognize old values from new values */
#define PROPORTION_LOWER_LIMIT 1000000

static gboolean
mail_shell_content_map_setting_to_proportion_cb (GValue *value,
						 GVariant *variant,
						 gpointer user_data)
{
	gint stored;
	gdouble proportion = 0.15;

	stored = g_variant_get_int32 (variant);

	if (stored >= PROPORTION_LOWER_LIMIT)
		proportion = (stored - PROPORTION_LOWER_LIMIT) / ((gdouble) PROPORTION_LOWER_LIMIT);

	g_value_set_double (value, proportion);

	return TRUE;
}

static GVariant *
mail_shell_content_map_proportion_to_setting_cb (const GValue *value,
						 const GVariantType *expected_type,
						 gpointer user_data)
{
	gdouble proportion;

	proportion = g_value_get_double (value);

	return g_variant_new_int32 (PROPORTION_LOWER_LIMIT + (gint32) (proportion * PROPORTION_LOWER_LIMIT));
}

static void
mail_shell_content_set_property (GObject *object,
                                 guint property_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
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
		case PROP_FORWARD_STYLE:
			g_value_set_enum (
				value, e_mail_reader_get_forward_style (
				E_MAIL_READER (object)));
			return;

		case PROP_GROUP_BY_THREADS:
			g_value_set_boolean (
				value, e_mail_reader_get_group_by_threads (
				E_MAIL_READER (object)));
			return;

		case PROP_MAIL_VIEW:
			g_value_set_object (
				value, e_mail_shell_content_get_mail_view (
				E_MAIL_SHELL_CONTENT (object)));
			return;

		case PROP_REPLY_STYLE:
			g_value_set_enum (
				value, e_mail_reader_get_reply_style (
				E_MAIL_READER (object)));
			return;

		case PROP_MARK_SEEN_ALWAYS:
			g_value_set_boolean (
				value, e_mail_reader_get_mark_seen_always (
				E_MAIL_READER (object)));
			return;

		case PROP_TO_DO_PANE:
			g_value_set_object (
				value, e_mail_shell_content_get_to_do_pane (
				E_MAIL_SHELL_CONTENT (object)));
			return;

		case PROP_DELETE_SELECTS_PREVIOUS:
			g_value_set_boolean (
				value, e_mail_reader_get_delete_selects_previous (
				E_MAIL_READER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_shell_content_dispose (GObject *object)
{
	EMailShellContent *self = E_MAIL_SHELL_CONTENT (object);

	g_clear_object (&self->priv->mail_view);

	/* Intentionally after freeing the mail_view, because
	   the widgets it contains/references can be freed already */
	e_mail_reader_dispose (E_MAIL_READER (object));

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_shell_content_parent_class)->dispose (object);
}

static void
mail_shell_content_constructed (GObject *object)
{
	EMailShellContent *self = E_MAIL_SHELL_CONTENT (object);
	EShellContent *shell_content;
	EShellView *shell_view;
	EAttachmentStore *attachment_store;
	EMailDisplay *display;
	GtkPaned *paned;
	GtkWidget *widget;
	GtkBox *vbox;
	GSettings *settings;

	/* Chain up to parent's constructed () method. */
	G_OBJECT_CLASS (e_mail_shell_content_parent_class)->constructed (object);

	shell_content = E_SHELL_CONTENT (object);
	shell_view = e_shell_content_get_shell_view (shell_content);

	/* Build content widgets. */

	widget = e_paned_new (GTK_ORIENTATION_HORIZONTAL);
	e_paned_set_fixed_resize (E_PANED (widget), FALSE);
	gtk_container_add (GTK_CONTAINER (shell_content), widget);
	gtk_widget_show (widget);

	paned = GTK_PANED (widget);

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
	gtk_paned_pack1 (paned, widget, TRUE, FALSE);
	gtk_widget_show (widget);

	vbox = GTK_BOX (widget);

	widget = e_mail_paned_view_new (shell_view);
	gtk_box_pack_start (vbox, widget, TRUE, TRUE, 0);

	self->priv->mail_view = E_MAIL_VIEW (g_object_ref (widget));
	gtk_widget_show (widget);

	g_signal_connect (
		widget, "changed",
		G_CALLBACK (reconnect_changed_event), object);
	g_signal_connect (
		widget, "folder-loaded",
		G_CALLBACK (reconnect_folder_loaded_event), object);

	display = e_mail_reader_get_mail_display (E_MAIL_READER (object));
	attachment_store = e_mail_display_get_attachment_store (display);
	widget = GTK_WIDGET (e_mail_display_get_attachment_view (display));

	e_binding_bind_property_full (
		attachment_store, "num-attachments",
		widget, "attachments-visible",
		G_BINDING_SYNC_CREATE,
		mail_shell_content_transform_num_attachments_to_visible_boolean_with_settings,
		NULL, NULL, NULL);

	widget = e_to_do_pane_new (shell_view);
	gtk_paned_pack2 (paned, widget, FALSE, FALSE);
	gtk_widget_show (widget);

	self->priv->to_do_pane = widget;

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	if (e_shell_window_is_main_instance (e_shell_view_get_shell_window (shell_view))) {
		g_settings_bind_with_mapping (
			settings, "to-do-bar-width",
			paned, "proportion",
			G_SETTINGS_BIND_DEFAULT,
			mail_shell_content_map_setting_to_proportion_cb,
			mail_shell_content_map_proportion_to_setting_cb,
			NULL, NULL);
	} else {
		g_settings_bind_with_mapping (
			settings, "to-do-bar-width-sub",
			paned, "proportion",
			G_SETTINGS_BIND_DEFAULT,
			mail_shell_content_map_setting_to_proportion_cb,
			mail_shell_content_map_proportion_to_setting_cb,
			NULL, NULL);
	}

	g_settings_bind (
		settings, "to-do-bar-show-completed-tasks",
		self->priv->to_do_pane, "show-completed-tasks",
		G_SETTINGS_BIND_DEFAULT);

	g_settings_bind (
		settings, "to-do-bar-show-no-duedate-tasks",
		self->priv->to_do_pane, "show-no-duedate-tasks",
		G_SETTINGS_BIND_DEFAULT);

	g_settings_bind (
		settings, "to-do-bar-show-n-days",
		self->priv->to_do_pane, "show-n-days",
		G_SETTINGS_BIND_DEFAULT);

	g_object_unref (settings);
}

static guint32
mail_shell_content_check_state (EShellContent *shell_content)
{
	EMailShellContent *mail_shell_content;
	EMailReader *reader;

	mail_shell_content = E_MAIL_SHELL_CONTENT (shell_content);

	if (!mail_shell_content->priv->mail_view)
		return 0;

	/* Forward this to our internal EMailView, which
	 * also implements the EMailReader interface. */
	reader = E_MAIL_READER (mail_shell_content->priv->mail_view);

	return e_mail_reader_check_state (reader);
}

static void
mail_shell_content_focus_search_results (EShellContent *shell_content)
{
	EMailShellContent *mail_shell_content;
	EShellWindow *shell_window;
	GtkWidget *message_list;
	EMailReader *reader;

	mail_shell_content = E_MAIL_SHELL_CONTENT (shell_content);

	if (!mail_shell_content->priv->mail_view)
		return;

	reader = E_MAIL_READER (mail_shell_content->priv->mail_view);
	message_list = e_mail_reader_get_message_list (reader);

	shell_window = e_shell_view_get_shell_window (e_shell_content_get_shell_view (shell_content));

	/* This can be called also when the window is showing, to focus default
	   widget, in which case do not skip the gtk_widget_grab_focus() call. */
	if (!message_list || (MESSAGE_LIST (message_list)->just_set_folder &&
	    gtk_widget_get_mapped (GTK_WIDGET (shell_window)) &&
	    gtk_window_get_focus (GTK_WINDOW (shell_window))))
		return;

	gtk_widget_grab_focus (message_list);
}

static guint
mail_shell_content_open_selected_mail (EMailReader *reader)
{
	EMailShellContent *mail_shell_content;

	mail_shell_content = E_MAIL_SHELL_CONTENT (reader);

	if (!mail_shell_content->priv->mail_view)
		return 0;

	/* Forward this to our internal EMailView, which
	 * also implements the EMailReader interface. */
	reader = E_MAIL_READER (mail_shell_content->priv->mail_view);

	return e_mail_reader_open_selected_mail (reader);
}

static GtkActionGroup *
mail_shell_content_get_action_group (EMailReader *reader,
                                     EMailReaderActionGroup group)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellContent *shell_content;
	const gchar *group_name;

	shell_content = E_SHELL_CONTENT (reader);
	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_window = e_shell_view_get_shell_window (shell_view);

	switch (group) {
		case E_MAIL_READER_ACTION_GROUP_STANDARD:
			group_name = "mail";
			break;
		case E_MAIL_READER_ACTION_GROUP_SEARCH_FOLDERS:
			group_name = "search-folders";
			break;
		case E_MAIL_READER_ACTION_GROUP_LABELS:
			group_name = "mail-labels";
			break;
		default:
			g_return_val_if_reached (NULL);
	}

	return e_shell_window_get_action_group (shell_window, group_name);
}

static EMailBackend *
mail_shell_content_get_backend (EMailReader *reader)
{
	EMailShellContent *mail_shell_content;

	mail_shell_content = E_MAIL_SHELL_CONTENT (reader);

	if (!mail_shell_content->priv->mail_view)
		return NULL;

	/* Forward this to our internal EMailView, which
	 * also implements the EMailReader interface. */
	reader = E_MAIL_READER (mail_shell_content->priv->mail_view);

	return e_mail_reader_get_backend (reader);
}

static EMailDisplay *
mail_shell_content_get_mail_display (EMailReader *reader)
{
	EMailShellContent *mail_shell_content;

	mail_shell_content = E_MAIL_SHELL_CONTENT (reader);

	if (!mail_shell_content->priv->mail_view)
		return NULL;

	/* Forward this to our internal EMailView, which
	 * also implements the EMailReader interface. */
	reader = E_MAIL_READER (mail_shell_content->priv->mail_view);

	return e_mail_reader_get_mail_display (reader);
}

static gboolean
mail_shell_content_get_hide_deleted (EMailReader *reader)
{
	EMailShellContent *mail_shell_content;

	mail_shell_content = E_MAIL_SHELL_CONTENT (reader);

	if (!mail_shell_content->priv->mail_view)
		return FALSE;

	/* Forward this to our internal EMailView, which
	 * also implements the EMailReader interface. */
	reader = E_MAIL_READER (mail_shell_content->priv->mail_view);

	return e_mail_reader_get_hide_deleted (reader);
}

static GtkWidget *
mail_shell_content_get_message_list (EMailReader *reader)
{
	EMailShellContent *mail_shell_content;

	mail_shell_content = E_MAIL_SHELL_CONTENT (reader);

	if (!mail_shell_content->priv->mail_view)
		return NULL;

	/* Forward this to our internal EMailView, which
	 * also implements the EMailReader interface. */
	reader = E_MAIL_READER (mail_shell_content->priv->mail_view);

	return e_mail_reader_get_message_list (reader);
}

static GtkMenu *
mail_shell_content_get_popup_menu (EMailReader *reader)
{
	EMailShellContent *mail_shell_content;

	mail_shell_content = E_MAIL_SHELL_CONTENT (reader);

	if (!mail_shell_content->priv->mail_view)
		return NULL;

	/* Forward this to our internal EMailView, which
	 * also implements the EMailReader interface. */
	reader = E_MAIL_READER (mail_shell_content->priv->mail_view);

	return e_mail_reader_get_popup_menu (reader);
}

static EPreviewPane *
mail_shell_content_get_preview_pane (EMailReader *reader)
{
	EMailShellContent *mail_shell_content;

	mail_shell_content = E_MAIL_SHELL_CONTENT (reader);

	if (!mail_shell_content->priv->mail_view)
		return NULL;

	/* Forward this to our internal EMailView, which
	 * also implements the EMailReader interface. */
	reader = E_MAIL_READER (mail_shell_content->priv->mail_view);

	return e_mail_reader_get_preview_pane (reader);
}

static GtkWindow *
mail_shell_content_get_window (EMailReader *reader)
{
	EMailShellContent *mail_shell_content;

	mail_shell_content = E_MAIL_SHELL_CONTENT (reader);

	if (!mail_shell_content->priv->mail_view)
		return NULL;

	/* Forward this to our internal EMailView, which
	 * also implements the EMailReader interface. */
	reader = E_MAIL_READER (mail_shell_content->priv->mail_view);

	return e_mail_reader_get_window (reader);
}

static void
mail_shell_content_set_folder (EMailReader *reader,
                               CamelFolder *folder)
{
	EMailShellContent *mail_shell_content;

	mail_shell_content = E_MAIL_SHELL_CONTENT (reader);

	if (!mail_shell_content->priv->mail_view)
		return;

	/* Forward this to our internal EMailView, which
	 * also implements the EMailReader interface. */
	reader = E_MAIL_READER (mail_shell_content->priv->mail_view);

	e_mail_reader_set_folder (reader, folder);
}

static void
mail_shell_content_update_actions (EMailReader *reader,
				   guint32 state)
{
	EMailShellContent *mail_shell_content;

	mail_shell_content = E_MAIL_SHELL_CONTENT (reader);

	if (!mail_shell_content->priv->mail_view)
		return;

	/* Forward this to our internal EMailView, which
	 * also implements the EMailReader interface. */
	reader = E_MAIL_READER (mail_shell_content->priv->mail_view);

	e_mail_reader_update_actions (reader, state);
}

static void
mail_shell_content_reload (EMailReader *reader)
{
	EMailShellContent *mail_shell_content;

	mail_shell_content = E_MAIL_SHELL_CONTENT (reader);

	if (!mail_shell_content->priv->mail_view)
		return;

	/* Forward this to our internal EMailView, which
	 * also implements the EMailReader interface. */
	reader = E_MAIL_READER (mail_shell_content->priv->mail_view);

	e_mail_reader_reload (reader);
}

static void
mail_shell_content_remove_ui (EMailReader *reader)
{
	EMailShellContent *mail_shell_content;

	mail_shell_content = E_MAIL_SHELL_CONTENT (reader);

	if (!mail_shell_content->priv->mail_view)
		return;

	/* Forward this to our internal EMailView, which
	 * also implements the EMailReader interface. */
	reader = E_MAIL_READER (mail_shell_content->priv->mail_view);

	e_mail_reader_remove_ui (reader);
}

static void
e_mail_shell_content_class_init (EMailShellContentClass *class)
{
	GObjectClass *object_class;
	EShellContentClass *shell_content_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_shell_content_set_property;
	object_class->get_property = mail_shell_content_get_property;
	object_class->dispose = mail_shell_content_dispose;
	object_class->constructed = mail_shell_content_constructed;

	shell_content_class = E_SHELL_CONTENT_CLASS (class);
	shell_content_class->check_state = mail_shell_content_check_state;
	shell_content_class->focus_search_results =
		mail_shell_content_focus_search_results;

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

	g_object_class_install_property (
		object_class,
		PROP_MAIL_VIEW,
		g_param_spec_object (
			"mail-view",
			"Mail View",
			NULL,
			E_TYPE_MAIL_VIEW,
			G_PARAM_READABLE));

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
		PROP_TO_DO_PANE,
		g_param_spec_object (
			"to-do-pane",
			"To Do Pane",
			NULL,
			E_TYPE_TO_DO_PANE,
			G_PARAM_READABLE));
}

static void
e_mail_shell_content_class_finalize (EMailShellContentClass *class)
{
}

static void
e_mail_shell_content_reader_init (EMailReaderInterface *iface)
{
	iface->get_action_group = mail_shell_content_get_action_group;
	iface->get_backend = mail_shell_content_get_backend;
	iface->get_mail_display = mail_shell_content_get_mail_display;
	iface->get_hide_deleted = mail_shell_content_get_hide_deleted;
	iface->get_message_list = mail_shell_content_get_message_list;
	iface->get_popup_menu = mail_shell_content_get_popup_menu;
	iface->get_preview_pane = mail_shell_content_get_preview_pane;
	iface->get_window = mail_shell_content_get_window;
	iface->set_folder = mail_shell_content_set_folder;
	iface->open_selected_mail = mail_shell_content_open_selected_mail;
	iface->update_actions = mail_shell_content_update_actions;
	iface->reload = mail_shell_content_reload;
	iface->remove_ui = mail_shell_content_remove_ui;
}

static void
e_mail_shell_content_init (EMailShellContent *mail_shell_content)
{
	mail_shell_content->priv = e_mail_shell_content_get_instance_private (mail_shell_content);

	/* Postpone widget construction until we have a shell view. */
}

void
e_mail_shell_content_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_mail_shell_content_register_type (type_module);
}

GtkWidget *
e_mail_shell_content_new (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return g_object_new (
		E_TYPE_MAIL_SHELL_CONTENT,
		"shell-view", shell_view, NULL);
}

EMailView *
e_mail_shell_content_get_mail_view (EMailShellContent *mail_shell_content)
{
	g_return_val_if_fail (
		E_IS_MAIL_SHELL_CONTENT (mail_shell_content), NULL);

	return mail_shell_content->priv->mail_view;
}

EShellSearchbar *
e_mail_shell_content_get_searchbar (EMailShellContent *mail_shell_content)
{
	GtkWidget *searchbar;
	EShellView *shell_view;
	EShellContent *shell_content;

	g_return_val_if_fail (
		E_IS_MAIL_SHELL_CONTENT (mail_shell_content), NULL);

	shell_content = E_SHELL_CONTENT (mail_shell_content);
	shell_view = e_shell_content_get_shell_view (shell_content);
	searchbar = e_shell_view_get_searchbar (shell_view);

	return E_SHELL_SEARCHBAR (searchbar);
}

GtkWidget *
e_mail_shell_content_get_to_do_pane (EMailShellContent *mail_shell_content)
{
	g_return_val_if_fail (E_IS_MAIL_SHELL_CONTENT (mail_shell_content), NULL);

	return mail_shell_content->priv->to_do_pane;
}
