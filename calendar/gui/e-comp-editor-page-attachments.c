/*
 * Copyright (C) 2015 Red Hat, Inc. (www.redhat.com)
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include <e-util/e-util.h>

#include "comp-util.h"

#include "e-comp-editor-page-attachments.h"

#define NUM_VIEWS 2

struct _ECompEditorPageAttachmentsPrivate {
	GtkTreeModel *store;
	GtkWidget *notebook;
	GtkWidget *combo_box;
	GtkWidget *controls_container;
	GtkWidget *icon_view;
	GtkWidget *tree_view;
	GtkWidget *status_icon;
	GtkWidget *status_label;

	gulong store_row_inserted_handler_id;
	gulong store_row_deleted_handler_id;

	gint active_view;
	GSList *temporary_files;
};

enum {
	PROP_0,
	PROP_ACTIVE_VIEW
};

G_DEFINE_TYPE (ECompEditorPageAttachments, e_comp_editor_page_attachments, E_TYPE_COMP_EDITOR_PAGE)

static void
ecep_attachments_action_attach_cb (GtkAction *action,
				   ECompEditorPageAttachments *page_attachments)
{
	ECompEditor *comp_editor;
	EAttachmentStore *store;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_ATTACHMENTS (page_attachments));

	comp_editor = e_comp_editor_page_ref_editor (E_COMP_EDITOR_PAGE (page_attachments));
	store = E_ATTACHMENT_STORE (page_attachments->priv->store);

	e_attachment_store_run_load_dialog (store, GTK_WINDOW (comp_editor));

	g_clear_object (&comp_editor);
}

static void
ecep_attachments_select_page_cb (GtkAction *action,
				 ECompEditorPage *page)
{
	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_ATTACHMENTS (page));

	e_comp_editor_page_select (page);
}

static void
temporary_file_free (gpointer ptr)
{
	gchar *temporary_file = ptr;

	if (temporary_file) {
		gchar *sep;

		g_unlink (temporary_file);

		sep = strrchr (temporary_file, G_DIR_SEPARATOR);
		if (sep) {
			*sep = '\0';
			g_rmdir (temporary_file);
		}

		g_free (temporary_file);
	}
}

static void
ecep_attachments_update_status (ECompEditorPageAttachments *page_attachments)
{
	EAttachmentStore *store;
	GtkLabel *label;
	guint num_attachments;
	guint64 total_size;
	gchar *display_size;
	gchar *markup;

	store = E_ATTACHMENT_STORE (page_attachments->priv->store);
	label = GTK_LABEL (page_attachments->priv->status_label);

	num_attachments = e_attachment_store_get_num_attachments (store);
	total_size = e_attachment_store_get_total_size (store);
	display_size = g_format_size (total_size);

	if (total_size > 0)
		markup = g_strdup_printf (
			"<b>%d</b> %s (%s)", num_attachments, g_dngettext (GETTEXT_PACKAGE,
			"Attachment", "Attachments", num_attachments),
			display_size);
	else
		markup = g_strdup_printf (
			"<b>%d</b> %s", num_attachments, g_dngettext (GETTEXT_PACKAGE,
			"Attachment", "Attachments", num_attachments));
	gtk_label_set_markup (label, markup);
	g_free (markup);

	g_free (display_size);
}

static void
ecep_attachments_attachment_loaded_cb (EAttachment *attachment,
				       GAsyncResult *result,
				       ECompEditorPageAttachments *page_attachments)
{
	GFileInfo *file_info;
	const gchar *display_name;
	const gchar *uid;
	gchar *new_name;
	GError *error = NULL;

	/* Prior to 2.27.2, attachment files were named:
	 *
	 *     <component-uid> '-' <actual-filename>
	 *     -------------------------------------
	 *              (one long filename)
	 *
	 * Here we fix the display name if this form is detected so we
	 * don't show the component UID in the user interface.  If the
	 * user saves changes in the editor, the attachment will be
	 * written to disk as:
	 *
	 *     <component-uid> / <actual-filename>
	 *     ---------------   -----------------
	 *       (directory)      (original name)
	 *
	 * So this is a lazy migration from the old form to the new.
	 */

	file_info = e_attachment_ref_file_info (attachment);
	if (file_info) {
		display_name = g_file_info_get_display_name (file_info);
		uid = g_object_get_data (G_OBJECT (attachment), "uid");

		if (g_str_has_prefix (display_name, uid)) {
			new_name = g_strdup (display_name + strlen (uid) + 1);
			g_file_info_set_display_name (file_info, new_name);
			g_object_notify (G_OBJECT (attachment), "file-info");
			g_free (new_name);
		}
	}

	if (!e_attachment_load_finish (attachment, result, &error)) {
		GtkTreeRowReference *reference;

		reference = e_attachment_get_reference (attachment);
		if (gtk_tree_row_reference_valid (reference)) {
			GtkTreeModel *model;

			model = gtk_tree_row_reference_get_model (reference);

			e_attachment_store_remove_attachment (
				E_ATTACHMENT_STORE (model), attachment);
		}

		/* Ignore cancellations. */
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
			ECompEditor *comp_editor;
			EAlert *alert;
			gchar *primary_text;

			comp_editor = e_comp_editor_page_ref_editor (E_COMP_EDITOR_PAGE (page_attachments));

			if (file_info)
				display_name = g_file_info_get_display_name (file_info);
			else
				display_name = NULL;

			if (display_name != NULL)
				primary_text = g_strdup_printf (_("Could not load '%s'"), display_name);
			else
				primary_text = g_strdup (_("Could not load the attachment"));

			alert = e_comp_editor_add_error (comp_editor, primary_text,
				error ? error->message : _("Unknown error"));

			g_clear_object (&comp_editor);
			g_clear_object (&alert);
			g_free (primary_text);
		}
	}

	g_clear_object (&file_info);
	g_clear_error (&error);
}

