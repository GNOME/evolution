/*
 * e-html-editor-image-dialog.h
 *
 * Copyright (C) 2012 Dan Vrátil <dvratil@redhat.com>
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
 */

#include "evolution-config.h"

#include <stdlib.h>
#include <glib/gi18n-lib.h>

#include "e-image-chooser-dialog.h"
#include "e-misc-utils.h"

#include "e-html-editor-image-dialog.h"

#define E_HTML_EDITOR_IMAGE_DIALOG_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_HTML_EDITOR_IMAGE_DIALOG, EHTMLEditorImageDialogPrivate))

struct _EHTMLEditorImageDialogPrivate {
	GtkWidget *file_chooser;
	GtkWidget *description_edit;

	GtkWidget *width_edit;
	GtkWidget *width_units;
	GtkWidget *height_edit;
	GtkWidget *height_units;
	GtkWidget *alignment;

	GtkWidget *x_padding_edit;
	GtkWidget *y_padding_edit;
	GtkWidget *border_edit;

	GtkWidget *url_edit;
	GtkWidget *test_url_button;
};

G_DEFINE_TYPE (
	EHTMLEditorImageDialog,
	e_html_editor_image_dialog,
	E_TYPE_HTML_EDITOR_DIALOG);

static void
html_editor_image_dialog_set_src (EHTMLEditorImageDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	gchar *uri;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	uri = gtk_file_chooser_get_uri (
		GTK_FILE_CHOOSER (dialog->priv->file_chooser));

	e_content_editor_image_set_src (cnt_editor, uri);

	g_free (uri);
}

static void
html_editor_image_dialog_set_alt (EHTMLEditorImageDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	const gchar *value;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	value = gtk_entry_get_text (GTK_ENTRY (dialog->priv->description_edit));

	e_content_editor_image_set_alt (cnt_editor, value);
}

static void
html_editor_image_dialog_set_width (EHTMLEditorImageDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	gint requested;
	gint32 natural = 0;
	gint width;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	natural = e_content_editor_image_get_natural_width (cnt_editor);

	requested = gtk_spin_button_get_value_as_int (
		GTK_SPIN_BUTTON (dialog->priv->width_edit));

	switch (gtk_combo_box_get_active (GTK_COMBO_BOX (dialog->priv->width_units))) {
		case 0:	/* px */
			width = requested;
			break;

		case 1: /* percent */
			width = natural * requested * 0.01;
			break;

		case 2: /* follow */
			width = natural;
			break;

		default:
			return;
	}

	e_content_editor_image_set_width (cnt_editor, width);
}

static void
html_editor_image_dialog_set_width_units (EHTMLEditorImageDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	gint requested;
	gint32 natural;
	gint width = 0;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	natural = e_content_editor_image_get_natural_width (cnt_editor);

	requested = gtk_spin_button_get_value_as_int (
			GTK_SPIN_BUTTON (dialog->priv->width_edit));

	switch (gtk_combo_box_get_active (
		GTK_COMBO_BOX (dialog->priv->width_units))) {

		case 0:	/* px */
			if (gtk_widget_is_sensitive (dialog->priv->width_edit)) {
				width = requested * natural * 0.01;
			} else {
				width = natural;
			}
			gtk_widget_set_sensitive (dialog->priv->width_edit, TRUE);
			break;

		case 1: /* percent */
			if (natural && gtk_widget_is_sensitive (dialog->priv->width_edit)) {
				width = (((gdouble) requested) / natural) * 100;
			} else {
				width = 100;
			}
			gtk_widget_set_sensitive (dialog->priv->width_edit, TRUE);
			break;

		case 2: /* follow */
			gtk_widget_set_sensitive (dialog->priv->width_edit, FALSE);
			break;
	}

	e_content_editor_image_set_width_follow (
		cnt_editor, !gtk_widget_get_sensitive (dialog->priv->width_edit));

	if (width != 0)
		gtk_spin_button_set_value (
			GTK_SPIN_BUTTON (dialog->priv->width_edit), width);
}

