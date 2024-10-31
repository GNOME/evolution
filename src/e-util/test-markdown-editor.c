/*
 * SPDX-FileCopyrightText: (C) 2022 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>
#include <e-util/e-util.h>

static gboolean
window_delete_event_cb (GtkWidget *widget,
			GdkEvent *event,
			gpointer user_data)
{
	gtk_main_quit ();

	return FALSE;
}

static void
editor_to_plain_text_cb (GObject *button,
			 EMarkdownEditor *editor)
{
	GtkTextView *text_view;
	GtkTextBuffer *buffer;
	gchar *text;

	text_view = g_object_get_data (button, "text_view");
	buffer = gtk_text_view_get_buffer (text_view);
	text = e_markdown_editor_dup_text (editor);

	gtk_text_buffer_set_text (buffer, text, -1);

	g_free (text);
}

static void
editor_to_html_cb (GObject *button,
		   EMarkdownEditor *editor)
{
	GtkTextView *text_view;
	GtkTextBuffer *buffer;
	gchar *text;

	text_view = g_object_get_data (button, "text_view");
	buffer = gtk_text_view_get_buffer (text_view);
	text = e_markdown_editor_dup_html (editor);

	gtk_text_buffer_set_text (buffer, text ? text : "NULL", -1);

	g_free (text);
}

/* The cmark can add an empty line at the end of the HTML, thus compare without it too */
static gboolean
texts_are_same (gchar *text1,
		gchar *text2)
{
	gint len1 = 0, len2 = 0;
	gboolean text1_modified = FALSE, text2_modified = FALSE;
	gboolean same;

	if (text1 && text2) {
		len1 = strlen (text1);
		len2 = strlen (text2);

		if (len1 + 1 == len2 && text2[len2 - 1] == '\n') {
			text2[len2 - 1] = '\0';
			text2_modified = TRUE;
		} else if (len1 == len2 + 1 && text1[len1 - 1] == '\n') {
			text1[len1 - 1] = '\0';
			text1_modified = TRUE;
		}
	}

	same = g_strcmp0 (text1, text2) == 0;

	if (text1_modified)
		text1[len1 - 1] = '\n';
	else if (text2_modified)
		text2[len2 - 1] = '\n';

	return same || ((text1_modified || text2_modified) && g_strcmp0 (text1, text2) == 0);
}

static void
plain_text_to_html_cb (GObject *button,
		       GtkLabel *label)
{
	GtkTextView *plain_text_view, *html_text_view;
	GtkTextBuffer *plain_buffer, *html_buffer;
	GtkTextIter start_iter, end_iter;
	gchar *old_text, *new_text, *tmp;

	plain_text_view = g_object_get_data (button, "plain_text_view");
	plain_buffer = gtk_text_view_get_buffer (plain_text_view);

	html_text_view = g_object_get_data (button, "html_text_view");
	html_buffer = gtk_text_view_get_buffer (html_text_view);

	gtk_text_buffer_get_bounds (html_buffer, &start_iter, &end_iter);
	old_text = gtk_text_buffer_get_text (html_buffer, &start_iter, &end_iter, FALSE);

	gtk_text_buffer_get_bounds (plain_buffer, &start_iter, &end_iter);
	tmp = gtk_text_buffer_get_text (plain_buffer, &start_iter, &end_iter, FALSE);

	new_text = e_markdown_utils_text_to_html (tmp, -1);

	gtk_text_buffer_set_text (html_buffer, new_text ? new_text : "NULL", -1);

	if (texts_are_same (new_text, old_text))
		gtk_label_set_text (label, "HTML text matches");
	else
		gtk_label_set_text (label, "Old and new HTML texts differ");

	g_free (old_text);
	g_free (new_text);
	g_free (tmp);
}

