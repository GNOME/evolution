/*
 * e-attachment-view.c
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

#include "e-attachment-view.h"

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include "e-attachment-dialog.h"
#include "e-attachment-handler-image.h"
#include "e-misc-utils.h"
#include "e-selection.h"

enum {
	UPDATE_ACTIONS,
	LAST_SIGNAL
};

/* Note: Do not use the info field. */
static GtkTargetEntry target_table[] = {
	{ (gchar *) "_NETSCAPE_URL", 0, 0 }
};

static const gchar *ui =
"<ui>"
"  <popup name='context'>"
"    <menuitem action='cancel'/>"
"    <menuitem action='save-as'/>"
"    <menuitem action='remove'/>"
"    <menuitem action='properties'/>"
"    <separator/>"
"    <placeholder name='inline-actions'/>"
"    <separator/>"
"    <placeholder name='custom-actions'/>"
"    <separator/>"
"    <menuitem action='add'/>"
"    <separator/>"
"    <placeholder name='open-actions'/>"
"    <menuitem action='open-with'/>"
"  </popup>"
"</ui>";

static gulong signals[LAST_SIGNAL];

G_DEFINE_INTERFACE (
	EAttachmentView,
	e_attachment_view,
	GTK_TYPE_WIDGET)

static void
action_add_cb (GtkAction *action,
               EAttachmentView *view)
{
	EAttachmentStore *store;
	gpointer parent;

	parent = gtk_widget_get_toplevel (GTK_WIDGET (view));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	store = e_attachment_view_get_store (view);
	e_attachment_store_run_load_dialog (store, parent);
}

static void
action_cancel_cb (GtkAction *action,
                  EAttachmentView *view)
{
	EAttachment *attachment;
	GList *list;

	list = e_attachment_view_get_selected_attachments (view);
	g_return_if_fail (g_list_length (list) == 1);
	attachment = list->data;

	e_attachment_cancel (attachment);

	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);
}

static void
action_open_with_cb (GtkAction *action,
                     EAttachmentView *view)
{
	EAttachment *attachment;
	EAttachmentStore *store;
	GtkWidget *dialog;
	GtkTreePath *path;
	GtkTreeIter iter;
	GAppInfo *app_info = NULL;
	GFileInfo *file_info;
	GList *list;
	gpointer parent;
	const gchar *content_type;

	parent = gtk_widget_get_toplevel (GTK_WIDGET (view));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	list = e_attachment_view_get_selected_paths (view);
	g_return_if_fail (g_list_length (list) == 1);
	path = list->data;

	store = e_attachment_view_get_store (view);
	gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, path);
	gtk_tree_model_get (
		GTK_TREE_MODEL (store), &iter,
		E_ATTACHMENT_STORE_COLUMN_ATTACHMENT, &attachment, -1);
	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	file_info = e_attachment_ref_file_info (attachment);
	g_return_if_fail (file_info != NULL);

	content_type = g_file_info_get_content_type (file_info);

	dialog = gtk_app_chooser_dialog_new_for_content_type (
		parent, 0, content_type);
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
		GtkAppChooser *app_chooser = GTK_APP_CHOOSER (dialog);
		app_info = gtk_app_chooser_get_app_info (app_chooser);
	}
	gtk_widget_destroy (dialog);

	if (app_info != NULL) {
		e_attachment_view_open_path (view, path, app_info);
		g_object_unref (app_info);
	}

	g_object_unref (file_info);

	g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (list);
}

static void
action_open_with_app_info_cb (GtkAction *action,
                              EAttachmentView *view)
{
	GAppInfo *app_info;
	GtkTreePath *path;
	GList *list;

	list = e_attachment_view_get_selected_paths (view);
	g_return_if_fail (g_list_length (list) == 1);
	path = list->data;

	app_info = g_object_get_data (G_OBJECT (action), "app-info");
	g_return_if_fail (G_IS_APP_INFO (app_info));

	e_attachment_view_open_path (view, path, app_info);

	g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (list);
}