static void
html_editor_image_dialog_set_height (EHTMLEditorImageDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	gint requested;
	gint32 natural = 0;
	gint height;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	natural = e_content_editor_image_get_natural_height (cnt_editor);

	requested = gtk_spin_button_get_value_as_int (
		GTK_SPIN_BUTTON (dialog->priv->height_edit));

	switch (gtk_combo_box_get_active (GTK_COMBO_BOX (dialog->priv->height_units))) {
		case 0:	/* px */
			height = requested;
			break;

		case 1: /* percent */
			height = natural * requested * 0.01;
			break;

		case 2: /* follow */
			height = natural;
			break;

		default:
			return;
	}

	e_content_editor_image_set_height (cnt_editor, height);
}

static void
html_editor_image_dialog_set_height_units (EHTMLEditorImageDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	gint requested;
	gulong natural;
	gint height = -1;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	natural = e_content_editor_image_get_natural_height (cnt_editor);

	requested = gtk_spin_button_get_value_as_int (
		GTK_SPIN_BUTTON (dialog->priv->height_edit));

	switch (gtk_combo_box_get_active (
		GTK_COMBO_BOX (dialog->priv->height_units))) {

		case 0:	/* px */
			if (gtk_widget_is_sensitive (dialog->priv->height_edit)) {
				height = requested * natural * 0.01;
			} else {
				height = natural;
			}
			gtk_widget_set_sensitive (dialog->priv->height_edit, TRUE);
			break;

		case 1: /* percent */
			if (natural && gtk_widget_is_sensitive (dialog->priv->height_edit)) {
				height = (((gdouble) requested) / natural) * 100;
			} else {
				height = 100;
			}
			gtk_widget_set_sensitive (dialog->priv->height_edit, TRUE);
			break;

		case 2: /* follow */
			gtk_widget_set_sensitive (dialog->priv->height_edit, FALSE);
			break;
	}

	e_content_editor_image_set_height_follow (
		cnt_editor, !gtk_widget_get_sensitive (dialog->priv->height_edit));

	if (height != -1)
		gtk_spin_button_set_value (
			GTK_SPIN_BUTTON (dialog->priv->height_edit), height);
}

static void
html_editor_image_dialog_set_alignment (EHTMLEditorImageDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	const gchar *value;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	value = gtk_combo_box_get_active_id (GTK_COMBO_BOX (dialog->priv->alignment));
	e_content_editor_image_set_align (cnt_editor, value);
}

static void
html_editor_image_dialog_set_x_padding (EHTMLEditorImageDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	gint value;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	value = gtk_spin_button_get_value_as_int (
		GTK_SPIN_BUTTON (dialog->priv->x_padding_edit));
	e_content_editor_image_set_hspace (cnt_editor, value);
}

static void
html_editor_image_dialog_set_y_padding (EHTMLEditorImageDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	gint value;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	value = gtk_spin_button_get_value_as_int (
		GTK_SPIN_BUTTON (dialog->priv->y_padding_edit));
	e_content_editor_image_set_vspace (cnt_editor, value);
}

static void
html_editor_image_dialog_set_border (EHTMLEditorImageDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	gint value;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	value = gtk_spin_button_get_value_as_int (
		GTK_SPIN_BUTTON (dialog->priv->border_edit));

	e_content_editor_image_set_border (cnt_editor, value);
}

static void
html_editor_image_dialog_set_url (EHTMLEditorImageDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	const gchar *value;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	value = gtk_entry_get_text (GTK_ENTRY (dialog->priv->url_edit));

	e_content_editor_image_set_url (cnt_editor, value);
}

static void
html_editor_image_dialog_test_url (EHTMLEditorImageDialog *dialog)
{
	e_show_uri (
		GTK_WINDOW (dialog),
		gtk_entry_get_text (GTK_ENTRY (dialog->priv->url_edit)));
}