static void
html_to_plain_text_cb (GObject *button,
		       GtkLabel *label)
{
	GtkTextView *plain_text_view, *html_text_view;
	GtkTextBuffer *plain_buffer, *html_buffer;
	GtkTextIter start_iter, end_iter;
	GtkToggleButton *disallow_html_check;
	GtkComboBox *link_to_text_combo;
	EHTMLLinkToText link_to_text;
	gchar *old_text, *new_text, *tmp;

	disallow_html_check = g_object_get_data (button, "disallow_html_check");
	link_to_text_combo = g_object_get_data (button, "link_to_text_combo");

	switch (gtk_combo_box_get_active (link_to_text_combo)) {
	case 1:
		link_to_text = E_HTML_LINK_TO_TEXT_INLINE;
		break;
	case 2:
		link_to_text = E_HTML_LINK_TO_TEXT_REFERENCE;
		break;
	case 3:
		link_to_text = E_HTML_LINK_TO_TEXT_REFERENCE_WITHOUT_LABEL;
		break;
	default:
		link_to_text = E_HTML_LINK_TO_TEXT_NONE;
		break;
	}

	plain_text_view = g_object_get_data (button, "plain_text_view");
	plain_buffer = gtk_text_view_get_buffer (plain_text_view);

	html_text_view = g_object_get_data (button, "html_text_view");
	html_buffer = gtk_text_view_get_buffer (html_text_view);

	gtk_text_buffer_get_bounds (plain_buffer, &start_iter, &end_iter);
	old_text = gtk_text_buffer_get_text (plain_buffer, &start_iter, &end_iter, FALSE);

	gtk_text_buffer_get_bounds (html_buffer, &start_iter, &end_iter);
	tmp = gtk_text_buffer_get_text (html_buffer, &start_iter, &end_iter, FALSE);

	new_text = e_markdown_utils_html_to_text (tmp, -1, E_MARKDOWN_HTML_TO_TEXT_FLAG_NONE |
		(gtk_toggle_button_get_active (disallow_html_check) ? (E_MARKDOWN_HTML_TO_TEXT_FLAG_PLAIN_TEXT |
		e_markdown_utils_link_to_text_to_flags (link_to_text)) : 0));

	gtk_text_buffer_set_text (plain_buffer, new_text ? new_text : "NULL", -1);

	if (texts_are_same (new_text, old_text))
		gtk_label_set_text (label, "Plain text matches");
	else
		gtk_label_set_text (label, "Old and new plain texts differ");

	g_free (old_text);
	g_free (new_text);
	g_free (tmp);
}

