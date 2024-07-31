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

#include "evolution-config.h"

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

gboolean	e_plugin_ui_init		(EUIManager *manager,
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
	help = gtk_label_new (_("For XEmacs use “xemacs”\nFor Vim use “gvim -f”"));
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
	EContentEditor *cnt_editor;
	EUIActionGroup *action_group;
	EUIManager *ui_manager;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	editor = e_msg_composer_get_editor (composer);
	cnt_editor = e_html_editor_get_content_editor (editor);
	ui_manager = e_html_editor_get_ui_manager (editor);

	e_content_editor_set_editable (cnt_editor, enable);

	e_ui_action_set_sensitive (E_HTML_EDITOR_ACTION_EDIT_MENU (editor), enable);
	e_ui_action_set_sensitive (E_HTML_EDITOR_ACTION_FORMAT_MENU (editor), enable);
	e_ui_action_set_sensitive (E_HTML_EDITOR_ACTION_INSERT_MENU (editor), enable);

	g_return_if_fail (e_ui_manager_has_action_group (ui_manager, "composer"));

	action_group = e_ui_manager_get_action_group (ui_manager, "composer");
	e_ui_action_group_set_sensitive (action_group, enable);
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

static gboolean
enable_composer_idle (gpointer user_data)
{
	EMsgComposer *composer = user_data;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);

	enable_composer (composer);

	g_object_unref (composer);

	return FALSE;
}

struct ExternalEditorData {
	EMsgComposer *composer;
	gchar *content;
	GDestroyNotify content_destroy_notify;
	guint cursor_position, cursor_offset;
};

static void
external_editor_data_free (gpointer ptr)
{
	struct ExternalEditorData *eed = ptr;

	if (eed) {
		g_clear_object (&eed->composer);
		if (eed->content_destroy_notify)
			eed->content_destroy_notify (eed->content);
		g_slice_free (struct ExternalEditorData, eed);
	}
}

/* needed because the new thread needs to call g_idle_add () */
static gboolean
update_composer_text (gpointer user_data)
{
	struct ExternalEditorData *eed = user_data;
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	g_return_val_if_fail (eed != NULL, FALSE);
	g_return_val_if_fail (E_IS_MSG_COMPOSER (eed->composer), FALSE);

	editor = e_msg_composer_get_editor (eed->composer);
	cnt_editor = e_html_editor_get_content_editor (editor);

	e_msg_composer_set_body_text (eed->composer, eed->content, FALSE);

	enable_composer (eed->composer);

	e_content_editor_set_changed (cnt_editor, TRUE);

	external_editor_data_free (eed);

	return FALSE;
}

struct run_error_dialog_data
{
	EMsgComposer *composer;
	const gchar *text;
};