static void
html_editor_image_dialog_show (GtkWidget *widget)
{
	EHTMLEditor *editor;
	EHTMLEditorImageDialog *dialog;
	EContentEditor *cnt_editor;
	gchar *value;

	dialog = E_HTML_EDITOR_IMAGE_DIALOG (widget);
	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	e_content_editor_on_image_dialog_open (cnt_editor);

	value = e_content_editor_image_get_src (cnt_editor);
	if (value && *value) {
		gtk_file_chooser_set_uri (
			GTK_FILE_CHOOSER (dialog->priv->file_chooser), value);
		gtk_widget_set_sensitive (
			GTK_WIDGET (dialog->priv->file_chooser), TRUE);
	} else {
		gtk_file_chooser_set_uri (
			GTK_FILE_CHOOSER (dialog->priv->file_chooser), "");
		gtk_widget_set_sensitive (
			GTK_WIDGET (dialog->priv->file_chooser), FALSE);
	}
	g_free (value);

	value = e_content_editor_image_get_alt (cnt_editor);
	gtk_entry_set_text (
		GTK_ENTRY (dialog->priv->description_edit), value ? value : "");
	g_free (value);

	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (dialog->priv->width_edit),
		e_content_editor_image_get_width (cnt_editor));

	gtk_combo_box_set_active_id (
		GTK_COMBO_BOX (dialog->priv->width_units), "units-px");

	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (dialog->priv->height_edit),
		e_content_editor_image_get_height (cnt_editor));

	gtk_combo_box_set_active_id (
		GTK_COMBO_BOX (dialog->priv->height_units), "units-px");

	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (dialog->priv->border_edit),
		e_content_editor_image_get_border (cnt_editor));

	value = e_content_editor_image_get_align (cnt_editor);
	gtk_combo_box_set_active_id (
		GTK_COMBO_BOX (dialog->priv->alignment),
		(value && *value) ? value : "bottom");
	g_free (value);

	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (dialog->priv->y_padding_edit),
		e_content_editor_image_get_hspace (cnt_editor));

	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (dialog->priv->y_padding_edit),
		e_content_editor_image_get_vspace (cnt_editor));

	value = e_content_editor_image_get_url (cnt_editor);
	if (value && *value)
		gtk_entry_set_text (GTK_ENTRY (dialog->priv->url_edit), value);
	g_free (value);

	/* Chain up to parent implementation */
	GTK_WIDGET_CLASS (e_html_editor_image_dialog_parent_class)->show (widget);
}

static void
html_editor_image_dialog_hide (GtkWidget *widget)
{
	EHTMLEditor *editor;
	EHTMLEditorImageDialog *dialog;
	EContentEditor *cnt_editor;

	dialog = E_HTML_EDITOR_IMAGE_DIALOG (widget);
	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	e_content_editor_on_image_dialog_close (cnt_editor);

	GTK_WIDGET_CLASS (e_html_editor_image_dialog_parent_class)->hide (widget);
}

static void
e_html_editor_image_dialog_class_init (EHTMLEditorImageDialogClass *class)
{
	GtkWidgetClass *widget_class;

	g_type_class_add_private (class, sizeof (EHTMLEditorImageDialogPrivate));

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->show = html_editor_image_dialog_show;
	widget_class->hide = html_editor_image_dialog_hide;
}

