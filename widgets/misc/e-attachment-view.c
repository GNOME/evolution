/*
 * e-attachment-view.c
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

#include "e-attachment-view.h"

#include <config.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <camel/camel-stream-mem.h>

#include "e-util/e-binding.h"
#include "e-util/e-util.h"
#include "e-attachment-dialog.h"
#include "e-attachment-handler-image.h"
#include "e-attachment-handler-sendto.h"

enum {
	UPDATE_ACTIONS,
	LAST_SIGNAL
};

/* Note: Do not use the info field. */
static GtkTargetEntry target_table[] = {
	{ (gchar *) "_NETSCAPE_URL",	0, 0 },
	{ (gchar *) "text/x-vcard",	0, 0 },
	{ (gchar *) "text/calendar",	0, 0 }
};

static const gchar *ui =
"<ui>"
"  <popup name='context'>"
"    <menuitem action='cancel'/>"
"    <menuitem action='save-as'/>"
"    <menuitem action='remove'/>"
"    <menuitem action='properties'/>"
"    <separator/>"
"    <placeholder name='inline-actions'>"
"      <menuitem action='show'/>"
"      <menuitem action='hide'/>"
"    </placeholder>"
"    <separator/>"
"    <placeholder name='custom-actions'/>"
"    <separator/>"
"    <menuitem action='add'/>"
"    <separator/>"
"    <placeholder name='open-actions'/>"
"  </popup>"
"</ui>";

static gulong signals[LAST_SIGNAL];

static void
action_add_cb (GtkAction *action,
               EAttachmentView *view)
{
	EAttachmentStore *store;
	gpointer parent;

	parent = gtk_widget_get_toplevel (GTK_WIDGET (view));
	parent = GTK_WIDGET_TOPLEVEL (parent) ? parent : NULL;

	store = e_attachment_view_get_store (view);
	e_attachment_store_run_load_dialog (store, parent);
}

static void
action_cancel_cb (GtkAction *action,
                  EAttachmentView *view)
{
	EAttachment *attachment;
	GList *selected;

	selected = e_attachment_view_get_selected_attachments (view);
	g_return_if_fail (g_list_length (selected) == 1);
	attachment = selected->data;

	e_attachment_cancel (attachment);

	g_list_foreach (selected, (GFunc) g_object_unref, NULL);
	g_list_free (selected);
}

static void
action_hide_cb (GtkAction *action,
                EAttachmentView *view)
{
	EAttachment *attachment;
	GList *selected;

	selected = e_attachment_view_get_selected_attachments (view);
	g_return_if_fail (g_list_length (selected) == 1);
	attachment = selected->data;

	e_attachment_set_shown (attachment, FALSE);

	g_list_foreach (selected, (GFunc) g_object_unref, NULL);
	g_list_free (selected);
}

static void
action_open_in_cb (GtkAction *action,
                   EAttachmentView *view)
{
	GAppInfo *app_info;
	GtkTreePath *path;
	GList *selected;

	selected = e_attachment_view_get_selected_paths (view);
	g_return_if_fail (g_list_length (selected) == 1);
	path = selected->data;

	app_info = g_object_get_data (G_OBJECT (action), "app-info");
	g_return_if_fail (G_IS_APP_INFO (app_info));

	e_attachment_view_open_path (view, path, app_info);

	g_list_foreach (selected, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (selected);
}

static void
action_properties_cb (GtkAction *action,
                      EAttachmentView *view)
{
	EAttachment *attachment;
	GtkWidget *dialog;
	GList *selected;
	gpointer parent;

	selected = e_attachment_view_get_selected_attachments (view);
	g_return_if_fail (g_list_length (selected) == 1);
	attachment = selected->data;

	parent = gtk_widget_get_toplevel (GTK_WIDGET (view));
	parent = GTK_WIDGET_TOPLEVEL (parent) ? parent : NULL;

	dialog = e_attachment_dialog_new (parent, attachment);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	g_list_foreach (selected, (GFunc) g_object_unref, NULL);
	g_list_free (selected);
}

static void
action_recent_cb (GtkAction *action,
                  EAttachmentView *view)
{
	GtkRecentChooser *chooser;
	EAttachmentStore *store;
	EAttachment *attachment;
	gpointer parent;
	gchar *uri;

	chooser = GTK_RECENT_CHOOSER (action);
	store = e_attachment_view_get_store (view);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (view));
	parent = GTK_WIDGET_TOPLEVEL (parent) ? parent : NULL;

	uri = gtk_recent_chooser_get_current_uri (chooser);
	attachment = e_attachment_new_for_uri (uri);
	e_attachment_store_add_attachment (store, attachment);
	e_attachment_load_async (
		attachment, (GAsyncReadyCallback)
		e_attachment_load_handle_error, parent);
	g_free (uri);
}

static void
action_remove_cb (GtkAction *action,
                  EAttachmentView *view)
{
	e_attachment_view_remove_selected (view, FALSE);
}

static void
action_save_all_cb (GtkAction *action,
                    EAttachmentView *view)
{
	EAttachmentStore *store;
	GList *selected, *iter;
	GFile *destination;
	gpointer parent;

	store = e_attachment_view_get_store (view);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (view));
	parent = GTK_WIDGET_TOPLEVEL (parent) ? parent : NULL;

	/* XXX We lose the previous selection. */
	e_attachment_view_select_all (view);
	selected = e_attachment_view_get_selected_attachments (view);
	e_attachment_view_unselect_all (view);

	destination = e_attachment_store_run_save_dialog (
		store, selected, parent);

	if (destination == NULL)
		goto exit;

	for (iter = selected; iter != NULL; iter = iter->next) {
		EAttachment *attachment = iter->data;

		e_attachment_save_async (
			attachment, destination, (GAsyncReadyCallback)
			e_attachment_save_handle_error, parent);
	}

	g_object_unref (destination);

