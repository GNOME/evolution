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

enum {
	UPDATE_ACTIONS,
	LAST_SIGNAL
};

enum {
	DND_TYPE_MESSAGE_RFC822,
	DND_TYPE_X_UID_LIST,
	DND_TYPE_TEXT_URI_LIST,
	DND_TYPE_NETSCAPE_URL,
	DND_TYPE_TEXT_VCARD,
	DND_TYPE_TEXT_CALENDAR
};

static GtkTargetEntry drop_types[] = {
	{ "message/rfc822",	0, DND_TYPE_MESSAGE_RFC822 },
	{ "x-uid-list",		0, DND_TYPE_X_UID_LIST },
	{ "text/uri-list",	0, DND_TYPE_TEXT_URI_LIST },
	{ "_NETSCAPE_URL",	0, DND_TYPE_NETSCAPE_URL },
	{ "text/x-vcard",	0, DND_TYPE_TEXT_VCARD },
	{ "text/calendar",	0, DND_TYPE_TEXT_CALENDAR }
};

/* The atoms need initialized at runtime. */
static struct {
	const gchar *target;
	GdkAtom atom;
	GdkDragAction actions;
} drag_info[] = {
	{ "message/rfc822",	NULL,	GDK_ACTION_COPY },
	{ "x-uid-list",		NULL,	GDK_ACTION_COPY |
					GDK_ACTION_MOVE |
					GDK_ACTION_ASK },
	{ "text/uri-list",	NULL,	GDK_ACTION_COPY },
	{ "_NETSCAPE_URL",	NULL,	GDK_ACTION_COPY },
	{ "text/x-vcard",	NULL,	GDK_ACTION_COPY },
	{ "text/calendar",	NULL,	GDK_ACTION_COPY }
};

static const gchar *ui =
"<ui>"
"  <popup name='context'>"
"    <menuitem action='cancel'/>"
"    <menuitem action='save-as'/>"
"    <menuitem action='remove'/>"
"    <menuitem action='properties'/>"
"    <separator/>"
"    <placeholder name='custom-actions'/>"
"    <separator/>"
"    <menuitem action='add'/>"
"    <separator/>"
"    <placeholder name='open-actions'/>"
"  </popup>"
"  <popup name='dnd'>"
"    <menuitem action='drag-copy'/>"
"    <menuitem action='drag-move'/>"
"    <separator/>"
"    <menuitem action='drag-cancel'/>"
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
action_drag_cancel_cb (GtkAction *action,
                       EAttachmentView *view)
{
	EAttachmentViewPrivate *priv;

	priv = e_attachment_view_get_private (view);
	gtk_drag_finish (priv->drag_context, FALSE, FALSE, priv->time);
}

static void
action_drag_copy_cb (GtkAction *action,
                     EAttachmentView *view)
{
	e_attachment_view_drag_action (view, GDK_ACTION_COPY);
}

