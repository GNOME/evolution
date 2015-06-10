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
#include "e-web-view.h"

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
	gboolean unlinking;

	WebKitDOMElement *link_element;

	EHTMLEditorViewHistoryEvent *history_event;
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
	dialog->priv->unlinking = TRUE;

	gtk_widget_hide (GTK_WIDGET (dialog));
}

static void
html_editor_link_dialog_ok (EHTMLEditorLinkDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorSelection *selection;
	EHTMLEditorView *view;
	WebKitDOMDocument *document;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);
	selection = e_html_editor_view_get_selection (view);
	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

	if (dialog->priv->link_element) {
		WebKitDOMElement *element;

		webkit_dom_html_anchor_element_set_href (
			WEBKIT_DOM_HTML_ANCHOR_ELEMENT (dialog->priv->link_element),
			gtk_entry_get_text (GTK_ENTRY (dialog->priv->url_edit)));
		webkit_dom_html_element_set_inner_html (
			WEBKIT_DOM_HTML_ELEMENT (dialog->priv->link_element),
			gtk_entry_get_text (GTK_ENTRY (dialog->priv->label_edit)),
			NULL);

		element = webkit_dom_document_create_element (document, "SPAN", NULL);
		webkit_dom_element_set_id (element, "-x-evo-selection-end-marker");
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (dialog->priv->link_element)),
			WEBKIT_DOM_NODE (element),
			webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (dialog->priv->link_element)),
			NULL);

		element = webkit_dom_document_create_element (document, "SPAN", NULL);
		webkit_dom_element_set_id (element, "-x-evo-selection-start-marker");
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (dialog->priv->link_element)),
			WEBKIT_DOM_NODE (element),
			webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (dialog->priv->link_element)),
			NULL);

		e_html_editor_selection_restore (selection);
	} else {
		WebKitDOMDOMWindow *dom_window;
		WebKitDOMDOMSelection *dom_selection;
		WebKitDOMRange *range;

		dom_window = webkit_dom_document_get_default_view (document);
		dom_selection = webkit_dom_dom_window_get_selection (dom_window);
		g_object_unref (dom_window);

		e_html_editor_selection_restore (selection);
		range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
		if (webkit_dom_range_get_collapsed (range, NULL)) {
			WebKitDOMElement *selection_marker;
			WebKitDOMElement *anchor;

			e_html_editor_selection_save (selection);
			selection_marker = webkit_dom_document_get_element_by_id (
				document, "-x-evo-selection-start-marker");
			anchor = webkit_dom_document_create_element (document, "A", NULL);
			webkit_dom_element_set_attribute (
				anchor, "href", gtk_entry_get_text (GTK_ENTRY (dialog->priv->url_edit)), NULL);
			webkit_dom_html_element_set_inner_text (
				WEBKIT_DOM_HTML_ELEMENT (anchor),
				gtk_entry_get_text (
					GTK_ENTRY (dialog->priv->label_edit)),
				NULL);
			dialog->priv->link_element = anchor;

			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (
					WEBKIT_DOM_NODE (selection_marker)),
				WEBKIT_DOM_NODE (anchor),
				WEBKIT_DOM_NODE (selection_marker),
				NULL);
			e_html_editor_selection_restore (selection);
		} else {
			gchar *text;

			text = webkit_dom_range_get_text (range);
			if (text && *text) {
				WebKitDOMElement *selection_marker;
				WebKitDOMNode *parent;

				e_html_editor_selection_create_link (
					selection, gtk_entry_get_text (GTK_ENTRY (dialog->priv->url_edit)));

				dialog->priv->history_event->data.dom.from =
					WEBKIT_DOM_NODE (webkit_dom_document_create_text_node (document, text));

				e_html_editor_selection_save (selection);
				selection_marker = webkit_dom_document_get_element_by_id (
					document, "-x-evo-selection-start-marker");
				parent = webkit_dom_node_get_parent_node (
					WEBKIT_DOM_NODE (selection_marker));
				if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (parent))
					dialog->priv->link_element = WEBKIT_DOM_ELEMENT (parent);
				e_html_editor_selection_restore (selection);
				webkit_dom_dom_selection_collapse_to_end (dom_selection, NULL);
			}
			g_free (text);
		}

		g_object_unref (range);
		g_object_unref (dom_selection);
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
	EHTMLEditorLinkDialog *dialog;
	EHTMLEditorSelection *selection;
	EHTMLEditorView *view;

	dialog = E_HTML_EDITOR_LINK_DIALOG (widget);
	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);
	selection = e_html_editor_view_get_selection (view);

	/* Reset to default values */
	gtk_entry_set_text (GTK_ENTRY (dialog->priv->url_edit), "http://");
	gtk_entry_set_text (GTK_ENTRY (dialog->priv->label_edit), "");
	gtk_widget_set_sensitive (dialog->priv->label_edit, TRUE);
	gtk_widget_set_sensitive (dialog->priv->remove_link_button, TRUE);
	dialog->priv->label_autofill = TRUE;
	dialog->priv->unlinking = FALSE;

	if (!e_html_editor_view_is_undo_redo_in_progress (view)) {
		EHTMLEditorViewHistoryEvent *ev;

		ev = g_new0 (EHTMLEditorViewHistoryEvent, 1);
		ev->type = HISTORY_LINK_DIALOG;

		e_html_editor_selection_get_selection_coordinates (
			selection, &ev->before.start.x, &ev->before.start.y, &ev->before.end.x, &ev->before.end.y);
		if (dialog->priv->link_element)
			ev->data.dom.from = webkit_dom_node_clone_node (
				WEBKIT_DOM_NODE (dialog->priv->link_element), TRUE);
		else
			ev->data.dom.from = NULL;
		dialog->priv->history_event = ev;
	}

	if (dialog->priv->link_element) {
		gchar *href, *text;

		href = webkit_dom_element_get_attribute (dialog->priv->link_element, "href");
		text = webkit_dom_html_element_get_inner_text (
			WEBKIT_DOM_HTML_ELEMENT (dialog->priv->link_element));

		gtk_entry_set_text (
			GTK_ENTRY (dialog->priv->url_edit), href);
		gtk_entry_set_text (
			GTK_ENTRY (dialog->priv->label_edit), text);

		g_free (text);
		g_free (href);
	} else {
		gchar *text;
		WebKitDOMDocument *document;
		WebKitDOMDOMWindow *dom_window;
		WebKitDOMDOMSelection *dom_selection;
		WebKitDOMRange *range;

		document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
		dom_window = webkit_dom_document_get_default_view (document);
		dom_selection = webkit_dom_dom_window_get_selection (dom_window);
		g_object_unref (dom_window);

		/* No selection at all */
		if (!dom_selection || webkit_dom_dom_selection_get_range_count (dom_selection) < 1)
			gtk_widget_set_sensitive (dialog->priv->remove_link_button, FALSE);

		range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
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

		g_object_unref (range);
		g_object_unref (dom_selection);

		e_html_editor_selection_save (selection);
	}

	/* Chain up to parent implementation */
	GTK_WIDGET_CLASS (e_html_editor_link_dialog_parent_class)->show (widget);
}

