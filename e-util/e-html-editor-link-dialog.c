/*
 * e-html-editor-link-dialog.h
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-html-editor-link-dialog.h"
#include "e-html-editor-selection.h"
#include "e-html-editor-utils.h"
#include "e-html-editor-view.h"

#include <glib/gi18n-lib.h>

#define E_HTML_EDITOR_LINK_DIALOG_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_HTML_EDITOR_LINK_DIALOG, EHTMLEditorLinkDialogPrivate))

G_DEFINE_TYPE (
	EHTMLEditorLinkDialog,
	e_html_editor_link_dialog,
	E_TYPE_HTML_EDITOR_DIALOG);

struct _EHTMLEditorLinkDialogPrivate {
	GtkWidget *url_edit;
	GtkWidget *label_edit;
	GtkWidget *test_button;

	GtkWidget *remove_link_button;
	GtkWidget *ok_button;

	gboolean label_autofill;
};

static void
html_editor_link_dialog_test_link (EHTMLEditorLinkDialog *dialog)
{
	gtk_show_uri (
		gtk_window_get_screen (GTK_WINDOW (dialog)),
		gtk_entry_get_text (GTK_ENTRY (dialog->priv->url_edit)),
		GDK_CURRENT_TIME,
		NULL);
}

static void
html_editor_link_dialog_url_changed (EHTMLEditorLinkDialog *dialog)
{
	if (dialog->priv->label_autofill &&
	    gtk_widget_is_sensitive (dialog->priv->label_edit)) {
		const gchar *text;

		text = gtk_entry_get_text (
			GTK_ENTRY (dialog->priv->url_edit));
		gtk_entry_set_text (
			GTK_ENTRY (dialog->priv->label_edit), text);
	}
}

static gboolean
html_editor_link_dialog_description_changed (EHTMLEditorLinkDialog *dialog)
{
	const gchar *text;

	text = gtk_entry_get_text (GTK_ENTRY (dialog->priv->label_edit));
	dialog->priv->label_autofill = (*text == '\0');

	return FALSE;
}

static void
html_editor_link_dialog_remove_link (EHTMLEditorLinkDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	EHTMLEditorSelection *selection;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);
	selection = e_html_editor_view_get_selection (view);
	e_html_editor_selection_unlink (selection);

	gtk_widget_hide (GTK_WIDGET (dialog));
}

static void
html_editor_link_dialog_ok (EHTMLEditorLinkDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	EHTMLEditorSelection *selection;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMRange *range;
	WebKitDOMElement *link;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);
	selection = e_html_editor_view_get_selection (view);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (window);

	if (!dom_selection ||
	    (webkit_dom_dom_selection_get_range_count (dom_selection) == 0)) {
		gtk_widget_hide (GTK_WIDGET (dialog));
		return;
	}

	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	link = e_html_editor_dom_node_find_parent_element (
			webkit_dom_range_get_start_container (range, NULL), "A");
	if (!link) {
		if ((webkit_dom_range_get_start_container (range, NULL) !=
			webkit_dom_range_get_end_container (range, NULL)) ||
		    (webkit_dom_range_get_start_offset (range, NULL) !=
			webkit_dom_range_get_end_offset (range, NULL))) {

			WebKitDOMDocumentFragment *fragment;
			fragment = webkit_dom_range_extract_contents (range, NULL);
			link = e_html_editor_dom_node_find_child_element (
				WEBKIT_DOM_NODE (fragment), "A");
			webkit_dom_range_insert_node (
				range, WEBKIT_DOM_NODE (fragment), NULL);

			webkit_dom_dom_selection_set_base_and_extent (
				dom_selection,
				webkit_dom_range_get_start_container (range, NULL),
				webkit_dom_range_get_start_offset (range, NULL),
				webkit_dom_range_get_end_container (range, NULL),
				webkit_dom_range_get_end_offset (range, NULL),
				NULL);
		} else {
			WebKitDOMNode *node;

			/* get element that was clicked on */
			node = webkit_dom_range_get_common_ancestor_container (range, NULL);
			if (node && !WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (node)) {
				link = e_html_editor_dom_node_find_parent_element (node, "A");
				if (link && !WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (link))
					link = NULL;
			} else
				link = WEBKIT_DOM_ELEMENT (node);
		}
	}

	if (link) {
		webkit_dom_html_anchor_element_set_href (
			WEBKIT_DOM_HTML_ANCHOR_ELEMENT (link),
			gtk_entry_get_text (GTK_ENTRY (dialog->priv->url_edit)));
		webkit_dom_html_element_set_inner_html (
			WEBKIT_DOM_HTML_ELEMENT (link),
			gtk_entry_get_text (GTK_ENTRY (dialog->priv->label_edit)),
			NULL);
	} else {
		gchar *text;

		/* Check whether a text is selected or not */
		text = webkit_dom_range_get_text (range);
		if (text && *text) {
			e_html_editor_selection_create_link (
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

			e_html_editor_view_exec_command (
				view, E_HTML_EDITOR_VIEW_COMMAND_INSERT_HTML, html);

			g_free (html);

		}

		g_free (text);
	}

	gtk_widget_hide (GTK_WIDGET (dialog));
}

static gboolean
html_editor_link_dialog_entry_key_pressed (EHTMLEditorLinkDialog *dialog,
                                           GdkEventKey *event)
{
	/* We can't do thins in key_released, because then you could not open
	 * this dialog from main menu by pressing enter on Insert->Link action */
	if (event->keyval == GDK_KEY_Return) {
		html_editor_link_dialog_ok (dialog);
		return TRUE;
	}

	return FALSE;
}