static void
action_drag_move_cb (GtkAction *action,
                     EAttachmentView *view)
{
	e_attachment_view_drag_action (view, GDK_ACTION_MOVE);
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

static GtkActionEntry standard_entries[] = {

	{ "cancel",
	  GTK_STOCK_CANCEL,
	  NULL,
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_cancel_cb) },

	{ "drag-cancel",
	  NULL,
	  N_("Cancel _Drag"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_drag_cancel_cb) },

	{ "drag-copy",
	  NULL,
	  N_("_Copy"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_drag_copy_cb) },

	{ "drag-move",
	  NULL,
	  N_("_Move"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_drag_move_cb) },

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

static void
drop_message_rfc822 (EAttachmentView *view,
                     GtkSelectionData *selection_data,
                     EAttachmentStore *store,
                     GdkDragAction action)
{
	EAttachmentViewPrivate *priv;
	EAttachment *attachment;
	CamelMimeMessage *message;
	CamelDataWrapper *wrapper;
	CamelStream *stream;
	const gchar *data;
	gboolean success = FALSE;
	gboolean delete = FALSE;
	gpointer parent;
	gint length;

	priv = e_attachment_view_get_private (view);

	data = (const gchar *) gtk_selection_data_get_data (selection_data);
	length = gtk_selection_data_get_length (selection_data);

	stream = camel_stream_mem_new ();
	camel_stream_write (stream, data, length);
	camel_stream_reset (stream);

	message = camel_mime_message_new ();
	wrapper = CAMEL_DATA_WRAPPER (message);

	if (camel_data_wrapper_construct_from_stream (wrapper, stream) == -1)
		goto exit;

	parent = gtk_widget_get_toplevel (GTK_WIDGET (view));
	parent = GTK_WIDGET_TOPLEVEL (parent) ? parent : NULL;

	attachment = e_attachment_new_for_message (message);
	e_attachment_store_add_attachment (store, attachment);
	e_attachment_load_async (
		attachment, (GAsyncReadyCallback)
		e_attachment_load_handle_error, parent);
	g_object_unref (attachment);

	success = TRUE;
	delete = (action == GDK_ACTION_MOVE);

exit:
	camel_object_unref (message);
	camel_object_unref (stream);

	gtk_drag_finish (priv->drag_context, success, delete, priv->time);
}

static void
drop_netscape_url (EAttachmentView *view,
                   GtkSelectionData *selection_data,
                   EAttachmentStore *store,
                   GdkDragAction action)
{
	EAttachmentViewPrivate *priv;
	EAttachment *attachment;
	const gchar *data;
	gpointer parent;
	gchar *copied_data;
	gchar **strv;
	gint length;

	/* _NETSCAPE_URL is represented as "URI\nTITLE" */

	priv = e_attachment_view_get_private (view);

	data = (const gchar *) gtk_selection_data_get_data (selection_data);
	length = gtk_selection_data_get_length (selection_data);

	copied_data = g_strndup (data, length);
	strv = g_strsplit (copied_data, "\n", 2);
	g_free (copied_data);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (view));
	parent = GTK_WIDGET_TOPLEVEL (parent) ? parent : NULL;

	attachment = e_attachment_new_for_uri (strv[0]);
	e_attachment_store_add_attachment (store, attachment);
	e_attachment_load_async (
		attachment, (GAsyncReadyCallback)
		e_attachment_load_handle_error, parent);
	g_object_unref (attachment);

	g_strfreev (strv);

	gtk_drag_finish (priv->drag_context, TRUE, FALSE, priv->time);
}

static void
drop_text_uri_list (EAttachmentView *view,
                    GtkSelectionData *selection_data,
                    EAttachmentStore *store,
                    GdkDragAction action)
{
	EAttachmentViewPrivate *priv;
	gpointer parent;
	gchar **uris;
	gint ii;

	priv = e_attachment_view_get_private (view);

	uris = gtk_selection_data_get_uris (selection_data);

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

	gtk_drag_finish (priv->drag_context, TRUE, FALSE, priv->time);
}

static void
drop_text_generic (EAttachmentView *view,
                   GtkSelectionData *selection_data,
                   EAttachmentStore *store,
                   GdkDragAction action)
{
	EAttachmentViewPrivate *priv;
	EAttachment *attachment;
	CamelMimePart *mime_part;
	GdkAtom atom;
	const gchar *data;
	gpointer parent;
	gchar *content_type;
	gint length;

	priv = e_attachment_view_get_private (view);

	data = (const gchar *) gtk_selection_data_get_data (selection_data);
	length = gtk_selection_data_get_length (selection_data);
	atom = gtk_selection_data_get_data_type (selection_data);

	mime_part = camel_mime_part_new ();

	content_type = gdk_atom_name (atom);
	camel_mime_part_set_content (mime_part, data, length, content_type);
	camel_mime_part_set_disposition (mime_part, "inline");
	g_free (content_type);

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

	gtk_drag_finish (priv->drag_context, TRUE, FALSE, priv->time);
}

static void
drop_x_uid_list (EAttachmentView *view,
                 GtkSelectionData *selection_data,
                 EAttachmentStore *store,
                 GdkDragAction action)
{
	EAttachmentViewPrivate *priv;

	/* FIXME  Ugh, this looks painful.  Requires mailer stuff. */

	priv = e_attachment_view_get_private (view);

	gtk_drag_finish (priv->drag_context, FALSE, FALSE, priv->time);
}

