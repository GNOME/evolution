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

#include "evolution-config.h"

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
	GtkWidget *add_uri_button;

	gulong store_row_inserted_handler_id;
	gulong store_row_deleted_handler_id;

	gint active_view;
	GSList *temporary_files;
};

enum {
	PROP_0,
	PROP_ACTIVE_VIEW
};

G_DEFINE_TYPE_WITH_PRIVATE (ECompEditorPageAttachments, e_comp_editor_page_attachments, E_TYPE_COMP_EDITOR_PAGE)

static gboolean
ecep_before_properties_popup_cb (EAttachmentView *view,
				 GtkPopover *popover,
				 gboolean is_new_attachment,
				 gpointer user_data)
{
	ECompEditorPageAttachments *self = user_data;

	if (is_new_attachment) {
		GdkRectangle rect;

		gtk_widget_get_allocation (self->priv->add_uri_button, &rect);
		rect.x = 0;
		rect.y = 0;

		gtk_popover_set_relative_to (popover, self->priv->add_uri_button);
		gtk_popover_set_pointing_to (popover, &rect);
	} else {
		EAttachmentView *active_view;

		if (self->priv->active_view == 0) {
			active_view = E_ATTACHMENT_VIEW (self->priv->icon_view);
		} else {
			active_view = E_ATTACHMENT_VIEW (self->priv->tree_view);
		}

		e_attachment_view_position_popover (active_view, popover, e_attachment_popover_get_attachment (E_ATTACHMENT_POPOVER (popover)));
	}

	return TRUE;
}

static void
ecep_attachments_action_attach_cb (EUIAction *action,
				   GVariant *parameter,
				   gpointer user_data)
{
	ECompEditorPageAttachments *page_attachments = user_data;
	ECompEditor *comp_editor;
	EAttachmentStore *store;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_ATTACHMENTS (page_attachments));

	comp_editor = e_comp_editor_page_ref_editor (E_COMP_EDITOR_PAGE (page_attachments));
	store = E_ATTACHMENT_STORE (page_attachments->priv->store);

	e_attachment_store_run_load_dialog (store, GTK_WINDOW (comp_editor));

	g_clear_object (&comp_editor);
}

static void
ecep_attachments_select_page_cb (EUIAction *action,
				 GVariant *parameter,
				 gpointer user_data)
{
	ECompEditorPage *page = user_data;

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
		const gchar *uid;
		const gchar *prefer_filename;

		if (g_file_info_has_attribute (file_info, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME))
			display_name = g_file_info_get_display_name (file_info);
		else
			display_name = NULL;
		uid = g_object_get_data (G_OBJECT (attachment), "uid");
		prefer_filename = g_object_get_data (G_OBJECT (attachment), "prefer-filename");

		if (prefer_filename && *prefer_filename) {
			g_file_info_set_display_name (file_info, prefer_filename);
			g_object_notify (G_OBJECT (attachment), "file-info");
		} else if (display_name && g_str_has_prefix (display_name, uid)) {
			gchar *new_name;

			new_name = g_strdup (display_name + strlen (uid) + 1);
			g_file_info_set_display_name (file_info, new_name);
			g_object_notify (G_OBJECT (attachment), "file-info");
			g_free (new_name);
		}
	}

	if (result && !e_attachment_load_finish (attachment, result, &error)) {
		g_signal_emit_by_name (attachment, "load-failed", NULL);

		/* Ignore cancellations. */
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
			ECompEditor *comp_editor;
			EAlert *alert;
			gchar *primary_text;

			comp_editor = e_comp_editor_page_ref_editor (E_COMP_EDITOR_PAGE (page_attachments));

			if (file_info && g_file_info_has_attribute (file_info, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME))
				display_name = g_file_info_get_display_name (file_info);
			else
				display_name = NULL;

			if (display_name != NULL)
				primary_text = g_strdup_printf (_("Could not load “%s”"), display_name);
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
	ECompEditor *comp_editor;
	EUIAction *action;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_ATTACHMENTS (page));

	E_COMP_EDITOR_PAGE_CLASS (e_comp_editor_page_attachments_parent_class)->sensitize_widgets (page, force_insensitive);

	comp_editor = e_comp_editor_page_ref_editor (page);

	action = e_comp_editor_get_action (comp_editor, "attachments-attach");
	e_ui_action_set_sensitive (action, !force_insensitive);

	g_clear_object (&comp_editor);
}

