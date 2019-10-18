/*
 * test-html-editor.c
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

#include <gtk/gtk.h>
#include <e-util/e-util.h>

#include <glib/gi18n-lib.h>

/* Enable it, once printing is implemented (it doesn't work to do it
   on a WebKit side, because the EContentEditor can be a different
   structure. That might be why EMsgComposer uses a "print" signal,
   which prints a constructed message, like within the message preview. */
/* #define ENABLE_PRINT */

static const gchar *file_ui =
"<ui>\n"
"  <menubar name='main-menu'>\n"
"    <menu action='file-menu'>\n"
"     <menuitem action='new-editor'/>\n"
"     <separator/>\n"
"     <menuitem action='save'/>\n"
"     <menuitem action='save-as'/>\n"
#ifdef ENABLE_PRINT
"     <separator/>\n"
"     <menuitem action='print-preview'/>\n"
"     <menuitem action='print'/>\n"
#endif /* ENABLE_PRINT */
"     <separator/>\n"
"     <menuitem action='disable-editor'/>\n"
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
"     <separator/>\n"
"     <menuitem action='view-webkit-inspector'/>\n"
"    </menu>\n"
"  </menubar>\n"
"</ui>";

static void create_new_editor (void);

static void
handle_error (GError **error)
{
	if (*error != NULL) {
		g_warning ("%s", (*error)->message);
		g_clear_error (error);
	}
}

#ifdef ENABLE_PRINT
static void
print (EHTMLEditor *editor,
       GtkPrintOperationAction action)
{
	WebKitWebFrame *frame;
	GtkPrintOperation *operation;
	GtkPrintOperationResult result;
	GError *error = NULL;

	operation = gtk_print_operation_new ();

	frame = webkit_web_view_get_main_frame (
		WEBKIT_WEB_VIEW (e_html_editor_get_view (editor)));
	webkit_web_frame_print_full (frame, operation, action, NULL);

	g_object_unref (operation);
	handle_error (&error);
}
#endif

static gint
save_dialog (EHTMLEditor *editor)
{
	GtkFileChooserNative *native;
	const gchar *filename;
	gint response;

	native = gtk_file_chooser_native_new (
		_("Save As"),
		GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (editor))),
		GTK_FILE_CHOOSER_ACTION_SAVE,
		_("_Save"), _("_Cancel"));

	gtk_file_chooser_set_do_overwrite_confirmation (
		GTK_FILE_CHOOSER (native), TRUE);

	filename = e_html_editor_get_filename (editor);

	if (filename != NULL)
		gtk_file_chooser_set_filename (
			GTK_FILE_CHOOSER (native), filename);
	else
		gtk_file_chooser_set_current_name (
			GTK_FILE_CHOOSER (native), _("Untitled document"));

	response = gtk_native_dialog_run (GTK_NATIVE_DIALOG (native));

	if (response == GTK_RESPONSE_ACCEPT) {
		gchar *new_filename;

		new_filename = gtk_file_chooser_get_filename (
			GTK_FILE_CHOOSER (native));
		e_html_editor_set_filename (editor, new_filename);
		g_free (new_filename);
	}

	g_object_unref (native);

	return response;
}

static void
view_source_dialog_show (EHTMLEditor *editor,
			 const gchar *title,
			 gboolean plain_text,
			 gboolean show_source,
			 const gchar *content_text)
{
	GtkWidget *dialog;
	GtkWidget *content;
	GtkWidget *content_area;
	GtkWidget *scrolled_window;

	dialog = gtk_dialog_new_with_buttons (
		title,
		GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (editor))),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		_("_Close"), GTK_RESPONSE_CLOSE,
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

	if (show_source || plain_text) {
		GtkTextBuffer *buffer;

		content = gtk_text_view_new ();
		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (content));
		gtk_text_buffer_set_text (buffer, content_text ? content_text : "", -1);
		gtk_text_view_set_editable (GTK_TEXT_VIEW (content), FALSE);
		gtk_text_view_set_monospace (GTK_TEXT_VIEW (content), TRUE);
		if (!plain_text)
			gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (content), GTK_WRAP_WORD_CHAR);
	} else {
		content = webkit_web_view_new ();
		webkit_web_view_load_html (WEBKIT_WEB_VIEW (content), content_text ? content_text : "", "evo-file://");
	}

	gtk_container_add (GTK_CONTAINER (scrolled_window), content);
	gtk_widget_show_all (scrolled_window);

	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