static void
action_properties_cb (GtkAction *action,
                      EAttachmentView *view)
{
	EAttachment *attachment;
	GtkWidget *dialog;
	GList *list;
	gpointer parent;

	list = e_attachment_view_get_selected_attachments (view);
	g_return_if_fail (g_list_length (list) == 1);
	attachment = list->data;

	parent = gtk_widget_get_toplevel (GTK_WIDGET (view));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	dialog = e_attachment_dialog_new (parent, attachment);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);
}

static void
action_remove_cb (GtkAction *action,
                  EAttachmentView *view)
{
	e_attachment_view_remove_selected (view, FALSE);
}

static void
call_attachment_save_handle_error (GObject *source_object,
				   GAsyncResult *result,
				   gpointer user_data)
{
	GtkWindow *window = user_data;

	g_return_if_fail (E_IS_ATTACHMENT (source_object));
	g_return_if_fail (!window || GTK_IS_WINDOW (window));

	e_attachment_save_handle_error (E_ATTACHMENT (source_object), result, window);

	g_clear_object (&window);
}

static void
action_save_all_cb (GtkAction *action,
                    EAttachmentView *view)
{
	EAttachmentStore *store;
	GList *list, *iter;
	GFile *destination;
	gpointer parent;

	store = e_attachment_view_get_store (view);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (view));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	/* XXX We lose the previous selection. */
	e_attachment_view_select_all (view);
	list = e_attachment_view_get_selected_attachments (view);
	e_attachment_view_unselect_all (view);

	destination = e_attachment_store_run_save_dialog (
		store, list, parent);

	if (destination == NULL)
		goto exit;

	for (iter = list; iter != NULL; iter = iter->next) {
		EAttachment *attachment = iter->data;

		e_attachment_save_async (
			attachment, destination, (GAsyncReadyCallback)
			call_attachment_save_handle_error, parent ? g_object_ref (parent) : NULL);
	}

	g_object_unref (destination);

exit:
	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);
}

static void
action_save_as_cb (GtkAction *action,
                   EAttachmentView *view)
{
	EAttachmentStore *store;
	GList *list, *iter;
	GFile *destination;
	gpointer parent;

	store = e_attachment_view_get_store (view);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (view));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	list = e_attachment_view_get_selected_attachments (view);

	destination = e_attachment_store_run_save_dialog (
		store, list, parent);

	if (destination == NULL)
		goto exit;

	for (iter = list; iter != NULL; iter = iter->next) {
		EAttachment *attachment = iter->data;

		e_attachment_save_async (
			attachment, destination, (GAsyncReadyCallback)
			call_attachment_save_handle_error, parent ? g_object_ref (parent) : NULL);
	}

	g_object_unref (destination);

exit:
	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);
}

static GtkActionEntry standard_entries[] = {

	{ "cancel",
	  "process-stop",
	  N_("_Cancel"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_cancel_cb) },

	{ "open-with",
	  NULL,
	  N_("Open With Other Application..."),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_open_with_cb) },

	{ "save-all",
	  "document-save-as",
	  N_("S_ave All"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_save_all_cb) },

	{ "save-as",
	  "document-save-as",
	  N_("Sa_ve As"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_save_as_cb) },

	/* Alternate "save-all" label, for when
	 * the attachment store has one row. */
	{ "save-one",
	  "document-save-as",
	  N_("Save _As"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_save_all_cb) },
};

static GtkActionEntry editable_entries[] = {

	{ "add",
	  "list-add",
	  N_("A_dd Attachment..."),
	  NULL,
	  N_("Attach a file"),
	  G_CALLBACK (action_add_cb) },

	{ "properties",
	  "document-properties",
	  N_("_Properties"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_properties_cb) },

	{ "remove",
	  "list-remove",
	  N_("_Remove"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_remove_cb) }
};

