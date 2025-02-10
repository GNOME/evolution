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
	PROP_MAIL_VIEW,
	PROP_TO_DO_PANE
};

G_DEFINE_DYNAMIC_TYPE_EXTENDED (EMailShellContent, e_mail_shell_content, E_TYPE_SHELL_CONTENT, 0,
	G_ADD_PRIVATE_DYNAMIC (EMailShellContent))

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
mail_shell_content_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_MAIL_VIEW:
			g_value_set_object (
				value, e_mail_shell_content_get_mail_view (
				E_MAIL_SHELL_CONTENT (object)));
			return;

		case PROP_TO_DO_PANE:
			g_value_set_object (
				value, e_mail_shell_content_get_to_do_pane (
				E_MAIL_SHELL_CONTENT (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_shell_content_dispose (GObject *object)
{
	EMailShellContent *self = E_MAIL_SHELL_CONTENT (object);

	g_clear_object (&self->priv->mail_view);

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

	display = e_mail_reader_get_mail_display (E_MAIL_READER (self->priv->mail_view));
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

	g_settings_bind (
		settings, "to-do-bar-time-in-smaller-font",
		self->priv->to_do_pane, "time-in-smaller-font",
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

static void
e_mail_shell_content_class_init (EMailShellContentClass *class)
{
	GObjectClass *object_class;
	EShellContentClass *shell_content_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = mail_shell_content_get_property;
	object_class->dispose = mail_shell_content_dispose;
	object_class->constructed = mail_shell_content_constructed;

	shell_content_class = E_SHELL_CONTENT_CLASS (class);
	shell_content_class->check_state = mail_shell_content_check_state;
	shell_content_class->focus_search_results = mail_shell_content_focus_search_results;

	g_object_class_install_property (
		object_class,
		PROP_MAIL_VIEW,
		g_param_spec_object (
			"mail-view",
			"Mail View",
			NULL,
			E_TYPE_MAIL_VIEW,
			G_PARAM_READABLE));

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
