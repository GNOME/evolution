/*
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Holger Macht <hmacht@suse.de>
 *		based on work by Sankar P <psankar@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>

#include <mail/em-config.h>
#include <mail/em-composer-utils.h>
#include <e-msg-composer.h>

#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <gdk/gdkkeysyms.h>

#include <sys/stat.h>
#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif

#include <stdlib.h>
#include <string.h>

#define d(x)

gboolean	e_plugin_ui_init		(GtkUIManager *manager,
						 EMsgComposer *composer);
GtkWidget *	e_plugin_lib_get_configure_widget
						(EPlugin *epl);
static void	ee_editor_command_changed
						(GtkWidget *textbox);
static void	ee_editor_immediate_launch_changed
						(GtkWidget *checkbox);
static gboolean	editor_running			(void);
static gboolean	key_press_cb			(GtkWidget *widget,
						 GdkEventKey *event,
						 EMsgComposer *composer);

/* used to track when the external editor is active */
static GThread *editor_thread;

gint e_plugin_lib_enable (EPlugin *ep, gint enable);

gint
e_plugin_lib_enable (EPlugin *ep,
                     gint enable)
{
	return 0;
}

void
ee_editor_command_changed (GtkWidget *textbox)
{
	const gchar *editor;
	GSettings *settings;

	editor = gtk_entry_get_text (GTK_ENTRY (textbox));
	d (printf ("\n\aeditor is : [%s] \n\a", editor));

	/* GSettings access for every key-press. Sucky ? */
	settings = e_util_ref_settings ("org.gnome.evolution.plugin.external-editor");
	g_settings_set_string (settings, "command", editor);
	g_object_unref (settings);
}

void
ee_editor_immediate_launch_changed (GtkWidget *checkbox)
{
	gboolean immediately;
	GSettings *settings;

	immediately = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbox));
	d (printf ("\n\aimmediate launch is : [%d] \n\a", immediately));

	settings = e_util_ref_settings ("org.gnome.evolution.plugin.external-editor");
	g_settings_set_boolean (settings, "launch-on-key-press", immediately);
	g_object_unref (settings);
}

GtkWidget *
e_plugin_lib_get_configure_widget (EPlugin *epl)
{
	GtkWidget *vbox, *textbox, *label, *help;
	GtkWidget *checkbox;
	GSettings *settings;
	gchar *editor;
	gboolean checked;

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
	textbox = gtk_entry_new ();
	label = gtk_label_new (_("Command to be executed to launch the editor: "));
	help = gtk_label_new (_("For XEmacs use \"xemacs\"\nFor Vim use \"gvim -f\""));
	settings = e_util_ref_settings ("org.gnome.evolution.plugin.external-editor");

	editor = g_settings_get_string (settings, "command");
	if (editor) {
		gtk_entry_set_text (GTK_ENTRY (textbox), editor);
		g_free (editor);
	}

	checkbox = gtk_check_button_new_with_mnemonic (
		_("_Automatically launch when a new mail is edited"));
	checked = g_settings_get_boolean (settings, "launch-on-key-press");
	if (checked)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbox), TRUE);
	g_object_unref (settings);

	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), textbox, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), help, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), checkbox, FALSE, FALSE, 0);

	g_signal_connect (
		textbox, "changed",
		G_CALLBACK (ee_editor_command_changed), textbox);

	g_signal_connect (
		checkbox, "toggled",
		G_CALLBACK (ee_editor_immediate_launch_changed), checkbox);

	gtk_widget_show_all (vbox);

	return vbox;
}

static void
enable_disable_composer (EMsgComposer *composer,
                         gboolean enable)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	GtkAction *action;
	GtkActionGroup *action_group;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	editor = e_msg_composer_get_editor (composer);
	view = e_html_editor_get_view (editor);

	webkit_web_view_set_editable (WEBKIT_WEB_VIEW (view), enable);

	action = E_HTML_EDITOR_ACTION_EDIT_MENU (editor);
	gtk_action_set_sensitive (action, enable);

	action = E_HTML_EDITOR_ACTION_FORMAT_MENU (editor);
	gtk_action_set_sensitive (action, enable);

	action = E_HTML_EDITOR_ACTION_INSERT_MENU (editor);
	gtk_action_set_sensitive (action, enable);

	action_group = e_html_editor_get_action_group (editor, "composer");
	gtk_action_group_set_sensitive (action_group, enable);
}

static void
enable_composer (EMsgComposer *composer)
{
	enable_disable_composer (composer, TRUE);
}

static void
disable_composer (EMsgComposer *composer)
{
	enable_disable_composer (composer, FALSE);
}

/* needed because the new thread needs to call g_idle_add () */
static gboolean
update_composer_text (GArray *array)
{
	EMsgComposer *composer;
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	gchar *text;

	composer = g_array_index (array, gpointer, 0);
	text = g_array_index (array, gpointer, 1);

	editor = e_msg_composer_get_editor (composer);
	view = e_html_editor_get_view (editor);

	e_msg_composer_set_body_text (composer, text, FALSE);

	enable_composer (composer);

	e_html_editor_view_set_changed (view, TRUE);

	g_free (text);

	return FALSE;
}