static void
call_attachment_load_handle_error (GObject *source_object,
				   GAsyncResult *result,
				   gpointer user_data)
{
	GtkWindow *window = user_data;

	g_return_if_fail (E_IS_ATTACHMENT (source_object));
	g_return_if_fail (!window || GTK_IS_WINDOW (window));

	e_attachment_load_handle_error (E_ATTACHMENT (source_object), result, window);

	g_clear_object (&window);
}

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
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	attachment = e_attachment_new_for_uri (strv[0]);
	e_attachment_store_add_attachment (store, attachment);
	e_attachment_load_async (
		attachment, (GAsyncReadyCallback)
		call_attachment_load_handle_error, parent ? g_object_ref (parent) : NULL);
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
	EAttachmentStore *store;
	EAttachment *attachment;
	CamelMimePart *mime_part;
	GdkAtom data_type;
	GdkAtom target;
	const gchar *data;
	gpointer parent;
	gchar *content_type;
	gint length;

	target = gtk_selection_data_get_target (selection_data);
	if (!e_targets_include_calendar (&target, 1))
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
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	attachment = e_attachment_new ();
	e_attachment_set_mime_part (attachment, mime_part);
	e_attachment_store_add_attachment (store, attachment);
	e_attachment_load_async (
		attachment, (GAsyncReadyCallback)
		call_attachment_load_handle_error, parent ? g_object_ref (parent) : NULL);
	g_object_unref (attachment);

	g_object_unref (mime_part);

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
	EAttachmentStore *store;
	EAttachment *attachment;
	CamelMimePart *mime_part;
	GdkAtom data_type;
	GdkAtom target;
	const gchar *data;
	gpointer parent;
	gchar *content_type;
	gint length;

	target = gtk_selection_data_get_target (selection_data);
	if (!e_targets_include_directory (&target, 1))
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
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	attachment = e_attachment_new ();
	e_attachment_set_mime_part (attachment, mime_part);
	e_attachment_store_add_attachment (store, attachment);
	e_attachment_load_async (
		attachment, (GAsyncReadyCallback)
		call_attachment_load_handle_error, parent ? g_object_ref (parent) : NULL);
	g_object_unref (attachment);

	g_object_unref (mime_part);

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
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	for (ii = 0; uris[ii] != NULL; ii++) {
		EAttachment *attachment;

		attachment = e_attachment_new_for_uri (uris[ii]);
		e_attachment_store_add_attachment (store, attachment);
		e_attachment_load_async (
			attachment, (GAsyncReadyCallback)
			call_attachment_load_handle_error, parent ? g_object_ref (parent) : NULL);
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

	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));

	priv = e_attachment_view_get_private (view);

	list = e_attachment_view_get_selected_attachments (view);
	n_selected = g_list_length (list);

	if (n_selected == 1) {
		attachment = g_object_ref (list->data);

		busy |= e_attachment_get_loading (attachment);
		busy |= e_attachment_get_saving (attachment);
	} else
		attachment = NULL;

	g_list_free_full (list, g_object_unref);

	action = e_attachment_view_get_action (view, "cancel");
	gtk_action_set_visible (action, busy);

	action = e_attachment_view_get_action (view, "open-with");
	gtk_action_set_visible (action, !busy && n_selected == 1);

	action = e_attachment_view_get_action (view, "properties");
	gtk_action_set_visible (action, !busy && n_selected == 1);

	action = e_attachment_view_get_action (view, "remove");
	gtk_action_set_visible (action, !busy && n_selected > 0);

	action = e_attachment_view_get_action (view, "save-as");
	gtk_action_set_visible (action, !busy && n_selected > 0);

	/* Clear out the "openwith" action group. */
	gtk_ui_manager_remove_ui (priv->ui_manager, priv->merge_id);
	action_group = e_attachment_view_get_action_group (view, "openwith");
	e_action_group_remove_all_actions (action_group);
	gtk_ui_manager_ensure_update (priv->ui_manager);

	if (!attachment || busy) {
		g_clear_object (&attachment);
		return;
	}

	list = e_attachment_list_apps (attachment);

	for (iter = list; iter != NULL; iter = iter->next) {
		GAppInfo *app_info = iter->data;
		GtkAction *action;
		GIcon *app_icon;
		const gchar *app_id;
		const gchar *app_name;
		gchar *action_tooltip;
		gchar *action_label;
		gchar *action_name;

		app_id = g_app_info_get_id (app_info);
		app_icon = g_app_info_get_icon (app_info);
		app_name = g_app_info_get_name (app_info);

		if (app_id == NULL)
			continue;

		/* Don't list 'Open With "Evolution"'. */
		if (g_str_equal (app_id, "evolution.desktop"))
			continue;

		action_name = g_strdup_printf ("open-with-%s", app_id);

		action_label = g_strdup_printf (
			_("Open With “%s”"), app_name);

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
			G_CALLBACK (action_open_with_app_info_cb), view);

		gtk_action_group_add_action (action_group, action);

		gtk_ui_manager_add_ui (
			priv->ui_manager, priv->merge_id,
			"/context/open-actions", action_name,
			action_name, GTK_UI_MANAGER_AUTO, FALSE);

		g_free (action_name);
		g_free (action_label);
		g_free (action_tooltip);
	}

	g_list_free_full (list, g_object_unref);
	g_object_unref (attachment);
}

