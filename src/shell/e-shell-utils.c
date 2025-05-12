/*
 * e-shell-utils.c
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/**
 * SECTION: e-shell-utils
 * @short_description: high-level utilities with shell integration
 * @include: shell/e-shell-utils.h
 **/

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include <libedataserver/libedataserver.h>

#include "e-shell-view.h"
#include "e-shell-window.h"
#include "e-shell-utils.h"

/**
 * e_shell_run_open_dialog:
 * @shell: an #EShell
 * @title: file chooser dialog title
 * @customize_func: optional dialog customization function
 * @customize_data: optional data to pass to @customize_func
 *
 * Runs a #GtkFileChooserNative in open mode with the given title and
 * returns the selected #GFile.  If @customize_func is provided, the
 * function is called just prior to running the dialog.  If the user
 * cancels the dialog the function will return %NULL.
 *
 * Returns: the #GFile to open, or %NULL
 **/
GFile *
e_shell_run_open_dialog (EShell *shell,
                         const gchar *title,
                         EShellOepnSaveCustomizeFunc customize_func,
                         gpointer customize_data)
{
	GtkFileChooser *file_chooser;
	GFile *chosen_file = NULL;
	GtkFileChooserNative *native;
	GtkWindow *parent;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	parent = e_shell_get_active_window (shell);

	native = gtk_file_chooser_native_new (
		title, parent,
		GTK_FILE_CHOOSER_ACTION_OPEN,
		_("_Open"), _("_Cancel"));

	file_chooser = GTK_FILE_CHOOSER (native);

	gtk_file_chooser_set_local_only (file_chooser, FALSE);

	e_util_load_file_chooser_folder (file_chooser);

	/* Allow further customizations before running the dialog. */
	if (customize_func != NULL)
		customize_func (native, customize_data);

	if (gtk_native_dialog_run (GTK_NATIVE_DIALOG (native)) == GTK_RESPONSE_ACCEPT) {
		e_util_save_file_chooser_folder (file_chooser);

		chosen_file = gtk_file_chooser_get_file (file_chooser);
	}

	g_object_unref (native);

	return chosen_file;
}

/**
 * e_shell_run_save_dialog:
 * @shell: an #EShell
 * @title: file chooser dialog title
 * @suggestion: file name suggestion, or %NULL
 * @filters: Possible filters for dialog, or %NULL
 * @customize_func: optional dialog customization function
 * @customize_data: optional data to pass to @customize_func
 *
 * Runs a #GtkFileChooserNative in save mode with the given title and
 * returns the selected #GFile.  If @customize_func is provided, the
 * function is called just prior to running the dialog.  If the user
 * cancels the dialog the function will return %NULL.
 *
 * With non-%NULL @filters will be added also file filters to the dialog.
 * The string format is "pat1:mt1;pat2:mt2:...", where 'pat' is a pattern
 * and 'mt' is a MIME type for the pattern to be used.  There can be more
 * than one MIME type, those are separated by comma.
 *
 * Returns: the #GFile to save to, or %NULL
 **/
