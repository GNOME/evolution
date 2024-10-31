/*
 * SPDX-FileCopyrightText: (C) 2023 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include "e-url-entry.h"
#include "e-html-editor-dialog.h"

#include "e-html-editor-link-popover.h"

struct _EHTMLEditorLinkPopover {
	GtkPopover parent;

	GtkWidget *uri_label;
	GtkWidget *uri_entry;
	GtkWidget *description_label;
	GtkWidget *description_entry;
	GtkWidget *name_label;
	GtkWidget *name_entry;
	GtkWidget *remove_button;
	GtkWidget *save_button;

	EHTMLEditor *editor;
	gboolean description_autofill;
};

G_DEFINE_TYPE (EHTMLEditorLinkPopover, e_html_editor_link_popover, GTK_TYPE_POPOVER)

static void
e_html_editor_link_popover_sensitize_save_button (EHTMLEditorLinkPopover *self)
{
	gboolean sensitive;

	sensitive = g_strcmp0 (gtk_entry_get_text (GTK_ENTRY (self->description_entry)), "") != 0 && (
		    g_strcmp0 (gtk_entry_get_text (GTK_ENTRY (self->uri_entry)), "") != 0 ||
		    g_strcmp0 (gtk_entry_get_text (GTK_ENTRY (self->name_entry)), "") != 0);

	gtk_widget_set_sensitive (self->save_button, sensitive);
}

static void
e_html_editor_link_popover_uri_changed_cb (EHTMLEditorLinkPopover *self)
{
	if (self->description_autofill) {
		const gchar *text;

		text = gtk_entry_get_text (GTK_ENTRY (self->uri_entry));
		gtk_entry_set_text (GTK_ENTRY (self->description_entry), text);
	}

	e_html_editor_link_popover_sensitize_save_button (self);
}

static void
e_html_editor_link_popover_description_changed_cb (EHTMLEditorLinkPopover *self)
{
	self->description_autofill = 0 == g_strcmp0 (
		gtk_entry_get_text (GTK_ENTRY (self->uri_entry)),
		gtk_entry_get_text (GTK_ENTRY (self->description_entry)));

	e_html_editor_link_popover_sensitize_save_button (self);
}

static void
e_html_editor_link_popover_save_clicked_cb (GtkButton *button,
					    gpointer user_data)
{
	EHTMLEditorLinkPopover *self = user_data;
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (self->editor);

	e_content_editor_link_set_properties (
		cnt_editor,
		gtk_entry_get_text (GTK_ENTRY (self->uri_entry)),
		gtk_entry_get_text (GTK_ENTRY (self->description_entry)),
		gtk_entry_get_text (GTK_ENTRY (self->name_entry)));

	gtk_popover_popdown (GTK_POPOVER (self));
}

static void
e_html_editor_link_popover_remove_clicked_cb (GtkButton *button,
					      gpointer user_data)
{
	EHTMLEditorLinkPopover *self = user_data;
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (self->editor);

	e_content_editor_selection_unlink (cnt_editor);

	gtk_popover_popdown (GTK_POPOVER (self));
}

static void
e_html_editor_link_popover_show (GtkWidget *widget)
{
	EHTMLEditorLinkPopover *self;
	EContentEditor *cnt_editor;
	gchar *href = NULL, *text = NULL, *name = NULL;

	self = E_HTML_EDITOR_LINK_POPOVER (widget);
	cnt_editor = e_html_editor_get_content_editor (self->editor);

	/* Reset to default values */
	gtk_entry_set_text (GTK_ENTRY (self->uri_entry), "https://");
	gtk_entry_set_text (GTK_ENTRY (self->description_entry), "");
	gtk_widget_set_sensitive (self->description_entry, TRUE);
	gtk_entry_set_text (GTK_ENTRY (self->name_entry), "");

	self->description_autofill = TRUE;

	e_content_editor_on_dialog_open (cnt_editor, E_CONTENT_EDITOR_DIALOG_LINK);

	e_content_editor_link_get_properties (cnt_editor, &href, &text, &name);
	if ((href && *href) || (name && *name)) {
		gtk_entry_set_text (GTK_ENTRY (self->uri_entry), href);
		gtk_button_set_label (GTK_BUTTON (self->save_button), _("Upd_ate"));
	} else {
		gtk_button_set_label (GTK_BUTTON (self->save_button), _("_Add"));
	}

	gtk_widget_set_visible (self->remove_button, (href && *href) || (name && *name));

	if (text && *text)
		gtk_entry_set_text (GTK_ENTRY (self->description_entry), text);

	if (name && *name)
		gtk_entry_set_text (GTK_ENTRY (self->name_entry), name);

	g_free (href);
	g_free (text);
	g_free (name);

	/* Chain up to parent's method. */
	GTK_WIDGET_CLASS (e_html_editor_link_popover_parent_class)->show (widget);

	gtk_widget_grab_focus (self->uri_entry);

	e_html_editor_link_popover_sensitize_save_button (self);
}