typedef struct _ViewSourceData {
	EHTMLEditor *editor;
	gchar *title;
	gboolean plain_text;
	gboolean show_source;
} ViewSourceData;

static ViewSourceData *
view_source_data_new (EHTMLEditor *editor,
		      const gchar *title,
		      gboolean plain_text,
		      gboolean show_source)
{
	ViewSourceData *vsd;

	vsd = g_slice_new (ViewSourceData);
	vsd->editor = g_object_ref (editor);
	vsd->title = g_strdup (title);
	vsd->plain_text = plain_text;
	vsd->show_source = show_source;

	return vsd;
}

static void
view_source_data_free (gpointer ptr)
{
	ViewSourceData *vsd = ptr;

	if (vsd) {
		g_clear_object (&vsd->editor);
		g_free (vsd->title);
		g_slice_free (ViewSourceData, vsd);
	}
}

static void
view_source_dialog_content_hash_ready_cb (GObject *source_object,
					  GAsyncResult *result,
					  gpointer user_data)
{
	ViewSourceData *vcd = user_data;
	EContentEditorContentHash *content_hash;
	GError *error = NULL;

	g_return_if_fail (vcd != NULL);
	g_return_if_fail (E_IS_CONTENT_EDITOR (source_object));

	content_hash = e_content_editor_get_content_finish (E_CONTENT_EDITOR (source_object), result, &error);

	if (!content_hash) {
		g_warning ("%s: Failed to get content: %s", G_STRFUNC, error ? error->message : "Unknown error");
	} else {
		view_source_dialog_show (vcd->editor, vcd->title, vcd->plain_text, vcd->show_source,
			e_content_editor_util_get_content_data (content_hash,
				vcd->plain_text ? E_CONTENT_EDITOR_GET_TO_SEND_PLAIN : E_CONTENT_EDITOR_GET_TO_SEND_HTML));

		e_content_editor_util_free_content_hash (content_hash);
	}

	view_source_data_free (vcd);
	g_clear_error (&error);
}

static void
view_source_dialog (EHTMLEditor *editor,
                    const gchar *title,
                    gboolean plain_text,
                    gboolean show_source)
{
	EContentEditor *cnt_editor;
	ViewSourceData *vcd;
	guint32 flags;

	vcd = view_source_data_new (editor, title, plain_text, show_source);

	cnt_editor = e_html_editor_get_content_editor (editor);

	if (plain_text) {
		flags = E_CONTENT_EDITOR_GET_TO_SEND_PLAIN;
	} else {
		flags = E_CONTENT_EDITOR_GET_INLINE_IMAGES | E_CONTENT_EDITOR_GET_TO_SEND_HTML;
	}

	e_content_editor_get_content (cnt_editor, flags, "test-domain", NULL,
		view_source_dialog_content_hash_ready_cb, vcd);
}

static void
action_new_editor_cb (GtkAction *action,
		      EHTMLEditor *editor)
{
	create_new_editor ();
}

#ifdef ENABLE_PRINT
static void
action_print_cb (GtkAction *action,
                 EHTMLEditor *editor)
{
	print (editor, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG);
}

static void
action_print_preview_cb (GtkAction *action,
                         EHTMLEditor *editor)
{
	print (editor, GTK_PRINT_OPERATION_ACTION_PREVIEW);
}
#endif /* ENABLE_PRINT */

static void
action_quit_cb (GtkAction *action,
                EHTMLEditor *editor)
{
	gtk_main_quit ();
}