static void
html_editor_link_dialog_show (GtkWidget *widget)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	EHTMLEditorLinkDialog *dialog;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMRange *range;
	WebKitDOMElement *link;

	dialog = E_HTML_EDITOR_LINK_DIALOG (widget);
	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (window);

	/* Reset to default values */
	gtk_entry_set_text (GTK_ENTRY (dialog->priv->url_edit), "http://");
	gtk_entry_set_text (GTK_ENTRY (dialog->priv->label_edit), "");
	gtk_widget_set_sensitive (dialog->priv->label_edit, TRUE);
	gtk_widget_set_sensitive (dialog->priv->remove_link_button, TRUE);
	dialog->priv->label_autofill = TRUE;

	/* No selection at all */
	if (!dom_selection ||
	    webkit_dom_dom_selection_get_range_count (dom_selection) < 1) {
		gtk_widget_set_sensitive (dialog->priv->remove_link_button, FALSE);
		goto chainup;
	}

	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	link = e_html_editor_dom_node_find_parent_element (
		webkit_dom_range_get_start_container (range, NULL), "A");
	if (!link) {
		if ((webkit_dom_range_get_start_container (range, NULL) !=
			webkit_dom_range_get_end_container (range, NULL)) ||
		    (webkit_dom_range_get_start_offset (range, NULL) !=
			webkit_dom_range_get_end_offset (range, NULL))) {

			WebKitDOMDocumentFragment *fragment;
			fragment = webkit_dom_range_clone_contents (range, NULL);
			link = e_html_editor_dom_node_find_child_element (
					WEBKIT_DOM_NODE (fragment), "A");
		} else {
			WebKitDOMNode *node;

			node = webkit_dom_range_get_common_ancestor_container (range, NULL);
			if (node && !WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (node)) {
				link = e_html_editor_dom_node_find_parent_element (node, "A");
				if (link && !WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (link))
					link = NULL;
			} else
				link = WEBKIT_DOM_ELEMENT (node);
		}
	}

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
	GTK_WIDGET_CLASS (e_html_editor_link_dialog_parent_class)->show (widget);
}

static void
e_html_editor_link_dialog_class_init (EHTMLEditorLinkDialogClass *class)
{
	GtkWidgetClass *widget_class;

	g_type_class_add_private (class, sizeof (EHTMLEditorLinkDialogPrivate));

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->show = html_editor_link_dialog_show;
}

static void
e_html_editor_link_dialog_init (EHTMLEditorLinkDialog *dialog)
{
	GtkGrid *main_layout;
	GtkBox *button_box;
	GtkWidget *widget;

	dialog->priv = E_HTML_EDITOR_LINK_DIALOG_GET_PRIVATE (dialog);

	main_layout = e_html_editor_dialog_get_container (E_HTML_EDITOR_DIALOG (dialog));

	widget = gtk_entry_new ();
	gtk_grid_attach (main_layout, widget, 1, 0, 1, 1);
	g_signal_connect_swapped (
		widget, "notify::text",
		G_CALLBACK (html_editor_link_dialog_url_changed), dialog);
	g_signal_connect_swapped (
		widget, "key-press-event",
		G_CALLBACK (html_editor_link_dialog_entry_key_pressed), dialog);
	dialog->priv->url_edit = widget;

	widget = gtk_label_new_with_mnemonic (_("_URL:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), dialog->priv->url_edit);
	gtk_grid_attach (main_layout, widget, 0, 0, 1, 1);

	widget = gtk_button_new_with_mnemonic (_("_Test URL..."));
	gtk_grid_attach (main_layout, widget, 2, 0, 1, 1);
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (html_editor_link_dialog_test_link), dialog);
	dialog->priv->test_button = widget;

	widget = gtk_entry_new ();
	gtk_grid_attach (main_layout, widget, 1, 1, 2, 1);
	g_signal_connect_swapped (
		widget, "key-release-event",
		G_CALLBACK (html_editor_link_dialog_description_changed), dialog);
	g_signal_connect_swapped (
		widget, "key-press-event",
		G_CALLBACK (html_editor_link_dialog_entry_key_pressed), dialog);
	dialog->priv->label_edit = widget;

	widget = gtk_label_new_with_mnemonic (_("_Description:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), dialog->priv->label_edit);
	gtk_grid_attach (main_layout, widget, 0, 1, 1, 1);

	button_box = e_html_editor_dialog_get_button_box (E_HTML_EDITOR_DIALOG (dialog));

	widget = gtk_button_new_with_mnemonic (_("_Remove Link"));
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (html_editor_link_dialog_remove_link), dialog);
	gtk_box_pack_start (button_box, widget, FALSE, FALSE, 5);
	dialog->priv->remove_link_button = widget;

	widget = gtk_button_new_with_mnemonic (_("_OK"));
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (html_editor_link_dialog_ok), dialog);
	gtk_box_pack_end (button_box, widget, FALSE, FALSE, 5);
	dialog->priv->ok_button = widget;

	gtk_widget_show_all (GTK_WIDGET (main_layout));
}

GtkWidget *
e_html_editor_link_dialog_new (EHTMLEditor *editor)
{
	return GTK_WIDGET (
		g_object_new (
			E_TYPE_HTML_EDITOR_LINK_DIALOG,
			"editor", editor,
			"icon-name", "insert-link",
			"title", _("Link Properties"),
			NULL));
}