static gint
on_idle_create_widget (gpointer user_data)
{
	GtkWidget *window, *editor, *widget, *button1, *button2;
	GtkGrid *grid;
	GtkTextView *plain_text_view;
	GtkTextView *html_text_view;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size (GTK_WINDOW (window), 640, 480);

	g_signal_connect (
		window, "delete-event",
		G_CALLBACK (window_delete_event_cb), NULL);

	widget = gtk_grid_new ();

	g_object_set (G_OBJECT (widget),
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"visible", TRUE,
		NULL);

	gtk_container_add (GTK_CONTAINER (window), widget);

	grid = GTK_GRID (widget);

	editor = e_markdown_editor_new ();

	g_object_set (G_OBJECT (editor),
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"visible", TRUE,
		NULL);

	gtk_grid_attach (grid, editor, 0, 0, 3, 1);

	widget = gtk_button_new_with_label ("vvv   As Plain Text   vvv");
	button1 = widget;

	g_object_set (G_OBJECT (widget),
		"halign", GTK_ALIGN_START,
		"hexpand", FALSE,
		"valign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"visible", TRUE,
		NULL);

	gtk_grid_attach (grid, widget, 0, 1, 1, 1);

	widget = gtk_button_new_with_label ("vvv   As HTML   vvv");
	button2 = widget;

	g_object_set (G_OBJECT (widget),
		"halign", GTK_ALIGN_START,
		"hexpand", FALSE,
		"valign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"visible", TRUE,
		NULL);

	gtk_grid_attach (grid, widget, 2, 1, 1, 1);

	widget = gtk_text_view_new ();
	plain_text_view = GTK_TEXT_VIEW (widget);

	g_object_set (G_OBJECT (widget),
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"visible", TRUE,
		"editable", TRUE,
		"wrap-mode", GTK_WRAP_WORD_CHAR,
		NULL);

	widget = gtk_scrolled_window_new (NULL, NULL);

	g_object_set (G_OBJECT (widget),
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"visible", TRUE,
		"hscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		NULL);

	gtk_container_add (GTK_CONTAINER (widget), GTK_WIDGET (plain_text_view));

	gtk_grid_attach (grid, widget, 0, 2, 1, 2);

	widget = gtk_text_view_new ();
	html_text_view = GTK_TEXT_VIEW (widget);

	g_object_set (G_OBJECT (widget),
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"visible", TRUE,
		"editable", TRUE,
		"wrap-mode", GTK_WRAP_WORD_CHAR,
		NULL);

	widget = gtk_scrolled_window_new (NULL, NULL);

	g_object_set (G_OBJECT (widget),
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"visible", TRUE,
		"hscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		NULL);

	gtk_container_add (GTK_CONTAINER (widget), GTK_WIDGET (html_text_view));

	gtk_grid_attach (grid, widget, 2, 2, 1, 2);

	g_object_set_data (G_OBJECT (button1), "text_view", plain_text_view);
	g_signal_connect (button1, "clicked", G_CALLBACK (editor_to_plain_text_cb), editor);

	g_object_set_data (G_OBJECT (button2), "text_view", html_text_view);
	g_signal_connect (button2, "clicked", G_CALLBACK (editor_to_html_cb), editor);

	widget = gtk_button_new_with_label (">\n>\n>");
	button1 = widget;

	g_object_set (G_OBJECT (widget),
		"halign", GTK_ALIGN_START,
		"hexpand", FALSE,
		"valign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"visible", TRUE,
		NULL);

	gtk_grid_attach (grid, widget, 1, 2, 1, 1);

	widget = gtk_button_new_with_label ("<\n<\n<");
	button2 = widget;

	g_object_set (G_OBJECT (widget),
		"halign", GTK_ALIGN_START,
		"hexpand", FALSE,
		"valign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"visible", TRUE,
		NULL);

	gtk_grid_attach (grid, widget, 1, 3, 1, 1);

	widget = gtk_label_new ("");

	g_object_set (G_OBJECT (widget),
		"halign", GTK_ALIGN_START,
		"hexpand", FALSE,
		"valign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"visible", TRUE,
		NULL);

	gtk_grid_attach (grid, widget, 0, 4, 3, 1);

	g_object_set_data (G_OBJECT (button1), "plain_text_view", plain_text_view);
	g_object_set_data (G_OBJECT (button1), "html_text_view", html_text_view);
	g_signal_connect (button1, "clicked", G_CALLBACK (plain_text_to_html_cb), widget);

	g_object_set_data (G_OBJECT (button2), "plain_text_view", plain_text_view);
	g_object_set_data (G_OBJECT (button2), "html_text_view", html_text_view);
	g_signal_connect (button2, "clicked", G_CALLBACK (html_to_plain_text_cb), widget);

	widget = gtk_check_button_new_with_label ("HTML2Text: Disallow HTML");

	g_object_set (G_OBJECT (widget),
		"halign", GTK_ALIGN_START,
		"hexpand", FALSE,
		"valign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"visible", TRUE,
		NULL);

	gtk_grid_attach (grid, widget, 0, 5, 3, 1);

	g_object_set_data (G_OBJECT (button2), "disallow_html_check", widget);

	widget = gtk_combo_box_text_new ();
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), "HTML Link to Text: None");
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), "HTML Link to Text: Inline");
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), "HTML Link to Text: Reference");
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), "HTML Link to Text: Reference without label");
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);

	g_object_set (G_OBJECT (widget),
		"halign", GTK_ALIGN_START,
		"hexpand", FALSE,
		"valign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"visible", TRUE,
		NULL);

	gtk_grid_attach (grid, widget, 1, 5, 3, 1);

	g_object_set_data (G_OBJECT (button2), "link_to_text_combo", widget);

	e_binding_bind_property (g_object_get_data (G_OBJECT (button2), "disallow_html_check"), "active",
		widget, "sensitive", G_BINDING_SYNC_CREATE);

	gtk_widget_show (window);

	return FALSE;
}

gint
main (gint argc,
      gchar **argv)
{
	GList *modules;

	bindtextdomain (GETTEXT_PACKAGE, EVOLUTION_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gtk_init (&argc, &argv);

	e_util_init_main_thread (NULL);
	e_passwords_init ();

	g_setenv ("EVOLUTION_SOURCE_WEBKITDATADIR", EVOLUTION_SOURCE_WEBKITDATADIR, FALSE);

	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (), EVOLUTION_ICONDIR);
	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (), E_DATA_SERVER_ICONDIR);

	modules = e_module_load_all_in_directory (EVOLUTION_MODULEDIR);
	g_list_free_full (modules, (GDestroyNotify) g_type_module_unuse);

	g_idle_add ((GSourceFunc) on_idle_create_widget, NULL);

	gtk_main ();

	e_misc_util_free_global_memory ();

	return 0;
}