static void
html_editor_save_done_cb (GObject *source_object,
			  GAsyncResult *result,
			  gpointer user_data)
{
	GError *error = NULL;

	e_html_editor_save_finish (E_HTML_EDITOR (source_object), result, &error);

	handle_error (&error);
}

static void
action_save_cb (GtkAction *action,
                EHTMLEditor *editor)
{
	const gchar *filename;
	gboolean as_html;

	if (e_html_editor_get_filename (editor) == NULL)
		if (save_dialog (editor) == GTK_RESPONSE_CANCEL)
			return;

	filename = e_html_editor_get_filename (editor);
	as_html = (e_content_editor_get_html_mode (e_html_editor_get_content_editor (editor)));

	e_html_editor_save (editor, filename, as_html, NULL, html_editor_save_done_cb, NULL);
}

static void
action_save_as_cb (GtkAction *action,
                   EHTMLEditor *editor)
{
	const gchar *filename;
	gboolean as_html;

	if (save_dialog (editor) == GTK_RESPONSE_CANCEL)
		return;

	filename = e_html_editor_get_filename (editor);
	as_html = (e_content_editor_get_html_mode (e_html_editor_get_content_editor (editor)));

	e_html_editor_save (editor, filename, as_html, NULL, html_editor_save_done_cb, NULL);
}

static void
action_toggle_editor (GtkAction *action,
                      EHTMLEditor *editor)
{
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_set_editable (cnt_editor, !e_content_editor_is_editable (cnt_editor));
}

static void
action_view_html_output (GtkAction *action,
                         EHTMLEditor *editor)
{
	view_source_dialog (editor, _("HTML Output"), FALSE, FALSE);
}

static void
action_view_html_source (GtkAction *action,
                         EHTMLEditor *editor)
{
	view_source_dialog (editor, _("HTML Source"), FALSE, TRUE);
}

static void
action_view_plain_source (GtkAction *action,
                          EHTMLEditor *editor)
{
	view_source_dialog (editor, _("Plain Source"), TRUE, FALSE);
}

static void
action_view_inspector (GtkAction *action,
                       EHTMLEditor *editor)
{
	WebKitWebInspector *inspector;
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (editor);
	if (WEBKIT_IS_WEB_VIEW (cnt_editor)) {
		inspector = webkit_web_view_get_inspector (WEBKIT_WEB_VIEW (cnt_editor));
		webkit_web_inspector_show (inspector);
	} else {
		g_print ("Cannot show the inspector, the content editor is not a WebKitWebView descendant\n");
	}
}

static GtkActionEntry file_entries[] = {
	{ "new-editor",
	  "document-new",
	  N_("_New editor"),
	  "<Control>N",
	  NULL,
	  G_CALLBACK (action_new_editor_cb) },

#ifdef ENABLE_PRINT
	{ "print",
	  "document-print",
	  N_("_Print…"),
	  "<Control>p",
	  NULL,
	  G_CALLBACK (action_print_cb) },

	{ "print-preview",
	  "document-print-preview",
	  N_("Print Pre_view"),
	  "<Control><Shift>p",
	  NULL,
	  G_CALLBACK (action_print_preview_cb) },
#endif /* ENABLE_PRINT */

	{ "quit",
	  "application-exit",
	  N_("_Quit"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_quit_cb) },

	{ "save",
	  "document-save",
	  N_("_Save"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_save_cb) },

	{ "save-as",
	  "document-save-as",
	  N_("Save _As…"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_save_as_cb) },

	{ "disable-editor",
	  NULL,
	  N_("Disable/Enable Editor"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_toggle_editor) },

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

	{ "view-webkit-inspector",
	  NULL,
	  N_("Inspector"),
	  NULL,
	  "<Control><Shift>I",
	  G_CALLBACK (action_view_inspector) },

	{ "view-menu",
	  NULL,
	  N_("_View"),
	  NULL,
	  NULL,
	  NULL }
};

