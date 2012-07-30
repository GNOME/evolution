/*
 * e-editor-find-dialog.h
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-editor-find-dialog.h"

#include <glib/gi18n-lib.h>
#include <gdk/gdkkeysyms.h>

G_DEFINE_TYPE (
	EEditorFindDialog,
	e_editor_find_dialog,
	GTK_TYPE_WINDOW);

struct _EEditorFindDialogPrivate {
	GtkWidget *entry;
	GtkWidget *backwards;
	GtkWidget *case_sensitive;
	GtkWidget *wrap_search;

	GtkWidget *find_button;
	GtkWidget *cancel_button;

	GtkWidget *result_label;

	EEditor *editor;
};

enum {
	PROP_0,
	PROP_EDITOR
};

static void
reset_dialog (EEditorFindDialog *dialog)
{
	gtk_widget_set_sensitive (dialog->priv->find_button, TRUE);
	gtk_widget_hide (dialog->priv->result_label);
}

static void
editor_find_dialog_show (GtkWidget *widget)
{
	EEditorFindDialog *dialog = E_EDITOR_FIND_DIALOG (widget);

	reset_dialog (dialog);
	gtk_widget_grab_focus (dialog->priv->entry);

	/* Chain up to parent's implementation */
	GTK_WIDGET_CLASS (e_editor_find_dialog_parent_class)->show (widget);
}

static void
editor_find_dialog_close_cb (EEditorFindDialog *dialog)
{
	gtk_widget_hide (GTK_WIDGET (dialog));
}

static void
editor_find_dialog_find_cb (EEditorFindDialog *dialog)
{
	gboolean found;
	EEditorWidget *editor_widget;

	editor_widget = e_editor_get_editor_widget (dialog->priv->editor);
	found = webkit_web_view_search_text (
			WEBKIT_WEB_VIEW (editor_widget),
			gtk_entry_get_text (
				GTK_ENTRY (dialog->priv->entry)),
			gtk_toggle_button_get_active (
				GTK_TOGGLE_BUTTON (
					dialog->priv->case_sensitive)),
			!gtk_toggle_button_get_active (
				GTK_TOGGLE_BUTTON (
					dialog->priv->backwards)),
			gtk_toggle_button_get_active (
				GTK_TOGGLE_BUTTON (
					dialog->priv->wrap_search)));

	gtk_widget_set_sensitive (dialog->priv->find_button, found);

	if (!found) {
		gtk_label_set_label (
			GTK_LABEL (dialog->priv->result_label),
			N_("No match found"));
		gtk_widget_show (dialog->priv->result_label);
	}
}

static gboolean
entry_key_release_event (GtkWidget *widget,
			 GdkEvent *event,
			 gpointer user_data)
{
	GdkEventKey *key = &event->key;
	EEditorFindDialog *dialog = user_data;

	if (key->keyval == GDK_KEY_Return) {
		editor_find_dialog_find_cb (dialog);
		return TRUE;
	}

	reset_dialog (dialog);
	return FALSE;
}