static void
e_html_editor_link_popover_hide (GtkWidget *widget)
{
	EHTMLEditorLinkPopover *self;
	EContentEditor *cnt_editor;

	self = E_HTML_EDITOR_LINK_POPOVER (widget);
	cnt_editor = e_html_editor_get_content_editor (self->editor);

	e_content_editor_on_dialog_close (cnt_editor, E_CONTENT_EDITOR_DIALOG_LINK);

	/* Chain up to parent's method. */
	GTK_WIDGET_CLASS (e_html_editor_link_popover_parent_class)->hide (widget);
}

static void
e_html_editor_link_popover_constructed (GObject *object)
{
	EHTMLEditorLinkPopover *self = E_HTML_EDITOR_LINK_POPOVER (object);
	PangoAttrList *bold;
	GtkWidget *widget;
	GtkLabel *label;
	GtkGrid *grid;
	GtkBox *box;
	gint row = 0;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_html_editor_link_popover_parent_class)->constructed (object);

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

	widget = gtk_label_new (_("Link Properties"));
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
		"visible", TRUE,
		"halign", GTK_ALIGN_END,
		"valign", GTK_ALIGN_CENTER,
		NULL);

	gtk_grid_attach (grid, widget, 0, row, 1, 1);

	self->uri_label = widget;
	label = GTK_LABEL (widget);

	widget = e_url_entry_new ();
	g_object_set (widget,
		"visible", TRUE,
		"halign", GTK_ALIGN_FILL,
		"valign", GTK_ALIGN_CENTER,
		NULL);

	gtk_label_set_mnemonic_widget (label, widget);

	gtk_grid_attach (grid, widget, 1, row, 1, 1);
	row++;

	self->uri_entry = widget;

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

	widget = gtk_label_new_with_mnemonic (_("_Name:"));
	g_object_set (widget,
		"visible", TRUE,
		"halign", GTK_ALIGN_END,
		"valign", GTK_ALIGN_CENTER,
		NULL);

	gtk_grid_attach (grid, widget, 0, row, 1, 1);

	self->name_label = widget;
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

	self->name_entry = widget;

	widget = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
	g_object_set (widget,
		"visible", TRUE,
		"halign", GTK_ALIGN_CENTER,
		"valign", GTK_ALIGN_CENTER,
		"margin-top", 4,
		NULL);

	gtk_grid_attach (grid, widget, 0, row, 2, 1);

	box = GTK_BOX (widget);

	widget = gtk_button_new_with_mnemonic (_("_Add"));
	g_object_set (widget,
		"visible", TRUE,
		"can-default", TRUE,
		"halign", GTK_ALIGN_CENTER,
		"valign", GTK_ALIGN_CENTER,
		NULL);

	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);

	self->save_button = widget;

	widget = gtk_button_new_with_mnemonic (_("_Remove"));
	g_object_set (widget,
		"visible", TRUE,
		"can-default", FALSE,
		"halign", GTK_ALIGN_CENTER,
		"valign", GTK_ALIGN_CENTER,
		NULL);

	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);

	self->remove_button = widget;

	gtk_popover_set_default_widget (GTK_POPOVER (self), self->save_button);

	g_signal_connect (self->save_button, "clicked",
		G_CALLBACK (e_html_editor_link_popover_save_clicked_cb), self);

	g_signal_connect (self->remove_button, "clicked",
		G_CALLBACK (e_html_editor_link_popover_remove_clicked_cb), self);

	g_signal_connect_swapped (self->uri_entry, "changed",
		G_CALLBACK (e_html_editor_link_popover_uri_changed_cb), self);

	g_signal_connect_swapped (self->description_entry, "changed",
		G_CALLBACK (e_html_editor_link_popover_description_changed_cb), self);

	g_signal_connect_swapped (self->name_entry, "changed",
		G_CALLBACK (e_html_editor_link_popover_sensitize_save_button), self);

	g_signal_connect_swapped (self->uri_entry, "focus-out-event",
		G_CALLBACK (e_html_editor_link_popover_sensitize_save_button), self);
}

