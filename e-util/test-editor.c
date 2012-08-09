/*
 * e-editor-test.c
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

#include <gtk/gtk.h>
#include <e-util/e-util.h>

#include <glib/gi18n-lib.h>

static const gchar *file_ui =
"<ui>\n"
"  <menubar name='main-menu'>\n"
"    <menu action='file-menu'>\n"
"     <menuitem action='save'/>\n"
"     <menuitem action='save-as'/>\n"
"     <separator/>\n"
"     <menuitem action='print-preview'/>\n"
"     <menuitem action='print'/>\n"
"     <separator/>\n"
"     <menuitem action='quit'/>\n"
"    </menu>\n"
"  </menubar>\n"
"</ui>";

static const gchar *view_ui =
"<ui>\n"
"  <menubar name='main-menu'>\n"
"    <menu action='view-menu'>\n"
"     <menuitem action='view-html-output'/>\n"
"     <menuitem action='view-html-source'/>\n"
"     <menuitem action='view-plain-source'/>\n"
"    </menu>\n"
"  </menubar>\n"
"</ui>";

static void
handle_error (GError **error)
{
	if (*error != NULL) {
		g_warning ("%s", (*error)->message);
		g_clear_error (error);
	}
}

static GtkPrintOperationResult
print (EEditor *editor,
       GtkPrintOperationAction action)
{
	WebKitWebFrame *frame;
	GtkPrintOperation *operation;
	GtkPrintOperationResult result;
	GError *error = NULL;

	operation = gtk_print_operation_new ();

	frame = webkit_web_view_get_main_frame (
		WEBKIT_WEB_VIEW (e_editor_get_editor_widget (editor)));
	result = webkit_web_frame_print_full (frame, operation, action, NULL);

	g_object_unref (operation);
	handle_error (&error);

	return result;
}

static gint
save_dialog (EEditor *editor)
{
	GtkWidget *dialog;
	const gchar *filename;
	gint response;

	dialog = gtk_file_chooser_dialog_new (
		_("Save As"),
		GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (editor))),
		GTK_FILE_CHOOSER_ACTION_SAVE,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
		NULL);

	gtk_file_chooser_set_do_overwrite_confirmation (
		GTK_FILE_CHOOSER (dialog), TRUE);

	filename = e_editor_get_filename (editor);

	if (filename != NULL)
		gtk_file_chooser_set_filename (
			GTK_FILE_CHOOSER (dialog), filename);
	else
		gtk_file_chooser_set_current_name (
			GTK_FILE_CHOOSER (dialog), _("Untitled document"));

	response = gtk_dialog_run (GTK_DIALOG (dialog));

	if (response == GTK_RESPONSE_ACCEPT) {
		gchar *new_filename;

		new_filename = gtk_file_chooser_get_filename (
			GTK_FILE_CHOOSER (dialog));
		e_editor_set_filename (editor, new_filename);
		g_free (new_filename);
	}

	gtk_widget_destroy (dialog);

	return response;
}

static void
view_source_dialog (EEditor *editor,
                    const gchar *title,
                    const gchar *content_type,
		    gboolean show_source)
{
	GtkWidget *dialog;
	GtkWidget *content;
	GtkWidget *content_area;
	GtkWidget *scrolled_window;
	gchar* html;

	dialog = gtk_dialog_new_with_buttons (
		title,
		GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (editor))),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
		NULL);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (scrolled_window),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_IN);
	gtk_box_pack_start (
		GTK_BOX (content_area),
		scrolled_window, TRUE, TRUE, 0);

	gtk_container_set_border_width (GTK_CONTAINER (dialog), 6);
	gtk_container_set_border_width (GTK_CONTAINER (scrolled_window), 6);
	gtk_window_set_default_size (GTK_WINDOW (dialog), 400, 300);

	html = e_editor_widget_get_text_html (
		e_editor_get_editor_widget (editor));
	if (show_source) {
		GtkTextBuffer *buffer;


		content = gtk_text_view_new ();
		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (content));
		gtk_text_buffer_set_text (buffer, html, -1);
		gtk_text_view_set_editable (GTK_TEXT_VIEW (content), FALSE);
	} else {
		content = webkit_web_view_new ();
		webkit_web_view_load_html_string (
			WEBKIT_WEB_VIEW (content), html, NULL);
	}
	g_free (html);

	gtk_container_add (GTK_CONTAINER (scrolled_window), content);
	gtk_widget_show_all (scrolled_window);

	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

static void
action_print_cb (GtkAction *action,
                 EEditor *editor)
{
	print (editor, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG);
}

static void
action_print_preview_cb (GtkAction *action,
                         EEditor *editor)
{
	print (editor, GTK_PRINT_OPERATION_ACTION_PREVIEW);
}

static void
action_quit_cb (GtkAction *action,
                EEditor *editor)
{
	gtk_main_quit ();
}

static void
action_save_cb (GtkAction *action,
                EEditor *editor)
{
	const gchar *filename;
	gboolean as_html;
	GError *error = NULL;

	if (e_editor_get_filename (editor) == NULL)
		if (save_dialog (editor) == GTK_RESPONSE_CANCEL)
			return;

	filename = e_editor_get_filename (editor);
	as_html = (e_editor_widget_get_mode (
			e_editor_get_editor_widget (editor)) == E_EDITOR_WIDGET_MODE_HTML);

	e_editor_save (editor, filename, as_html, &error);
	handle_error (&error);
}

static void
action_save_as_cb (GtkAction *action,
                   EEditor *editor)
{
	const gchar *filename;
	gboolean as_html;
	GError *error = NULL;

	if (save_dialog (editor) == GTK_RESPONSE_CANCEL)
		return;

	filename = e_editor_get_filename (editor);
	as_html = (e_editor_widget_get_mode (
			e_editor_get_editor_widget (editor)) == E_EDITOR_WIDGET_MODE_HTML);

	e_editor_save (editor, filename, as_html, &error);
	handle_error (&error);
}

static void
action_view_html_output (GtkAction *action,
                         EEditor *editor)
{
	view_source_dialog (editor, _("HTML Output"), "text/html", FALSE);
}

static void
action_view_html_source (GtkAction *action,
                         EEditor *editor)
{
	view_source_dialog (editor, _("HTML Source"), "text/html", TRUE);
}

static void
action_view_plain_source (GtkAction *action,
                          EEditor *editor)
{
	view_source_dialog (editor, _("Plain Source"), "text/plain", FALSE);
}

static GtkActionEntry file_entries[] = {

	{ "print",
	  GTK_STOCK_PRINT,
	  N_("_Print..."),
	  NULL,
	  NULL,
	  G_CALLBACK (action_print_cb) },

	{ "print-preview",
	  GTK_STOCK_PRINT_PREVIEW,
	  N_("Print Pre_view"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_print_preview_cb) },

	{ "quit",
	  GTK_STOCK_QUIT,
	  N_("_Quit"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_quit_cb) },

	{ "save",
	  GTK_STOCK_SAVE,
	  N_("_Save"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_save_cb) },

	{ "save-as",
	  GTK_STOCK_SAVE_AS,
	  N_("Save _As..."),
	  NULL,
	  NULL,
	  G_CALLBACK (action_save_as_cb) },

	{ "file-menu",
	  NULL,
	  N_("_File"),
	  NULL,
	  NULL,
	  NULL }
};

static GtkActionEntry view_entries[] = {

	{ "view-html-output",
	  NULL,
	  N_("HTML _Output"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_view_html_output) },

	{ "view-html-source",
	  NULL,
	  N_("_HTML Source"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_view_html_source) },

	{ "view-plain-source",
	  NULL,
	  N_("_Plain Source"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_view_plain_source) },

	{ "view-menu",
	  NULL,
	  N_("_View"),
	  NULL,
	  NULL,
	  NULL }
};


static WebKitWebView *
open_inspector (WebKitWebInspector *inspector,
		WebKitWebView *webview,
		gpointer user_data)
{
	GtkWidget *window;
	GtkWidget *inspector_view;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	inspector_view = webkit_web_view_new ();

	gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (inspector_view));

	gtk_widget_set_size_request (window, 600, 480);
	gtk_widget_show (window);

	return WEBKIT_WEB_VIEW (inspector_view);
}

gint main (gint argc,
	   gchar **argv)
{
	GtkActionGroup *action_group;
	GtkUIManager *manager;
	GtkWidget *window, *editor;
	WebKitWebInspector *inspector;

	GError *error = NULL;

	bindtextdomain (GETTEXT_PACKAGE, EVOLUTION_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gtk_init (&argc, &argv);

        window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
        gtk_widget_set_size_request (window, 600, 400);
        g_signal_connect_swapped (window, "destroy",
                G_CALLBACK (gtk_main_quit), NULL);

	editor = e_editor_new ();
	gtk_container_add (GTK_CONTAINER (window), editor);
	gtk_widget_show (editor);

	inspector = webkit_web_view_get_inspector (
			WEBKIT_WEB_VIEW (e_editor_get_editor_widget (
				E_EDITOR (editor))));
	g_signal_connect (inspector, "inspect-web-view",
			  G_CALLBACK (open_inspector), NULL);


	manager = e_editor_get_ui_manager (E_EDITOR (editor));

	gtk_ui_manager_add_ui_from_string (manager, file_ui, -1, &error);
	handle_error (&error);

	gtk_ui_manager_add_ui_from_string (manager, view_ui, -1, &error);
	handle_error (&error);

	action_group = gtk_action_group_new ("file");
	gtk_action_group_set_translation_domain (
		action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (
		action_group, file_entries,
		G_N_ELEMENTS (file_entries), editor);
	gtk_ui_manager_insert_action_group (manager, action_group, 0);

	action_group = gtk_action_group_new ("view");
	gtk_action_group_set_translation_domain (
		action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (
		action_group, view_entries,
		G_N_ELEMENTS (view_entries), editor);
	gtk_ui_manager_insert_action_group (manager, action_group, 0);

	gtk_ui_manager_ensure_update (manager);
	gtk_widget_show (window);

	g_signal_connect (
		editor, "destroy",
		G_CALLBACK (gtk_main_quit), NULL);

	gtk_main ();

	return 0;
}
