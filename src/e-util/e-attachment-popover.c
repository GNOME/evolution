/*
 * SPDX-FileCopyrightText: (C) 2023 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include "e-attachment.h"
#include "e-misc-utils.h"
#include "e-url-entry.h"

#include "e-attachment-popover.h"

struct _EAttachmentPopover {
	GtkPopover parent;

	GtkWidget *filename_entry;
	GtkWidget *uri_label;
	GtkWidget *uri_entry;
	GtkWidget *description_label;
	GtkWidget *description_entry;
	GtkWidget *mime_type_text_label;
	GtkWidget *mime_type_entry;
	GtkWidget *prefer_inline_check;
	GtkWidget *save_button;

	EAttachment *attachment;
	gboolean changes_saved;
	gboolean allow_disposition;
	gboolean in_refresh;
};

G_DEFINE_TYPE (EAttachmentPopover, e_attachment_popover, GTK_TYPE_POPOVER)

static void
e_attachment_popover_sensitize_save_button (EAttachmentPopover *self)
{
	gboolean sensitive = FALSE;

	if (self->attachment) {
		sensitive = !e_attachment_is_uri (self->attachment) ||
			g_strcmp0 (gtk_entry_get_text (GTK_ENTRY (self->uri_entry)), "") != 0;
	}

	gtk_widget_set_sensitive (self->save_button, sensitive);
}

static gboolean
e_attachment_popover_uri_entry_focus_out_cb (EAttachmentPopover *self)
{
	if (!self->in_refresh && gtk_widget_get_visible (self->uri_entry)) {
		const gchar *text;

		text = gtk_entry_get_text (GTK_ENTRY (self->mime_type_entry));

		if (!text || !*text || g_ascii_strcasecmp (text, "application/octet-stream") == 0) {
			text = gtk_entry_get_text (GTK_ENTRY (self->uri_entry));

			if (text && *text) {
				gchar *guessed_mime_type;

				guessed_mime_type = e_util_guess_mime_type (text, FALSE);
				if (guessed_mime_type) {
					gtk_entry_set_text (GTK_ENTRY (self->mime_type_entry), guessed_mime_type);
					g_free (guessed_mime_type);
				}
			}
		} else {
			text = gtk_entry_get_text (GTK_ENTRY (self->uri_entry));
		}

		if (text && *text && g_strcmp0 (gtk_entry_get_text (GTK_ENTRY (self->filename_entry)), "") == 0) {
			GUri *guri;

			guri = g_uri_parse (text, G_URI_FLAGS_PARSE_RELAXED, NULL);
			if (guri) {
				const gchar *path;

				path = g_uri_get_path (guri);

				if (path) {
					path = strrchr (path, '/');
					if (path)
						path++;
				}

				if (path && *path)
					gtk_entry_set_text (GTK_ENTRY (self->filename_entry), path);

				g_uri_unref (guri);
			}
		}
	}

	e_attachment_popover_sensitize_save_button (self);

	return FALSE;
}

static void
e_attachment_popover_refresh (EAttachmentPopover *self)
{
	GFileInfo *file_info = NULL;
	const gchar *content_type = NULL;
	const gchar *display_name = NULL;
	gchar *description = NULL;
	gchar *disposition = NULL;
	gboolean is_uri;

	self->in_refresh = TRUE;

	is_uri = self->attachment && e_attachment_is_uri (self->attachment);

	if (self->attachment) {
		file_info = e_attachment_ref_file_info (self->attachment);
		description = e_attachment_dup_description (self->attachment);
		disposition = e_attachment_dup_disposition (self->attachment);
	}

	if (file_info) {
		if (g_file_info_has_attribute (file_info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE))
			content_type = g_file_info_get_content_type (file_info);

		if (g_file_info_has_attribute (file_info, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME))
			display_name = g_file_info_get_display_name (file_info);
	}

	if (content_type) {
		gchar *type_description;
		gchar *comment;
		gchar *mime_type;

		comment = g_content_type_get_description (content_type);
		mime_type = g_content_type_get_mime_type (content_type);

		if (gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL)
			type_description = g_strdup_printf ("(%s) %s", mime_type, comment);
		else
			type_description = g_strdup_printf ("%s (%s)", comment, mime_type);

		gtk_label_set_text (GTK_LABEL (self->mime_type_text_label), type_description);
		gtk_entry_set_text (GTK_ENTRY (self->mime_type_entry), mime_type ? mime_type : "");

		g_free (comment);
		g_free (mime_type);
		g_free (type_description);
	} else {
		gtk_label_set_text (GTK_LABEL (self->mime_type_text_label), "");
		gtk_entry_set_text (GTK_ENTRY (self->mime_type_entry), "");
	}

	gtk_entry_set_text (GTK_ENTRY (self->filename_entry), display_name ? display_name : "");
	gtk_entry_set_text (GTK_ENTRY (self->uri_entry), "");
	gtk_entry_set_text (GTK_ENTRY (self->description_entry), description ? description : "");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->prefer_inline_check), g_strcmp0 (disposition, "inline") == 0);

	if (is_uri) {
		GFile *file;

		file = e_attachment_ref_file (self->attachment);
		if (file) {
			gchar *uri;

			uri = g_file_get_uri (file);
			if (uri) {
				gtk_entry_set_text (GTK_ENTRY (self->uri_entry), uri);
				g_free (uri);
			}
		}

		g_clear_object (&file);
	}

	gtk_widget_set_visible (self->uri_label, is_uri);
	gtk_widget_set_visible (self->uri_entry, is_uri);
	gtk_widget_set_visible (self->description_label, !is_uri && self->allow_disposition);
	gtk_widget_set_visible (self->description_entry, !is_uri && self->allow_disposition);
	gtk_widget_set_visible (self->mime_type_text_label, !is_uri);
	gtk_widget_set_visible (self->mime_type_entry, is_uri);
	gtk_widget_set_visible (self->prefer_inline_check, !is_uri && self->allow_disposition);

	g_clear_object (&file_info);
	g_free (description);
	g_free (disposition);

	e_attachment_popover_sensitize_save_button (self);

	self->in_refresh = FALSE;
}

static void
e_attachment_popover_save_changes_cb (EAttachmentPopover *self)
{
	CamelMimePart *mime_part;
	GFileInfo *file_info;

	if (!self->attachment) {
		gtk_popover_popdown (GTK_POPOVER (self));
		return;
	}

	file_info = e_attachment_ref_file_info (self->attachment);
	g_return_if_fail (file_info != NULL);

	mime_part = e_attachment_ref_mime_part (self->attachment);

	g_file_info_set_attribute_string (file_info, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
		gtk_entry_get_text (GTK_ENTRY (self->filename_entry)));

	if (mime_part)
		camel_mime_part_set_filename (mime_part, gtk_entry_get_text (GTK_ENTRY (self->filename_entry)));

	if (e_attachment_is_uri (self->attachment)) {
		GFile *file;

		file = g_file_new_for_uri (gtk_entry_get_text (GTK_ENTRY (self->uri_entry)));
		e_attachment_set_file (self->attachment, file);
		g_clear_object (&file);

		g_file_info_set_attribute_string (file_info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
			gtk_entry_get_text (GTK_ENTRY (self->mime_type_entry)));
	} else {
		const gchar *disposition;

		g_file_info_set_attribute_string (file_info, G_FILE_ATTRIBUTE_STANDARD_DESCRIPTION,
			gtk_entry_get_text (GTK_ENTRY (self->description_entry)));

		if (mime_part)
			camel_mime_part_set_description (mime_part, gtk_entry_get_text (GTK_ENTRY (self->description_entry)));

		disposition = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->prefer_inline_check)) ? "inline" : "attachment";
		e_attachment_set_disposition (self->attachment, disposition);

		if (mime_part)
			camel_mime_part_set_disposition (mime_part, disposition);
	}

	g_clear_object (&mime_part);
	g_clear_object (&file_info);

	self->changes_saved = TRUE;

	g_object_notify (G_OBJECT (self->attachment), "file-info");

	gtk_popover_popdown (GTK_POPOVER (self));
}

static void
e_attachment_popover_constructed (GObject *object)
{
	EAttachmentPopover *self = E_ATTACHMENT_POPOVER (object);
	PangoAttrList *bold;
	GtkWidget *widget;
	GtkLabel *label;
	GtkGrid *grid;
	gint row = 0;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_attachment_popover_parent_class)->constructed (object);

	widget = gtk_grid_new ();
	g_object_set (widget,
		"visible", TRUE,
		"halign", GTK_ALIGN_FILL,
		"valign", GTK_ALIGN_FILL,
		"margin", 12,
		"row-spacing", 4,
		"column-spacing", 4,
		NULL);

	gtk_container_add (GTK_CONTAINER (self), widget);

	grid = GTK_GRID (widget);

	bold = pango_attr_list_new ();
	pango_attr_list_insert (bold, pango_attr_weight_new (PANGO_WEIGHT_BOLD));

	widget = gtk_label_new (_("Attachment Properties"));
	g_object_set (widget,
		"visible", TRUE,
		"halign", GTK_ALIGN_CENTER,
		"valign", GTK_ALIGN_CENTER,
		"attributes", bold,
		NULL);

	gtk_grid_attach (grid, widget, 0, row, 2, 1);
	row++;

	g_clear_pointer (&bold, pango_attr_list_unref);

	widget = gtk_label_new_with_mnemonic (_("_URI:"));
	g_object_set (widget,
		"visible", FALSE,
		"halign", GTK_ALIGN_END,
		"valign", GTK_ALIGN_CENTER,
		NULL);

	gtk_grid_attach (grid, widget, 0, row, 1, 1);

	self->uri_label = widget;
	label = GTK_LABEL (widget);

	widget = e_url_entry_new ();
	g_object_set (widget,
		"visible", FALSE,
		"halign", GTK_ALIGN_FILL,
		"valign", GTK_ALIGN_CENTER,
		NULL);

	gtk_label_set_mnemonic_widget (label, widget);

	gtk_grid_attach (grid, widget, 1, row, 1, 1);
	row++;

	self->uri_entry = widget;

	widget = gtk_label_new_with_mnemonic (_("_Filename:"));
	g_object_set (widget,
		"visible", TRUE,
		"halign", GTK_ALIGN_END,
		"valign", GTK_ALIGN_CENTER,
		NULL);

	gtk_grid_attach (grid, widget, 0, row, 1, 1);

	label = GTK_LABEL (widget);

	widget = gtk_entry_new ();
	g_object_set (widget,
		"visible", TRUE,
		"halign", GTK_ALIGN_FILL,
		"valign", GTK_ALIGN_CENTER,
		"width-request", 250,
		NULL);

	gtk_label_set_mnemonic_widget (label, widget);

	gtk_grid_attach (grid, widget, 1, row, 1, 1);
	row++;

	self->filename_entry = widget;

	g_signal_connect_swapped (widget, "activate",
		G_CALLBACK (e_attachment_popover_save_changes_cb), self);

	widget = gtk_label_new_with_mnemonic (_("_Description:"));
	g_object_set (widget,
		"visible", TRUE,
		"halign", GTK_ALIGN_END,
		"valign", GTK_ALIGN_CENTER,
		NULL);

	gtk_grid_attach (grid, widget, 0, row, 1, 1);

	self->description_label = widget;
	label = GTK_LABEL (widget);

	widget = gtk_entry_new ();
	g_object_set (widget,
		"visible", TRUE,
		"halign", GTK_ALIGN_FILL,
		"valign", GTK_ALIGN_CENTER,
		NULL);

	gtk_label_set_mnemonic_widget (label, widget);

	gtk_grid_attach (grid, widget, 1, row, 1, 1);
	row++;

	self->description_entry = widget;

	g_signal_connect_swapped (widget, "activate",
		G_CALLBACK (e_attachment_popover_save_changes_cb), self);

	widget = gtk_label_new_with_mnemonic (_("_MIME Type:"));
	g_object_set (widget,
		"visible", TRUE,
		"halign", GTK_ALIGN_END,
		"valign", GTK_ALIGN_CENTER,
		NULL);

	gtk_grid_attach (grid, widget, 0, row, 1, 1);

	label = GTK_LABEL (widget);

	widget = gtk_label_new ("");
	g_object_set (widget,
		"visible", TRUE,
		"halign", GTK_ALIGN_START,
		"valign", GTK_ALIGN_CENTER,
		NULL);

	gtk_grid_attach (grid, widget, 1, row, 1, 1);

	self->mime_type_text_label = widget;

	widget = gtk_entry_new ();
	g_object_set (widget,
		"visible", FALSE,
		"halign", GTK_ALIGN_FILL,
		"valign", GTK_ALIGN_CENTER,
		NULL);

	gtk_label_set_mnemonic_widget (label, widget);

	gtk_grid_attach (grid, widget, 1, row, 1, 1);
	row++;

	self->mime_type_entry = widget;

	widget = gtk_check_button_new_with_mnemonic (_("Suggest _automatic display of attachment"));
	g_object_set (widget,
		"visible", TRUE,
		"halign", GTK_ALIGN_START,
		"valign", GTK_ALIGN_CENTER,
		NULL);

	gtk_grid_attach (grid, widget, 0, row, 2, 1);
	row++;

	self->prefer_inline_check = widget;

	widget = gtk_button_new_with_mnemonic (_("_Save Changes"));
	g_object_set (widget,
		"visible", TRUE,
		"can-default", TRUE,
		"halign", GTK_ALIGN_CENTER,
		"valign", GTK_ALIGN_CENTER,
		"margin-top", 4,
		NULL);

	gtk_grid_attach (grid, widget, 0, row, 2, 1);

	self->save_button = widget;

	gtk_popover_set_default_widget (GTK_POPOVER (self), widget);

	g_signal_connect_swapped (widget, "clicked",
		G_CALLBACK (e_attachment_popover_save_changes_cb), self);

	g_signal_connect_swapped (self->uri_entry, "changed",
		G_CALLBACK (e_attachment_popover_sensitize_save_button), self);

	g_signal_connect_swapped (self->uri_entry, "focus-out-event",
		G_CALLBACK (e_attachment_popover_uri_entry_focus_out_cb), self);
}

static void
e_attachment_popover_dispose (GObject *object)
{
	EAttachmentPopover *self = E_ATTACHMENT_POPOVER (object);

	g_clear_object (&self->attachment);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_attachment_popover_parent_class)->dispose (object);
}

static void
e_attachment_popover_class_init (EAttachmentPopoverClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->constructed = e_attachment_popover_constructed;
	object_class->dispose = e_attachment_popover_dispose;
}

static void
e_attachment_popover_init (EAttachmentPopover *self)
{
}

GtkWidget *
e_attachment_popover_new (GtkWidget *relative_to,
			  EAttachment *attachment)
{
	GtkWidget *self;

	self = g_object_new (E_TYPE_ATTACHMENT_POPOVER,
		"modal", TRUE,
		"position", GTK_POS_BOTTOM,
		"relative-to", relative_to,
		NULL);

	e_attachment_popover_set_attachment (E_ATTACHMENT_POPOVER (self), attachment);

	return self;
}

EAttachment *
e_attachment_popover_get_attachment (EAttachmentPopover *self)
{
	g_return_val_if_fail (E_IS_ATTACHMENT_POPOVER (self), NULL);

	return self->attachment;
}

void
e_attachment_popover_set_attachment (EAttachmentPopover *self,
				     EAttachment *attachment)
{
	g_return_if_fail (E_IS_ATTACHMENT_POPOVER (self));
	if (attachment)
		g_return_if_fail (E_IS_ATTACHMENT (attachment));

	g_set_object (&self->attachment, attachment);

	e_attachment_popover_refresh (self);
	self->changes_saved = FALSE;
}

gboolean
e_attachment_popover_get_changes_saved (EAttachmentPopover *self)
{
	g_return_val_if_fail (E_IS_ATTACHMENT_POPOVER (self), FALSE);

	return self->changes_saved;
}

void
e_attachment_popover_set_changes_saved (EAttachmentPopover *self,
					gboolean changes_saved)
{
	g_return_if_fail (E_IS_ATTACHMENT_POPOVER (self));

	self->changes_saved = changes_saved;
}

gboolean
e_attachment_popover_get_allow_disposition (EAttachmentPopover *self)
{
	g_return_val_if_fail (E_IS_ATTACHMENT_POPOVER (self), FALSE);

	return self->allow_disposition;
}

void
e_attachment_popover_set_allow_disposition (EAttachmentPopover *self,
					    gboolean allow_disposition)
{
	g_return_if_fail (E_IS_ATTACHMENT_POPOVER (self));

	self->allow_disposition = allow_disposition;

	e_attachment_popover_refresh (self);
}

void
e_attachment_popover_popup (EAttachmentPopover *self)
{
	g_return_if_fail (E_IS_ATTACHMENT_POPOVER (self));

	gtk_popover_popup (GTK_POPOVER (self));

	if (self->attachment && e_attachment_is_uri (self->attachment))
		gtk_widget_grab_focus (self->uri_entry);
	else
		gtk_widget_grab_focus (self->filename_entry);
}