static void
e_html_editor_link_popover_dispose (GObject *object)
{
	EHTMLEditorLinkPopover *self = E_HTML_EDITOR_LINK_POPOVER (object);

	self->editor = NULL;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_html_editor_link_popover_parent_class)->dispose (object);
}

static void
e_html_editor_link_popover_class_init (EHTMLEditorLinkPopoverClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->constructed = e_html_editor_link_popover_constructed;
	object_class->dispose = e_html_editor_link_popover_dispose;

	widget_class = GTK_WIDGET_CLASS (klass);
	widget_class->show = e_html_editor_link_popover_show;
	widget_class->hide = e_html_editor_link_popover_hide;
}

static void
e_html_editor_link_popover_init (EHTMLEditorLinkPopover *self)
{
}

/**
 * e_html_editor_link_popover_new:
 * @editor: an #EHTMLEditor
 *
 * Created a new popover to add/edit links into the @editor.
 *
 * Returns: (transfer full): a new #EHTMLEditorLinkPopover
 *
 * Since: 3.52
 **/
GtkWidget *
e_html_editor_link_popover_new (EHTMLEditor *editor)
{
	EHTMLEditorLinkPopover *self;

	g_return_val_if_fail (E_IS_HTML_EDITOR (editor), NULL);

	self = g_object_new (E_TYPE_HTML_EDITOR_LINK_POPOVER,
		"modal", TRUE,
		"position", GTK_POS_BOTTOM,
		"relative-to", editor,
		NULL);

	self->editor = editor;

	return GTK_WIDGET (self);
}

/**
 * e_html_editor_link_popover_popup:
 * @self: an #EHTMLEditorLinkPopover
 *
 * Pops up the link popover pointing to the editor's caret position, if it's known.
 *
 * Since: 3.52
 **/
void
e_html_editor_link_popover_popup (EHTMLEditorLinkPopover *self)
{
	EContentEditor *cnt_editor;
	GtkPopover *popover;
	GtkWidget *relative_to;
	GdkRectangle rect = { 0, 0, -1, -1 };

	g_return_if_fail (E_IS_HTML_EDITOR_LINK_POPOVER (self));

	cnt_editor = e_html_editor_get_content_editor (self->editor);
	e_content_editor_get_caret_client_rect (cnt_editor, &rect);

	popover = GTK_POPOVER (self);

	if (rect.width >= 0 && rect.height >= 0 && rect.x + rect.width >= 0 && rect.y + rect.height >= 0) {
		relative_to = GTK_WIDGET (cnt_editor);
	} else {
		relative_to = GTK_WIDGET (self->editor);

		rect.x = 0;
		rect.y = 0;
		rect.width = gtk_widget_get_allocated_width (relative_to);
		rect.height = 0;
	}

	gtk_popover_set_relative_to (popover, relative_to);
	gtk_popover_set_pointing_to (popover, &rect);

	gtk_popover_popup (popover);
}