static void
html_editor_link_dialog_hide (GtkWidget *widget)
{
	EHTMLEditorLinkDialogPrivate *priv;
	EHTMLEditorViewHistoryEvent *ev;

	priv = E_HTML_EDITOR_LINK_DIALOG_GET_PRIVATE (widget);
	ev = priv->history_event;

	if (priv->unlinking || !priv->link_element) {
		g_clear_object (&ev->data.dom.from);
		g_free (ev);
	} else if (ev) {
		EHTMLEditorLinkDialog *dialog;
		EHTMLEditor *editor;
		EHTMLEditorSelection *selection;
		EHTMLEditorView *view;

		dialog = E_HTML_EDITOR_LINK_DIALOG (widget);
		editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
		view = e_html_editor_get_view (editor);
		selection = e_html_editor_view_get_selection (view);

		ev->data.dom.to = webkit_dom_node_clone_node (
			WEBKIT_DOM_NODE (priv->link_element), TRUE);

		if (!webkit_dom_node_is_equal_node (ev->data.dom.from, ev->data.dom.to)) {
			e_html_editor_selection_get_selection_coordinates (
				selection, &ev->after.start.x, &ev->after.start.y, &ev->after.end.x, &ev->after.end.y);
			e_html_editor_view_insert_new_history_event (view, ev);
			if (!ev->data.dom.from)
				g_object_unref (priv->link_element);
		} else {
			g_object_unref (ev->data.dom.from);
			g_object_unref (ev->data.dom.to);
			g_free (ev);
		}
	}

	priv->link_element = NULL;

	GTK_WIDGET_CLASS (e_html_editor_link_dialog_parent_class)->hide (widget);
}

static void
e_html_editor_link_dialog_class_init (EHTMLEditorLinkDialogClass *class)
{
	GtkWidgetClass *widget_class;

	g_type_class_add_private (class, sizeof (EHTMLEditorLinkDialogPrivate));

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->show = html_editor_link_dialog_show;
	widget_class->hide = html_editor_link_dialog_hide;
}

static void
e_html_editor_link_dialog_init (EHTMLEditorLinkDialog *dialog)
{
	GtkGrid *main_layout;
	GtkBox *button_box;
	GtkWidget *widget;

	dialog->priv = E_HTML_EDITOR_LINK_DIALOG_GET_PRIVATE (dialog);
	dialog->priv->link_element = NULL;

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

void
e_html_editor_link_dialog_show (EHTMLEditorLinkDialog *dialog,
                                WebKitDOMNode *link)
{
	EHTMLEditorLinkDialogClass *class;

	g_return_if_fail (E_IS_HTML_EDITOR_LINK_DIALOG (dialog));

	dialog->priv->link_element = link ? WEBKIT_DOM_ELEMENT (link) : NULL;

	class = E_HTML_EDITOR_LINK_DIALOG_GET_CLASS (dialog);
	GTK_WIDGET_CLASS (class)->show (GTK_WIDGET (dialog));
}