/* needed because the new thread needs to call g_idle_add () */
static gboolean
run_error_dialog (gpointer user_data)
{
	struct run_error_dialog_data *data = user_data;

	g_return_val_if_fail (data != NULL, FALSE);

	e_alert_run_dialog_for_args (GTK_WINDOW (data->composer), data->text, NULL);
	enable_composer (data->composer);

	g_clear_object (&data->composer);
	g_slice_free (struct run_error_dialog_data, data);

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

static gboolean external_editor_running = FALSE;
static GMutex external_editor_running_lock;

static gpointer
external_editor_thread (gpointer user_data)
{
	struct ExternalEditorData *eed = user_data;
	gchar *filename = NULL;
	gint status = 0;
	GSettings *settings;
	gchar *editor_cmd_line = NULL, *editor_cmd = NULL;
	gint fd;

	g_return_val_if_fail (eed != NULL, NULL);
	g_return_val_if_fail (E_IS_MSG_COMPOSER (eed->composer), NULL);

	/* prefix temp files with evo so .*vimrc can be setup to recognize them */
	fd = g_file_open_tmp ("evoXXXXXX", &filename, NULL);
	if (fd > 0) {
		close (fd);
		d (printf ("\n\aTemporary-file Name is : [%s] \n\a", filename));

		/* Push the text (if there is one) from the composer to the file */
		if (eed->content && *eed->content)
			g_file_set_contents (filename, eed->content, strlen (eed->content), NULL);
	} else {
		struct run_error_dialog_data *data;

		data = g_slice_new0 (struct run_error_dialog_data);
		data->composer = g_object_ref (eed->composer);
		data->text = "org.gnome.evolution.plugins.external-editor:no-temp-file";

		g_warning ("Temporary file fd is null");

		/* run_error_dialog also calls enable_composer */
		g_idle_add (run_error_dialog, data);

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
	    eed->cursor_position > 0) {
		gchar *tmp = editor_cmd;
		gint lineno;
		gboolean set_nofork;

		set_nofork = g_strrstr (editor_cmd, "gvim") != NULL;

		/* calculate the line number that the cursor is in */
		lineno = numlines (eed->content, eed->cursor_position);

		/* Increment by 1 so that entering vim insert mode places you
		 * in the same entry position you were at in the html. */
		editor_cmd = g_strdup_printf (
			"%s \"+call cursor(%d,%d)\"%s%s",
			tmp, lineno, eed->cursor_offset + 1,
			set_nofork ? " " : "",
			set_nofork ? "--nofork" : "");

		g_free (tmp);
	}

	editor_cmd_line = g_strconcat (editor_cmd, " ", filename, NULL);

	if (!g_spawn_command_line_sync (editor_cmd_line, NULL, NULL, &status, NULL)) {
		struct run_error_dialog_data *data;

		g_warning ("Unable to launch %s: ", editor_cmd_line);

		data = g_slice_new0 (struct run_error_dialog_data);
		data->composer = g_object_ref (eed->composer);
		data->text = "org.gnome.evolution.plugins.external-editor:editor-not-launchable";

		/* run_error_dialog also calls enable_composer */
		g_idle_add (run_error_dialog, data);

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
		g_idle_add (enable_composer_idle, g_object_ref (eed->composer));
		goto finished;
	} else {
		gchar *buf = NULL;

		if (g_file_get_contents (filename, &buf, NULL, NULL)) {
			struct ExternalEditorData *eed2;

			eed2 = g_slice_new0 (struct ExternalEditorData);
			eed2->composer = g_object_ref (eed->composer);
			eed2->content = camel_text_to_html (buf, CAMEL_MIME_FILTER_TOHTML_PRE, 0);
			eed2->content_destroy_notify = g_free;

			g_idle_add ((GSourceFunc) update_composer_text, eed2);

			/* We no longer need that temporary file */
			if (g_remove (filename) == -1)
				g_warning (
					"%s: Failed to remove file '%s': %s",
					G_STRFUNC, filename, g_strerror (errno));
			g_free (filename);
			g_free (buf);
		}
	}

finished:
	g_mutex_lock (&external_editor_running_lock);
	external_editor_running = FALSE;
	g_mutex_unlock (&external_editor_running_lock);

	external_editor_data_free (eed);

	return NULL;
}

static void
launch_editor_content_ready_cb (GObject *source_object,
				GAsyncResult *result,
				gpointer user_data)
{
	struct ExternalEditorData *eed = user_data;
	EContentEditor *cnt_editor;
	EContentEditorContentHash *content_hash;
	GThread *editor_thread;
	GError *error = NULL;

	g_return_if_fail (E_IS_CONTENT_EDITOR (source_object));
	g_return_if_fail (eed != NULL);

	cnt_editor = E_CONTENT_EDITOR (source_object);

	content_hash = e_content_editor_get_content_finish (cnt_editor, result, &error);

	if (!content_hash)
		g_warning ("%s: Faild to get content: %s", G_STRFUNC, error ? error->message : "Unknown error");

	eed->content = content_hash ? e_content_editor_util_steal_content_data (content_hash, E_CONTENT_EDITOR_GET_TO_SEND_PLAIN, &(eed->content_destroy_notify)) : NULL;

	editor_thread = g_thread_new (NULL, external_editor_thread, eed);
	g_thread_unref (editor_thread);

	e_content_editor_util_free_content_hash (content_hash);
	g_clear_error (&error);
}

static void
launch_editor_cb (EUIAction *action,
		  GVariant *parameter,
		  gpointer user_data)
{
	EMsgComposer *composer = user_data;
	struct ExternalEditorData *eed;
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	d (printf ("\n\nexternal_editor plugin is launched \n\n"));

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	if (editor_running ()) {
		d (printf ("not opening editor, because it's still running\n"));
		return;
	}

	editor = e_msg_composer_get_editor (composer);
	cnt_editor = e_html_editor_get_content_editor (editor);

	e_content_editor_clear_undo_redo_history (cnt_editor);
	disable_composer (composer);

	g_mutex_lock (&external_editor_running_lock);
	external_editor_running = TRUE;
	g_mutex_unlock (&external_editor_running_lock);

	eed = g_slice_new0 (struct ExternalEditorData);
	eed->composer = g_object_ref (composer);

	e_content_editor_get_content (cnt_editor, E_CONTENT_EDITOR_GET_TO_SEND_PLAIN, NULL, NULL,
		launch_editor_content_ready_cb, eed);
}

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

	launch_editor_cb (NULL, NULL, composer);

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
e_plugin_ui_init (EUIManager *manager,
                  EMsgComposer *composer)
{
	static const gchar *eui =
		"<eui>"
		  "<menu id='main-menu'>"
		    "<placeholder id='pre-edit-menu'>"
		      "<submenu action='file-menu'>"
			"<placeholder id='external-editor-holder'>"
			  "<item action='external-editor'/>"
			"</placeholder>"
		      "</submenu>"
		    "</placeholder>"
		  "</menu>"
		"</eui>";

	static const EUIActionEntry entries[] = {
		{ "external-editor",
		  NULL,
		  N_("Compose in External Editor"),
		  "<Shift><Control>e",
		  N_("Compose in External Editor"),
		  launch_editor_cb, NULL, NULL, NULL }
	};

	EHTMLEditor *editor;
	EUIManager *ui_manager;
	EContentEditor *cnt_editor;

	editor = e_msg_composer_get_editor (composer);
	cnt_editor = e_html_editor_get_content_editor (editor);
	ui_manager = e_html_editor_get_ui_manager (editor);

	e_ui_manager_add_actions_with_eui_data (ui_manager, "composer", GETTEXT_PACKAGE,
		entries, G_N_ELEMENTS (entries), composer, eui);

	g_signal_connect (
		cnt_editor, "key_press_event",
		G_CALLBACK (key_press_cb), composer);

	g_signal_connect (
		cnt_editor, "delete-event",
		G_CALLBACK (delete_cb), composer);

	return TRUE;
}
