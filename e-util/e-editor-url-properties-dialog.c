/*
 * e-editor-url-properties-dialog.h
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

#include "e-editor-url-properties-dialog.h"
#include "e-editor-selection.h"
#include "e-editor-utils.h"

#include <glib/gi18n-lib.h>

G_DEFINE_TYPE (
	EEditorUrlPropertiesDialog,
	e_editor_url_properties_dialog,
	GTK_TYPE_WINDOW);

struct _EEditorUrlPropertiesDialogPrivate {
	EEditor *editor;

	GtkWidget *url_edit;
	GtkWidget *label_edit;
	GtkWidget *test_button;

	GtkWidget *remove_link_button;
	GtkWidget *close_button;
	GtkWidget *ok_button;
};

enum {
	PROP_0,
	PROP_EDITOR
};

static WebKitDOMElement *
find_anchor_element (WebKitDOMRange *range)
{
	WebKitDOMElement *link;
	WebKitDOMNode *node;

	node = webkit_dom_range_get_start_container (range, NULL);

	/* Try to find if the selection is within a link */
	link = NULL;
	link = e_editor_dom_node_get_parent_element (
			node, WEBKIT_TYPE_DOM_HTML_ANCHOR_ELEMENT);

	/* ...or if there is a link within selection */
	if (!link) {
		WebKitDOMNode *start_node = node;
		gboolean found = FALSE;
		do {
			if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (node)) {
				found = TRUE;
				break;
			}

			if (webkit_dom_node_has_child_nodes (node)) {
				node = webkit_dom_node_get_first_child (node);
			} else if (webkit_dom_node_get_next_sibling (node)) {
				node = webkit_dom_node_get_next_sibling (node);
			} else {
				node = webkit_dom_node_get_parent_node (node);
			}
		} while (!webkit_dom_node_is_same_node (node, start_node));

		if (found) {
			link = WEBKIT_DOM_ELEMENT (node);
		}
	}

	return link;
}


static void
editor_url_properties_dialog_test_url (EEditorUrlPropertiesDialog *dialog)
{
	gtk_show_uri (
		gtk_window_get_screen (GTK_WINDOW (dialog)),
		gtk_entry_get_text (GTK_ENTRY (dialog->priv->url_edit)),
		GDK_CURRENT_TIME,
		NULL);
}

static void
editor_url_properties_dialog_close (EEditorUrlPropertiesDialog *dialog)
{
	gtk_widget_hide (GTK_WIDGET (dialog));
}

static void
editor_url_properties_dialog_remove_link (EEditorUrlPropertiesDialog *dialog)
{
	EEditorSelection *selection;
	EEditorWidget *widget;

	widget = e_editor_get_editor_widget (dialog->priv->editor);
	selection = e_editor_widget_get_selection (widget);
	e_editor_selection_unlink (selection);

	editor_url_properties_dialog_close (dialog);
}

static void
editor_url_properties_dialog_ok (EEditorUrlPropertiesDialog *dialog)
{
	EEditorWidget *widget;
	EEditorSelection *selection;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMRange *range;
	WebKitDOMElement *link;

	widget = e_editor_get_editor_widget (dialog->priv->editor);
	selection = e_editor_widget_get_selection (widget);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));
	window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (window);

	if (!dom_selection ||
	    (webkit_dom_dom_selection_get_range_count (dom_selection) == 0)) {
		return;
	}

	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	link = find_anchor_element (range);

	if (link) {
		webkit_dom_html_anchor_element_set_href (
			WEBKIT_DOM_HTML_ANCHOR_ELEMENT (link),
			gtk_entry_get_text (GTK_ENTRY (dialog->priv->url_edit)));
	} else {
		gchar *text;

		/* Check whether a text is selected or not */
		text = webkit_dom_range_get_text (range);
		if (text && *text) {
			e_editor_selection_create_link (
				selection,
				gtk_entry_get_text (
					GTK_ENTRY (dialog->priv->url_edit)));
		} else {
			gchar *html = g_strdup_printf (
				"<a href=\"%s\">%s</a>",
				gtk_entry_get_text (
					GTK_ENTRY (dialog->priv->url_edit)),
				gtk_entry_get_text (
					GTK_ENTRY (dialog->priv->label_edit)));

			webkit_dom_document_exec_command (
				document, "insertHTML", FALSE, html);

			g_free (html);

		}

		g_free (text);
	}

	editor_url_properties_dialog_close (dialog);
}