exit:
	g_list_foreach (selected, (GFunc) g_object_unref, NULL);
	g_list_free (selected);
}

static void
action_save_as_cb (GtkAction *action,
                   EAttachmentView *view)
{
	EAttachmentStore *store;
	GList *selected, *iter;
	GFile *destination;
	gpointer parent;

	store = e_attachment_view_get_store (view);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (view));
	parent = GTK_WIDGET_TOPLEVEL (parent) ? parent : NULL;

	selected = e_attachment_view_get_selected_attachments (view);

	destination = e_attachment_store_run_save_dialog (
		store, selected, parent);

	if (destination == NULL)
		goto exit;

	for (iter = selected; iter != NULL; iter = iter->next) {
		EAttachment *attachment = iter->data;

		e_attachment_save_async (
			attachment, destination, (GAsyncReadyCallback)
			e_attachment_save_handle_error, parent);
	}

	g_object_unref (destination);

exit:
	g_list_foreach (selected, (GFunc) g_object_unref, NULL);
	g_list_free (selected);
}

static void
action_show_cb (GtkAction *action,
                EAttachmentView *view)
{
	EAttachment *attachment;
	GList *selected;

	selected = e_attachment_view_get_selected_attachments (view);
	g_return_if_fail (g_list_length (selected) == 1);
	attachment = selected->data;

	e_attachment_set_shown (attachment, TRUE);

	g_list_foreach (selected, (GFunc) g_object_unref, NULL);
	g_list_free (selected);
}

static GtkActionEntry standard_entries[] = {

	{ "cancel",
	  GTK_STOCK_CANCEL,
	  NULL,
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_cancel_cb) },

	{ "save-all",
	  GTK_STOCK_SAVE_AS,
	  N_("S_ave All"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_save_all_cb) },

	{ "save-as",
	  GTK_STOCK_SAVE_AS,
	  NULL,
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_save_as_cb) },

	/* Alternate "save-all" label, for when
	 * the attachment store has one row. */
	{ "save-one",
	  GTK_STOCK_SAVE_AS,
	  NULL,
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_save_all_cb) },
};

static GtkActionEntry editable_entries[] = {

	{ "add",
	  GTK_STOCK_ADD,
	  N_("A_dd Attachment..."),
	  NULL,
	  N_("Attach a file"),
	  G_CALLBACK (action_add_cb) },

	{ "properties",
	  GTK_STOCK_PROPERTIES,
	  NULL,
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_properties_cb) },

	{ "remove",
	  GTK_STOCK_REMOVE,
	  NULL,
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_remove_cb) }
};

static GtkActionEntry inline_entries[] = {

	{ "hide",
	  NULL,
	  N_("_Hide"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_hide_cb) },

	{ "show",
	  NULL,
	  N_("_View Inline"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_show_cb) }
};

static void
attachment_view_netscape_url (EAttachmentView *view,
                              GdkDragContext *drag_context,
                              gint x,
                              gint y,
                              GtkSelectionData *selection_data,
                              guint info,
                              guint time)
{
	static GdkAtom atom = GDK_NONE;
	EAttachmentStore *store;
	EAttachment *attachment;
	const gchar *data;
	gpointer parent;
	gchar *copied_data;
	gchar **strv;
	gint length;

	if (G_UNLIKELY (atom == GDK_NONE))
		atom = gdk_atom_intern_static_string ("_NETSCAPE_URL");

	if (gtk_selection_data_get_target (selection_data) != atom)
		return;

	g_signal_stop_emission_by_name (view, "drag-data-received");

	/* _NETSCAPE_URL is represented as "URI\nTITLE" */

	data = (const gchar *) gtk_selection_data_get_data (selection_data);
	length = gtk_selection_data_get_length (selection_data);

	copied_data = g_strndup (data, length);
	strv = g_strsplit (copied_data, "\n", 2);
	g_free (copied_data);

	store = e_attachment_view_get_store (view);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (view));
	parent = GTK_WIDGET_TOPLEVEL (parent) ? parent : NULL;

	attachment = e_attachment_new_for_uri (strv[0]);
	e_attachment_store_add_attachment (store, attachment);
	e_attachment_load_async (
		attachment, (GAsyncReadyCallback)
		e_attachment_load_handle_error, parent);
	g_object_unref (attachment);

	g_strfreev (strv);

	gtk_drag_finish (drag_context, TRUE, FALSE, time);
}

static void
attachment_view_text_calendar (EAttachmentView *view,
                               GdkDragContext *drag_context,
                               gint x,
                               gint y,
                               GtkSelectionData *selection_data,
                               guint info,
                               guint time)
{
	static GdkAtom atom = GDK_NONE;
	EAttachmentStore *store;
	EAttachment *attachment;
	CamelMimePart *mime_part;
	GdkAtom data_type;
	const gchar *data;
	gpointer parent;
	gchar *content_type;
	gint length;

	if (G_UNLIKELY (atom == GDK_NONE))
		atom = gdk_atom_intern_static_string ("text/calendar");

	if (gtk_selection_data_get_target (selection_data) != atom)
		return;

	g_signal_stop_emission_by_name (view, "drag-data-received");

	data = (const gchar *) gtk_selection_data_get_data (selection_data);
	length = gtk_selection_data_get_length (selection_data);
	data_type = gtk_selection_data_get_data_type (selection_data);

	mime_part = camel_mime_part_new ();

	content_type = gdk_atom_name (data_type);
	camel_mime_part_set_content (mime_part, data, length, content_type);
	camel_mime_part_set_disposition (mime_part, "inline");
	g_free (content_type);

	store = e_attachment_view_get_store (view);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (view));
	parent = GTK_WIDGET_TOPLEVEL (parent) ? parent : NULL;

	attachment = e_attachment_new ();
	e_attachment_set_mime_part (attachment, mime_part);
	e_attachment_store_add_attachment (store, attachment);
	e_attachment_load_async (
		attachment, (GAsyncReadyCallback)
		e_attachment_load_handle_error, parent);
	g_object_unref (attachment);

	camel_object_unref (mime_part);

	gtk_drag_finish (drag_context, TRUE, FALSE, time);
}