static void
attachment_view_update_actions (EAttachmentView *view)
{
	EAttachmentViewPrivate *priv;
	EAttachment *attachment;
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

	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);

	action = e_attachment_view_get_action (view, "cancel");
	gtk_action_set_visible (action, busy);

	action = e_attachment_view_get_action (view, "properties");
	gtk_action_set_visible (action, !busy && n_selected == 1);

	action = e_attachment_view_get_action (view, "remove");
	gtk_action_set_visible (action, !busy && n_selected > 0);

	action = e_attachment_view_get_action (view, "save-as");
	gtk_action_set_visible (action, !busy && n_selected > 0);

	/* Clear out the "openwith" action group. */
	gtk_ui_manager_remove_ui (priv->ui_manager, priv->merge_id);
	e_action_group_remove_all_actions (priv->openwith_actions);

	if (attachment == NULL || busy)
		return;

	list = e_attachment_list_apps (attachment);

	for (iter = list; iter != NULL; iter = iter->next) {
		GAppInfo *app_info = iter->data;
		GtkAction *action;
		const gchar *app_executable;
		const gchar *app_name;
		gchar *action_tooltip;
		gchar *action_label;
		gchar *action_name;

		if (!g_app_info_should_show (app_info))
			continue;

		app_executable = g_app_info_get_executable (app_info);
		app_name = g_app_info_get_name (app_info);

		action_name = g_strdup_printf ("open-in-%s", app_executable);
		action_label = g_strdup_printf (_("Open in %s..."), app_name);

		action_tooltip = g_strdup_printf (
			_("Open this attachment in %s"), app_name);

		action = gtk_action_new (
			action_name, action_label, action_tooltip, NULL);

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

		gtk_action_group_add_action (priv->openwith_actions, action);

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
attachment_view_class_init (EAttachmentViewIface *iface)
{
	gint ii;

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

	for (ii = 0; ii < G_N_ELEMENTS (drag_info); ii++) {
		const gchar *target = drag_info[ii].target;
		drag_info[ii].atom = gdk_atom_intern (target, FALSE);
	}
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
	}

	return type;
}

void
e_attachment_view_init (EAttachmentView *view)
{
	EAttachmentViewPrivate *priv;
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;
	const gchar *domain = GETTEXT_PACKAGE;
	GType *children;
	guint ii;
	GError *error = NULL;

	priv = e_attachment_view_get_private (view);

	e_attachment_view_drag_source_set (view);
	e_attachment_view_drag_dest_set (view);

	ui_manager = gtk_ui_manager_new ();
	priv->merge_id = gtk_ui_manager_new_merge_id (ui_manager);
	priv->ui_manager = ui_manager;

	action_group = gtk_action_group_new ("standard");
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_action_group_add_actions (
		action_group, standard_entries,
		G_N_ELEMENTS (standard_entries), view);
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
	priv->standard_actions = action_group;

	action_group = gtk_action_group_new ("editable");
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_action_group_add_actions (
		action_group, editable_entries,
		G_N_ELEMENTS (editable_entries), view);
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
	priv->editable_actions = action_group;

	action_group = gtk_action_group_new ("openwith");
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
	priv->openwith_actions = action_group;

	/* Because we are loading from a hard-coded string, there is
	 * no chance of I/O errors.  Failure here implies a malformed
	 * UI definition.  Full stop. */
	gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, &error);
	if (error != NULL)
		g_error ("%s", error->message);

	e_mutual_binding_new (
		G_OBJECT (view), "editable",
		G_OBJECT (priv->editable_actions), "visible");

	/* Instantiate attachment handlers. */
	children = g_type_children (E_TYPE_ATTACHMENT_HANDLER, NULL);
	for (ii = 0; children[ii] != G_TYPE_INVALID; ii++) {
		EAttachmentHandler *handler;
		handler = g_object_new (children[ii], "view", view, NULL);
		priv->handlers = g_list_prepend (priv->handlers, handler);
	}
	g_free (children);
}