GFile *
e_shell_run_save_dialog (EShell *shell,
                         const gchar *title,
                         const gchar *suggestion,
                         const gchar *filters,
                         EShellOepnSaveCustomizeFunc customize_func,
                         gpointer customize_data)
{
	GtkFileChooser *file_chooser;
	GFile *chosen_file = NULL;
	GtkFileChooserNative *native;
	GtkWindow *parent;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	parent = e_shell_get_active_window (shell);

	native = gtk_file_chooser_native_new (
		title, parent,
		GTK_FILE_CHOOSER_ACTION_SAVE,
		_("_Save"), _("_Cancel"));

	file_chooser = GTK_FILE_CHOOSER (native);

	gtk_file_chooser_set_local_only (file_chooser, FALSE);
	gtk_file_chooser_set_do_overwrite_confirmation (file_chooser, TRUE);

	if (suggestion != NULL) {
		gchar *current_name;

		current_name = g_strdup (suggestion);
		e_util_make_safe_filename (current_name);
		gtk_file_chooser_set_current_name (file_chooser, current_name);
		g_free (current_name);
	}

	if (filters != NULL) {
		gchar **flts = g_strsplit (filters, ";", -1);
		gint i;

		for (i = 0; flts && flts[i]; i++) {
			GtkFileFilter *filter = gtk_file_filter_new ();
			gchar *flt = flts[i];
			gchar *delim = strchr (flt, ':'), *next = NULL;

			if (delim) {
				*delim = 0;
				next = strchr (delim + 1, ',');
			}

			gtk_file_filter_add_pattern (filter, flt);
			if (g_ascii_strcasecmp (flt, "*.mbox") == 0)
				gtk_file_filter_set_name (
					filter, _("Berkeley Mailbox (mbox)"));
			else if (g_ascii_strcasecmp (flt, "*.eml") == 0)
				gtk_file_filter_set_name (
					filter, _("Mail Message (eml)"));
			else if (g_ascii_strcasecmp (flt, "*.vcf") == 0)
				gtk_file_filter_set_name (
					filter, _("vCard (.vcf)"));
			else if (g_ascii_strcasecmp (flt, "*.ics") == 0)
				gtk_file_filter_set_name (
					filter, _("iCalendar (.ics)"));
			else
				gtk_file_filter_set_name (filter, flt);

			while (delim) {
				delim++;
				if (next)
					*next = 0;

				gtk_file_filter_add_mime_type (filter, delim);

				delim = next;
				if (next)
					next = strchr (next + 1, ',');
			}

			gtk_file_chooser_add_filter (file_chooser, filter);
		}

		if (flts && flts[0]) {
			GtkFileFilter *filter = gtk_file_filter_new ();

			gtk_file_filter_add_pattern (filter, "*");
			gtk_file_filter_set_name (filter, _("All Files (*)"));
			gtk_file_chooser_add_filter (file_chooser, filter);
		}

		g_strfreev (flts);
	}

	e_util_load_file_chooser_folder (file_chooser);

	/* Allow further customizations before running the dialog. */
	if (customize_func != NULL)
		customize_func (native, customize_data);

	if (gtk_native_dialog_run (GTK_NATIVE_DIALOG (native)) == GTK_RESPONSE_ACCEPT) {
		e_util_save_file_chooser_folder (file_chooser);

		chosen_file = gtk_file_chooser_get_file (file_chooser);
	}

	g_object_unref (native);

	return chosen_file;
}

/**
 * e_shell_utils_import_uris:
 * @shell: The #EShell instance
 * @uris: %NULL-terminated list of URIs to import
 *
 * Imports given URIs to Evolution, giving user a choice what to import
 * if more than one importer can be applied, and where to import it, if
 * the importer itself is configurable.
 *
 * URIs should be either a filename or URI of form file://.
 * All others are skipped.
 *
 * Returns: the number of URIs successfully handled
 **/
guint
e_shell_utils_import_uris (EShell *shell,
                           const gchar * const *uris)
{
	GtkWindow *parent;
	GtkWidget *assistant;

	g_return_val_if_fail (shell != NULL, 0);
	g_return_val_if_fail (uris != NULL, 0);

	parent = e_shell_get_active_window (shell);
	assistant = e_import_assistant_new_simple (parent, uris);

	if (assistant) {
		g_signal_connect_after (
			assistant, "cancel",
			G_CALLBACK (gtk_widget_destroy), NULL);

		g_signal_connect_after (
			assistant, "finished",
			G_CALLBACK (gtk_widget_destroy), NULL);

		gtk_application_add_window (
			GTK_APPLICATION (shell),
			GTK_WINDOW (assistant));

		gtk_widget_show (assistant);
	} else
		g_warning ("Cannot import any of the given URIs");

	return g_strv_length ((gchar **) uris);
}