static void
editor_url_properties_dialog_show (GtkWidget *widget)
{
	EEditorUrlPropertiesDialog *dialog;
	EEditorWidget *editor_widget;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMRange *range;
	WebKitDOMElement *link;

	dialog = E_EDITOR_URL_PROPERTIES_DIALOG (widget);
	editor_widget = e_editor_get_editor_widget (dialog->priv->editor);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (editor_widget));
	window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (window);

	/* Reset to default values */
	gtk_entry_set_text (GTK_ENTRY (dialog->priv->url_edit), "http://");
	gtk_entry_set_text (GTK_ENTRY (dialog->priv->label_edit), "");
	gtk_widget_set_sensitive (dialog->priv->label_edit, TRUE);
	gtk_widget_set_sensitive (dialog->priv->remove_link_button, TRUE);

	/* No selection at all */
	if (!dom_selection ||
	    webkit_dom_dom_selection_get_range_count (dom_selection) < 1) {

		goto chainup;
	}

	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	link = find_anchor_element (range);

	if (link) {
		gchar *href, *text;

		href = webkit_dom_html_anchor_element_get_href (
				WEBKIT_DOM_HTML_ANCHOR_ELEMENT (link));
		text = webkit_dom_html_element_get_inner_text (
				WEBKIT_DOM_HTML_ELEMENT (link));

		gtk_entry_set_text (
			GTK_ENTRY (dialog->priv->url_edit), href);
		gtk_entry_set_text (
			GTK_ENTRY (dialog->priv->label_edit), text);
		gtk_widget_set_sensitive (dialog->priv->label_edit, FALSE);

		g_free (text);
		g_free (href);
	} else {
		gchar *text;

		text = webkit_dom_range_get_text (range);
		if (text && *text) {
			gtk_entry_set_text (
				GTK_ENTRY (dialog->priv->label_edit), text);
			gtk_widget_set_sensitive (
				dialog->priv->label_edit, FALSE);
			gtk_widget_set_sensitive (
				dialog->priv->remove_link_button, FALSE);
		}
		g_free (text);
	}

 chainup:
	/* Chain up to parent implementation */
	GTK_WIDGET_CLASS (e_editor_url_properties_dialog_parent_class)->show (widget);
}

static void
editor_url_properties_dialog_set_property (GObject *object,
					   guint property_id,
					   const GValue *value,
					   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_EDITOR:
			E_EDITOR_URL_PROPERTIES_DIALOG (object)->priv->editor =
				g_object_ref (g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
editor_url_properties_dialog_finalize (GObject *object)
{
	EEditorUrlPropertiesDialogPrivate *priv;
	priv = E_EDITOR_URL_PROPERTIES_DIALOG (object)->priv;

	g_clear_object (&priv->editor);

	/* Chain up to parent implementation */
	G_OBJECT_CLASS (e_editor_url_properties_dialog_parent_class)->finalize (object);
}

static void
e_editor_url_properties_dialog_class_init (EEditorUrlPropertiesDialogClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	e_editor_url_properties_dialog_parent_class  = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (EEditorUrlPropertiesDialogPrivate));


	widget_class = GTK_WIDGET_CLASS (klass);
	widget_class->show = editor_url_properties_dialog_show;

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = editor_url_properties_dialog_set_property;
	object_class->finalize = editor_url_properties_dialog_finalize;

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
e_editor_url_properties_dialog_init (EEditorUrlPropertiesDialog *dialog)
{
	GtkGrid *main_layout;
	GtkBox *button_box;
	GtkWidget *widget;

	dialog->priv = G_TYPE_INSTANCE_GET_PRIVATE (
				dialog, E_TYPE_EDITOR_URL_PROPERTIES_DIALOG,
				EEditorUrlPropertiesDialogPrivate);

	main_layout = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_row_spacing (main_layout, 10);
	gtk_grid_set_column_spacing (main_layout, 10);
	gtk_container_add (GTK_CONTAINER (dialog), GTK_WIDGET (main_layout));
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 10);

	widget = gtk_entry_new ();
	gtk_grid_attach (main_layout, widget, 1, 0, 1, 1);
	dialog->priv->url_edit = widget;

	widget = gtk_label_new_with_mnemonic (_("URL:"));
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), dialog->priv->url_edit);
	gtk_grid_attach (main_layout, widget, 0, 0, 1, 1);

	widget = gtk_button_new_with_label (_("Test URL..."));
	gtk_grid_attach (main_layout, widget, 2, 0, 1, 1);
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (editor_url_properties_dialog_test_url), dialog);
	dialog->priv->test_button = widget;

	widget = gtk_entry_new ();
	gtk_grid_attach (main_layout, widget, 1, 1, 2, 1);
	dialog->priv->label_edit = widget;

	widget = gtk_label_new_with_mnemonic (_("Description:"));
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), dialog->priv->label_edit);
	gtk_grid_attach (main_layout, widget, 0, 1, 1, 1);

	button_box = GTK_BOX (gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL));
	gtk_box_set_spacing (button_box, 5);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (button_box), GTK_BUTTONBOX_END);
	gtk_grid_attach (main_layout, GTK_WIDGET (button_box), 0, 2, 3, 1);

	widget = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (editor_url_properties_dialog_close), dialog);
	gtk_box_pack_start (button_box, widget, FALSE, FALSE, 5);
	dialog->priv->close_button = widget;

	widget = gtk_button_new_with_label (_("Remove Link"));
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (editor_url_properties_dialog_remove_link), dialog);
	gtk_box_pack_start (button_box, widget, FALSE, FALSE, 5);
	dialog->priv->remove_link_button = widget;

	widget = gtk_button_new_from_stock (GTK_STOCK_OK);
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (editor_url_properties_dialog_ok), dialog);
	gtk_box_pack_start (button_box, widget, FALSE, FALSE, 5);
	dialog->priv->ok_button = widget;

	gtk_widget_show_all (GTK_WIDGET (main_layout));
}

GtkWidget *
e_editor_url_properties_dialog_new (EEditor *editor)
{
	return GTK_WIDGET (
		g_object_new (
			E_TYPE_EDITOR_URL_PROPERTIES_DIALOG,
			"destroy-with-parent", TRUE,
			"events", GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK,
			"editor", editor,
			"icon-name", "insert-link",
			"resizable", FALSE,
			"title", N_("Link Properties"),
			"transient-for", gtk_widget_get_toplevel (GTK_WIDGET (editor)),
			"type", GTK_WINDOW_TOPLEVEL,
			"window-position", GTK_WIN_POS_CENTER_ON_PARENT,
			NULL));
}