static void
attachment_view_init_drag_dest (EAttachmentView *view)
{
	EAttachmentViewPrivate *priv;
	GtkTargetList *target_list;

	priv = e_attachment_view_get_private (view);

	target_list = gtk_target_list_new (
		target_table, G_N_ELEMENTS (target_table));

	gtk_target_list_add_uri_targets (target_list, 0);
	e_target_list_add_calendar_targets (target_list, 0);
	e_target_list_add_directory_targets (target_list, 0);

	priv->target_list = target_list;
	priv->drag_actions = GDK_ACTION_COPY;
}

static void
e_attachment_view_default_init (EAttachmentViewInterface *iface)
{
	iface->update_actions = attachment_view_update_actions;

	g_object_interface_install_property (
		iface,
		g_param_spec_boolean (
			"dragging",
			"Dragging",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

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
		G_STRUCT_OFFSET (EAttachmentViewInterface, update_actions),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	/* Register known handler types. */
	g_type_ensure (E_TYPE_ATTACHMENT_HANDLER_IMAGE);
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

	e_binding_bind_property (
		view, "editable",
		action_group, "visible",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);
	gtk_action_group_add_actions (
		action_group, editable_entries,
		G_N_ELEMENTS (editable_entries), view);

	e_attachment_view_add_action_group (view, "openwith");

	/* Because we are loading from a hard-coded string, there is
	 * no chance of I/O errors.  Failure here implies a malformed
	 * UI definition.  Full stop. */
	gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, &error);
	if (error != NULL)
		g_error ("%s", error->message);

	attachment_view_init_drag_dest (view);

	e_attachment_view_drag_source_set (view);

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

	g_list_foreach (priv->event_list, (GFunc) gdk_event_free, NULL);
	g_list_free (priv->event_list);

	g_list_foreach (priv->selected, (GFunc) g_object_unref, NULL);
	g_list_free (priv->selected);
}

EAttachmentViewPrivate *
e_attachment_view_get_private (EAttachmentView *view)
{
	EAttachmentViewInterface *iface;

	g_return_val_if_fail (E_IS_ATTACHMENT_VIEW (view), NULL);

	iface = E_ATTACHMENT_VIEW_GET_INTERFACE (view);
	g_return_val_if_fail (iface->get_private != NULL, NULL);

	return iface->get_private (view);
}

EAttachmentStore *
e_attachment_view_get_store (EAttachmentView *view)
{
	EAttachmentViewInterface *iface;

	g_return_val_if_fail (E_IS_ATTACHMENT_VIEW (view), NULL);

	iface = E_ATTACHMENT_VIEW_GET_INTERFACE (view);
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

	if (editable)
		e_attachment_view_drag_dest_set (view);
	else
		e_attachment_view_drag_dest_unset (view);

	g_object_notify (G_OBJECT (view), "editable");
}

gboolean
e_attachment_view_get_dragging (EAttachmentView *view)
{
	EAttachmentViewPrivate *priv;

	g_return_val_if_fail (E_IS_ATTACHMENT_VIEW (view), FALSE);

	priv = e_attachment_view_get_private (view);

	return priv->dragging;
}

void
e_attachment_view_set_dragging (EAttachmentView *view,
                                gboolean dragging)
{
	EAttachmentViewPrivate *priv;

	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));

	priv = e_attachment_view_get_private (view);

	priv->dragging = dragging;

	g_object_notify (G_OBJECT (view), "dragging");
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