static guint glob_editors = 0;

static void
editor_destroyed_cb (GtkWidget *editor)
{
	g_return_if_fail (glob_editors > 0);

	glob_editors--;

	if (!glob_editors)
		gtk_main_quit ();
}

static void
create_new_editor_cb (GObject *source_object,
		      GAsyncResult *result,
		      gpointer user_data)
{
	GtkActionGroup *action_group;
	GtkUIManager *manager;
	GtkWidget *container;
	GtkWidget *widget;
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	EFocusTracker *focus_tracker;
	GError *error = NULL;

	widget = e_html_editor_new_finish (result, &error);
	if (error) {
		g_warning ("%s: Failed to create HTML editor: %s", G_STRFUNC, error->message);
		g_clear_error (&error);
		editor_destroyed_cb (NULL);
		return;
	}

	editor = E_HTML_EDITOR (widget);
	cnt_editor = e_html_editor_get_content_editor (editor);

	g_object_set (G_OBJECT (editor),
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		NULL);

	g_object_set (G_OBJECT (cnt_editor),
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		NULL);

	if (WEBKIT_IS_WEB_VIEW (cnt_editor)) {
		WebKitSettings *web_settings;

		web_settings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (cnt_editor));
		webkit_settings_set_allow_file_access_from_file_urls (web_settings, TRUE);
		webkit_settings_set_enable_developer_extras (web_settings, TRUE);
	}

	widget = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_widget_set_size_request (widget, 600, 400);
	gtk_widget_show (widget);

	focus_tracker = e_focus_tracker_new (GTK_WINDOW (widget));
	g_object_set_data_full (G_OBJECT (widget), "e-focus-tracker", focus_tracker, g_object_unref);

	e_focus_tracker_set_cut_clipboard_action (focus_tracker,
		e_html_editor_get_action (editor, "cut"));

	e_focus_tracker_set_copy_clipboard_action (focus_tracker,
		e_html_editor_get_action (editor, "copy"));

	e_focus_tracker_set_paste_clipboard_action (focus_tracker,
		e_html_editor_get_action (editor, "paste"));

	e_focus_tracker_set_select_all_action (focus_tracker,
		e_html_editor_get_action (editor, "select-all"));

	e_focus_tracker_set_undo_action (focus_tracker,
		e_html_editor_get_action (editor, "undo"));

	e_focus_tracker_set_redo_action (focus_tracker,
		e_html_editor_get_action (editor, "redo"));

	g_signal_connect_swapped (
		widget, "destroy",
		G_CALLBACK (editor_destroyed_cb), NULL);

	g_signal_connect (widget, "key-press-event",
		G_CALLBACK (e_util_check_gtk_bindings_in_key_press_event_cb), NULL);

	container = widget;

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	container = widget;

	widget = e_html_editor_get_managed_widget (editor, "/main-menu");
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = e_html_editor_get_managed_widget (editor, "/main-toolbar");
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = GTK_WIDGET (editor);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	gtk_widget_grab_focus (GTK_WIDGET (cnt_editor));

	manager = e_html_editor_get_ui_manager (editor);

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

	if (!WEBKIT_IS_WEB_VIEW (cnt_editor)) {
		GtkAction *action;

		action = e_html_editor_get_action (editor, "view-webkit-inspector");
		gtk_action_set_visible (action, FALSE);
	}

	gtk_ui_manager_ensure_update (manager);
}

static void
create_new_editor (void)
{
	glob_editors++;

	e_html_editor_new (create_new_editor_cb, NULL);
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

	g_setenv ("E_HTML_EDITOR_TEST_SOURCES", "1", FALSE);

	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (), EVOLUTION_ICONDIR);

	modules = e_module_load_all_in_directory (EVOLUTION_MODULEDIR);
	g_list_free_full (modules, (GDestroyNotify) g_type_module_unuse);

	create_new_editor ();

	gtk_main ();

	e_misc_util_free_global_memory ();

	return 0;
}