void
e_shell_utils_run_preferences (EShell *shell)
{
	GtkWidget *preferences_window;
	GtkWindow *window;

	preferences_window = e_shell_get_preferences_window (shell);
	e_preferences_window_setup (E_PREFERENCES_WINDOW (preferences_window));

	window = e_shell_get_active_window (shell);
	g_return_if_fail (GTK_IS_WINDOW (window));

	gtk_window_set_transient_for (
		GTK_WINDOW (preferences_window),
		window);
	gtk_window_set_position (
		GTK_WINDOW (preferences_window),
		GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_present (GTK_WINDOW (preferences_window));

	if (E_IS_SHELL_WINDOW (window)) {
		EShellView *shell_view;
		EShellWindow *shell_window;
		EShellBackend *shell_backend;
		EShellBackendClass *shell_backend_class;
		const gchar *view_name;

		shell_window = E_SHELL_WINDOW (window);
		view_name = e_shell_window_get_active_view (shell_window);
		shell_view = e_shell_window_get_shell_view (shell_window, view_name);

		shell_backend = e_shell_view_get_shell_backend (shell_view);
		shell_backend_class = E_SHELL_BACKEND_GET_CLASS (shell_backend);

		if (shell_backend_class->preferences_page != NULL)
			e_preferences_window_show_page (
				E_PREFERENCES_WINDOW (preferences_window),
				shell_backend_class->preferences_page);
	}
}

void
e_shell_utils_run_help_about (EShell *shell)
{
	#define EVOLUTION_COPYRIGHT \
		"Copyright \xC2\xA9 1999 - 2008 Novell, Inc. and Others\n" \
		"Copyright \xC2\xA9 Since 2008 The Evolution Team"

	/* Authors and Documenters
	 *
	 * The names below must be in UTF-8.  The breaking of escaped strings
	 * is so the hexadecimal sequences don't swallow too many characters.
	 *
	 * SO THAT MEANS, FOR 8-BIT CHARACTERS USE \xXX HEX ENCODING ONLY!
	 *
	 * Not all environments are UTF-8 and not all editors can handle it.
	 */
	static const gchar *authors[] = {
		"The Evolution Team",
		"",
		"Bharath Acharya",
		"Matthew Barnes",
		"Milan Crha",
		"Fabiano Fid\xC3\xAAncio",
		"Chenthill Palanisamy",
		"Tomas Popela",
		"Srinivasa Ragavan",
		"Dan Vr\xC3\xA1til",
		"",
		"and many past contributors",
		NULL
	};

	static const gchar *documenters[] = {
		"Andre Klapper",
		NULL
	};

	gchar *translator_credits;

	/* The translator-credits string is for translators to list
	 * per-language credits for translation, displayed in the
	 * about dialog. */
	translator_credits = _("translator-credits");
	if (strcmp (translator_credits, "translator-credits") == 0)
		translator_credits = NULL;

	gtk_show_about_dialog (
		e_shell_get_active_window (shell),
		"program-name", "Evolution",
		"version", VERSION VERSION_SUBSTRING " " VERSION_COMMENT,
		"copyright", EVOLUTION_COPYRIGHT,
		"comments", _("Groupware Suite"),
		"website", PACKAGE_URL,
		"website-label", _("Website"),
		"authors", authors,
		"documenters", documenters,
		"translator-credits", translator_credits,
		"logo-icon-name", "evolution",
		"license-type", GTK_LICENSE_GPL_2_0,
		NULL);
}

void
e_shell_utils_run_help_contents (EShell *shell)
{
#ifdef G_OS_WIN32
	gchar *online_help_url;
#endif
	GtkWindow *window;

	window = e_shell_get_active_window (shell);
#ifdef G_OS_WIN32
	/* On Windows, link to online help instead.
	 * See https://bugzilla.gnome.org/show_bug.cgi?id=576478 */

	online_help_url = g_strconcat (
		"https://infrastructure.pages.gitlab.gnome.org/help.gnome.org/evolution/",
		BASE_VERSION, NULL);
	e_show_uri (window, online_help_url);
	g_free (online_help_url);
#else
	e_display_help (window, NULL);
#endif
}

/**
 * e_shell_utils_find_alternate_alert_sink:
 * @widget: a #GtkWidget for which to do the search
 *
 * Search an alternate #EAlertSink in the widget hierarchy up-wards
 * from the @widget (skipping the @widget itself).
 *
 * Returns: (nullable) (transfer none): an alert sink, different than @widget,
 *    or %NULL, when none found
 *
 * Since: 3.24
 **/
EAlertSink *
e_shell_utils_find_alternate_alert_sink (GtkWidget *widget)
{
	g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

	while (widget = gtk_widget_get_parent (widget), widget) {
		if (E_IS_ALERT_SINK (widget))
			return E_ALERT_SINK (widget);
	}

	return NULL;
}