static void
e_html_editor_image_dialog_init (EHTMLEditorImageDialog *dialog)
{
	GtkGrid *main_layout, *grid;
	GtkWidget *widget;
	GtkFileFilter *file_filter;

	dialog->priv = E_HTML_EDITOR_IMAGE_DIALOG_GET_PRIVATE (dialog);

	main_layout = e_html_editor_dialog_get_container (E_HTML_EDITOR_DIALOG (dialog));

	/* == General == */
	widget = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (widget), _("<b>General</b>"));
	gtk_misc_set_alignment (GTK_MISC (widget), 0, 0.5);
	gtk_grid_attach (main_layout, widget, 0, 0, 1, 1);

	grid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_row_spacing (grid, 5);
	gtk_grid_set_column_spacing (grid, 5);
	gtk_grid_attach (main_layout, GTK_WIDGET (grid), 0, 1, 1, 1);
	gtk_widget_set_margin_left (GTK_WIDGET (grid), 10);

	/* Source */
	widget = e_image_chooser_dialog_new (
			_("Choose Background Image"),
			GTK_WINDOW (dialog));
	gtk_file_chooser_set_action (
		GTK_FILE_CHOOSER (widget), GTK_FILE_CHOOSER_ACTION_OPEN);

	file_filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (file_filter, _("Images"));
	gtk_file_filter_add_mime_type (file_filter, "image/*");

	widget = gtk_file_chooser_button_new_with_dialog (widget);
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_grid_attach (grid, widget, 1, 0, 1, 1);
	g_signal_connect_swapped (
		widget, "file-set",
		G_CALLBACK (html_editor_image_dialog_set_src), dialog);
	dialog->priv->file_chooser = widget;

	widget = gtk_label_new_with_mnemonic (_("_Source:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), dialog->priv->file_chooser);
	gtk_grid_attach (grid, widget, 0, 0, 1, 1);

	/* Description */
	widget = gtk_entry_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_grid_attach (grid, widget, 1, 1, 1, 1);
	g_signal_connect_swapped (
		widget, "notify::text",
		G_CALLBACK (html_editor_image_dialog_set_alt), dialog);
	dialog->priv->description_edit = widget;

	widget = gtk_label_new_with_mnemonic (_("_Description:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), dialog->priv->description_edit);
	gtk_grid_attach (grid, widget, 0, 1, 1, 1);

	/* == Layout == */
	widget = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (widget), _("<b>Layout</b>"));
	gtk_misc_set_alignment (GTK_MISC (widget), 0, 0.5);
	gtk_grid_attach (main_layout, widget, 0, 2, 1, 1);

	grid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_row_spacing (grid, 5);
	gtk_grid_set_column_spacing (grid, 5);
	gtk_grid_attach (main_layout, GTK_WIDGET (grid), 0, 3, 1, 1);
	gtk_widget_set_margin_left (GTK_WIDGET (grid), 10);

	/* Width */
	widget = gtk_spin_button_new_with_range (1, G_MAXUINT, 1);
	gtk_grid_attach (grid, widget, 1, 0, 1, 1);
	g_signal_connect_swapped (
		widget, "value-changed",
		G_CALLBACK (html_editor_image_dialog_set_width), dialog);
	dialog->priv->width_edit = widget;

	widget = gtk_label_new_with_mnemonic (_("_Width:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), dialog->priv->width_edit);
	gtk_grid_attach (grid, widget, 0, 0, 1, 1);

	widget = gtk_combo_box_text_new ();
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "units-px", "px");
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "units-percent", "%");
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "units-follow", "follow");
	gtk_combo_box_set_active_id (GTK_COMBO_BOX (widget), "units-px");
	gtk_grid_attach (grid, widget, 2, 0, 1, 1);
	g_signal_connect_swapped (
		widget, "changed",
		G_CALLBACK (html_editor_image_dialog_set_width_units), dialog);
	dialog->priv->width_units = widget;

	/* Height */
	widget = gtk_spin_button_new_with_range (1, G_MAXUINT, 1);
	gtk_grid_attach (grid, widget, 1, 1, 1, 1);
	g_signal_connect_swapped (
		widget, "value-changed",
		G_CALLBACK (html_editor_image_dialog_set_height), dialog);
	dialog->priv->height_edit = widget;

	widget = gtk_label_new_with_mnemonic (_("_Height:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), dialog->priv->height_edit);
	gtk_grid_attach (grid, widget, 0, 1, 1, 1);

	widget = gtk_combo_box_text_new ();
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "units-px", "px");
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "units-percent", "%");
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "units-follow", "follow");
	gtk_combo_box_set_active_id (GTK_COMBO_BOX (widget), "units-px");
	gtk_grid_attach (grid, widget, 2, 1, 1, 1);
	g_signal_connect_swapped (
		widget, "changed",
		G_CALLBACK (html_editor_image_dialog_set_height_units), dialog);
	dialog->priv->height_units = widget;

	/* Alignment */
	widget = gtk_combo_box_text_new ();
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "top", _("Top"));
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "middle", _("Middle"));
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "bottom", _("Bottom"));
	gtk_combo_box_set_active_id (GTK_COMBO_BOX (widget), "bottom");
	gtk_grid_attach (grid, widget, 1, 2, 1, 1);
	g_signal_connect_swapped (
		widget, "changed",
		G_CALLBACK (html_editor_image_dialog_set_alignment), dialog);
	dialog->priv->alignment = widget;

	widget = gtk_label_new_with_mnemonic (_("_Alignment"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), dialog->priv->alignment);
	gtk_grid_attach (grid, widget, 0, 2, 1, 1);

	/* X Padding */
	widget = gtk_spin_button_new_with_range (0, G_MAXUINT, 1);
	gtk_grid_attach (grid, widget, 5, 0, 1, 1);
	g_signal_connect_swapped (
		widget, "value-changed",
		G_CALLBACK (html_editor_image_dialog_set_x_padding), dialog);
	dialog->priv->x_padding_edit = widget;

	widget = gtk_label_new_with_mnemonic (_("_X-Padding:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), dialog->priv->x_padding_edit);
	gtk_grid_attach (grid, widget, 4, 0, 1, 1);

	widget = gtk_label_new ("px");
	gtk_grid_attach (grid, widget, 6, 0, 1, 1);

	/* Y Padding */
	widget = gtk_spin_button_new_with_range (0, G_MAXUINT, 1);
	gtk_grid_attach (grid, widget, 5, 1, 1, 1);
	g_signal_connect_swapped (
		widget, "value-changed",
		G_CALLBACK (html_editor_image_dialog_set_y_padding), dialog);
	dialog->priv->y_padding_edit = widget;

	widget = gtk_label_new_with_mnemonic (_("_Y-Padding:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), dialog->priv->y_padding_edit);
	gtk_grid_attach (grid, widget, 4, 1, 1, 1);

	widget = gtk_label_new ("px");
	gtk_grid_attach (grid, widget, 6, 1, 1, 1);

	/* Border */
	widget = gtk_spin_button_new_with_range (0, G_MAXUINT, 1);
	gtk_grid_attach (grid, widget, 5, 2, 1, 1);
	g_signal_connect_swapped (
		widget, "value-changed",
		G_CALLBACK (html_editor_image_dialog_set_border), dialog);
	dialog->priv->border_edit = widget;

	widget = gtk_label_new_with_mnemonic (_("_Border:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), dialog->priv->border_edit);
	gtk_grid_attach (grid, widget, 4, 2, 1, 1);

	widget = gtk_label_new ("px");
	gtk_grid_attach (grid, widget, 6, 2, 1, 1);

	/* == Layout == */
	widget = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (widget), _("<b>Link</b>"));
	gtk_misc_set_alignment (GTK_MISC (widget), 0, 0.5);
	gtk_grid_attach (main_layout, widget, 0, 4, 1, 1);

	grid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_row_spacing (grid, 5);
	gtk_grid_set_column_spacing (grid, 5);
	gtk_grid_attach (main_layout, GTK_WIDGET (grid), 0, 5, 1, 1);
	gtk_widget_set_margin_left (GTK_WIDGET (grid), 10);

	widget = gtk_entry_new ();
	gtk_grid_attach (grid, widget, 1 ,0, 1, 1);
	gtk_widget_set_hexpand (widget, TRUE);
	g_signal_connect_swapped (
		widget, "notify::text",
		G_CALLBACK (html_editor_image_dialog_set_url), dialog);
	dialog->priv->url_edit = widget;

	widget = gtk_label_new_with_mnemonic (_("_URL:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), dialog->priv->url_edit);
	gtk_grid_attach (grid, widget, 0, 0, 1, 1);

	widget = gtk_button_new_with_mnemonic (_("_Test URL..."));
	gtk_grid_attach (grid, widget, 2, 0, 1, 1);
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (html_editor_image_dialog_test_url), dialog);
	dialog->priv->test_url_button = widget;

	gtk_widget_show_all (GTK_WIDGET (main_layout));
}

GtkWidget *
e_html_editor_image_dialog_new (EHTMLEditor *editor)
{
	return GTK_WIDGET (
		g_object_new (
			E_TYPE_HTML_EDITOR_IMAGE_DIALOG,
			"editor", editor,
			"title", _("Image Properties"),
			NULL));
}

void
e_html_editor_image_dialog_show (EHTMLEditorImageDialog *dialog)
{
	EHTMLEditorImageDialogClass *class;

	g_return_if_fail (E_IS_HTML_EDITOR_IMAGE_DIALOG (dialog));

	class = E_HTML_EDITOR_IMAGE_DIALOG_GET_CLASS (dialog);
	GTK_WIDGET_CLASS (class)->show (GTK_WIDGET (dialog));
}