static void
ecep_attachments_sensitize_widgets (ECompEditorPage *page,
				    gboolean force_insensitive)
{
	ECompEditorPageAttachments *page_attachments;
	ECompEditor *comp_editor;
	GtkAction *action;
	guint32 flags;
	gboolean is_organizer;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_ATTACHMENTS (page));

	E_COMP_EDITOR_PAGE_CLASS (e_comp_editor_page_attachments_parent_class)->sensitize_widgets (page, force_insensitive);

	comp_editor = e_comp_editor_page_ref_editor (page);
	flags = e_comp_editor_get_flags (comp_editor);

	is_organizer = (flags & (E_COMP_EDITOR_FLAG_IS_NEW | E_COMP_EDITOR_FLAG_ORGANIZER_IS_USER)) != 0;

	page_attachments = E_COMP_EDITOR_PAGE_ATTACHMENTS (page);

	gtk_widget_set_sensitive (page_attachments->priv->controls_container, !force_insensitive);
	gtk_widget_set_sensitive (page_attachments->priv->notebook, !force_insensitive);

	action = e_comp_editor_get_action (comp_editor, "attachments-attach");
	gtk_action_set_sensitive (action, !force_insensitive && is_organizer);

	g_clear_object (&comp_editor);
}

static void
ecep_attachments_fill_widgets (ECompEditorPage *page,
			       icalcomponent *component)
{
	ECompEditorPageAttachments *page_attachments;
	EAttachmentStore *store;
	icalproperty *prop;
	const gchar *uid;
	gint index;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_ATTACHMENTS (page));
	g_return_if_fail (component != NULL);

	E_COMP_EDITOR_PAGE_CLASS (e_comp_editor_page_attachments_parent_class)->fill_widgets (page, component);

	page_attachments = E_COMP_EDITOR_PAGE_ATTACHMENTS (page);
	store = E_ATTACHMENT_STORE (page_attachments->priv->store);

	uid = icalcomponent_get_uid (component);

	g_slist_free_full (page_attachments->priv->temporary_files, temporary_file_free);
	page_attachments->priv->temporary_files = NULL;

	e_attachment_store_remove_all (store);

	for (prop = icalcomponent_get_first_property (component, ICAL_ATTACH_PROPERTY), index = 0;
	     prop;
	     prop = icalcomponent_get_next_property (component, ICAL_ATTACH_PROPERTY), index++) {
		icalattach *attach;
		gchar *uri = NULL;

		attach = icalproperty_get_attach (prop);
		if (!attach)
			continue;

		if (icalattach_get_is_url (attach)) {
			const gchar *data;
			gsize buf_size;

			data = icalattach_get_url (attach);
			buf_size = strlen (data);
			uri = g_malloc0 (buf_size + 1);

			icalvalue_decode_ical_string (data, uri, buf_size);
		} else {
			gchar *temporary_filename = NULL;
			icalparameter *encoding_par = icalproperty_get_first_parameter (prop, ICAL_ENCODING_PARAMETER);
			if (encoding_par) {
				gchar *str_value = icalproperty_get_value_as_string_r (prop);

				if (str_value) {
					icalparameter_encoding encoding = icalparameter_get_encoding (encoding_par);
					guint8 *data = NULL;
					gsize data_len = 0;

					switch (encoding) {
					case ICAL_ENCODING_8BIT:
						data = (guint8 *) str_value;
						data_len = strlen (str_value);
						str_value = NULL;
						break;
					case ICAL_ENCODING_BASE64:
						data = g_base64_decode (str_value, &data_len);
						break;
					default:
						break;
					}

					if (data) {
						gchar *dir, *id_str;
						struct icaltimetype rid_tt;
						gchar *rid;

						rid_tt = icalcomponent_get_recurrenceid (component);
						if (icaltime_is_null_time (rid_tt) || !icaltime_is_valid_time (rid_tt))
							rid = NULL;
						else
							rid = icaltime_as_ical_string_r (rid_tt);

						id_str = g_strconcat (uid, rid ? "-" : NULL, rid, NULL);

						dir = g_build_filename (e_get_user_cache_dir (), "tmp", "calendar", id_str, NULL);

						g_free (rid);
						g_free (id_str);

						if (g_mkdir_with_parents (dir, 0700) >= 0) {
							icalparameter *param;
							gchar *file = NULL;

							for (param = icalproperty_get_first_parameter (prop, ICAL_X_PARAMETER);
							     param && !file;
							     param = icalproperty_get_next_parameter (prop, ICAL_X_PARAMETER)) {
								if (e_util_strstrcase (icalparameter_get_xname (param), "NAME") &&
								    icalparameter_get_xvalue (param) &&
								    *icalparameter_get_xvalue (param))
									file = g_strdup (icalparameter_get_xvalue (param));
							}

							if (!file)
								file = g_strdup_printf ("%d.dat", index);

							temporary_filename = g_build_filename (dir, file, NULL);
							if (!g_file_set_contents (temporary_filename, (const gchar *) data, data_len, NULL)) {
								g_free (temporary_filename);
								temporary_filename = NULL;
							}
						}

						g_free (dir);
					}

					g_free (str_value);
					g_free (data);
				}
			}

			if (temporary_filename) {
				uri = g_filename_to_uri (temporary_filename, NULL, NULL);
				page_attachments->priv->temporary_files = g_slist_prepend (page_attachments->priv->temporary_files, temporary_filename);
			}
		}

		if (uri) {
			EAttachment *attachment;

			attachment = e_attachment_new_for_uri (uri);
			e_attachment_store_add_attachment (store, attachment);
			g_object_set_data_full (
				G_OBJECT (attachment),
				"uid", g_strdup (uid),
				(GDestroyNotify) g_free);
			e_attachment_load_async (
				attachment, (GAsyncReadyCallback)
				ecep_attachments_attachment_loaded_cb, page_attachments);
			g_object_unref (attachment);
		}

		g_free (uri);
	}
}