void
e_attachment_view_add_drag_actions (EAttachmentView *view,
                                    GdkDragAction drag_actions)
{
	EAttachmentViewPrivate *priv;

	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));

	priv = e_attachment_view_get_private (view);

	priv->drag_actions |= drag_actions;
}

GList *
e_attachment_view_get_selected_attachments (EAttachmentView *view)
{
	EAttachmentStore *store;
	GtkTreeModel *model;
	GList *list, *item;
	gint column_id;

	g_return_val_if_fail (E_IS_ATTACHMENT_VIEW (view), NULL);

	column_id = E_ATTACHMENT_STORE_COLUMN_ATTACHMENT;
	list = e_attachment_view_get_selected_paths (view);
	store = e_attachment_view_get_store (view);
	model = GTK_TREE_MODEL (store);

	/* Convert the GtkTreePaths to EAttachments. */
	for (item = list; item != NULL; item = item->next) {
		EAttachment *attachment;
		GtkTreePath *path;
		GtkTreeIter iter;

		path = item->data;

		gtk_tree_model_get_iter (model, &iter, path);
		gtk_tree_model_get (model, &iter, column_id, &attachment, -1);
		gtk_tree_path_free (path);

		item->data = attachment;
	}

	return list;
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
	gboolean iter_valid;
	gpointer parent;
	gint column_id;

	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));
	g_return_if_fail (path != NULL);

	column_id = E_ATTACHMENT_STORE_COLUMN_ATTACHMENT;
	store = e_attachment_view_get_store (view);
	model = GTK_TREE_MODEL (store);

	iter_valid = gtk_tree_model_get_iter (model, &iter, path);
	g_return_if_fail (iter_valid);

	gtk_tree_model_get (model, &iter, column_id, &attachment, -1);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (view));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

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
	GList *list, *item;
	gint column_id;

	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));

	column_id = E_ATTACHMENT_STORE_COLUMN_ATTACHMENT;
	list = e_attachment_view_get_selected_paths (view);
	store = e_attachment_view_get_store (view);
	model = GTK_TREE_MODEL (store);

	/* Remove attachments in reverse order to avoid invalidating
	 * tree paths as we iterate over the list.  Note, the list is
	 * probably already sorted but we sort again just to be safe. */
	list = g_list_reverse (g_list_sort (
		list, (GCompareFunc) gtk_tree_path_compare));

	for (item = list; item != NULL; item = item->next) {
		EAttachment *attachment;
		GtkTreePath *path = item->data;
		GtkTreeIter iter;

		gtk_tree_model_get_iter (model, &iter, path);
		gtk_tree_model_get (model, &iter, column_id, &attachment, -1);
		e_attachment_store_remove_attachment (store, attachment);
		g_object_unref (attachment);
	}

	/* If we only removed one attachment, try to select another. */
	if (select_next && g_list_length (list) == 1) {
		GtkTreePath *path = list->data;

		e_attachment_view_select_path (view, path);
		if (!e_attachment_view_path_is_selected (view, path))
			if (gtk_tree_path_prev (path))
				e_attachment_view_select_path (view, path);
	}

	g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (list);
}