static void
attachment_view_text_x_vcard (EAttachmentView *view,
                              GdkDragContext *drag_context,
                              gint x,
                              gint y,
                              GtkSelectionData *selection_data,
                              guint info,
                              guint time)
{
	static GdkAtom atom = GDK_NONE;
	EAttachmentStore *store;
	EAttachment *attachment;
	CamelMimePart *mime_part;
	GdkAtom data_type;
	const gchar *data;
	gpointer parent;
	gchar *content_type;
	gint length;

	if (G_UNLIKELY (atom == GDK_NONE))
		atom = gdk_atom_intern_static_string ("text/x-vcard");

	if (gtk_selection_data_get_target (selection_data) != atom)
		return;

	g_signal_stop_emission_by_name (view, "drag-data-received");

	data = (const gchar *) gtk_selection_data_get_data (selection_data);
	length = gtk_selection_data_get_length (selection_data);
	data_type = gtk_selection_data_get_data_type (selection_data);

	mime_part = camel_mime_part_new ();

	content_type = gdk_atom_name (data_type);
	camel_mime_part_set_content (mime_part, data, length, content_type);
	camel_mime_part_set_disposition (mime_part, "inline");
	g_free (content_type);

	store = e_attachment_view_get_store (view);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (view));
	parent = GTK_WIDGET_TOPLEVEL (parent) ? parent : NULL;

	attachment = e_attachment_new ();
	e_attachment_set_mime_part (attachment, mime_part);
	e_attachment_store_add_attachment (store, attachment);
	e_attachment_load_async (
		attachment, (GAsyncReadyCallback)
		e_attachment_load_handle_error, parent);
	g_object_unref (attachment);

	camel_object_unref (mime_part);

	gtk_drag_finish (drag_context, TRUE, FALSE, time);
}

static void
attachment_view_uris (EAttachmentView *view,
                      GdkDragContext *drag_context,
                      gint x,
                      gint y,
                      GtkSelectionData *selection_data,
                      guint info,
                      guint time)
{
	EAttachmentStore *store;
	gpointer parent;
	gchar **uris;
	gint ii;

	uris = gtk_selection_data_get_uris (selection_data);

	if (uris == NULL)
		return;

	g_signal_stop_emission_by_name (view, "drag-data-received");

	store = e_attachment_view_get_store (view);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (view));
	parent = GTK_WIDGET_TOPLEVEL (parent) ? parent : NULL;

	for (ii = 0; uris[ii] != NULL; ii++) {
		EAttachment *attachment;

		attachment = e_attachment_new_for_uri (uris[ii]);
		e_attachment_store_add_attachment (store, attachment);
		e_attachment_load_async (
			attachment, (GAsyncReadyCallback)
			e_attachment_load_handle_error, parent);
		g_object_unref (attachment);
	}

	g_strfreev (uris);

	gtk_drag_finish (drag_context, TRUE, FALSE, time);
}

static void
attachment_view_update_actions (EAttachmentView *view)
{
	EAttachmentViewPrivate *priv;
	EAttachment *attachment;
	GtkActionGroup *action_group;
	GtkAction *action;
	GList *list, *iter;
	guint n_selected;
	gboolean busy = FALSE;
	gboolean can_show = FALSE;
	gboolean shown = FALSE;

	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));

	priv = e_attachment_view_get_private (view);
	list = e_attachment_view_get_selected_attachments (view);
	n_selected = g_list_length (list);

	if (n_selected == 1) {
		attachment = g_object_ref (list->data);
		busy |= e_attachment_get_loading (attachment);
		busy |= e_attachment_get_saving (attachment);
		can_show = e_attachment_get_can_show (attachment);
		shown = e_attachment_get_shown (attachment);
	} else
		attachment = NULL;

	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);

	action = e_attachment_view_get_action (view, "cancel");
	gtk_action_set_visible (action, busy);

	action = e_attachment_view_get_action (view, "hide");
	gtk_action_set_visible (action, can_show && shown);

	action = e_attachment_view_get_action (view, "properties");
	gtk_action_set_visible (action, !busy && n_selected == 1);

	action = e_attachment_view_get_action (view, "remove");
	gtk_action_set_visible (action, !busy && n_selected > 0);

	action = e_attachment_view_get_action (view, "save-as");
	gtk_action_set_visible (action, !busy && n_selected > 0);

	action = e_attachment_view_get_action (view, "show");
	gtk_action_set_visible (action, can_show && !shown);

	/* Clear out the "openwith" action group. */
	gtk_ui_manager_remove_ui (priv->ui_manager, priv->merge_id);
	action_group = e_attachment_view_get_action_group (view, "openwith");
	e_action_group_remove_all_actions (action_group);

	if (attachment == NULL || busy)
		return;

	list = e_attachment_list_apps (attachment);

	for (iter = list; iter != NULL; iter = iter->next) {
		GAppInfo *app_info = iter->data;
		GtkAction *action;
		GIcon *app_icon;
		const gchar *app_executable;
		const gchar *app_name;
		gchar *action_tooltip;
		gchar *action_label;
		gchar *action_name;

		app_executable = g_app_info_get_executable (app_info);
		app_icon = g_app_info_get_icon (app_info);
		app_name = g_app_info_get_name (app_info);

		action_name = g_strdup_printf ("open-in-%s", app_executable);
		action_label = g_strdup_printf (_("Open with \"%s\""), app_name);

		action_tooltip = g_strdup_printf (
			_("Open this attachment in %s"), app_name);

		action = gtk_action_new (
			action_name, action_label, action_tooltip, NULL);

		gtk_action_set_gicon (action, app_icon);

		g_object_set_data_full (
			G_OBJECT (action),
			"app-info", g_object_ref (app_info),
			(GDestroyNotify) g_object_unref);

		g_object_set_data_full (
			G_OBJECT (action),
			"attachment", g_object_ref (attachment),
			(GDestroyNotify) g_object_unref);

		g_signal_connect (
			action, "activate",
			G_CALLBACK (action_open_in_cb), view);

		gtk_action_group_add_action (action_group, action);

		gtk_ui_manager_add_ui (
			priv->ui_manager, priv->merge_id,
			"/context/open-actions", action_name,
			action_name, GTK_UI_MANAGER_AUTO, FALSE);

		g_free (action_name);
		g_free (action_label);
		g_free (action_tooltip);
	}

	g_object_unref (attachment);
	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);
}