static gboolean
ecep_attachments_fill_component (ECompEditorPage *page,
				 icalcomponent *component)
{
	ECompEditorPageAttachments *page_attachments;
	ECompEditor *comp_editor;
	GList *attachments, *link;
	icalproperty *prop;
	gboolean success = TRUE;

	g_return_val_if_fail (E_IS_COMP_EDITOR_PAGE_ATTACHMENTS (page), FALSE);
	g_return_val_if_fail (component != NULL, FALSE);

	comp_editor = e_comp_editor_page_ref_editor (page);
	page_attachments = E_COMP_EDITOR_PAGE_ATTACHMENTS (page);

	if (e_attachment_store_get_num_loading (E_ATTACHMENT_STORE (page_attachments->priv->store)) > 0) {
		e_comp_editor_set_validation_error (comp_editor, page, NULL,
			_("Some attachments are still being downloaded. Please wait until the download is finished."));
		g_clear_object (&comp_editor);
		return FALSE;
	}

	cal_comp_util_remove_all_properties (component, ICAL_ATTACH_PROPERTY);

	attachments = e_attachment_store_get_attachments (E_ATTACHMENT_STORE (page_attachments->priv->store));
	for (link = attachments; link; link = g_list_next (link)) {
		EAttachment *attachment = link->data;
		icalattach *attach;
		gsize buf_size;
		gchar *buf, *uri, *description;
		GFile *file;

		if (!attachment)
			continue;

		description = e_attachment_dup_description (attachment);

		file = e_attachment_ref_file (attachment);
		if (!file) {
			gchar *error_message;

			success = FALSE;

			error_message = g_strdup_printf (
				_("Attachment '%s' cannot be found, remove it from the list, please"),
				description);

			e_comp_editor_set_validation_error (comp_editor, page, NULL, error_message);

			g_free (description);
			g_free (error_message);
			break;
		}

		uri = g_file_get_uri (file);
		if (!uri) {
			gchar *error_message;

			success = FALSE;

			error_message = g_strdup_printf (
				_("Attachment '%s' doesn't have valid URI, remove it from the list, please"),
				description);

			e_comp_editor_set_validation_error (comp_editor, page, NULL, error_message);

			g_free (description);
			g_free (error_message);
			g_object_unref (file);
			break;
		}

		g_object_unref (file);
		g_free (description);

		buf_size = 2 * strlen (uri) + 1;
		buf = g_malloc0 (buf_size);

		icalvalue_encode_ical_string (uri, buf, buf_size);
		attach = icalattach_new_from_url (buf);
		prop = icalproperty_new_attach (attach);
		icalcomponent_add_property (component, prop);

		icalattach_unref (attach);
		g_free (buf);
		g_free (uri);
	}

	g_list_free_full (attachments, g_object_unref);
	g_clear_object (&comp_editor);

	return success && E_COMP_EDITOR_PAGE_CLASS (e_comp_editor_page_attachments_parent_class)->fill_component (page, component);
}