gboolean
e_attachment_view_button_press_event (EAttachmentView *view,
                                      GdkEventButton *event)
{
	EAttachmentViewPrivate *priv;
	GtkTreePath *path;
	gboolean editable;
	gboolean handled = FALSE;
	gboolean path_is_selected = FALSE;

	g_return_val_if_fail (E_IS_ATTACHMENT_VIEW (view), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	priv = e_attachment_view_get_private (view);

	if (g_list_find (priv->event_list, event) != NULL)
		return FALSE;

	if (priv->event_list != NULL) {
		/* Save the event to be propagated in order. */
		priv->event_list = g_list_append (
			priv->event_list,
			gdk_event_copy ((GdkEvent *) event));
		return TRUE;
	}

	editable = e_attachment_view_get_editable (view);
	path = e_attachment_view_get_path_at_pos (view, event->x, event->y);
	path_is_selected = e_attachment_view_path_is_selected (view, path);

	if (event->button == 1 && event->type == GDK_BUTTON_PRESS) {
		GList *list, *iter;
		gboolean busy = FALSE;

		list = e_attachment_view_get_selected_attachments (view);

		for (iter = list; iter != NULL; iter = iter->next) {
			EAttachment *attachment = iter->data;
			busy |= e_attachment_get_loading (attachment);
			busy |= e_attachment_get_saving (attachment);
		}

		/* Prepare for dragging if the clicked item is selected
		 * and none of the selected items are loading or saving. */
		if (path_is_selected && !busy) {
			priv->start_x = event->x;
			priv->start_y = event->y;
			priv->event_list = g_list_append (
				priv->event_list,
				gdk_event_copy ((GdkEvent *) event));
			handled = TRUE;
		}

		g_list_foreach (list, (GFunc) g_object_unref, NULL);
		g_list_free (list);
	}

	if (event->button == 3 && event->type == GDK_BUTTON_PRESS) {
		/* If the user clicked on a selected item, retain the
		 * current selection.  If the user clicked on an unselected
		 * item, select the clicked item only.  If the user did not
		 * click on an item, clear the current selection. */
		if (path == NULL)
			e_attachment_view_unselect_all (view);
		else if (!path_is_selected) {
			e_attachment_view_unselect_all (view);
			e_attachment_view_select_path (view, path);
		}

		/* Non-editable attachment views should only show a
		 * popup menu when right-clicking on an attachment,
		 * but editable views can show the menu any time. */
		if (path != NULL || editable) {
			e_attachment_view_show_popup_menu (
				view, event, NULL, NULL);
			handled = TRUE;
		}
	}

	if (path != NULL)
		gtk_tree_path_free (path);

	return handled;
}

gboolean
e_attachment_view_button_release_event (EAttachmentView *view,
                                        GdkEventButton *event)
{
	EAttachmentViewPrivate *priv;
	GtkWidget *widget = GTK_WIDGET (view);
	GList *iter;

	g_return_val_if_fail (E_IS_ATTACHMENT_VIEW (view), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	priv = e_attachment_view_get_private (view);

	for (iter = priv->event_list; iter != NULL; iter = iter->next) {
		GdkEvent *event = iter->data;

		gtk_propagate_event (widget, event);
		gdk_event_free (event);
	}

	g_list_free (priv->event_list);
	priv->event_list = NULL;

	return FALSE;
}

gboolean
e_attachment_view_motion_notify_event (EAttachmentView *view,
                                       GdkEventMotion *event)
{
	EAttachmentViewPrivate *priv;
	GtkWidget *widget = GTK_WIDGET (view);
	GtkTargetList *targets;

	g_return_val_if_fail (E_IS_ATTACHMENT_VIEW (view), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	priv = e_attachment_view_get_private (view);

	if (priv->event_list == NULL)
		return FALSE;

	if (!gtk_drag_check_threshold (
		widget, priv->start_x, priv->start_y, event->x, event->y))
		return TRUE;

	g_list_foreach (priv->event_list, (GFunc) gdk_event_free, NULL);
	g_list_free (priv->event_list);
	priv->event_list = NULL;

	targets = gtk_drag_source_get_target_list (widget);

	gtk_drag_begin (
		widget, targets, GDK_ACTION_COPY, 1, (GdkEvent *) event);

	return TRUE;
}

gboolean
e_attachment_view_key_press_event (EAttachmentView *view,
                                   GdkEventKey *event)
{
	gboolean editable;

	g_return_val_if_fail (E_IS_ATTACHMENT_VIEW (view), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	editable = e_attachment_view_get_editable (view);

	if (event->keyval == GDK_KEY_Delete && editable) {
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
	EAttachmentViewInterface *iface;

	g_return_val_if_fail (E_IS_ATTACHMENT_VIEW (view), NULL);

	iface = E_ATTACHMENT_VIEW_GET_INTERFACE (view);
	g_return_val_if_fail (iface->get_path_at_pos != NULL, NULL);

	return iface->get_path_at_pos (view, x, y);
}

GList *
e_attachment_view_get_selected_paths (EAttachmentView *view)
{
	EAttachmentViewInterface *iface;

	g_return_val_if_fail (E_IS_ATTACHMENT_VIEW (view), NULL);

	iface = E_ATTACHMENT_VIEW_GET_INTERFACE (view);
	g_return_val_if_fail (iface->get_selected_paths != NULL, NULL);

	return iface->get_selected_paths (view);
}

gboolean
e_attachment_view_path_is_selected (EAttachmentView *view,
                                    GtkTreePath *path)
{
	EAttachmentViewInterface *iface;

	g_return_val_if_fail (E_IS_ATTACHMENT_VIEW (view), FALSE);

	/* Handle NULL paths gracefully. */
	if (path == NULL)
		return FALSE;

	iface = E_ATTACHMENT_VIEW_GET_INTERFACE (view);
	g_return_val_if_fail (iface->path_is_selected != NULL, FALSE);

	return iface->path_is_selected (view, path);
}

void
e_attachment_view_select_path (EAttachmentView *view,
                               GtkTreePath *path)
{
	EAttachmentViewInterface *iface;

	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));
	g_return_if_fail (path != NULL);

	iface = E_ATTACHMENT_VIEW_GET_INTERFACE (view);
	g_return_if_fail (iface->select_path != NULL);

	iface->select_path (view, path);
}

void
e_attachment_view_unselect_path (EAttachmentView *view,
                                 GtkTreePath *path)
{
	EAttachmentViewInterface *iface;

	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));
	g_return_if_fail (path != NULL);

	iface = E_ATTACHMENT_VIEW_GET_INTERFACE (view);
	g_return_if_fail (iface->unselect_path != NULL);

	iface->unselect_path (view, path);
}

void
e_attachment_view_select_all (EAttachmentView *view)
{
	EAttachmentViewInterface *iface;

	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));

	iface = E_ATTACHMENT_VIEW_GET_INTERFACE (view);
	g_return_if_fail (iface->select_all != NULL);

	iface->select_all (view);
}