void
e_attachment_view_dispose (EAttachmentView *view)
{
	EAttachmentViewPrivate *priv;

	priv = e_attachment_view_get_private (view);

	g_list_foreach (priv->handlers, (GFunc) g_object_unref, NULL);
	g_list_free (priv->handlers);
	priv->handlers = NULL;

	if (priv->ui_manager != NULL) {
		g_object_unref (priv->ui_manager);
		priv->ui_manager = NULL;
	}

	if (priv->standard_actions != NULL) {
		g_object_unref (priv->standard_actions);
		priv->standard_actions = NULL;
	}

	if (priv->editable_actions != NULL) {
		g_object_unref (priv->editable_actions);
		priv->editable_actions = NULL;
	}

	if (priv->openwith_actions != NULL) {
		g_object_unref (priv->openwith_actions);
		priv->openwith_actions = NULL;
	}

	if (priv->drag_context != NULL) {
		g_object_unref (priv->drag_context);
		priv->drag_context = NULL;
	}
}

void
e_attachment_view_finalize (EAttachmentView *view)
{
	EAttachmentViewPrivate *priv;

	priv = e_attachment_view_get_private (view);

	if (priv->selection_data != NULL)
		gtk_selection_data_free (priv->selection_data);
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
			e_attachment_view_show_popup_menu (view, event);
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

void
e_attachment_view_drag_data_get (EAttachmentView *view,
                                 GdkDragContext *context,
                                 GtkSelectionData *selection,
                                 guint info,
                                 guint time)
{
	GList *selected, *iter;
	gchar **uris;
	gint ii = 0;

	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));
	g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));
	g_return_if_fail (selection != NULL);

	selected = e_attachment_view_get_selected_attachments (view);
	if (selected == NULL)
		return;

	uris = g_malloc0 (sizeof (gchar *) * (g_list_length (selected) + 1));

	for (iter = selected; iter != NULL; iter = iter->next) {
		EAttachment *attachment = iter->data;
		GFile *file;

		/* FIXME Need to handle attachments with no GFile. */
		file = e_attachment_get_file (attachment);
		if (file == NULL)
			continue;

		uris[ii++] = g_file_get_uri (file);
	}

	gtk_selection_data_set_uris (selection, uris);

	g_strfreev (uris);
}

void
e_attachment_view_drag_dest_set (EAttachmentView *view)
{
	GtkTargetEntry *targets;
	GtkTargetList *list;
	gint n_targets;

	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));

	list = gtk_target_list_new (NULL, 0);
	/* FIXME Add targets here... */
	targets = gtk_target_table_new_from_list (list, &n_targets);

	gtk_drag_dest_set (
		GTK_WIDGET (view), GTK_DEST_DEFAULT_ALL,
		targets, n_targets, GDK_ACTION_COPY);

	gtk_target_table_free (targets, n_targets);
	gtk_target_list_unref (list);
}

void
e_attachment_view_drag_dest_unset (EAttachmentView *view)
{
	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));

	gtk_drag_dest_unset (GTK_WIDGET (view));
}

void
e_attachment_view_drag_action (EAttachmentView *view,
                               GdkDragAction action)
{
	EAttachmentViewPrivate *priv;
	GtkSelectionData *selection_data;
	EAttachmentStore *store;
	GdkAtom atom;
	gchar *name;

	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));

	priv = e_attachment_view_get_private (view);

	selection_data = priv->selection_data;
	store = e_attachment_view_get_store (view);
	atom = gtk_selection_data_get_data_type (selection_data);

	switch (priv->info) {
		case DND_TYPE_MESSAGE_RFC822:
			drop_message_rfc822 (
				view, selection_data, store, action);
			return;

		case DND_TYPE_NETSCAPE_URL:
			drop_netscape_url (
				view, selection_data, store, action);
			return;

		case DND_TYPE_TEXT_URI_LIST:
			drop_text_uri_list (
				view, selection_data, store, action);
			return;

		case DND_TYPE_TEXT_VCARD:
		case DND_TYPE_TEXT_CALENDAR:
			drop_text_generic (
				view, selection_data, store, action);
			return;

		case DND_TYPE_X_UID_LIST:
			drop_x_uid_list (
				view, selection_data, store, action);
			return;

		default:
			name = gdk_atom_name (atom);
			g_warning ("Unknown drag type: %s", name);
			g_free (name);
			break;
	}

	gtk_drag_finish (priv->drag_context, FALSE, FALSE, priv->time);
}