static gboolean
ecep_attachments_drag_motion (GtkWidget *widget,
			      GdkDragContext *context,
			      gint x,
			      gint y,
			      guint time)
{
	ECompEditorPageAttachments *page_attachments;
	EAttachmentView *attachment_view;

	g_return_val_if_fail (E_IS_COMP_EDITOR_PAGE_ATTACHMENTS (widget), FALSE);

	page_attachments = E_COMP_EDITOR_PAGE_ATTACHMENTS (widget);
	attachment_view = E_ATTACHMENT_VIEW (page_attachments->priv->icon_view);

	return e_attachment_view_drag_motion (attachment_view, context, x, y, time);
}

static void
ecep_attachments_drag_data_received (GtkWidget *widget,
				     GdkDragContext *context,
				     gint x,
				     gint y,
				     GtkSelectionData *selection,
				     guint info,
				     guint time)
{
	ECompEditorPageAttachments *page_attachments;
	EAttachmentView *attachment_view;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_ATTACHMENTS (widget));

	page_attachments = E_COMP_EDITOR_PAGE_ATTACHMENTS (widget);
	attachment_view = E_ATTACHMENT_VIEW (page_attachments->priv->icon_view);

	/* Forward the data to the attachment view.  Note that calling
	 * e_attachment_view_drag_data_received() will not work because
	 * that function only handles the case where all the other drag
	 * handlers have failed. */

	/* XXX Dirty hack for forwarding drop events. */
	g_signal_emit_by_name (
		attachment_view, "drag-data-received",
		context, x, y, selection, info, time);
}