static void
ecep_attachments_fill_widgets (ECompEditorPage *page,
			       ICalComponent *component)
{
	ECompEditorPageAttachments *page_attachments;
	EAttachmentStore *store;
	GPtrArray *attach_props;
	ICalProperty *prop;
	const gchar *uid;
	gint index;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_ATTACHMENTS (page));
	g_return_if_fail (I_CAL_IS_COMPONENT (component));

	E_COMP_EDITOR_PAGE_CLASS (e_comp_editor_page_attachments_parent_class)->fill_widgets (page, component);

	page_attachments = E_COMP_EDITOR_PAGE_ATTACHMENTS (page);
	store = E_ATTACHMENT_STORE (page_attachments->priv->store);

	uid = i_cal_component_get_uid (component);

	g_slist_free_full (page_attachments->priv->temporary_files, temporary_file_free);
	page_attachments->priv->temporary_files = NULL;

	e_attachment_store_remove_all (store);

	index = i_cal_component_count_properties (component, I_CAL_ATTACH_PROPERTY);
	if (index <= 0)
		return;

	attach_props = g_ptr_array_new_full (index, g_object_unref);

	/* because e_cal_util_component_get_recurid_as_string() also uses i_cal_component_get_first_property(),
	   which breaks this cycle */
	for (prop = i_cal_component_get_first_property (component, I_CAL_ATTACH_PROPERTY);
	     prop;
	     prop = i_cal_component_get_next_property (component, I_CAL_ATTACH_PROPERTY)) {
		g_ptr_array_add (attach_props, prop);
	}

	for (index = 0; index < attach_props->len; index++) {
		ICalParameter *param;
		ICalAttach *attach;
		gchar *uri = NULL, *filename = NULL;

		prop = g_ptr_array_index (attach_props, index);
		attach = i_cal_property_get_attach (prop);
		if (!attach)
			continue;

		param = i_cal_property_get_first_parameter (prop, I_CAL_FILENAME_PARAMETER);
		if (param) {
			filename = g_strdup (i_cal_parameter_get_filename (param));
			if (!filename || !*filename) {
				g_free (filename);
				filename = NULL;
			}

			g_clear_object (&param);
		}

		if (i_cal_attach_get_is_url (attach)) {
			const gchar *data;

			data = i_cal_attach_get_url (attach);
			uri = i_cal_value_decode_ical_string (data);

			if (!filename)
				filename = cal_comp_util_dup_attach_filename (prop, TRUE);
		} else {
			gchar *temporary_filename = NULL;
			ICalParameter *encoding_par = i_cal_property_get_first_parameter (prop, I_CAL_ENCODING_PARAMETER);
			if (encoding_par) {
				gchar *str_value = i_cal_property_get_value_as_string (prop);

				if (str_value) {
					ICalParameterEncoding encoding = i_cal_parameter_get_encoding (encoding_par);
					guint8 *data = NULL;
					gsize data_len = 0;

					switch (encoding) {
					case I_CAL_ENCODING_8BIT:
						data = (guint8 *) str_value;
						data_len = strlen (str_value);
						str_value = NULL;
						break;
					case I_CAL_ENCODING_BASE64:
						data = g_base64_decode (str_value, &data_len);
						break;
					default:
						break;
					}

					if (data) {
						gchar *dir, *id_str;
						gchar *rid;

						rid = e_cal_util_component_get_recurid_as_string (component);
						if (rid && !*rid) {
							g_free (rid);
							rid = NULL;
						}

						id_str = g_strconcat (uid, rid ? "-" : NULL, rid, NULL);

						dir = g_build_filename (e_get_user_cache_dir (), "tmp", "calendar", id_str, NULL);

						g_free (rid);
						g_free (id_str);

						if (g_mkdir_with_parents (dir, 0700) >= 0) {
							for (param = i_cal_property_get_first_parameter (prop, I_CAL_X_PARAMETER);
							     param && !filename;
							     g_object_unref (param), param = i_cal_property_get_next_parameter (prop, I_CAL_X_PARAMETER)) {
								if (e_util_strstrcase (i_cal_parameter_get_xname (param), "NAME") &&
								    i_cal_parameter_get_xvalue (param) &&
								    *i_cal_parameter_get_xvalue (param)) {
									filename = g_strdup (i_cal_parameter_get_xvalue (param));
									if (!filename || !*filename) {
										g_free (filename);
										filename = NULL;
									}
								}
							}

							g_clear_object (&param);

							if (!filename)
								filename = g_strdup_printf ("%d.dat", index);

							temporary_filename = g_build_filename (dir, filename, NULL);
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

				g_object_unref (encoding_par);
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
			g_object_set_data_full (G_OBJECT (attachment), "uid", g_strdup (uid), g_free);
			if (filename)
				g_object_set_data_full (G_OBJECT (attachment), "prefer-filename", g_strdup (filename), g_free);

			if (g_ascii_strncasecmp (uri, "file://", 7) == 0) {
				e_attachment_load_async (attachment, (GAsyncReadyCallback) ecep_attachments_attachment_loaded_cb, page_attachments);
			} else {
				GFileInfo *file_info;
				GPtrArray *add_params = NULL;

				for (param = i_cal_property_get_first_parameter (prop, I_CAL_ANY_PARAMETER);
				     param;
				     g_object_unref (param), param = i_cal_property_get_next_parameter (prop, I_CAL_ANY_PARAMETER)) {
					ICalParameterKind kind = i_cal_parameter_isa (param);

					/* preserve unknown/unset parameters from the existing property */
					if (kind != I_CAL_FILENAME_PARAMETER &&
					    kind != I_CAL_FMTTYPE_PARAMETER &&
					    kind != I_CAL_XLICCOMPARETYPE_PARAMETER &&
					    kind != I_CAL_XLICERRORTYPE_PARAMETER) {
						if (!add_params)
							add_params = g_ptr_array_new_with_free_func (g_object_unref);
						g_ptr_array_add (add_params, i_cal_parameter_clone (param));
					}
				}

				if (add_params)
					g_object_set_data_full (G_OBJECT (attachment), "ical-params", add_params, (GDestroyNotify) g_ptr_array_unref);

				file_info = g_file_info_new ();
				g_file_info_set_content_type (file_info, "application/octet-stream");

				param = i_cal_property_get_first_parameter (prop, I_CAL_FMTTYPE_PARAMETER);
				if (param) {
					const gchar *fmttype;

					fmttype = i_cal_parameter_get_fmttype (param);
					if (fmttype && *fmttype)
						g_file_info_set_content_type (file_info, fmttype);

					g_clear_object (&param);
				}

				if (g_ascii_strncasecmp (uri, "http://", 7) == 0 ||
				    g_ascii_strncasecmp (uri, "https://", 8) == 0 ||
				    g_ascii_strncasecmp (uri, "ftp://", 6) == 0) {
					GIcon *icon;

					icon = g_themed_icon_new ("emblem-web");

					g_file_info_set_icon (file_info, icon);

					g_clear_object (&icon);
				}

				e_attachment_set_file_info (attachment, file_info);

				g_clear_object (&file_info);

				ecep_attachments_attachment_loaded_cb (attachment, NULL, page_attachments);
			}

			g_object_unref (attachment);
		}

		g_object_unref (attach);
		g_free (filename);
		g_free (uri);
	}

	g_ptr_array_unref (attach_props);
}

static gboolean
ecep_attachments_fill_component (ECompEditorPage *page,
				 ICalComponent *component)
{
	ECompEditorPageAttachments *page_attachments;
	ECompEditor *comp_editor;
	GList *attachments, *link;
	ICalProperty *prop;
	gboolean success = TRUE;

	g_return_val_if_fail (E_IS_COMP_EDITOR_PAGE_ATTACHMENTS (page), FALSE);
	g_return_val_if_fail (I_CAL_IS_COMPONENT (component), FALSE);

	comp_editor = e_comp_editor_page_ref_editor (page);
	page_attachments = E_COMP_EDITOR_PAGE_ATTACHMENTS (page);

	if (e_attachment_store_get_num_loading (E_ATTACHMENT_STORE (page_attachments->priv->store)) > 0) {
		e_comp_editor_set_validation_error (comp_editor, page, NULL,
			_("Some attachments are still being downloaded. Please wait until the download is finished."));
		g_clear_object (&comp_editor);
		return FALSE;
	}

	e_cal_util_component_remove_property_by_kind (component, I_CAL_ATTACH_PROPERTY, TRUE);

	attachments = e_attachment_store_get_attachments (E_ATTACHMENT_STORE (page_attachments->priv->store));
	for (link = attachments; link; link = g_list_next (link)) {
		EAttachment *attachment = link->data;
		ICalAttach *attach;
		gchar *buf, *uri, *description;
		GFile *file;
		GFileInfo *file_info;
		GPtrArray *add_params;

		if (!attachment)
			continue;

		description = e_attachment_dup_description (attachment);

		file = e_attachment_ref_file (attachment);
		if (!file) {
			gchar *error_message;

			success = FALSE;

			error_message = g_strdup_printf (
				_("Attachment “%s” cannot be found, remove it from the list, please"),
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
				_("Attachment “%s” doesn’t have valid URI, remove it from the list, please"),
				description);

			e_comp_editor_set_validation_error (comp_editor, page, NULL, error_message);

			g_free (description);
			g_free (error_message);
			g_object_unref (file);
			break;
		}

		g_object_unref (file);
		g_free (description);

		buf = i_cal_value_encode_ical_string (uri);
		attach = i_cal_attach_new_from_url (buf);
		prop = i_cal_property_new_attach (attach);

		file_info = e_attachment_ref_file_info (attachment);
		if (file_info) {
			if (g_file_info_has_attribute (file_info, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME)) {
				const gchar *display_name = g_file_info_get_display_name (file_info);

				if (display_name && *display_name) {
					ICalParameter *param;

					param = i_cal_parameter_new_filename (display_name);
					i_cal_property_take_parameter (prop, param);
				}
			}

			if (g_file_info_has_attribute (file_info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE)) {
				const gchar *content_type;

				content_type = g_file_info_get_content_type (file_info);
				if (content_type && *content_type) {
					gchar *mime_type;

					mime_type = g_content_type_get_mime_type (content_type);
					if (mime_type) {
						ICalParameter *param;

						param = i_cal_parameter_new_fmttype (mime_type);
						i_cal_property_take_parameter (prop, param);

						g_free (mime_type);
					}
				}
			}

			g_object_unref (file_info);
		}

		add_params = g_object_get_data (G_OBJECT (attachment), "ical-params");
		if (add_params) {
			guint ii;

			for (ii = 0; ii < add_params->len; ii++) {
				ICalParameter *param = g_ptr_array_index (add_params, ii);

				if (param)
					i_cal_property_take_parameter (prop, i_cal_parameter_clone (param));
			}
		}

		i_cal_component_take_property (component, prop);

		g_object_unref (attach);
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
	static const gchar *eui =
		"<eui>"
		  "<menu id='main-menu'>"
		    "<submenu action='insert-menu'>"
		      "<item action='attachments-attach'/>"
		    "</submenu>"
		    "<submenu action='options-menu'>"
		      "<placeholder id='tabs'>"
			"<item action='page-attachments'/>"
		      "</placeholder>"
		    "</submenu>"
		  "</menu>"
		"</eui>";

	static const EUIActionEntry editable_entries[] = {
		{ "attachments-attach",
		  "mail-attachment",
		  N_("_Attachment…"),
		  "<Control>m",
		  N_("Attach a file"),
		  ecep_attachments_action_attach_cb, NULL, NULL, NULL }
	};

	static const EUIActionEntry options_entries[] = {
		{ "page-attachments",
		  "mail-attachment",
		  N_("_Attachments"),
		  NULL,
		  N_("Show attachments"),
		  ecep_attachments_select_page_cb, NULL, NULL, NULL }
	};

	ECompEditor *comp_editor;
	EUIManager *ui_manager;
	EUIAction *action;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_ATTACHMENTS (page_attachments));

	comp_editor = e_comp_editor_page_ref_editor (E_COMP_EDITOR_PAGE (page_attachments));
	ui_manager = e_comp_editor_get_ui_manager (comp_editor);

	e_ui_manager_add_actions (ui_manager, "editable", GETTEXT_PACKAGE,
		editable_entries, G_N_ELEMENTS (editable_entries), page_attachments);

	action = e_comp_editor_get_action (comp_editor, "attachments-attach");

	e_binding_bind_property (
		page_attachments, "visible",
		action, "visible",
		G_BINDING_SYNC_CREATE);

	e_ui_manager_add_actions_with_eui_data (ui_manager, "individual", GETTEXT_PACKAGE,
		options_entries, G_N_ELEMENTS (options_entries), page_attachments, eui);

	action = e_comp_editor_get_action (comp_editor, "page-attachments");

	e_binding_bind_property (
		page_attachments, "visible",
		action, "visible",
		G_BINDING_SYNC_CREATE);

	g_clear_object (&comp_editor);
}

static void
ecep_attachments_constructed (GObject *object)
{
	ECompEditorPageAttachments *page_attachments;
	ECompEditor *comp_editor;
	EUIAction *action;
	EUIManager *ui_manager;
	GSettings *settings;
	GtkSizeGroup *size_group;
	GtkWidget *container;
	GtkWidget *widget;

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
		"allow-uri", TRUE,
		NULL);
	gtk_widget_show (widget);

	/* needed to be able to execute the actions from the attachment view */
	ui_manager = e_attachment_view_get_ui_manager (E_ATTACHMENT_VIEW (widget));
	e_ui_manager_add_action_groups_to_widget (ui_manager, GTK_WIDGET (page_attachments));

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
		"allow-uri", TRUE,
		NULL);
	gtk_widget_show (widget);

	/* Construct the Controls */

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_grid_attach (GTK_GRID (page_attachments), widget, 0, 0, 1, 1);
	gtk_widget_show (widget);
	page_attachments->priv->controls_container = widget;

	container = widget;

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_margin_end (widget, 6);
	gtk_widget_set_margin_start (widget, 6);
	gtk_box_pack_end (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	/* The "Add URI" button proxies the "add-uri" action from
	 * one of the two attachment views.  Doesn't matter which. */
	widget = gtk_button_new ();
	action = e_attachment_view_get_action (E_ATTACHMENT_VIEW (page_attachments->priv->icon_view), "add-uri");
	e_ui_action_util_assign_to_widget (action, widget);
	gtk_button_set_image (GTK_BUTTON (widget), gtk_image_new_from_icon_name (e_ui_action_get_icon_name (action), GTK_ICON_SIZE_BUTTON));
	gtk_box_pack_end (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	page_attachments->priv->add_uri_button = widget;

	/* The "Add Attachment" button proxies the "add" action from
	 * one of the two attachment views.  Doesn't matter which. */
	widget = gtk_button_new ();
	action = e_attachment_view_get_action (E_ATTACHMENT_VIEW (page_attachments->priv->icon_view), "add");
	e_ui_action_util_assign_to_widget (action, widget);
	gtk_button_set_image (GTK_BUTTON (widget), gtk_image_new_from_icon_name (e_ui_action_get_icon_name (action), GTK_ICON_SIZE_BUTTON));
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

	g_signal_connect_object (page_attachments->priv->tree_view, "before-properties-popup",
		G_CALLBACK (ecep_before_properties_popup_cb), page_attachments, 0);

	g_signal_connect_object (page_attachments->priv->icon_view, "before-properties-popup",
		G_CALLBACK (ecep_before_properties_popup_cb), page_attachments, 0);

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
		page_attachments->priv->icon_view, "editable",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		action, "sensitive",
		page_attachments->priv->tree_view, "editable",
		G_BINDING_SYNC_CREATE);

	g_clear_object (&comp_editor);
}

static void
e_comp_editor_page_attachments_init (ECompEditorPageAttachments *page_attachments)
{
	page_attachments->priv = e_comp_editor_page_attachments_get_instance_private (page_attachments);
}

static void
e_comp_editor_page_attachments_class_init (ECompEditorPageAttachmentsClass *klass)
{
	ECompEditorPageClass *page_class;
	GtkWidgetClass *widget_class;
	GObjectClass *object_class;

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