static void
editor_find_dialog_set_property (GObject *object,
				 guint property_id,
				 const GValue *value,
				 GParamSpec *pspec)
{
	EEditorFindDialog *dialog = E_EDITOR_FIND_DIALOG (object);

	switch (property_id) {
		case PROP_EDITOR:
			dialog->priv->editor =
				g_object_ref (g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
editor_find_dialog_finalize (GObject *object)
{
	EEditorFindDialogPrivate *priv = E_EDITOR_FIND_DIALOG (object)->priv;

	g_clear_object (&priv->editor);

	/* Chain up to parent's finalize */
	G_OBJECT_CLASS (e_editor_find_dialog_parent_class)->finalize (object);
}

static void
e_editor_find_dialog_class_init (EEditorFindDialogClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	e_editor_find_dialog_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (EEditorFindDialogPrivate));

	widget_class = GTK_WIDGET_CLASS (klass);
	widget_class->show = editor_find_dialog_show;

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = editor_find_dialog_set_property;
	object_class->finalize = editor_find_dialog_finalize;

	g_object_class_install_property (
		object_class,
		PROP_EDITOR,
		g_param_spec_object (
			"editor",
		        NULL,
		        NULL,
		        E_TYPE_EDITOR,
		        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
e_editor_find_dialog_init (EEditorFindDialog *dialog)
{
	GtkBox *main_layout, *box;
	GtkWidget *widget;

	dialog->priv = G_TYPE_INSTANCE_GET_PRIVATE (
		dialog, E_TYPE_EDITOR_FIND_DIALOG, EEditorFindDialogPrivate);

	main_layout = GTK_BOX (gtk_vbox_new (FALSE, 5));
	gtk_container_add (GTK_CONTAINER (dialog), GTK_WIDGET (main_layout));
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 10);

	widget = gtk_entry_new ();
	gtk_box_pack_start (main_layout, widget, TRUE, TRUE, 5);
	dialog->priv->entry = widget;
	g_signal_connect (
		widget, "key-release-event",
		G_CALLBACK (entry_key_release_event), dialog);

	box = GTK_BOX (gtk_hbox_new (FALSE, 5));
	gtk_box_pack_start (main_layout, GTK_WIDGET (box), TRUE, TRUE, 0);

	widget = gtk_check_button_new_with_mnemonic (N_("Search _backwards"));
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);
	dialog->priv->backwards = widget;
	g_signal_connect_swapped (
		widget, "toggled",
		G_CALLBACK (reset_dialog), dialog);

	widget = gtk_check_button_new_with_mnemonic (N_("Case _Sensitive"));
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);
	dialog->priv->case_sensitive = widget;
	g_signal_connect_swapped (
		widget, "toggled",
		G_CALLBACK (reset_dialog), dialog);

	widget = gtk_check_button_new_with_mnemonic (N_("_Wrap Search"));
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);
	dialog->priv->wrap_search = widget;
	g_signal_connect_swapped (
		widget, "toggled",
		G_CALLBACK (reset_dialog), dialog);

	box = GTK_BOX (gtk_hbox_new (FALSE, 5));
	gtk_box_pack_start (main_layout, GTK_WIDGET (box), TRUE, TRUE, 0);

	widget = gtk_label_new ("");
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);
	dialog->priv->result_label = widget;

	widget = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_box_set_spacing (GTK_BOX (widget), 5);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (widget), GTK_BUTTONBOX_END);
	gtk_box_pack_end (box, widget, TRUE, TRUE, 0);
	box = GTK_BOX (widget);

	widget = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
	gtk_box_pack_start (box, widget, FALSE, FALSE, 5);
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (editor_find_dialog_close_cb), dialog);
	dialog->priv->cancel_button = widget;

	widget = gtk_button_new_from_stock (GTK_STOCK_FIND);
	gtk_box_pack_start (box, widget, FALSE, FALSE, 5);
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (editor_find_dialog_find_cb), dialog);
	dialog->priv->find_button = widget;

	gtk_widget_show_all (GTK_WIDGET (main_layout));
}

GtkWidget *
e_editor_find_dialog_new(EEditor *editor)
{
	return GTK_WIDGET (
		g_object_new (
			E_TYPE_EDITOR_FIND_DIALOG,
			"destroy-with-parent", TRUE,
			"events", GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK,
			"editor", editor,
			"icon-name", GTK_STOCK_FIND,
			"resizable", FALSE,
			"title", N_("Find"),
			"transient-for", gtk_widget_get_toplevel (GTK_WIDGET (editor)),
			"type", GTK_WINDOW_TOPLEVEL,
			"type-hint", GDK_WINDOW_TYPE_HINT_POPUP_MENU,
			"window-position", GTK_WIN_POS_CENTER_ON_PARENT,
			NULL));
}

void
e_editor_find_dialog_find_next (EEditorFindDialog *dialog)
{
	if (gtk_entry_get_text_length (GTK_ENTRY (dialog->priv->entry)) == 0) {
		return;
	}

	editor_find_dialog_find_cb (dialog);
}