static void
attachment_view_add_handler (GType type,
                             EAttachmentView *view)
{
	EAttachmentViewPrivate *priv;
	EAttachmentHandler *handler;
	const GtkTargetEntry *targets;
	guint n_targets;

	priv = e_attachment_view_get_private (view);

	handler = g_object_new (type, "view", view, NULL);

	targets = e_attachment_handler_get_target_table (handler, &n_targets);
	gtk_target_list_add_table (priv->target_list, targets, n_targets);
	priv->drag_actions |= e_attachment_handler_get_drag_actions (handler);

	g_ptr_array_add (priv->handlers, handler);
}

static void
attachment_view_init_handlers (EAttachmentView *view)
{
	EAttachmentViewPrivate *priv;
	GtkTargetList *target_list;
	GType *children;
	guint ii;

	priv = e_attachment_view_get_private (view);

	target_list = gtk_target_list_new (
		target_table, G_N_ELEMENTS (target_table));

	gtk_target_list_add_uri_targets (target_list, 0);

	priv->handlers = g_ptr_array_new ();
	priv->target_list = target_list;
	priv->drag_actions = GDK_ACTION_COPY;

	children = g_type_children (E_TYPE_ATTACHMENT_HANDLER, NULL);

	for (ii = 0; children[ii] != G_TYPE_INVALID; ii++)
		attachment_view_add_handler (children[ii], view);

	g_free (children);
}

static void
attachment_view_class_init (EAttachmentViewIface *iface)
{
	iface->update_actions = attachment_view_update_actions;

	g_object_interface_install_property (
		iface,
		g_param_spec_boolean (
			"editable",
			"Editable",
			NULL,
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	signals[UPDATE_ACTIONS] = g_signal_new (
		"update-actions",
		G_TYPE_FROM_INTERFACE (iface),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EAttachmentViewIface, update_actions),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

GType
e_attachment_view_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EAttachmentViewIface),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) attachment_view_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			0,     /* instance_size */
			0,     /* n_preallocs */
			(GInstanceInitFunc) NULL,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			G_TYPE_INTERFACE, "EAttachmentView", &type_info, 0);

		g_type_interface_add_prerequisite (type, GTK_TYPE_WIDGET);

		/* Register known handler types. */
		e_attachment_handler_image_get_type ();
		e_attachment_handler_sendto_get_type ();
	}

	return type;
}

void
e_attachment_view_init (EAttachmentView *view)
{
	EAttachmentViewPrivate *priv;
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;
	GError *error = NULL;

	priv = e_attachment_view_get_private (view);

	ui_manager = gtk_ui_manager_new ();
	priv->merge_id = gtk_ui_manager_new_merge_id (ui_manager);
	priv->ui_manager = ui_manager;

	action_group = e_attachment_view_add_action_group (view, "standard");

	gtk_action_group_add_actions (
		action_group, standard_entries,
		G_N_ELEMENTS (standard_entries), view);

	action_group = e_attachment_view_add_action_group (view, "editable");

	e_mutual_binding_new (
		G_OBJECT (view), "editable",
		G_OBJECT (action_group), "visible");
	gtk_action_group_add_actions (
		action_group, editable_entries,
		G_N_ELEMENTS (editable_entries), view);

	action_group = e_attachment_view_add_action_group (view, "inline");

	gtk_action_group_add_actions (
		action_group, inline_entries,
		G_N_ELEMENTS (inline_entries), view);
	gtk_action_group_set_visible (action_group, FALSE);

	e_attachment_view_add_action_group (view, "openwith");

	/* Because we are loading from a hard-coded string, there is
	 * no chance of I/O errors.  Failure here implies a malformed
	 * UI definition.  Full stop. */
	gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, &error);
	if (error != NULL)
		g_error ("%s", error->message);

	attachment_view_init_handlers (view);

	e_attachment_view_drag_source_set (view);
	e_attachment_view_drag_dest_set (view);

	/* Connect built-in drag and drop handlers. */

	g_signal_connect (
		view, "drag-data-received",
		G_CALLBACK (attachment_view_netscape_url), NULL);

	g_signal_connect (
		view, "drag-data-received",
		G_CALLBACK (attachment_view_text_calendar), NULL);

	g_signal_connect (
		view, "drag-data-received",
		G_CALLBACK (attachment_view_text_x_vcard), NULL);

	g_signal_connect (
		view, "drag-data-received",
		G_CALLBACK (attachment_view_uris), NULL);
}