void
e_attachment_view_unselect_all (EAttachmentView *view)
{
	EAttachmentViewInterface *iface;

	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));

	iface = E_ATTACHMENT_VIEW_GET_INTERFACE (view);
	g_return_if_fail (iface->unselect_all != NULL);

	iface->unselect_all (view);
}

void
e_attachment_view_sync_selection (EAttachmentView *view,
                                  EAttachmentView *target)
{
	GList *list, *iter;

	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));
	g_return_if_fail (E_IS_ATTACHMENT_VIEW (target));

	list = e_attachment_view_get_selected_paths (view);
	e_attachment_view_unselect_all (target);

	for (iter = list; iter != NULL; iter = iter->next)
		e_attachment_view_select_path (target, iter->data);

	g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (list);
}

void
e_attachment_view_drag_source_set (EAttachmentView *view)
{
	EAttachmentViewInterface *iface;
	GtkTargetEntry *targets;
	GtkTargetList *list;
	gint n_targets;

	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));

	iface = E_ATTACHMENT_VIEW_GET_INTERFACE (view);
	if (iface->drag_source_set == NULL)
		return;

	list = gtk_target_list_new (NULL, 0);
	gtk_target_list_add_uri_targets (list, 0);
	targets = gtk_target_table_new_from_list (list, &n_targets);

	iface->drag_source_set (
		view, GDK_BUTTON1_MASK,
		targets, n_targets, GDK_ACTION_COPY);

	gtk_target_table_free (targets, n_targets);
	gtk_target_list_unref (list);
}

void
e_attachment_view_drag_source_unset (EAttachmentView *view)
{
	EAttachmentViewInterface *iface;

	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));

	iface = E_ATTACHMENT_VIEW_GET_INTERFACE (view);
	if (iface->drag_source_unset == NULL)
		return;

	iface->drag_source_unset (view);
}