gboolean
e_attachment_view_drag_motion (EAttachmentView *view,
                               GdkDragContext *context,
                               gint x,
                               gint y,
                               guint time)
{
	GList *iter;
	GdkDragAction actions = 0;
	GdkDragAction chosen_action;

	g_return_val_if_fail (E_IS_ATTACHMENT_VIEW (view), FALSE);
	g_return_val_if_fail (GDK_IS_DRAG_CONTEXT (context), FALSE);

	/* Disallow drops if we're not editable. */
	if (!e_attachment_view_get_editable (view))
		return FALSE;

	for (iter = context->targets; iter != NULL; iter = iter->next) {
		GdkAtom atom = iter->data;
		gint ii;

		for (ii = 0; ii < G_N_ELEMENTS (drag_info); ii++)
			if (atom == drag_info[ii].atom)
				actions |= drag_info[ii].actions;
	}

	actions &= context->actions;
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
                                      GdkDragContext *context,
                                      gint x,
                                      gint y,
                                      GtkSelectionData *selection,
                                      guint info,
                                      guint time)
{
	EAttachmentViewPrivate *priv;
	GtkUIManager *ui_manager;
	GdkDragAction action;

	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));
	g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));
	g_return_if_fail (selection != NULL);

	priv = e_attachment_view_get_private (view);
	ui_manager = e_attachment_view_get_ui_manager (view);

	action = context->action;

	if (gtk_selection_data_get_data (selection) == NULL)
		return;

	if (gtk_selection_data_get_length (selection) == -1)
		return;

	if (priv->drag_context != NULL)
		g_object_unref (priv->drag_context);

	if (priv->selection_data != NULL)
		gtk_selection_data_free (priv->selection_data);

	priv->drag_context = g_object_ref (context);
	priv->selection_data = gtk_selection_data_copy (selection);
	priv->info = info;
	priv->time = time;

	if (action == GDK_ACTION_ASK) {
		GtkWidget *menu;

		menu = gtk_ui_manager_get_widget (ui_manager, "/dnd");
		g_return_if_fail (GTK_IS_MENU (menu));

		gtk_menu_popup (
			GTK_MENU (menu), NULL, NULL, NULL, NULL, 0, time);
	} else
		e_attachment_view_drag_action (view, action);
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
e_attachment_view_get_action_group (EAttachmentView *view,
                                    const gchar *group_name)
{
	GtkUIManager *ui_manager;

	g_return_val_if_fail (E_IS_ATTACHMENT_VIEW (view), NULL);
	g_return_val_if_fail (group_name != NULL, NULL);

	ui_manager = e_attachment_view_get_ui_manager (view);

	return e_lookup_action_group (ui_manager, group_name);
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
                                   GdkEventButton *event)
{
	GtkUIManager *ui_manager;
	GtkWidget *menu;

	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));

	e_attachment_view_update_actions (view);

	ui_manager = e_attachment_view_get_ui_manager (view);
	menu = gtk_ui_manager_get_widget (ui_manager, "/context");
	g_return_if_fail (GTK_IS_MENU (menu));

	if (event != NULL)
		gtk_menu_popup (
			GTK_MENU (menu), NULL, NULL, NULL, NULL,
			event->button, event->time);
	else
		gtk_menu_popup (
			GTK_MENU (menu), NULL, NULL, NULL, NULL,
			0, gtk_get_current_event_time ());
}

void
e_attachment_view_update_actions (EAttachmentView *view)
{
	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));

	g_signal_emit (view, signals[UPDATE_ACTIONS], 0);
}