void
e_attachment_view_dispose (EAttachmentView *view)
{
	EAttachmentViewPrivate *priv;

	priv = e_attachment_view_get_private (view);

	g_ptr_array_foreach (priv->handlers, (GFunc) g_object_unref, NULL);
	g_ptr_array_set_size (priv->handlers, 0);

	if (priv->target_list != NULL) {
		gtk_target_list_unref (priv->target_list);
		priv->target_list = NULL;
	}

	if (priv->ui_manager != NULL) {
		g_object_unref (priv->ui_manager);
		priv->ui_manager = NULL;
	}
}

void
e_attachment_view_finalize (EAttachmentView *view)
{
	EAttachmentViewPrivate *priv;

	priv = e_attachment_view_get_private (view);

	g_ptr_array_free (priv->handlers, TRUE);
}

EAttachmentViewPrivate *
e_attachment_view_get_private (EAttachmentView *view)
{
	EAttachmentViewIface *iface;

	g_return_val_if_fail (E_IS_ATTACHMENT_VIEW (view), NULL);

	iface = E_ATTACHMENT_VIEW_GET_IFACE (view);
	g_return_val_if_fail (iface->get_private != NULL, NULL);

	return iface->get_private (view);
}

EAttachmentStore *
e_attachment_view_get_store (EAttachmentView *view)
{
	EAttachmentViewIface *iface;

	g_return_val_if_fail (E_IS_ATTACHMENT_VIEW (view), NULL);

	iface = E_ATTACHMENT_VIEW_GET_IFACE (view);
	g_return_val_if_fail (iface->get_store != NULL, NULL);

	return iface->get_store (view);
}

gboolean
e_attachment_view_get_editable (EAttachmentView *view)
{
	EAttachmentViewPrivate *priv;

	g_return_val_if_fail (E_IS_ATTACHMENT_VIEW (view), FALSE);

	priv = e_attachment_view_get_private (view);

	return priv->editable;
}

void
e_attachment_view_set_editable (EAttachmentView *view,
                                gboolean editable)
{
	EAttachmentViewPrivate *priv;

	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));

	priv = e_attachment_view_get_private (view);
	priv->editable = editable;

	g_object_notify (G_OBJECT (view), "editable");
}

GtkTargetList *
e_attachment_view_get_target_list (EAttachmentView *view)
{
	EAttachmentViewPrivate *priv;

	g_return_val_if_fail (E_IS_ATTACHMENT_VIEW (view), NULL);

	priv = e_attachment_view_get_private (view);

	return priv->target_list;
}

GdkDragAction
e_attachment_view_get_drag_actions (EAttachmentView *view)
{
	EAttachmentViewPrivate *priv;

	g_return_val_if_fail (E_IS_ATTACHMENT_VIEW (view), 0);

	priv = e_attachment_view_get_private (view);

	return priv->drag_actions;
}

GList *
e_attachment_view_get_selected_attachments (EAttachmentView *view)
{
	EAttachmentStore *store;
	GtkTreeModel *model;
	GList *selected, *item;
	gint column_id;

	g_return_val_if_fail (E_IS_ATTACHMENT_VIEW (view), NULL);

	column_id = E_ATTACHMENT_STORE_COLUMN_ATTACHMENT;
	selected = e_attachment_view_get_selected_paths (view);
	store = e_attachment_view_get_store (view);
	model = GTK_TREE_MODEL (store);

	/* Convert the GtkTreePaths to EAttachments. */
	for (item = selected; item != NULL; item = item->next) {
		EAttachment *attachment;
		GtkTreePath *path;
		GtkTreeIter iter;

		path = item->data;

		gtk_tree_model_get_iter (model, &iter, path);
		gtk_tree_model_get (model, &iter, column_id, &attachment, -1);
		gtk_tree_path_free (path);

		item->data = attachment;
	}

	return selected;
}

void
e_attachment_view_open_path (EAttachmentView *view,
                             GtkTreePath *path,
                             GAppInfo *app_info)
{
	EAttachmentStore *store;
	EAttachment *attachment;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gpointer parent;
	gint column_id;

	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));
	g_return_if_fail (path != NULL);

	column_id = E_ATTACHMENT_STORE_COLUMN_ATTACHMENT;
	store = e_attachment_view_get_store (view);
	model = GTK_TREE_MODEL (store);

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter, column_id, &attachment, -1);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (view));
	parent = GTK_WIDGET_TOPLEVEL (parent) ? parent : NULL;

	e_attachment_open_async (
		attachment, app_info, (GAsyncReadyCallback)
		e_attachment_open_handle_error, parent);

	g_object_unref (attachment);
}