void
e_attachment_view_drag_begin (EAttachmentView *view,
                              GdkDragContext *context)
{
	EAttachmentViewPrivate *priv;
	guint n_selected;

	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));
	g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));

	priv = e_attachment_view_get_private (view);

	e_attachment_view_set_dragging (view, TRUE);

	g_warn_if_fail (priv->selected == NULL);
	priv->selected = e_attachment_view_get_selected_attachments (view);
	n_selected = g_list_length (priv->selected);

	if (n_selected == 1) {
		EAttachment *attachment;
		GtkIconTheme *icon_theme;
		GtkIconInfo *icon_info;
		GIcon *icon;
		gint width, height;

		attachment = E_ATTACHMENT (priv->selected->data);
		icon = e_attachment_ref_icon (attachment);
		g_return_if_fail (icon != NULL);

		icon_theme = gtk_icon_theme_get_default ();
		gtk_icon_size_lookup (GTK_ICON_SIZE_DND, &width, &height);

		icon_info = gtk_icon_theme_lookup_by_gicon (
			icon_theme, icon, MIN (width, height),
			GTK_ICON_LOOKUP_USE_BUILTIN);

		if (icon_info != NULL) {
			GdkPixbuf *pixbuf;
			GError *error = NULL;

			pixbuf = gtk_icon_info_load_icon (icon_info, &error);

			if (pixbuf != NULL) {
				gtk_drag_set_icon_pixbuf (
					context, pixbuf, 0, 0);
				g_object_unref (pixbuf);
			} else if (error != NULL) {
				g_warning ("%s", error->message);
				g_error_free (error);
			}

			gtk_icon_info_free (icon_info);
		}

		g_object_unref (icon);
	}
}

void
e_attachment_view_drag_end (EAttachmentView *view,
                            GdkDragContext *context)
{
	EAttachmentViewPrivate *priv;

	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));
	g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));

	priv = e_attachment_view_get_private (view);

	e_attachment_view_set_dragging (view, FALSE);

	g_list_foreach (priv->selected, (GFunc) g_object_unref, NULL);
	g_list_free (priv->selected);
	priv->selected = NULL;
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
	EAttachmentViewPrivate *priv;
	EAttachmentStore *store;

	struct {
		gchar **uris;
		gboolean done;
	} status;

	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));
	g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));
	g_return_if_fail (selection != NULL);

	status.uris = NULL;
	status.done = FALSE;

	priv = e_attachment_view_get_private (view);
	store = e_attachment_view_get_store (view);

	if (priv->selected == NULL)
		return;

	e_attachment_store_get_uris_async (
		store, priv->selected, (GAsyncReadyCallback)
		attachment_view_got_uris_cb, &status);

	/* We can't return until we have results, so crank
	 * the main loop until the callback gets triggered. */
	while (!status.done)
		if (gtk_main_iteration ())
			break;

	if (status.uris != NULL)
		gtk_selection_data_set_uris (selection, status.uris);

	g_strfreev (status.uris);
}

void
e_attachment_view_drag_dest_set (EAttachmentView *view)
{
	EAttachmentViewPrivate *priv;
	EAttachmentViewInterface *iface;
	GtkTargetEntry *targets;
	gint n_targets;

	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));

	iface = E_ATTACHMENT_VIEW_GET_INTERFACE (view);
	if (iface->drag_dest_set == NULL)
		return;

	priv = e_attachment_view_get_private (view);

	targets = gtk_target_table_new_from_list (
		priv->target_list, &n_targets);

	iface->drag_dest_set (
		view, targets, n_targets, priv->drag_actions);

	gtk_target_table_free (targets, n_targets);
}

void
e_attachment_view_drag_dest_unset (EAttachmentView *view)
{
	EAttachmentViewInterface *iface;

	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));

	iface = E_ATTACHMENT_VIEW_GET_INTERFACE (view);
	if (iface->drag_dest_unset == NULL)
		return;

	iface->drag_dest_unset (view);
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

	/* Disallow drops if we initiated the drag.
	 * This helps prevent duplicate attachments. */
	if (e_attachment_view_get_dragging (view))
		return FALSE;

	actions = gdk_drag_context_get_actions (context);
	actions &= priv->drag_actions;
	chosen_action = gdk_drag_context_get_suggested_action (context);

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

	/* Disallow drops if we initiated the drag.
	 * This helps prevent duplicate attachments. */
	return !e_attachment_view_get_dragging (view);
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

	if (!gtk_menu_get_attach_widget (GTK_MENU (menu)))
		gtk_menu_attach_to_widget (GTK_MENU (menu),
					   GTK_WIDGET (view),
					   NULL);

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