static void
ecep_attachments_set_property (GObject *object,
			       guint property_id,
			       const GValue *value,
			       GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACTIVE_VIEW:
			e_comp_editor_page_attachments_set_active_view (
				E_COMP_EDITOR_PAGE_ATTACHMENTS (object),
				g_value_get_int (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
ecep_attachments_get_property (GObject *object,
                               guint property_id,
                               GValue *value,
                               GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACTIVE_VIEW:
			g_value_set_int (
				value,
				e_comp_editor_page_attachments_get_active_view (
				E_COMP_EDITOR_PAGE_ATTACHMENTS (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
ecep_attachments_dispose (GObject *object)
{
	ECompEditorPageAttachments *page_attachments;

	page_attachments = E_COMP_EDITOR_PAGE_ATTACHMENTS (object);

	if (page_attachments->priv->store) {
		gpointer store = page_attachments->priv->store;

		e_signal_disconnect_notify_handler (store, &page_attachments->priv->store_row_inserted_handler_id);
		e_signal_disconnect_notify_handler (store, &page_attachments->priv->store_row_deleted_handler_id);
	}

	g_clear_object (&page_attachments->priv->store);

	g_slist_free_full (page_attachments->priv->temporary_files, temporary_file_free);
	page_attachments->priv->temporary_files = NULL;

	G_OBJECT_CLASS (e_comp_editor_page_attachments_parent_class)->dispose (object);
}

static void
ecep_attachments_setup_ui (ECompEditorPageAttachments *page_attachments)
{
	const gchar *ui =
		"<ui>"
		"  <menubar action='main-menu'>"
		"    <menu action='insert-menu'>"
		"      <menuitem action='attachments-attach'/>"
		"    </menu>"
		"    <menu action='options-menu'>"
		"      <placeholder name='tabs'>"
		"        <menuitem action='page-attachments'/>"
		"      </placeholder>"
		"    </menu>"
		"  </menubar>"
		"  <toolbar name='main-toolbar'>"
		"    <placeholder name='content'>\n"
		"      <toolitem action='page-attachments'/>\n"
		"    </placeholder>"
		"  </toolbar>"
		"</ui>";

	GtkActionEntry editable_entries[] = {
		{ "attachments-attach",
		  "mail-attachment",
		  N_("_Attachment..."),
		  "<Control>m",
		  N_("Attach a file"),
		  G_CALLBACK (ecep_attachments_action_attach_cb) }
	};

	GtkActionEntry options_entries[] = {
		{ "page-attachments",
		  "mail-attachment",
		  N_("_Attachments"),
		  NULL,
		  N_("Show attachments"),
		  G_CALLBACK (ecep_attachments_select_page_cb) }
	};

	ECompEditor *comp_editor;
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;
	GError *error = NULL;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_ATTACHMENTS (page_attachments));

	comp_editor = e_comp_editor_page_ref_editor (E_COMP_EDITOR_PAGE (page_attachments));
	ui_manager = e_comp_editor_get_ui_manager (comp_editor);
	action_group = e_comp_editor_get_action_group (comp_editor, "editable");

	gtk_action_group_add_actions (
		action_group, editable_entries,
		G_N_ELEMENTS (editable_entries), page_attachments);

	action_group = e_comp_editor_get_action_group (comp_editor, "individual");

	gtk_action_group_add_actions (
		action_group, options_entries,
		G_N_ELEMENTS (options_entries), page_attachments);

	gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, &error);
	if (error != NULL) {
		g_warning ("%s: Failed to add UI from string: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}

	g_clear_object (&comp_editor);
}

static void
ecep_attachments_constructed (GObject *object)
{
	ECompEditorPageAttachments *page_attachments;
	ECompEditor *comp_editor;
	GSettings *settings;
	GtkSizeGroup *size_group;
	GtkWidget *container;
	GtkWidget *widget;
	GtkAction *action;

	G_OBJECT_CLASS (e_comp_editor_page_attachments_parent_class)->constructed (object);

	page_attachments = E_COMP_EDITOR_PAGE_ATTACHMENTS (object);

	page_attachments->priv->store = e_attachment_store_new ();

	page_attachments->priv->store_row_inserted_handler_id =
		g_signal_connect_swapped (page_attachments->priv->store, "row-inserted",
			G_CALLBACK (e_comp_editor_page_emit_changed), page_attachments);
	page_attachments->priv->store_row_deleted_handler_id =
		g_signal_connect_swapped (page_attachments->priv->store, "row-deleted",
			G_CALLBACK (e_comp_editor_page_emit_changed), page_attachments);

	/* Keep the expander label and combo box the same height. */
	size_group = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);

	/* Construct the Attachment Views */

	widget = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (widget), FALSE);
	gtk_grid_attach (GTK_GRID (page_attachments), widget, 0, 1, 1, 1);
	page_attachments->priv->notebook = widget;
	gtk_widget_show (widget);

	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		NULL);

	container = page_attachments->priv->notebook;

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_notebook_append_page (GTK_NOTEBOOK (container), widget, NULL);
	gtk_widget_show (widget);

	container = widget;

	widget = e_attachment_icon_view_new ();
	gtk_widget_set_can_focus (widget, TRUE);
	gtk_icon_view_set_model (GTK_ICON_VIEW (widget), page_attachments->priv->store);
	gtk_container_add (GTK_CONTAINER (container), widget);
	page_attachments->priv->icon_view = widget;
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		NULL);
	gtk_widget_show (widget);

	container = page_attachments->priv->notebook;

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_notebook_append_page (GTK_NOTEBOOK (container), widget, NULL);
	gtk_widget_show (widget);

	container = widget;

	widget = e_attachment_tree_view_new ();
	gtk_widget_set_can_focus (widget, TRUE);
	gtk_tree_view_set_model (GTK_TREE_VIEW (widget), page_attachments->priv->store);
	gtk_container_add (GTK_CONTAINER (container), widget);
	page_attachments->priv->tree_view = widget;
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		NULL);
	gtk_widget_show (widget);

	/* Construct the Controls */

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_grid_attach (GTK_GRID (page_attachments), widget, 0, 0, 1, 1);
	gtk_widget_show (widget);
	page_attachments->priv->controls_container = widget;

	container = widget;

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_margin_right (widget, 6);
	gtk_widget_set_margin_left (widget, 6);
	gtk_box_pack_end (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	/* The "Add Attachment" button proxies the "add" action from
	 * one of the two attachment views.  Doesn't matter which. */
	widget = gtk_button_new ();
	action = e_attachment_view_get_action (E_ATTACHMENT_VIEW (page_attachments->priv->icon_view), "add");
	gtk_button_set_image (GTK_BUTTON (widget), gtk_image_new ());
	gtk_activatable_set_related_action (GTK_ACTIVATABLE (widget), action);
	gtk_box_pack_end (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = gtk_combo_box_text_new ();
	gtk_size_group_add_widget (size_group, widget);
	gtk_combo_box_text_append_text (
		GTK_COMBO_BOX_TEXT (widget), _("Icon View"));
	gtk_combo_box_text_append_text (
		GTK_COMBO_BOX_TEXT (widget), _("List View"));
	gtk_box_pack_end (GTK_BOX (container), widget, FALSE, FALSE, 0);
	page_attachments->priv->combo_box = widget;
	gtk_widget_show (widget);

	widget = gtk_image_new_from_icon_name (
		"mail-attachment", GTK_ICON_SIZE_MENU);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	page_attachments->priv->status_icon = widget;
	gtk_widget_show (widget);

	widget = gtk_label_new (NULL);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	page_attachments->priv->status_label = widget;
	gtk_widget_show (widget);

	e_signal_connect_notify_swapped (
		page_attachments->priv->store, "notify::num-attachments",
		G_CALLBACK (ecep_attachments_update_status), page_attachments);

	e_signal_connect_notify_swapped (
		page_attachments->priv->store, "notify::total-size",
		G_CALLBACK (ecep_attachments_update_status), page_attachments);

	g_object_unref (size_group);

	ecep_attachments_update_status (page_attachments);

	e_binding_bind_property (
		object, "active-view",
		page_attachments->priv->combo_box, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		object, "active-view",
		page_attachments->priv->notebook, "page",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	settings = e_util_ref_settings ("org.gnome.evolution.shell");

	g_settings_bind (
		settings, "attachment-view",
		object, "active-view",
		G_SETTINGS_BIND_DEFAULT);

	g_clear_object (&settings);

	ecep_attachments_setup_ui (page_attachments);

	comp_editor = e_comp_editor_page_ref_editor (E_COMP_EDITOR_PAGE (page_attachments));
	action = e_comp_editor_get_action (comp_editor, "attachments-attach");

	e_binding_bind_property (
		action, "sensitive",
		e_attachment_view_get_action (E_ATTACHMENT_VIEW (page_attachments->priv->icon_view), "add"), "sensitive",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		action, "sensitive",
		e_attachment_view_get_action (E_ATTACHMENT_VIEW (page_attachments->priv->tree_view), "add"), "sensitive",
		G_BINDING_SYNC_CREATE);

	g_clear_object (&comp_editor);
}

static void
e_comp_editor_page_attachments_init (ECompEditorPageAttachments *page_attachments)
{
	page_attachments->priv = G_TYPE_INSTANCE_GET_PRIVATE (page_attachments,
		E_TYPE_COMP_EDITOR_PAGE_ATTACHMENTS,
		ECompEditorPageAttachmentsPrivate);
}

static void
e_comp_editor_page_attachments_class_init (ECompEditorPageAttachmentsClass *klass)
{
	ECompEditorPageClass *page_class;
	GtkWidgetClass *widget_class;
	GObjectClass *object_class;

	g_type_class_add_private (klass, sizeof (ECompEditorPageAttachmentsPrivate));

	page_class = E_COMP_EDITOR_PAGE_CLASS (klass);
	page_class->sensitize_widgets = ecep_attachments_sensitize_widgets;
	page_class->fill_widgets = ecep_attachments_fill_widgets;
	page_class->fill_component = ecep_attachments_fill_component;

	widget_class = GTK_WIDGET_CLASS (klass);
	widget_class->drag_motion = ecep_attachments_drag_motion;
	widget_class->drag_data_received = ecep_attachments_drag_data_received;

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = ecep_attachments_set_property;
	object_class->get_property = ecep_attachments_get_property;
	object_class->dispose = ecep_attachments_dispose;
	object_class->constructed = ecep_attachments_constructed;

	g_object_class_install_property (
		object_class,
		PROP_ACTIVE_VIEW,
		g_param_spec_int (
			"active-view",
			"Active View",
			NULL,
			0,
			NUM_VIEWS,
			0,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));
}

ECompEditorPage *
e_comp_editor_page_attachments_new (ECompEditor *editor)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR (editor), NULL);

	return g_object_new (E_TYPE_COMP_EDITOR_PAGE_ATTACHMENTS,
		"editor", editor,
		NULL);
}

gint
e_comp_editor_page_attachments_get_active_view (ECompEditorPageAttachments *page_attachments)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR_PAGE_ATTACHMENTS (page_attachments), 0);

	return page_attachments->priv->active_view;
}

void
e_comp_editor_page_attachments_set_active_view (ECompEditorPageAttachments *page_attachments,
						gint view)
{
	EAttachmentView *source;
	EAttachmentView *target;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_ATTACHMENTS (page_attachments));
	g_return_if_fail (view >= 0 && view < NUM_VIEWS);

	if (view == page_attachments->priv->active_view)
		return;

	page_attachments->priv->active_view = view;

	/* Synchronize the item selection of the view we're
	 * switching TO with the view we're switching FROM. */
	if (view == 0) {
		/* from tree view to icon view */
		source = E_ATTACHMENT_VIEW (page_attachments->priv->tree_view);
		target = E_ATTACHMENT_VIEW (page_attachments->priv->icon_view);
	} else {
		/* from icon view to tree view */
		source = E_ATTACHMENT_VIEW (page_attachments->priv->icon_view);
		target = E_ATTACHMENT_VIEW (page_attachments->priv->tree_view);
	}

	e_attachment_view_sync_selection (source, target);

	g_object_notify (G_OBJECT (page_attachments), "active-view");
}

EAttachmentStore *
e_comp_editor_page_attachments_get_store (ECompEditorPageAttachments *page_attachments)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR_PAGE_ATTACHMENTS (page_attachments), NULL);

	return E_ATTACHMENT_STORE (page_attachments->priv->store);
}