void
e_attachment_view_remove_selected (EAttachmentView *view,
                                   gboolean select_next)
{
	EAttachmentStore *store;
	GtkTreeModel *model;
	GList *selected, *item;
	gint column_id;

	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));

	column_id = E_ATTACHMENT_STORE_COLUMN_ATTACHMENT;
	selected = e_attachment_view_get_selected_paths (view);
	store = e_attachment_view_get_store (view);
	model = GTK_TREE_MODEL (store);

	/* Remove attachments in reverse order to avoid invalidating
	 * tree paths as we iterate over the list.  Note, the list is
	 * probably already sorted but we sort again just to be safe. */
	selected = g_list_reverse (g_list_sort (
		selected, (GCompareFunc) gtk_tree_path_compare));

	for (item = selected; item != NULL; item = item->next) {
		EAttachment *attachment;
		GtkTreePath *path = item->data;
		GtkTreeIter iter;

		gtk_tree_model_get_iter (model, &iter, path);
		gtk_tree_model_get (model, &iter, column_id, &attachment, -1);
		e_attachment_store_remove_attachment (store, attachment);
		g_object_unref (attachment);
	}

	/* If we only removed one attachment, try to select another. */
	if (select_next && g_list_length (selected) == 1) {
		GtkTreePath *path = selected->data;

		e_attachment_view_select_path (view, path);
		if (!e_attachment_view_path_is_selected (view, path))
			if (gtk_tree_path_prev (path))
				e_attachment_view_select_path (view, path);
	}

	g_list_foreach (selected, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (selected);
}

gboolean
e_attachment_view_button_press_event (EAttachmentView *view,
                                      GdkEventButton *event)
{
	GtkTreePath *path;
	gboolean editable;
	gboolean item_clicked;

	g_return_val_if_fail (E_IS_ATTACHMENT_VIEW (view), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	editable = e_attachment_view_get_editable (view);

	/* If the user clicked on a selected item, retain the current
	 * selection.  If the user clicked on an unselected item, select
	 * the clicked item only.  If the user did not click on an item,
	 * clear the current selection. */
	path = e_attachment_view_get_path_at_pos (view, event->x, event->y);
	if (path != NULL) {
		if (!e_attachment_view_path_is_selected (view, path)) {
			e_attachment_view_unselect_all (view);
			e_attachment_view_select_path (view, path);
		}
		gtk_tree_path_free (path);
		item_clicked = TRUE;
	} else {
		e_attachment_view_unselect_all (view);
		item_clicked = FALSE;
	}

	/* Cancel drag and drop if there are no selected items,
	 * or if any of the selected items are loading or saving. */
	if (event->button == 1 && event->type == GDK_BUTTON_PRESS) {
		GList *selected, *iter;
		gboolean busy = FALSE;

		selected = e_attachment_view_get_selected_attachments (view);
		for (iter = selected; iter != NULL; iter = iter->next) {
			EAttachment *attachment = iter->data;
			busy |= e_attachment_get_loading (attachment);
			busy |= e_attachment_get_saving (attachment);
		}
		if (selected == NULL || busy)
			e_attachment_view_drag_source_unset (view);
		g_list_foreach (selected, (GFunc) g_object_unref, NULL);
		g_list_free (selected);
	}

	if (event->button == 3 && event->type == GDK_BUTTON_PRESS) {
		/* Non-editable attachment views should only show a
		 * popup menu when right-clicking on an attachment,
		 * but editable views can show the menu any time. */
		if (item_clicked || editable) {
			e_attachment_view_show_popup_menu (
				view, event, NULL, NULL);
			return TRUE;
		}
	}

	return FALSE;
}

gboolean
e_attachment_view_button_release_event (EAttachmentView *view,
                                        GdkEventButton *event)
{
	g_return_val_if_fail (E_IS_ATTACHMENT_VIEW (view), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	/* Restore the attachment view as a drag source, in case
	 * we had to cancel during a button press event. */
	if (event->button == 1)
		e_attachment_view_drag_source_set (view);

	return FALSE;
}

gboolean
e_attachment_view_key_press_event (EAttachmentView *view,
                                   GdkEventKey *event)
{
	gboolean editable;

	g_return_val_if_fail (E_IS_ATTACHMENT_VIEW (view), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	editable = e_attachment_view_get_editable (view);

	if (event->keyval == GDK_Delete && editable) {
		e_attachment_view_remove_selected (view, TRUE);
		return TRUE;
	}

	return FALSE;
}

GtkTreePath *
e_attachment_view_get_path_at_pos (EAttachmentView *view,
                                   gint x,
                                   gint y)
{
	EAttachmentViewIface *iface;

	g_return_val_if_fail (E_IS_ATTACHMENT_VIEW (view), NULL);

	iface = E_ATTACHMENT_VIEW_GET_IFACE (view);
	g_return_val_if_fail (iface->get_path_at_pos != NULL, NULL);

	return iface->get_path_at_pos (view, x, y);
}

GList *
e_attachment_view_get_selected_paths (EAttachmentView *view)
{
	EAttachmentViewIface *iface;

	g_return_val_if_fail (E_IS_ATTACHMENT_VIEW (view), NULL);

	iface = E_ATTACHMENT_VIEW_GET_IFACE (view);
	g_return_val_if_fail (iface->get_selected_paths != NULL, NULL);

	return iface->get_selected_paths (view);
}

gboolean
e_attachment_view_path_is_selected (EAttachmentView *view,
                                    GtkTreePath *path)
{
	EAttachmentViewIface *iface;

	g_return_val_if_fail (E_IS_ATTACHMENT_VIEW (view), FALSE);
	g_return_val_if_fail (path != NULL, FALSE);

	iface = E_ATTACHMENT_VIEW_GET_IFACE (view);
	g_return_val_if_fail (iface->path_is_selected != NULL, FALSE);

	return iface->path_is_selected (view, path);
}

void
e_attachment_view_select_path (EAttachmentView *view,
                               GtkTreePath *path)
{
	EAttachmentViewIface *iface;

	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));
	g_return_if_fail (path != NULL);

	iface = E_ATTACHMENT_VIEW_GET_IFACE (view);
	g_return_if_fail (iface->select_path != NULL);

	iface->select_path (view, path);
}

void
e_attachment_view_unselect_path (EAttachmentView *view,
                                 GtkTreePath *path)
{
	EAttachmentViewIface *iface;

	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));
	g_return_if_fail (path != NULL);

	iface = E_ATTACHMENT_VIEW_GET_IFACE (view);
	g_return_if_fail (iface->unselect_path != NULL);

	iface->unselect_path (view, path);
}