struct run_error_dialog_data
{
	EMsgComposer *composer;
	const gchar *text;
};

/* needed because the new thread needs to call g_idle_add () */
static gboolean
run_error_dialog (struct run_error_dialog_data *data)
{
	g_return_val_if_fail (data != NULL, FALSE);

	e_alert_run_dialog_for_args (GTK_WINDOW (data->composer), data->text, NULL);
	enable_composer (data->composer);

	g_free (data);

	return FALSE;
}

static gint
numlines (const gchar *text,
          gint pos)
{
	gint ctr = 0;
	gint lineno = 0;

	while (text && *text && ctr <= pos) {
		if (*text == '\n')
			lineno++;

		text++;
		ctr++;
	}

	if (lineno > 0) {
		lineno++;
	}

	return lineno;
}

static gint32
get_caret_offset (EHTMLEditorView *view)
{
	GDBusProxy *web_extension;
	gint position = 0;
	GVariant *result;

	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		return 0;

	result = g_dbus_proxy_call_sync (
		web_extension,
		"DOMGetCaretOffset",
		g_variant_new (
			"(t)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view))),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		position = g_variant_get_int32 (result);
		g_variant_unref (result);
	}

	return position;
}

static gint32
get_caret_position (EHTMLEditorView *view)
{
	GDBusProxy *web_extension;
	gint position = 0;
	GVariant *result;

	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		return 0;

	result = g_dbus_proxy_call_sync (
		web_extension,
		"DOMGetCaretPosition",
		g_variant_new (
			"(t)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view))),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		position = g_variant_get_int32 (result);
		g_variant_unref (result);
	}

	return position;
}

static void
clear_undo_redo_history (EHTMLEditorView *view)
{
	GDBusProxy *web_extension;
	GVariant *result;

	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		return;

	result = g_dbus_proxy_call_sync (
		web_extension,
		"DOMClearUndoRedoHistory",
		g_variant_new (
			"(t)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view))),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result)
		g_variant_unref (result);
}

static gboolean external_editor_running = FALSE;
static GMutex external_editor_running_lock;

static gpointer
external_editor_thread (gpointer user_data)
{
	EMsgComposer *composer = user_data;
	gchar *filename = NULL;
	gint status = 0;
	GSettings *settings;
	gchar *editor_cmd_line = NULL, *editor_cmd = NULL, *content;
	gint fd, position = -1, offset = -1;
	EHTMLEditor *editor;
	EHTMLEditorView *view;

	editor = e_msg_composer_get_editor (composer);
	view = e_html_editor_get_view (editor);

	/* prefix temp files with evo so .*vimrc can be setup to recognize them */
	fd = g_file_open_tmp ("evoXXXXXX", &filename, NULL);
	if (fd > 0) {
		close (fd);
		d (printf ("\n\aTemporary-file Name is : [%s] \n\a", filename));

		/* Push the text (if there is one) from the composer to the file */
		content = e_html_editor_view_get_text_plain (view);
		if (content && *content)
			g_file_set_contents (filename, content, strlen (content), NULL);
	} else {
		struct run_error_dialog_data *data;

		data = g_new0 (struct run_error_dialog_data, 1);
		data->composer = composer;
		data->text = "org.gnome.evolution.plugins.external-editor:no-temp-file";

		g_warning ("Temporary file fd is null");

		/* run_error_dialog also calls enable_composer */
		g_idle_add ((GSourceFunc) run_error_dialog, data);

		goto finished;
	}

	settings = e_util_ref_settings ("org.gnome.evolution.plugin.external-editor");
	editor_cmd = g_settings_get_string (settings, "command");
	if (!editor_cmd) {
		if (!(editor_cmd = g_strdup (g_getenv ("EDITOR"))) )
			/* Make gedit the default external editor,
			 * if the default schemas are not installed
			 * and no $EDITOR is set. */
			editor_cmd = g_strdup ("gedit");
	}
	g_object_unref (settings);

	if (g_strrstr (editor_cmd, "vim") != NULL &&
	    ((position = get_caret_position (view)) > 0)) {
		gchar *tmp = editor_cmd;
		gint lineno;
		gboolean set_nofork;

		set_nofork = g_strrstr (editor_cmd, "gvim") != NULL;

		offset = get_caret_offset (view);
		/* Increment by 1 so that entering vim insert mode places you
		 * in the same entry position you were at in the html. */
		offset++;

		/* calculate the line number that the cursor is in */
		lineno = numlines (content, position);

		editor_cmd = g_strdup_printf (
			"%s \"+call cursor(%d,%d)\"%s%s",
			tmp, lineno, offset,
			set_nofork ? " " : "",
			set_nofork ? "--nofork" : "");

		g_free (tmp);
	}

	g_free (content);

	editor_cmd_line = g_strconcat (editor_cmd, " ", filename, NULL);

	if (!g_spawn_command_line_sync (editor_cmd_line, NULL, NULL, &status, NULL)) {
		struct run_error_dialog_data *data;

		g_warning ("Unable to launch %s: ", editor_cmd_line);

		data = g_new0 (struct run_error_dialog_data, 1);
		data->composer = composer;
		data->text = "org.gnome.evolution.plugins.external-editor:editor-not-launchable";

		/* run_error_dialog also calls enable_composer */
		g_idle_add ((GSourceFunc) run_error_dialog, data);

		g_free (filename);
		g_free (editor_cmd_line);
		g_free (editor_cmd);
		goto finished;
	}
	g_free (editor_cmd_line);
	g_free (editor_cmd);

#ifdef HAVE_SYS_WAIT_H
	if (WEXITSTATUS (status) != 0) {
#else
	if (status) {
#endif
		d (printf ("\n\nsome problem here with external editor\n\n"));
		g_idle_add ((GSourceFunc) enable_composer, composer);
		goto finished;
	} else {
		gchar *buf;

		if (g_file_get_contents (filename, &buf, NULL, NULL)) {
			gchar *htmltext;
			GArray *array;

			htmltext = camel_text_to_html (
				buf, CAMEL_MIME_FILTER_TOHTML_PRE, 0);

			array = g_array_sized_new (
				TRUE, TRUE,
				sizeof (gpointer), 2 * sizeof (gpointer));
			array = g_array_append_val (array, composer);
			array = g_array_append_val (array, htmltext);

			g_idle_add ((GSourceFunc) update_composer_text, array);

			/* We no longer need that temporary file */
			if (g_remove (filename) == -1)
				g_warning (
					"%s: Failed to remove file '%s': %s",
					G_STRFUNC, filename, g_strerror (errno));
			g_free (filename);
		}
	}

finished:
	g_mutex_lock (&external_editor_running_lock);
	external_editor_running = FALSE;
	g_mutex_unlock (&external_editor_running_lock);

	return NULL;
}

static void launch_editor (GtkAction *action, EMsgComposer *composer)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;

	d (printf ("\n\nexternal_editor plugin is launched \n\n"));

	if (editor_running ()) {
		d (printf ("not opening editor, because it's still running\n"));
		return;
	}

	editor = e_msg_composer_get_editor (composer);
	view = e_html_editor_get_view (editor);

	clear_undo_redo_history (view);
	disable_composer (composer);

	g_mutex_lock (&external_editor_running_lock);
	external_editor_running = TRUE;
	g_mutex_unlock (&external_editor_running_lock);

	editor_thread = g_thread_new (
		NULL, external_editor_thread, composer);
	g_thread_unref (editor_thread);
}

static GtkActionEntry entries[] = {
	{ "ExternalEditor",
	  NULL,
	  N_("Compose in External Editor"),
	  "<Shift><Control>e",
	  N_("Compose in External Editor"),
	  G_CALLBACK (launch_editor) }
};

static gboolean
key_press_cb (GtkWidget *widget,
              GdkEventKey *event,
              EMsgComposer *composer)
{
	GSettings *settings;
	gboolean immediately;

	/* we don't want to start the editor on any modifier keys */
	switch (event->keyval) {
	case GDK_KEY_Alt_L:
	case GDK_KEY_Alt_R:
	case GDK_KEY_Super_L:
	case GDK_KEY_Super_R:
	case GDK_KEY_Control_L:
	case GDK_KEY_Control_R:
		return FALSE;
	default:
		break;
	}

	settings = e_util_ref_settings ("org.gnome.evolution.plugin.external-editor");
	immediately = g_settings_get_boolean (settings, "launch-on-key-press");
	g_object_unref (settings);
	if (!immediately)
		return FALSE;

	launch_editor (NULL, composer);

	return TRUE;
}

static gboolean
editor_running (void)
{
	gboolean running;

	g_mutex_lock (&external_editor_running_lock);
	running = external_editor_running;
	g_mutex_unlock (&external_editor_running_lock);

	return running;
}

static gboolean
delete_cb (GtkWidget *widget,
           EMsgComposer *composer)
{
	if (editor_running ()) {
		e_alert_run_dialog_for_args (
			NULL, "org.gnome.evolution.plugins."
			"external-editor:editor-still-running", NULL);
		return TRUE;
	}

	return FALSE;
}

gboolean
e_plugin_ui_init (GtkUIManager *manager,
                  EMsgComposer *composer)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;

	editor = e_msg_composer_get_editor (composer);
	view = e_html_editor_get_view (editor);

	/* Add actions to the "composer" action group. */
	gtk_action_group_add_actions (
		e_html_editor_get_action_group (editor, "composer"),
		entries, G_N_ELEMENTS (entries), composer);

	g_signal_connect (
		view, "key_press_event",
		G_CALLBACK (key_press_cb), composer);

	g_signal_connect (
		view, "delete-event",
		G_CALLBACK (delete_cb), composer);

	return TRUE;
}
