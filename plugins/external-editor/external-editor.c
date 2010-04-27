/*
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

#include <mail/em-config.h>
#include <mail/em-composer-utils.h>
#include <mail/mail-config.h>
#include <e-util/e-alert-dialog.h>
#include <e-msg-composer.h>

#include <glib/gi18n-lib.h>
#include <glib-object.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gdk/gdkkeysyms.h>

#include <sys/stat.h>
#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif

#include <stdlib.h>
#include <string.h>

#include <gconf/gconf-client.h>

#define d(x)

#define EDITOR_GCONF_KEY_COMMAND "/apps/evolution/eplugin/external-editor/editor-command"
#define EDITOR_GCONF_KEY_IMMEDIATE "/apps/evolution/eplugin/external-editor/launch-on-key-press"

gboolean e_plugin_ui_init (GtkUIManager *manager, EMsgComposer *composer);
GtkWidget * e_plugin_lib_get_configure_widget (EPlugin *epl);
static void ee_editor_command_changed (GtkWidget *textbox);
static void ee_editor_immediate_launch_changed (GtkWidget *checkbox);
static void async_external_editor (EMsgComposer *composer);
static gboolean editor_running (void);
static gboolean key_press_cb(GtkWidget * widget, GdkEventKey * event, EMsgComposer *composer);

/* used to track when the external editor is active */
static GThread *editor_thread;

gint e_plugin_lib_enable (EPlugin *ep, gint enable);

gint
e_plugin_lib_enable (EPlugin *ep, gint enable)
{
	return 0;
}

void
ee_editor_command_changed (GtkWidget *textbox)
{
	const gchar *editor;
	GConfClient *gconf;

	editor = gtk_entry_get_text (GTK_ENTRY(textbox));
	d(printf ("\n\aeditor is : [%s] \n\a", editor));

	/* gconf access for every key-press. Sucky ? */
	gconf = gconf_client_get_default ();
	gconf_client_set_string (gconf, EDITOR_GCONF_KEY_COMMAND, editor, NULL);
	g_object_unref (gconf);
}

void
ee_editor_immediate_launch_changed (GtkWidget *checkbox)
{
	gboolean immediately;
	GConfClient *gconf;

	immediately = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbox));
	d(printf ("\n\aimmediate launch is : [%d] \n\a", immediately));

	gconf = gconf_client_get_default ();
	gconf_client_set_bool (gconf, EDITOR_GCONF_KEY_IMMEDIATE, immediately, NULL);
	g_object_unref (gconf);
}

GtkWidget *
e_plugin_lib_get_configure_widget (EPlugin *epl)
{
	GtkWidget *vbox, *textbox, *label, *help;
	GtkWidget *checkbox;
	GConfClient *gconf;
	gchar *editor;
	gboolean checked;

	vbox = gtk_vbox_new (FALSE, 10);
	textbox = gtk_entry_new ();
	label = gtk_label_new (_("Command to be executed to launch the editor: "));
	help = gtk_label_new (_("For Emacs use \"xemacs\"\nFor VI use \"gvim -f\""));
	gconf = gconf_client_get_default ();

	editor = gconf_client_get_string (gconf, EDITOR_GCONF_KEY_COMMAND, NULL);
	if (editor) {
		gtk_entry_set_text (GTK_ENTRY(textbox), editor);
		g_free (editor);
	}

	checkbox = gtk_check_button_new_with_label (
		_("Automatically launch when a new mail is edited"));
	checked = gconf_client_get_bool (gconf, EDITOR_GCONF_KEY_IMMEDIATE, NULL);
	if (checked)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbox), TRUE);
	g_object_unref (gconf);

	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), textbox, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), help, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), checkbox, FALSE, FALSE, 0);

	g_signal_connect (textbox, "changed", G_CALLBACK(ee_editor_command_changed), textbox);

	g_signal_connect (checkbox, "toggled",
			  G_CALLBACK(ee_editor_immediate_launch_changed), checkbox);

	gtk_widget_show_all (vbox);
	return vbox;
}