void
e_attachment_view_select_all (EAttachmentView *view)
{
	EAttachmentViewIface *iface;

	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));

	iface = E_ATTACHMENT_VIEW_GET_IFACE (view);
	g_return_if_fail (iface->select_all != NULL);

	iface->select_all (view);
}

void
e_attachment_view_unselect_all (EAttachmentView *view)
{
	EAttachmentViewIface *iface;

	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));

	iface = E_ATTACHMENT_VIEW_GET_IFACE (view);
	g_return_if_fail (iface->unselect_all != NULL);

	iface->unselect_all (view);
}

void
e_attachment_view_sync_selection (EAttachmentView *view,
                                  EAttachmentView *target)
{
	GList *selected, *iter;

	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));
	g_return_if_fail (E_IS_ATTACHMENT_VIEW (target));

	selected = e_attachment_view_get_selected_paths (view);
	e_attachment_view_unselect_all (target);

	for (iter = selected; iter != NULL; iter = iter->next)
		e_attachment_view_select_path (target, iter->data);

	g_list_foreach (selected, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (selected);
}

void
e_attachment_view_drag_source_set (EAttachmentView *view)
{
	GtkTargetEntry *targets;
	GtkTargetList *list;
	gint n_targets;

	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));

	list = gtk_target_list_new (NULL, 0);
	gtk_target_list_add_uri_targets (list, 0);
	targets = gtk_target_table_new_from_list (list, &n_targets);

	gtk_drag_source_set (
		GTK_WIDGET (view), GDK_BUTTON1_MASK,
		targets, n_targets, GDK_ACTION_COPY);

	gtk_target_table_free (targets, n_targets);
	gtk_target_list_unref (list);
}

void
e_attachment_view_drag_source_unset (EAttachmentView *view)
{
	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));

	gtk_drag_source_unset (GTK_WIDGET (view));
}

void
e_attachment_view_drag_begin (EAttachmentView *view,
                              GdkDragContext *context)
{
	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));
	g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));
}

void
e_attachment_view_drag_end (EAttachmentView *view,
                            GdkDragContext *context)
{
	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));
	g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));
}

static void
attachment_view_got_uris_cb (EAttachmentStore *store,
                             GAsyncResult *result,
                             gpointer user_data)
{
	struct {
		gchar **uris;
		gboolean done;
	} *status = user_data;

	/* XXX Since this is a best-effort function,
	 *     should we care about errors? */
	status->uris = e_attachment_store_get_uris_finish (
		store, result, NULL);

	status->done = TRUE;
}

void
e_attachment_view_drag_data_get (EAttachmentView *view,
                                 GdkDragContext *context,
                                 GtkSelectionData *selection,
                                 guint info,
                                 guint time)
{
	EAttachmentStore *store;
	GList *selected;

	struct {
		gchar **uris;
		gboolean done;
	} status;

	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));
	g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));
	g_return_if_fail (selection != NULL);

	status.uris = NULL;
	status.done = FALSE;

	store = e_attachment_view_get_store (view);

	selected = e_attachment_view_get_selected_attachments (view);
	if (selected == NULL)
		return;

	e_attachment_store_get_uris_async (
		store, selected, (GAsyncReadyCallback)
		attachment_view_got_uris_cb, &status);

	g_list_foreach (selected, (GFunc) g_object_unref, NULL);
	g_list_free (selected);

	/* We can't return until we have results, so crank
	 * the main loop until the callback gets triggered.
	 * Note: Don't test for main loop exit.  There is
	 * some kind of nested main loop at work here. */
	while (!status.done)
		gtk_main_iteration ();

	if (status.uris != NULL)
		gtk_selection_data_set_uris (selection, status.uris);

	g_strfreev (status.uris);
}

void
e_attachment_view_drag_dest_set (EAttachmentView *view)
{
	EAttachmentViewPrivate *priv;
	GtkTargetEntry *targets;
	gint n_targets;

	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));

	priv = e_attachment_view_get_private (view);

	targets = gtk_target_table_new_from_list (
		priv->target_list, &n_targets);

	gtk_drag_dest_set (
		GTK_WIDGET (view), GTK_DEST_DEFAULT_ALL,
		targets, n_targets, priv->drag_actions);

	gtk_target_table_free (targets, n_targets);
}

void
e_attachment_view_drag_dest_unset (EAttachmentView *view)
{
	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));

	gtk_drag_dest_unset (GTK_WIDGET (view));
}

gboolean
e_attachment_view_drag_motion (EAttachmentView *view,
                               GdkDragContext *context,
                               gint x,
                               gint y,
                               guint time)
{
	EAttachmentViewPrivate *priv;
	GdkDragAction actions;
	GdkDragAction chosen_action;

	g_return_val_if_fail (E_IS_ATTACHMENT_VIEW (view), FALSE);
	g_return_val_if_fail (GDK_IS_DRAG_CONTEXT (context), FALSE);

	priv = e_attachment_view_get_private (view);

	/* Disallow drops if we're not editable. */
	if (!e_attachment_view_get_editable (view))
		return FALSE;

	actions = priv->drag_actions & context->actions;
	chosen_action = context->suggested_action;

	if (chosen_action == GDK_ACTION_ASK) {
		GdkDragAction mask;

		mask = GDK_ACTION_COPY | GDK_ACTION_MOVE;
		if ((actions & mask) != mask)
			chosen_action = GDK_ACTION_COPY;
	}

	gdk_drag_status (context, chosen_action, time);

	return (chosen_action != 0);
}

gboolean
e_attachment_view_drag_drop (EAttachmentView *view,
                             GdkDragContext *context,
                             gint x,
                             gint y,
                             guint time)
{
	g_return_val_if_fail (E_IS_ATTACHMENT_VIEW (view), FALSE);
	g_return_val_if_fail (GDK_IS_DRAG_CONTEXT (context), FALSE);

	/* Disallow drops if we're not editable. */
	if (!e_attachment_view_get_editable (view))
		return FALSE;

	return TRUE;
}

void
e_attachment_view_drag_data_received (EAttachmentView *view,
                                      GdkDragContext *drag_context,
                                      gint x,
                                      gint y,
                                      GtkSelectionData *selection_data,
                                      guint info,
                                      guint time)
{
	GdkAtom atom;
	gchar *name;

	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));
	g_return_if_fail (GDK_IS_DRAG_CONTEXT (drag_context));

	/* Drop handlers are supposed to stop further emission of the
	 * "drag-data-received" signal if they can handle the data.  If
	 * we get this far it means none of the handlers were successful,
	 * so report the drop as failed. */

	atom = gtk_selection_data_get_target (selection_data);

	name = gdk_atom_name (atom);
	g_warning ("Unknown selection target: %s", name);
	g_free (name);

	gtk_drag_finish (drag_context, FALSE, FALSE, time);
}

GtkAction *
e_attachment_view_get_action (EAttachmentView *view,
                              const gchar *action_name)
{
	GtkUIManager *ui_manager;

	g_return_val_if_fail (E_IS_ATTACHMENT_VIEW (view), NULL);
	g_return_val_if_fail (action_name != NULL, NULL);

	ui_manager = e_attachment_view_get_ui_manager (view);

	return e_lookup_action (ui_manager, action_name);
}

GtkActionGroup *
e_attachment_view_add_action_group (EAttachmentView *view,
                                    const gchar *group_name)
{
	GtkActionGroup *action_group;
	GtkUIManager *ui_manager;
	const gchar *domain;

	g_return_val_if_fail (E_IS_ATTACHMENT_VIEW (view), NULL);
	g_return_val_if_fail (group_name != NULL, NULL);

	ui_manager = e_attachment_view_get_ui_manager (view);
	domain = GETTEXT_PACKAGE;

	action_group = gtk_action_group_new (group_name);
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
	g_object_unref (action_group);

	return action_group;
}

GtkActionGroup *
e_attachment_view_get_action_group (EAttachmentView *view,
                                    const gchar *group_name)
{
	GtkUIManager *ui_manager;

	g_return_val_if_fail (E_IS_ATTACHMENT_VIEW (view), NULL);
	g_return_val_if_fail (group_name != NULL, NULL);

	ui_manager = e_attachment_view_get_ui_manager (view);

	return e_lookup_action_group (ui_manager, group_name);
}

GtkWidget *
e_attachment_view_get_popup_menu (EAttachmentView *view)
{
	GtkUIManager *ui_manager;
	GtkWidget *menu;

	g_return_val_if_fail (E_IS_ATTACHMENT_VIEW (view), NULL);

	ui_manager = e_attachment_view_get_ui_manager (view);
	menu = gtk_ui_manager_get_widget (ui_manager, "/context");
	g_return_val_if_fail (GTK_IS_MENU (menu), NULL);

	return menu;
}

GtkUIManager *
e_attachment_view_get_ui_manager (EAttachmentView *view)
{
	EAttachmentViewPrivate *priv;

	g_return_val_if_fail (E_IS_ATTACHMENT_VIEW (view), NULL);

	priv = e_attachment_view_get_private (view);

	return priv->ui_manager;
}

GtkAction *
e_attachment_view_recent_action_new (EAttachmentView *view,
                                     const gchar *action_name,
                                     const gchar *action_label)
{
	GtkAction *action;
	GtkRecentChooser *chooser;

	g_return_val_if_fail (E_IS_ATTACHMENT_VIEW (view), NULL);
	g_return_val_if_fail (action_name != NULL, NULL);

	action = gtk_recent_action_new (
		action_name, action_label, NULL, NULL);
	gtk_recent_action_set_show_numbers (GTK_RECENT_ACTION (action), TRUE);

	chooser = GTK_RECENT_CHOOSER (action);
	gtk_recent_chooser_set_show_icons (chooser, TRUE);
	gtk_recent_chooser_set_show_not_found (chooser, FALSE);
	gtk_recent_chooser_set_show_private (chooser, FALSE);
	gtk_recent_chooser_set_show_tips (chooser, TRUE);
	gtk_recent_chooser_set_sort_type (chooser, GTK_RECENT_SORT_MRU);

	g_signal_connect (
		action, "item-activated",
		G_CALLBACK (action_recent_cb), view);

	return action;
}

void
e_attachment_view_show_popup_menu (EAttachmentView *view,
                                   GdkEventButton *event,
                                   GtkMenuPositionFunc func,
                                   gpointer user_data)
{
	GtkWidget *menu;

	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));

	e_attachment_view_update_actions (view);

	menu = e_attachment_view_get_popup_menu (view);

	if (event != NULL)
		gtk_menu_popup (
			GTK_MENU (menu), NULL, NULL, func,
			user_data, event->button, event->time);
	else
		gtk_menu_popup (
			GTK_MENU (menu), NULL, NULL, func,
			user_data, 0, gtk_get_current_event_time ());
}

void
e_attachment_view_update_actions (EAttachmentView *view)
{
	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));

	g_signal_emit (view, signals[UPDATE_ACTIONS], 0);
}