static void
enable_disable_composer (EMsgComposer *composer, gboolean enable)
{
	GtkhtmlEditor *editor;
	GtkAction *action;
	GtkActionGroup *action_group;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	editor = GTKHTML_EDITOR (composer);

	if (enable)
		gtkhtml_editor_run_command (editor, "editable-on");
	else
		gtkhtml_editor_run_command (editor, "editable-off");

	action = GTKHTML_EDITOR_ACTION_EDIT_MENU (composer);
	gtk_action_set_sensitive (action, enable);

	action = GTKHTML_EDITOR_ACTION_FORMAT_MENU (composer);
	gtk_action_set_sensitive (action, enable);

	action = GTKHTML_EDITOR_ACTION_INSERT_MENU (composer);
	gtk_action_set_sensitive (action, enable);

	action_group = gtkhtml_editor_get_action_group (editor, "composer");
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
	gchar *text;

	composer = g_array_index (array, gpointer, 0);
	text = g_array_index (array, gpointer, 1);

	e_msg_composer_set_body_text (composer, text, -1);

	enable_composer (composer);

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
numlines (const gchar *text, gint pos)
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

void
async_external_editor (EMsgComposer *composer)
{
	gchar *filename = NULL;
	gint status = 0;
	GConfClient *gconf;
	gchar *editor_cmd_line = NULL, *editor_cmd = NULL, *content;
	gint fd, position = -1, offset = -1;

	/* prefix temp files with evo so .*vimrc can be setup to recognize them */
	fd = g_file_open_tmp ("evoXXXXXX", &filename, NULL);
	if (fd > 0) {
		gsize length = 0;

		close (fd);
		d(printf ("\n\aTemporary-file Name is : [%s] \n\a", filename));

		/* Push the text (if there is one) from the composer to the file */
		content = gtkhtml_editor_get_text_plain (GTKHTML_EDITOR (composer), &length);
		g_file_set_contents (filename, content, length, NULL);
	} else {
		struct run_error_dialog_data *data = g_new0 (struct run_error_dialog_data, 1);

		data->composer = composer;
		data->text = "org.gnome.evolution.plugins.external-editor:no-temp-file";

		g_warning ("Temporary file fd is null");

		/* run_error_dialog also calls enable_composer */
		g_idle_add ((GSourceFunc) run_error_dialog, data);
		return;
	}

	gconf = gconf_client_get_default ();
	editor_cmd = gconf_client_get_string (gconf, EDITOR_GCONF_KEY_COMMAND, NULL);
	if (!editor_cmd) {
		if (!(editor_cmd = g_strdup (g_getenv ("EDITOR"))) )
			/* Make gedit the default external editor,
			   if the default schemas are not installed
			   and no $EDITOR is set. */
			editor_cmd = g_strdup ("gedit");
	}
	g_object_unref (gconf);

	if (g_strrstr (editor_cmd, "vim") != NULL
	    && gtk_html_get_cursor_pos (gtkhtml_editor_get_html (GTKHTML_EDITOR (composer)), &position, &offset)
	    && position >= 0 && offset >= 0) {
		gchar *tmp = editor_cmd;
		gint lineno;
		gboolean set_nofork;

		set_nofork = g_strrstr (editor_cmd, "gvim") != NULL;
		/* increment 1 so that entering vim insert mode places you in the same
		 * entry position you were at in the html  */
		offset++;

		/* calculate the line number that the cursor is in */
		lineno = numlines (content, position);

		editor_cmd = g_strdup_printf ("%s \"+call cursor(%d,%d)\"%s%s", tmp, lineno, offset, set_nofork ? " " : "", set_nofork ? "--nofork" : "");

		g_free (tmp);
	}

	g_free (content);

	editor_cmd_line = g_strconcat (editor_cmd, " ", filename, NULL);

	if (!g_spawn_command_line_sync (editor_cmd_line, NULL, NULL, &status, NULL)) {
		struct run_error_dialog_data *data = g_new0 (struct run_error_dialog_data, 1);

		g_warning ("Unable to launch %s: ", editor_cmd_line);

		data->composer = composer;
		data->text = "org.gnome.evolution.plugins.external-editor:editor-not-launchable";

		/* run_error_dialog also calls enable_composer */
		g_idle_add ((GSourceFunc) run_error_dialog, data);

		g_free (filename);
		g_free (editor_cmd_line);
		g_free (editor_cmd);
		return;
	}
	g_free (editor_cmd_line);
	g_free (editor_cmd);

#ifdef HAVE_SYS_WAIT_H
	if (WEXITSTATUS (status) != 0) {
#else
	if (status) {
#endif
		d(printf ("\n\nsome problem here with external editor\n\n"));
		g_idle_add ((GSourceFunc) enable_composer, composer);
		return;
	} else {
		gchar *buf;

		if (g_file_get_contents (filename, &buf, NULL, NULL)) {
			gchar *htmltext;
			GArray *array;

			htmltext = camel_text_to_html(buf, CAMEL_MIME_FILTER_TOHTML_PRE, 0);

			array = g_array_sized_new (TRUE, TRUE,
						   sizeof (gpointer), 2 * sizeof(gpointer));
			array = g_array_append_val (array, composer);
			array = g_array_append_val (array, htmltext);

			g_idle_add ((GSourceFunc) update_composer_text, array);

			/* We no longer need that temporary file */
			g_remove (filename);
			g_free (filename);
		}
	}
}

static void launch_editor (GtkAction *action, EMsgComposer *composer)
{
	d(printf ("\n\nexternal_editor plugin is launched \n\n"));

	if (editor_running()) {
		d(printf("not opening editor, because it's still running\n"));
		return;
	}

	disable_composer (composer);

	editor_thread = g_thread_create ((GThreadFunc)async_external_editor, composer, FALSE, NULL);
}

static GtkActionEntry entries[] = {
	{ "ExternalEditor",
	  GTK_STOCK_EDIT,
	  N_("Compose in External Editor"),
	  "<Shift><Control>e",
	  N_("Compose in External Editor"),
	  G_CALLBACK (launch_editor) }
};

static gboolean
key_press_cb(GtkWidget * widget, GdkEventKey * event, EMsgComposer *composer)
{
	GConfClient *gconf;
	gboolean immediately;

	/* we don't want to start the editor on any modifier keys */
	switch (event->keyval) {
	case GDK_Alt_L:
	case GDK_Alt_R:
	case GDK_Super_L:
	case GDK_Super_R:
	case GDK_Control_L:
	case GDK_Control_R:
		return FALSE;
	default:
		break;
	}

	gconf = gconf_client_get_default ();
	immediately = gconf_client_get_bool (gconf, EDITOR_GCONF_KEY_IMMEDIATE, NULL);
	g_object_unref (gconf);
	if (!immediately)
		return FALSE;

	launch_editor (NULL, composer);

	return TRUE;
}

static void
editor_running_thread_func (GThread *thread, gpointer running)
{
	if (thread == editor_thread)
		*(gboolean*)running = TRUE;
}

/* Racy? */
static gboolean
editor_running (void)
{
	gboolean running = FALSE;

	g_thread_foreach ((GFunc)editor_running_thread_func, &running);

	return running;
}

static gboolean
delete_cb (GtkWidget *widget, EMsgComposer *composer)
{
	if (editor_running()) {
		e_alert_run_dialog_for_args (NULL, "org.gnome.evolution.plugins.external-editor:editor-still-running", NULL);
		return TRUE;
	}

	return FALSE;
}

gboolean
e_plugin_ui_init (GtkUIManager *manager, EMsgComposer *composer)
{
	GtkhtmlEditor *editor;
	GtkHTML *html;

	editor = GTKHTML_EDITOR (composer);

	/* Add actions to the "composer" action group. */
	gtk_action_group_add_actions (
		gtkhtml_editor_get_action_group (editor, "composer"),
		entries, G_N_ELEMENTS (entries), composer);

	html = gtkhtml_editor_get_html (editor);

	g_signal_connect (G_OBJECT(html), "key_press_event",
			  G_CALLBACK(key_press_cb), composer);

	g_signal_connect (G_OBJECT(composer), "delete-event",
			  G_CALLBACK(delete_cb), composer);

	return TRUE;
}
