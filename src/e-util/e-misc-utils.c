/*
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
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "e-misc-utils.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include <locale.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#ifdef G_OS_WIN32
#include <windows.h>
#else
#include <gio/gdesktopappinfo.h>
#endif

#include <camel/camel.h>
#include <libedataserver/libedataserver.h>

#include <webkit2/webkit2.h>

#ifdef HAVE_LDAP
#include <ldap.h>
#ifndef SUNLDAP
#include <ldap_schema.h>
#endif
#endif /* HAVE_LDAP */

#include "e-alert-dialog.h"
#include "e-alert-sink.h"
#include "e-client-cache.h"
#include "e-filter-option.h"
#include "e-mktemp.h"
#include "e-simple-async-result.h"
#include "e-spell-checker.h"
#include "e-util-private.h"
#include "e-xml-utils.h"
#include "e-supported-locales-private.h"

typedef struct _WindowData WindowData;

struct _WindowData {
	GtkWindow *window;
	GSettings *settings;
	ERestoreWindowFlags flags;
	gint premax_width;
	gint premax_height;
	guint timeout_id;
};

static void
window_data_free (WindowData *data)
{
	if (data->settings != NULL)
		g_object_unref (data->settings);

	if (data->timeout_id > 0)
		g_source_remove (data->timeout_id);

	g_slice_free (WindowData, data);
}

static gboolean
window_update_settings (gpointer user_data)
{
	WindowData *data = user_data;
	GSettings *settings = data->settings;

	if (data->flags & E_RESTORE_WINDOW_SIZE) {
		GdkWindowState state;
		GdkWindow *window;
		gboolean maximized;

		window = gtk_widget_get_window (GTK_WIDGET (data->window));
		state = gdk_window_get_state (window);
		maximized = ((state & GDK_WINDOW_STATE_MAXIMIZED) != 0);

		g_settings_set_boolean (settings, "maximized", maximized);

		if (!maximized) {
			gint width, height;

			gtk_window_get_size (data->window, &width, &height);

			g_settings_set_int (settings, "width", width);
			g_settings_set_int (settings, "height", height);
		}
	}

	if (data->flags & E_RESTORE_WINDOW_POSITION) {
		gint x, y;

		gtk_window_get_position (data->window, &x, &y);

		g_settings_set_int (settings, "x", x);
		g_settings_set_int (settings, "y", y);
	}

	data->timeout_id = 0;

	return FALSE;
}

static void
window_delayed_update_settings (WindowData *data)
{
	if (data->timeout_id > 0)
		g_source_remove (data->timeout_id);

	data->timeout_id = e_named_timeout_add_seconds (
		1, window_update_settings, data);
}

static gboolean
window_configure_event_cb (GtkWindow *window,
                           GdkEventConfigure *event,
                           WindowData *data)
{
	window_delayed_update_settings (data);

	return FALSE;
}

static gboolean
window_state_event_cb (GtkWindow *window,
                       GdkEventWindowState *event,
                       WindowData *data)
{
	gboolean window_was_unmaximized;

	if (data->timeout_id > 0) {
		g_source_remove (data->timeout_id);
		data->timeout_id = 0;
	}

	window_was_unmaximized =
		((event->changed_mask & GDK_WINDOW_STATE_MAXIMIZED) != 0) &&
		((event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) == 0);

	if (window_was_unmaximized) {
		gint width, height;

		width = data->premax_width;
		data->premax_width = 0;

		height = data->premax_height;
		data->premax_height = 0;

		/* This only applies when the window is initially restored
		 * as maximized and is then unmaximized.  GTK+ handles the
		 * unmaximized window size thereafter. */
		if (width > 0 && height > 0)
			gtk_window_resize (window, width, height);
	}

	window_delayed_update_settings (data);

	return FALSE;
}

static gboolean
window_unmap_cb (GtkWindow *window,
                 WindowData *data)
{
	if (data->timeout_id > 0) {
		g_source_remove (data->timeout_id);
		data->timeout_id = 0;
	}

	/* Reset the flags so the window position and size are not
	 * accidentally reverted to their default value at the next run. */
	data->flags = 0;

	return FALSE;
}

/**
 * e_show_uri:
 * @parent: a parent #GtkWindow or %NULL
 * @uri: the URI to show
 *
 * Launches the default application to show the given URI.  The URI must
 * be of a form understood by GIO.  If the URI cannot be shown, it presents
 * a dialog describing the error.  The dialog is set as transient to @parent
 * if @parent is non-%NULL.
 **/
void
e_show_uri (GtkWindow *parent,
            const gchar *uri)
{
	GtkWidget *dialog;
	gchar *scheme;
	GError *error = NULL;
	guint32 timestamp;
	gboolean success;

	g_return_if_fail (uri != NULL);

	timestamp = gtk_get_current_event_time ();
	scheme = g_uri_parse_scheme (uri);

	if (!scheme || !*scheme) {
		gchar *schemed_uri;

		schemed_uri = g_strconcat ("http://", uri, NULL);
		success = gtk_show_uri_on_window (parent, schemed_uri, timestamp, &error);
		g_free (schemed_uri);
	} else {
		success = gtk_show_uri_on_window (parent, uri, timestamp, &error);
	}

	g_free (scheme);

	if (success)
		return;

	dialog = gtk_message_dialog_new_with_markup (
		parent, GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
		"<big><b>%s</b></big>",
		_("Could not open the link."));

	gtk_message_dialog_format_secondary_text (
		GTK_MESSAGE_DIALOG (dialog), "%s", error ? error->message : _("Unknown error"));

	gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);
	g_clear_error (&error);
}

static gboolean
e_misc_utils_is_help_package_installed (GAppInfo **out_yelp_info)
{
	gboolean is_installed;
	gchar *path;

	/* Viewing user documentation requires the evolution help
	 * files. Look for one of the files it installs. */
	path = g_build_filename (EVOLUTION_DATADIR, "help", "C", PACKAGE, "index.page", NULL);

	is_installed = g_file_test (path, G_FILE_TEST_IS_REGULAR);

	g_free (path);

	if (is_installed) {
		GAppInfo *yelp_info = NULL;
		GList *app_infos, *link;

		app_infos = g_app_info_get_all_for_type ("x-scheme-handler/help");

		for (link = app_infos; link; link = g_list_next (link)) {
			GAppInfo *app_info = link->data;
			const gchar *executable;

			executable = g_app_info_get_executable (app_info);

			if (executable && camel_strstrcase (executable, "yelp")) {
				yelp_info = app_info;
				break;
			}
		}

		is_installed = yelp_info && g_app_info_get_commandline (yelp_info);

		if (is_installed)
			*out_yelp_info = g_object_ref (yelp_info);

		g_list_free_full (app_infos, g_object_unref);
	}

	return is_installed;
}

/**
 * e_display_help:
 * @parent: a parent #GtkWindow or %NULL
 * @link_id: help section to present or %NULL
 *
 * Opens the user documentation to the section given by @link_id, or to the
 * table of contents if @link_id is %NULL.  If the user documentation cannot
 * be opened, it presents a dialog describing the error.  The dialog is set
 * as transient to @parent if @parent is non-%NULL.
 **/
void
e_display_help (GtkWindow *parent,
                const gchar *link_id)
{
	GAppInfo *yelp_info = NULL;
	GString *uri;
	GtkWidget *dialog;
	GError *error = NULL;
	guint32 timestamp;

	if (e_misc_utils_is_help_package_installed (&yelp_info)) {
		uri = g_string_new ("help:" PACKAGE);
	} else {
		uri = g_string_new ("https://gnome.pages.gitlab.gnome.org/evolution/help");
	}

	timestamp = gtk_get_current_event_time ();

	if (link_id != NULL) {
		g_string_append_c (uri, '/');
		g_string_append (uri, link_id);
	}

	if (yelp_info) {
		GAppLaunchContext *context = NULL;
		GList *uris;
		gboolean success;

		uris = g_list_prepend (NULL, uri->str);

		if (parent) {
			GdkAppLaunchContext *gdk_context;

			gdk_context = gdk_display_get_app_launch_context (gtk_widget_get_display (GTK_WIDGET (parent)));

			if (gdk_context)
				context = G_APP_LAUNCH_CONTEXT (gdk_context);
		}

		success = g_app_info_launch_uris (yelp_info, uris, context, &error);

		g_list_free (uris);
		g_clear_object (&context);

		if (success)
			goto exit;
	} else if (gtk_show_uri_on_window (parent, uri->str, timestamp, &error)) {
		goto exit;
	}

	dialog = gtk_message_dialog_new_with_markup (
		parent, GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
		"<big><b>%s</b></big>",
		_("Could not display help for Evolution."));

	gtk_message_dialog_format_secondary_text (
		GTK_MESSAGE_DIALOG (dialog), "%s", error->message);

	gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);
	g_error_free (error);

exit:
	g_string_free (uri, TRUE);
	g_clear_object (&yelp_info);
}

/**
 * e_restore_window:
 * @window: a #GtkWindow
 * @settings_path: a #GSettings path
 * @flags: flags indicating which window features to restore
 *
 * This function can restore one of or both a window's size and position
 * using #GSettings keys at @settings_path which conform to the relocatable
 * schema "org.gnome.evolution.window".
 *
 * If #E_RESTORE_WINDOW_SIZE is present in @flags, restore @window's
 * previously recorded size and maximize state.
 *
 * If #E_RESTORE_WINDOW_POSITION is present in @flags, move @window to
 * the previously recorded screen coordinates.
 *
 * The respective #GSettings values will be updated when the window is
 * resized and/or moved.
 **/
void
e_restore_window (GtkWindow *window,
                  const gchar *settings_path,
                  ERestoreWindowFlags flags)
{
	WindowData *data;
	GSettings *settings;
	const gchar *schema;

	g_return_if_fail (GTK_IS_WINDOW (window));
	g_return_if_fail (settings_path != NULL);

	schema = "org.gnome.evolution.window";
	settings = g_settings_new_with_path (schema, settings_path);

	data = g_slice_new0 (WindowData);
	data->window = window;
	data->settings = g_object_ref (settings);
	data->flags = flags;

	if (flags & E_RESTORE_WINDOW_SIZE) {
		GdkDisplay *display;
		GdkMonitor *monitor;
		GdkRectangle monitor_area;
		gint x, y, width, height;

		x = g_settings_get_int (settings, "x");
		y = g_settings_get_int (settings, "y");

		display = gtk_widget_get_display (GTK_WIDGET (window));
		monitor = gdk_display_get_monitor_at_point (display, x, y);

		gdk_monitor_get_workarea (monitor, &monitor_area);

		width = g_settings_get_int (settings, "width");
		height = g_settings_get_int (settings, "height");

		/* Clamp the GSettings value to actual monitor area before restoring the size */
		if (width > 0 && height > 0) {
			if (width > 1.5 * monitor_area.width)
				width = 1.5 * monitor_area.width;

			if (height > 1.5 * monitor_area.height)
				height = 1.5 * monitor_area.height;
		}

		if (width > 0 && height > 0)
			gtk_window_resize (window, width, height);

		if (g_settings_get_boolean (settings, "maximized")) {
			gtk_window_get_size (window, &width, &height);

			data->premax_width = width;
			data->premax_height = height;

			gtk_window_resize (
				window,
				monitor_area.width,
				monitor_area.height);

			gtk_window_maximize (window);
		}
	}

	if (flags & E_RESTORE_WINDOW_POSITION) {
		gint x, y;

		x = g_settings_get_int (settings, "x");
		y = g_settings_get_int (settings, "y");

		gtk_window_move (window, x, y);
	}

	g_object_set_data_full (
		G_OBJECT (window),
		"e-util-window-data", data,
		(GDestroyNotify) window_data_free);

	g_signal_connect (
		window, "configure-event",
		G_CALLBACK (window_configure_event_cb), data);

	g_signal_connect (
		window, "window-state-event",
		G_CALLBACK (window_state_event_cb), data);

	g_signal_connect (
		window, "unmap",
		G_CALLBACK (window_unmap_cb), data);

	g_object_unref (settings);
}

/**
 * e_builder_get_widget:
 * @builder: a #GtkBuilder
 * @widget_name: name of a widget in @builder
 *
 * Gets the widget named @widget_name.  Note that this function does not
 * increment the reference count of the returned widget.  If @widget_name
 * could not be found in the @builder<!-- -->'s object tree, a run-time
 * warning is emitted since this usually indicates a programming error.
 *
 * This is a convenience function to work around the awkwardness of
 * #GtkBuilder returning #GObject pointers, when the vast majority of
 * the time you want a #GtkWidget pointer.
 *
 * If you need something from @builder other than a #GtkWidget, or you
 * want to test for the existence of some widget name without incurring
 * a run-time warning, use gtk_builder_get_object().
 *
 * Returns: the widget named @widget_name, or %NULL
 **/
GtkWidget *
e_builder_get_widget (GtkBuilder *builder,
                      const gchar *widget_name)
{
	GObject *object;

	g_return_val_if_fail (GTK_IS_BUILDER (builder), NULL);
	g_return_val_if_fail (widget_name != NULL, NULL);

	object = gtk_builder_get_object (builder, widget_name);
	if (object == NULL) {
		g_warning ("Could not find widget '%s'", widget_name);
		return NULL;
	}

	return GTK_WIDGET (object);
}

/**
 * e_load_ui_builder_definition:
 * @builder: a #GtkBuilder
 * @basename: basename of the UI definition file
 *
 * Loads a UI definition into @builder from Evolution's UI directory.
 * Failure here is fatal, since the application can't function without
 * its UI definitions.
 **/
void
e_load_ui_builder_definition (GtkBuilder *builder,
                              const gchar *basename)
{
	gchar *filename;
	GError *error = NULL;

	g_return_if_fail (GTK_IS_BUILDER (builder));
	g_return_if_fail (basename != NULL);

	filename = g_build_filename (EVOLUTION_UIDIR, basename, NULL);
	gtk_builder_add_from_file (builder, filename, &error);
	g_free (filename);

	if (error != NULL) {
		g_error ("%s: %s", basename, error->message);
		g_warn_if_reached ();
	}
}

/* Helper for e_categories_add_change_hook() */
static void
categories_changed_cb (GObject *useless_opaque_object,
                       GHookList *hook_list)
{
	/* e_categories_register_change_listener() is broken because
	 * it requires callbacks to allow for some opaque GObject as
	 * the first argument (not does it document this). */
	g_hook_list_invoke (hook_list, FALSE);
}

/* Helper for e_categories_add_change_hook() */
static void
categories_weak_notify_cb (GHookList *hook_list,
                           gpointer where_the_object_was)
{
	GHook *hook;

	/* This should not happen, but if we fail to find the hook for
	 * some reason, g_hook_destroy_link() will warn about the NULL
	 * pointer, which is all we would do anyway so no need to test
	 * for it ourselves. */
	hook = g_hook_find_data (hook_list, TRUE, where_the_object_was);
	g_hook_destroy_link (hook_list, hook);
}

/**
 * e_categories_add_change_hook:
 * @func: a hook function
 * @object: a #GObject to be passed to @func, or %NULL
 *
 * A saner alternative to e_categories_register_change_listener().
 *
 * Adds a hook function to be called when a category is added, removed or
 * modified.  If @object is not %NULL, the hook function is automatically
 * removed when @object is finalized.
 **/
void
e_categories_add_change_hook (GHookFunc func,
                              gpointer object)
{
	static gboolean initialized = FALSE;
	static GHookList hook_list;
	GHook *hook;

	g_return_if_fail (func != NULL);

	if (object != NULL)
		g_return_if_fail (G_IS_OBJECT (object));

	if (!initialized) {
		g_hook_list_init (&hook_list, sizeof (GHook));
		e_categories_register_change_listener (
			G_CALLBACK (categories_changed_cb), &hook_list);
		initialized = TRUE;
	}

	hook = g_hook_alloc (&hook_list);

	hook->func = func;
	hook->data = object;

	if (object != NULL)
		g_object_weak_ref (
			G_OBJECT (object), (GWeakNotify)
			categories_weak_notify_cb, &hook_list);

	g_hook_append (&hook_list, hook);
}

/**
 * e_flexible_strtod:
 * @nptr:    the string to convert to a numeric value.
 * @endptr:  if non-NULL, it returns the character after
 *           the last character used in the conversion.
 *
 * Converts a string to a gdouble value.  This function detects
 * strings either in the standard C locale or in the current locale.
 *
 * This function is typically used when reading configuration files or
 * other non-user input that should not be locale dependent, but may
 * have been in the past.  To handle input from the user you should
 * normally use the locale-sensitive system strtod function.
 *
 * To convert from a double to a string in a locale-insensitive way, use
 * @g_ascii_dtostr.
 *
 * Returns: the gdouble value
 **/
gdouble
e_flexible_strtod (const gchar *nptr,
                   gchar **endptr)
{
	gchar *fail_pos;
	gdouble val;
	struct lconv *locale_data;
	const gchar *decimal_point;
	gint decimal_point_len;
	const gchar *p, *decimal_point_pos;
	const gchar *end = NULL; /* Silence gcc */
	gchar *copy, *c;

	g_return_val_if_fail (nptr != NULL, 0);

	fail_pos = NULL;

	locale_data = localeconv ();
	decimal_point = locale_data->decimal_point;
	decimal_point_len = strlen (decimal_point);

	g_return_val_if_fail (decimal_point_len != 0, 0);

	decimal_point_pos = NULL;
	if (!strcmp (decimal_point, "."))
		return strtod (nptr, endptr);

	p = nptr;

	/* Skip leading space */
	while (isspace ((guchar) * p))
		p++;

	/* Skip leading optional sign */
	if (*p == '+' || *p == '-')
		p++;

	if (p[0] == '0' &&
	    (p[1] == 'x' || p[1] == 'X')) {
		p += 2;
		/* HEX - find the (optional) decimal point */

		while (isxdigit ((guchar) * p))
			p++;

		if (*p == '.') {
			decimal_point_pos = p++;

			while (isxdigit ((guchar) * p))
				p++;

			if (*p == 'p' || *p == 'P')
				p++;
			if (*p == '+' || *p == '-')
				p++;
			while (isdigit ((guchar) * p))
				p++;
			end = p;
		} else if (strncmp (p, decimal_point, decimal_point_len) == 0) {
			return strtod (nptr, endptr);
		}
	} else {
		while (isdigit ((guchar) * p))
			p++;

		if (*p == '.') {
			decimal_point_pos = p++;

			while (isdigit ((guchar) * p))
				p++;

			if (*p == 'e' || *p == 'E')
				p++;
			if (*p == '+' || *p == '-')
				p++;
			while (isdigit ((guchar) * p))
				p++;
			end = p;
		} else if (strncmp (p, decimal_point, decimal_point_len) == 0) {
			return strtod (nptr, endptr);
		}
	}
	/* For the other cases, we need not convert the decimal point */

	if (!decimal_point_pos)
		return strtod (nptr, endptr);

	/* We need to convert the '.' to the locale specific decimal point */
	copy = g_malloc (end - nptr + 1 + decimal_point_len);

	c = copy;
	memcpy (c, nptr, decimal_point_pos - nptr);
	c += decimal_point_pos - nptr;
	memcpy (c, decimal_point, decimal_point_len);
	c += decimal_point_len;
	memcpy (c, decimal_point_pos + 1, end - (decimal_point_pos + 1));
	c += end - (decimal_point_pos + 1);
	*c = 0;

	val = strtod (copy, &fail_pos);

	if (fail_pos) {
		if (fail_pos > decimal_point_pos)
			fail_pos =
				(gchar *) nptr + (fail_pos - copy) -
				(decimal_point_len - 1);
		else
			fail_pos = (gchar *) nptr + (fail_pos - copy);
	}

	g_free (copy);

	if (endptr)
		*endptr = fail_pos;

	return val;
}

/**
 * e_ascii_dtostr:
 * @buffer: A buffer to place the resulting string in
 * @buf_len: The length of the buffer.
 * @format: The printf-style format to use for the
 *          code to use for converting.
 * @d: The double to convert
 *
 * Converts a double to a string, using the '.' as
 * decimal_point. To format the number you pass in
 * a printf-style formating string. Allowed conversion
 * specifiers are eEfFgG.
 *
 * If you want to generates enough precision that converting
 * the string back using @g_strtod gives the same machine-number
 * (on machines with IEEE compatible 64bit doubles) use the format
 * string "%.17g". If you do this it is guaranteed that the size
 * of the resulting string will never be larger than
 * @G_ASCII_DTOSTR_BUF_SIZE bytes.
 *
 * Returns: the pointer to the buffer with the converted string
 **/
gchar *
e_ascii_dtostr (gchar *buffer,
                gint buf_len,
                const gchar *format,
                gdouble d)
{
	struct lconv *locale_data;
	const gchar *decimal_point;
	gint decimal_point_len;
	gchar *p;
	gint rest_len;
	gchar format_char;

	g_return_val_if_fail (buffer != NULL, NULL);
	g_return_val_if_fail (format[0] == '%', NULL);
	g_return_val_if_fail (strpbrk (format + 1, "'l%") == NULL, NULL);

	format_char = format[strlen (format) - 1];

	g_return_val_if_fail (format_char == 'e' || format_char == 'E' ||
			      format_char == 'f' || format_char == 'F' ||
			      format_char == 'g' || format_char == 'G',
			      NULL);

	if (format[0] != '%')
		return NULL;

	if (strpbrk (format + 1, "'l%"))
		return NULL;

	if (!(format_char == 'e' || format_char == 'E' ||
	      format_char == 'f' || format_char == 'F' ||
	      format_char == 'g' || format_char == 'G'))
		return NULL;

	g_snprintf (buffer, buf_len, format, d);

	locale_data = localeconv ();
	decimal_point = locale_data->decimal_point;
	decimal_point_len = strlen (decimal_point);

	g_return_val_if_fail (decimal_point_len != 0, NULL);

	if (strcmp (decimal_point, ".")) {
		p = buffer;

		if (*p == '+' || *p == '-')
			p++;

		while (isdigit ((guchar) * p))
			p++;

		if (strncmp (p, decimal_point, decimal_point_len) == 0) {
			*p = '.';
			p++;
			if (decimal_point_len > 1) {
				rest_len = strlen (p + (decimal_point_len - 1));
				memmove (
					p, p + (decimal_point_len - 1),
					rest_len);
				p[rest_len] = 0;
			}
		}
	}

	return buffer;
}

/**
 * e_str_without_underscores:
 * @string: the string to strip underscores from
 *
 * Strips underscores from a string in the same way
 * @gtk_label_new_with_mnemonics does.  The returned string should be freed
 * using g_free().
 *
 * Returns: a newly-allocated string without underscores
 */
gchar *
e_str_without_underscores (const gchar *string)
{
	gchar *new_string;
	const gchar *sp;
	gchar *dp;

	new_string = g_malloc (strlen (string) + 1);

	dp = new_string;
	for (sp = string; *sp != '\0'; sp++) {
		if (*sp != '_') {
			*dp = *sp;
			dp++;
		} else if (sp[1] == '_') {
			/* Translate "__" in "_".  */
			*dp = '_';
			dp++;
			sp++;
		}
	}
	*dp = 0;

	return new_string;
}

/**
 * e_str_replace_string
 * @text: the string to replace
 * @before: the string to be replaced
 * @after: the string to replaced with
 *
 * Replaces every occurrence of the string @before with the string @after in
 * the string @text and returns a #GString with result that should be freed
 * with g_string_free().
 *
 * Returns: a newly-allocated #GString
 */
GString *
e_str_replace_string (const gchar *text,
                      const gchar *before,
                      const gchar *after)
{
	const gchar *p, *next;
	GString *str;
	gint find_len;

	g_return_val_if_fail (text != NULL, NULL);
	g_return_val_if_fail (before != NULL, NULL);
	g_return_val_if_fail (*before, NULL);

	find_len = strlen (before);
	str = g_string_new ("");

	p = text;
	while (next = strstr (p, before), next) {
		if (p < next)
			g_string_append_len (str, p, next - p);

		if (after && *after)
			g_string_append (str, after);

		p = next + find_len;
	}

	return g_string_append (str, p);
}

/**
 * e_str_is_empty:
 * @value: A string.
 *
 * Returns whether a string is %NULL, the empty string, or completely made up of
 * whitespace characters.
 *
 * Returns: %TRUE if the string is empty, %FALSE otherwise.
 *
 * Since: 3.56
 **/
gboolean
e_str_is_empty (const gchar *value)
{
	const gchar *p;
	gboolean empty;

	empty = TRUE;

	if (value) {
		p = value;
		while (*p) {
			if (!isspace ((guchar) *p)) {
				empty = FALSE;
				break;
			}
			p++;
		}
	}

	return empty;
}

gint
e_str_compare (gconstpointer x,
               gconstpointer y)
{
	if (x == NULL || y == NULL) {
		if (x == y)
			return 0;
		else
			return x ? -1 : 1;
	}

	return strcmp (x, y);
}

gint
e_str_case_compare (gconstpointer x,
                    gconstpointer y)
{
	gchar *cx, *cy;
	gint res;

	if (x == NULL || y == NULL) {
		if (x == y)
			return 0;
		else
			return x ? -1 : 1;
	}

	cx = g_utf8_casefold (x, -1);
	cy = g_utf8_casefold (y, -1);

	res = g_utf8_collate (cx, cy);

	g_free (cx);
	g_free (cy);

	return res;
}

gint
e_collate_compare (gconstpointer x,
                   gconstpointer y)
{
	if (x == NULL || y == NULL) {
		if (x == y)
			return 0;
		else
			return x ? -1 : 1;
	}

	return g_utf8_collate (x, y);
}

gint
e_int_compare (gconstpointer x,
               gconstpointer y)
{
	gint nx = GPOINTER_TO_INT (x);
	gint ny = GPOINTER_TO_INT (y);

	return (nx == ny) ? 0 : (nx < ny) ? -1 : 1;
}

/**
 * e_rgba_to_value:
 * @rgba: a #GdkRGBA
 *
 * Converts #GdkRGBA to a 24-bit RGB color value
 *
 * Returns: a 24-bit color value
 **/
guint32
e_rgba_to_value (const GdkRGBA *rgba)
{
	guint16 red;
	guint16 green;
	guint16 blue;

	g_return_val_if_fail (rgba != NULL, 0);

	red = 255 * rgba->red;
	green = 255 * rgba->green;
	blue = 255 * rgba->blue;

	return (guint32)
		((((red & 0xFFu) << 16) |
		((green & 0xFFu) << 8) |
		(blue & 0xFFu)) & 0xffffffu);
}

/**
 * e_utils_get_theme_color:
 * @widget: a #GtkWidget instance
 * @color_names: comma-separated theme color names
 * @fallback_color_ident: fallback color identificator, in a format for gdk_rgba_parse()
 * @rgba: where to store the read color
 *
 * Reads named theme color from a #GtkStyleContext of @widget.
 * The @color_names are read one after another from left to right,
 * the next are meant as fallbacks, in case the theme doesn't
 * define the previous color. If none is found then the @fallback_color_ident
 * is set to @rgba.
 **/
void
e_utils_get_theme_color (GtkWidget *widget,
			 const gchar *color_names,
			 const gchar *fallback_color_ident,
			 GdkRGBA *rgba)
{
	GtkStyleContext *style_context;
	gchar **names;
	gint ii;

	g_return_if_fail (GTK_IS_WIDGET (widget));
	g_return_if_fail (color_names != NULL);
	g_return_if_fail (fallback_color_ident != NULL);
	g_return_if_fail (rgba != NULL);

	style_context = gtk_widget_get_style_context (widget);

	names = g_strsplit (color_names, ",", -1);
	for (ii = 0; names && names[ii]; ii++) {
		if (gtk_style_context_lookup_color (style_context, names[ii], rgba)) {
			g_strfreev (names);
			return;
		}
	}

	g_strfreev (names);

	g_warn_if_fail (gdk_rgba_parse (rgba, fallback_color_ident));
}

/* This is copied from gtk+ sources */
static void
rgb_to_hls (gdouble *r,
	    gdouble *g,
	    gdouble *b)
{
	gdouble min;
	gdouble max;
	gdouble red;
	gdouble green;
	gdouble blue;
	gdouble h, l, s;
	gdouble delta;

	red = *r;
	green = *g;
	blue = *b;

	if (red > green) {
		if (red > blue)
			max = red;
		else
			max = blue;
      
		if (green < blue)
			min = green;
		else
			min = blue;
	} else {
		if (green > blue)
			max = green;
		else
			max = blue;
      
		if (red < blue)
			min = red;
		else
			min = blue;
	}

	l = (max + min) / 2;
	s = 0;
	h = 0;

	if (max != min) {
		if (l <= 0.5)
			s = (max - min) / (max + min);
		else
			s = (max - min) / (2 - max - min);
      
		delta = max -min;
		if (red == max)
			h = (green - blue) / delta;
		else if (green == max)
			h = 2 + (blue - red) / delta;
		else if (blue == max)
			h = 4 + (red - green) / delta;

		h *= 60;
		if (h < 0.0)
			h += 360;
	}

	*r = h;
	*g = l;
	*b = s;
}

/* This is copied from gtk+ sources */
static void
hls_to_rgb (gdouble *h,
	    gdouble *l,
	    gdouble *s)
{
	gdouble hue;
	gdouble lightness;
	gdouble saturation;
	gdouble m1, m2;
	gdouble r, g, b;

	lightness = *l;
	saturation = *s;

	if (lightness <= 0.5)
		m2 = lightness * (1 + saturation);
	else
		m2 = lightness + saturation - lightness * saturation;
	m1 = 2 * lightness - m2;
  
	if (saturation == 0) {
		*h = lightness;
		*l = lightness;
		*s = lightness;
	} else {
		hue = *h + 120;
		while (hue > 360)
			hue -= 360;
		while (hue < 0)
			hue += 360;
      
		if (hue < 60)
			r = m1 + (m2 - m1) * hue / 60;
		else if (hue < 180)
			r = m2;
		else if (hue < 240)
			r = m1 + (m2 - m1) * (240 - hue) / 60;
		else
			r = m1;

		hue = *h;
		while (hue > 360)
			hue -= 360;
		while (hue < 0)
			hue += 360;

		if (hue < 60)
			g = m1 + (m2 - m1) * hue / 60;
		else if (hue < 180)
			g = m2;
		else if (hue < 240)
			g = m1 + (m2 - m1) * (240 - hue) / 60;
		else
			g = m1;

		hue = *h - 120;
		while (hue > 360)
			hue -= 360;
		while (hue < 0)
			hue += 360;

		if (hue < 60)
			b = m1 + (m2 - m1) * hue / 60;
		else if (hue < 180)
			b = m2;
		else if (hue < 240)
			b = m1 + (m2 - m1) * (240 - hue) / 60;
		else
			b = m1;

		*h = r;
		*l = g;
		*s = b;
	}
}

/* This is copied from gtk+ sources */
void
e_utils_shade_color (const GdkRGBA *a,
		     GdkRGBA *b,
		     gdouble mult)
{
	gdouble red;
	gdouble green;
	gdouble blue;

	g_return_if_fail (a != NULL);
	g_return_if_fail (b != NULL);

	red = a->red;
	green = a->green;
	blue = a->blue;

	rgb_to_hls (&red, &green, &blue);

	green *= mult;
	if (green > 1.0)
		green = 1.0;
	else if (green < 0.0)
		green = 0.0;

	blue *= mult;
	if (blue > 1.0)
		blue = 1.0;
	else if (blue < 0.0)
		blue = 0.0;

	hls_to_rgb (&red, &green, &blue);

	b->red = red;
	b->green = green;
	b->blue = blue;
	b->alpha = a->alpha;
}

gdouble
e_utils_get_color_brightness (const GdkRGBA *rgba)
{
	gdouble brightness;

	g_return_val_if_fail (rgba != NULL, 0.0);

	brightness =
		(0.2109 * 255.0 * rgba->red) +
		(0.5870 * 255.0 * rgba->green) +
		(0.1021 * 255.0 * rgba->blue);

	return brightness;
}

GdkRGBA
e_utils_get_text_color_for_background (const GdkRGBA *bg_rgba)
{
	GdkRGBA text_rgba = { 0.0, 0.0, 0.0, 1.0 };
	gdouble brightness;

	g_return_val_if_fail (bg_rgba != NULL, text_rgba);

	brightness = e_utils_get_color_brightness (bg_rgba);

	if (brightness <= 140.0) {
		text_rgba.red = 1.0;
		text_rgba.green = 1.0;
		text_rgba.blue = 1.0;
	} else {
		text_rgba.red = 0.0;
		text_rgba.green = 0.0;
		text_rgba.blue = 0.0;
	}

	text_rgba.alpha = 1.0;

	return text_rgba;
}

static gint
epow10 (gint number)
{
	gint value = 1;

	while (number-- > 0)
		value *= 10;

	return value;
}

gchar *
e_format_number (gint number)
{
	GList *iterator, *list = NULL;
	struct lconv *locality;
	gint char_length = 0;
	gint group_count = 0;
	gchar *grouping;
	gint last_count = 3;
	gint divider;
	gchar *value;
	gchar *value_iterator;

	locality = localeconv ();
	grouping = locality->grouping;
	while (number) {
		gchar *group;
		switch (*grouping) {
		default:
			last_count = *grouping;
			grouping++;
			/* coverity[fallthrough] */
			/* falls through */
		case 0:
			divider = epow10 (last_count);
			if (number >= divider) {
				group = g_strdup_printf (
					"%0*d", last_count, number % divider);
			} else {
				group = g_strdup_printf (
					"%d", number % divider);
			}
			number /= divider;
			break;
		case CHAR_MAX:
			group = g_strdup_printf ("%d", number);
			number = 0;
			break;
		}
		char_length += strlen (group);
		list = g_list_prepend (list, group);
		group_count++;
	}

	if (list) {
		value = g_new (
			gchar, 1 + char_length + (group_count - 1) *
			strlen (locality->thousands_sep));

		iterator = list;
		value_iterator = value;

		strcpy (value_iterator, iterator->data);
		value_iterator += strlen (iterator->data);
		for (iterator = iterator->next; iterator; iterator = iterator->next) {
			strcpy (value_iterator, locality->thousands_sep);
			value_iterator += strlen (locality->thousands_sep);

			strcpy (value_iterator, iterator->data);
			value_iterator += strlen (iterator->data);
		}
		g_list_foreach (list, (GFunc) g_free, NULL);
		g_list_free (list);
		return value;
	} else {
		return g_strdup ("0");
	}
}

/* Perform a binary search for key in base which has nmemb elements
 * of size bytes each.  The comparisons are done by (*compare)().  */
void
e_bsearch (gconstpointer key,
           gconstpointer base,
           gsize nmemb,
           gsize size,
           ESortCompareFunc compare,
           gpointer closure,
           gsize *start,
           gsize *end)
{
	gsize l, u, idx;
	gconstpointer p;
	gint comparison;
	if (!(start || end))
		return;

	l = 0;
	u = nmemb;
	while (l < u) {
		idx = (l + u) / 2;
		p = (((const gchar *) base) + (idx * size));
		comparison = (*compare) (key, p, closure);
		if (comparison < 0)
			u = idx;
		else if (comparison > 0)
			l = idx + 1;
		else {
			gsize lsave, usave;
			lsave = l;
			usave = u;
			if (start) {
				while (l < u) {
					idx = (l + u) / 2;
					p = (((const gchar *) base) + (idx * size));
					comparison = (*compare) (key, p, closure);
					if (comparison <= 0)
						u = idx;
					else
						l = idx + 1;
				}
				*start = l;

				l = lsave;
				u = usave;
			}
			if (end) {
				while (l < u) {
					idx = (l + u) / 2;
					p = (((const gchar *) base) + (idx * size));
					comparison = (*compare) (key, p, closure);
					if (comparison < 0)
						u = idx;
					else
						l = idx + 1;
				}
				*end = l;
			}
			return;
		}
	}

	if (start)
		*start = l;
	if (end)
		*end = l;
}

/* Function to do a last minute fixup of the AM/PM stuff if the locale
 * and gettext haven't done it right. Most English speaking countries
 * except the USA use the 24 hour clock (UK, Australia etc). However
 * since they are English nobody bothers to write a language
 * translation (gettext) file. So the locale turns off the AM/PM, but
 * gettext does not turn on the 24 hour clock. Leaving a mess.
 *
 * This routine checks if AM/PM are defined in the locale, if not it
 * forces the use of the 24 hour clock.
 *
 * The function itself is a front end on strftime and takes exactly
 * the same arguments.
 *
 * TODO: Actually remove the '%p' from the fixed up string so that
 * there isn't a stray space.
 */

gsize
e_strftime_fix_am_pm (gchar *str,
                      gsize max,
                      const gchar *fmt,
                      const struct tm *tm)
{
	gchar buf[10];
	gchar *sp;
	gchar *ffmt;
	gsize ret;

	if (strstr (fmt, "%p") == NULL && strstr (fmt, "%P") == NULL) {
		/* No AM/PM involved - can use the fmt string directly */
		ret = e_strftime (str, max, fmt, tm);
	} else {
		/* Get the AM/PM symbol from the locale */
		e_strftime (buf, 10, "%p", tm);

		if (buf[0]) {
			/* AM/PM have been defined in the locale
			 * so we can use the fmt string directly. */
			ret = e_strftime (str, max, fmt, tm);
		} else {
			/* No AM/PM defined by locale
			 * must change to 24 hour clock. */
			ffmt = g_strdup (fmt);
			for (sp = ffmt; (sp = strstr (sp, "%l")); sp++) {
				/* Maybe this should be 'k', but I have never
				 * seen a 24 clock actually use that format. */
				sp[1]='H';
			}
			for (sp = ffmt; (sp = strstr (sp, "%I")); sp++) {
				sp[1]='H';
			}
			ret = e_strftime (str, max, ffmt, tm);
			g_free (ffmt);
		}
	}

	return (ret);
}

gsize
e_utf8_strftime_fix_am_pm (gchar *str,
                           gsize max,
                           const gchar *fmt,
                           const struct tm *tm)
{
	gsize sz, ret;
	gchar *locale_fmt, *buf;

	locale_fmt = g_locale_from_utf8 (fmt, -1, NULL, &sz, NULL);
	if (!locale_fmt)
		return 0;

	ret = e_strftime_fix_am_pm (str, max, locale_fmt, tm);
	if (!ret) {
		g_free (locale_fmt);
		return 0;
	}

	buf = g_locale_to_utf8 (str, ret, NULL, &sz, NULL);
	if (!buf) {
		g_free (locale_fmt);
		return 0;
	}

	if (sz >= max) {
		gchar *tmp = buf + max - 1;
		tmp = g_utf8_find_prev_char (buf, tmp);
		if (tmp)
			sz = tmp - buf;
		else
			sz = 0;
	}
	memcpy (str, buf, sz);
	str[sz] = '\0';
	g_free (locale_fmt);
	g_free (buf);
	return sz;
}

/**
 * e_utf8_strftime_match_lc_messages:
 * @string: The string to store the result in.
 * @max: The size of the @string.
 * @fmt: The formatting to use on @tm.
 * @tm: The time value to format.
 *
 * The UTF-8 equivalent of e_strftime (), which also
 * makes sure that the locale used for time and date
 * formatting matches the locale used by the
 * application so that, for example, the quoted
 * message header produced by the mail composer in a
 * reply uses only one locale (i.e. LC_MESSAGES, where
 * available, overrides LC_TIME for consistency).
 *
 * Returns: The number of characters placed in @string.
 *
 * Since: 3.22
 **/
gsize
e_utf8_strftime_match_lc_messages (gchar *string,
				    gsize max,
				    const gchar *fmt,
				    const struct tm *tm)
{
	gsize ret;
#if defined(LC_MESSAGES) && defined(LC_TIME)
	gchar *ctime, *cmessages, *saved_locale;

	/* Use LC_MESSAGES instead of LC_TIME for time
	 * formatting (for consistency).
	 */
	ctime = setlocale (LC_TIME, NULL);
	saved_locale = g_strdup (ctime);
	g_return_val_if_fail (saved_locale != NULL, 0);
	cmessages = setlocale (LC_MESSAGES, NULL);
	setlocale (LC_TIME, cmessages);
#endif

	ret = e_utf8_strftime(string, max, fmt, tm);

#if defined(LC_MESSAGES) && defined(LC_TIME)
	/* Restore LC_TIME, if it has been changed to match
	 * LC_MESSAGES.
	 */
	setlocale (LC_TIME, saved_locale);
	g_free (saved_locale);
#endif

	return ret;
}

/**
 * e_get_month_name:
 * @month: month index
 * @abbreviated: if %TRUE, abbreviate the month name
 *
 * Returns the localized name for @month.  If @abbreviated is %TRUE,
 * returns the locale's abbreviated month name.
 *
 * Returns: localized month name
 **/
const gchar *
e_get_month_name (GDateMonth month,
                  gboolean abbreviated)
{
	/* Make the indices correspond to the enum values. */
	static const gchar *abbr_names[G_DATE_DECEMBER + 1];
	static const gchar *full_names[G_DATE_DECEMBER + 1];
	static gboolean first_time = TRUE;

	g_return_val_if_fail (month >= G_DATE_JANUARY, NULL);
	g_return_val_if_fail (month <= G_DATE_DECEMBER, NULL);

	if (G_UNLIKELY (first_time)) {
		gchar buffer[256];
		GDateMonth ii;
		GDate date;

		memset (abbr_names, 0, sizeof (abbr_names));
		memset (full_names, 0, sizeof (full_names));

		/* First Julian day was in January. */
		g_date_set_julian (&date, 1);

		for (ii = G_DATE_JANUARY; ii <= G_DATE_DECEMBER; ii++) {
			g_date_strftime (buffer, sizeof (buffer), "%b", &date);
			abbr_names[ii] = g_intern_string (buffer);
			g_date_strftime (buffer, sizeof (buffer), "%B", &date);
			full_names[ii] = g_intern_string (buffer);
			g_date_add_months (&date, 1);
		}

		first_time = FALSE;
	}

	return abbreviated ? abbr_names[month] : full_names[month];
}

/**
 * e_get_weekday_name:
 * @weekday: weekday index
 * @abbreviated: if %TRUE, abbreviate the weekday name
 *
 * Returns the localized name for @weekday.  If @abbreviated is %TRUE,
 * returns the locale's abbreviated weekday name.
 *
 * Returns: localized weekday name
 **/
const gchar *
e_get_weekday_name (GDateWeekday weekday,
                    gboolean abbreviated)
{
	/* Make the indices correspond to the enum values. */
	static const gchar *abbr_names[G_DATE_SUNDAY + 1];
	static const gchar *full_names[G_DATE_SUNDAY + 1];
	static gboolean first_time = TRUE;

	g_return_val_if_fail (weekday >= G_DATE_MONDAY, NULL);
	g_return_val_if_fail (weekday <= G_DATE_SUNDAY, NULL);

	if (G_UNLIKELY (first_time)) {
		gchar buffer[256];
		GDateWeekday ii;
		GDate date;

		memset (abbr_names, 0, sizeof (abbr_names));
		memset (full_names, 0, sizeof (full_names));

		/* First Julian day was a Monday. */
		g_date_set_julian (&date, 1);

		for (ii = G_DATE_MONDAY; ii <= G_DATE_SUNDAY; ii++) {
			g_date_strftime (buffer, sizeof (buffer), "%a", &date);
			abbr_names[ii] = g_intern_string (buffer);
			g_date_strftime (buffer, sizeof (buffer), "%A", &date);
			full_names[ii] = g_intern_string (buffer);
			g_date_add_days (&date, 1);
		}

		first_time = FALSE;
	}

	return abbreviated ? abbr_names[weekday] : full_names[weekday];
}

/**
 * e_weekday_get_next:
 * @weekday: a #GDateWeekday
 *
 * Returns the #GDateWeekday after @weekday.
 *
 * Returns: the day after @weekday
 **/
GDateWeekday
e_weekday_get_next (GDateWeekday weekday)
{
	GDateWeekday next;

	/* Verbose for readability. */
	switch (weekday) {
		case G_DATE_MONDAY:
			next = G_DATE_TUESDAY;
			break;
		case G_DATE_TUESDAY:
			next = G_DATE_WEDNESDAY;
			break;
		case G_DATE_WEDNESDAY:
			next = G_DATE_THURSDAY;
			break;
		case G_DATE_THURSDAY:
			next = G_DATE_FRIDAY;
			break;
		case G_DATE_FRIDAY:
			next = G_DATE_SATURDAY;
			break;
		case G_DATE_SATURDAY:
			next = G_DATE_SUNDAY;
			break;
		case G_DATE_SUNDAY:
			next = G_DATE_MONDAY;
			break;
		default:
			next = G_DATE_BAD_WEEKDAY;
			break;
	}

	return next;
}

/**
 * e_weekday_get_prev:
 * @weekday: a #GDateWeekday
 *
 * Returns the #GDateWeekday before @weekday.
 *
 * Returns: the day before @weekday
 **/
GDateWeekday
e_weekday_get_prev (GDateWeekday weekday)
{
	GDateWeekday prev;

	/* Verbose for readability. */
	switch (weekday) {
		case G_DATE_MONDAY:
			prev = G_DATE_SUNDAY;
			break;
		case G_DATE_TUESDAY:
			prev = G_DATE_MONDAY;
			break;
		case G_DATE_WEDNESDAY:
			prev = G_DATE_TUESDAY;
			break;
		case G_DATE_THURSDAY:
			prev = G_DATE_WEDNESDAY;
			break;
		case G_DATE_FRIDAY:
			prev = G_DATE_THURSDAY;
			break;
		case G_DATE_SATURDAY:
			prev = G_DATE_FRIDAY;
			break;
		case G_DATE_SUNDAY:
			prev = G_DATE_SATURDAY;
			break;
		default:
			prev = G_DATE_BAD_WEEKDAY;
			break;
	}

	return prev;
}

/**
 * e_weekday_add_days:
 * @weekday: a #GDateWeekday
 * @n_days: number of days to add
 *
 * Increments @weekday by @n_days.
 *
 * Returns: a #GDateWeekday
 **/
GDateWeekday
e_weekday_add_days (GDateWeekday weekday,
                    guint n_days)
{
	g_return_val_if_fail (
		g_date_valid_weekday (weekday),
		G_DATE_BAD_WEEKDAY);

	n_days %= 7;  /* Weekdays repeat every 7 days. */

	while (n_days-- > 0)
		weekday = e_weekday_get_next (weekday);

	return weekday;
}

/**
 * e_weekday_get_days_between:
 * @weekday1: a #GDateWeekday
 * @weekday2: a #GDateWeekday
 *
 * Counts the number of days starting at @weekday1 and ending at @weekday2.
 *
 * Returns: the number of days
 **/
guint
e_weekday_get_days_between (GDateWeekday weekday1,
                            GDateWeekday weekday2)
{
	guint n_days = 0;

	g_return_val_if_fail (g_date_valid_weekday (weekday1), 0);
	g_return_val_if_fail (g_date_valid_weekday (weekday2), 0);

	while (weekday1 != weekday2) {
		n_days++;
		weekday1 = e_weekday_get_next (weekday1);
	}

	return n_days;
}

/**
 * e_weekday_to_tm_wday:
 * @weekday: a #GDateWeekday
 *
 * Converts a #GDateWeekday to the numbering used in
 * <structname>struct tm</structname>.
 *
 * Returns: number of days since Sunday (0 - 6)
 **/
gint
e_weekday_to_tm_wday (GDateWeekday weekday)
{
	gint tm_wday;

	switch (weekday) {
		case G_DATE_MONDAY:
			tm_wday = 1;
			break;
		case G_DATE_TUESDAY:
			tm_wday = 2;
			break;
		case G_DATE_WEDNESDAY:
			tm_wday = 3;
			break;
		case G_DATE_THURSDAY:
			tm_wday = 4;
			break;
		case G_DATE_FRIDAY:
			tm_wday = 5;
			break;
		case G_DATE_SATURDAY:
			tm_wday = 6;
			break;
		case G_DATE_SUNDAY:
			tm_wday = 0;
			break;
		default:
			g_return_val_if_reached (-1);
	}

	return tm_wday;
}

/**
 * e_weekday_from_tm_wday:
 * @tm_wday: number of days since Sunday (0 - 6)
 *
 * Converts a weekday in the numbering used in
 * <structname>struct tm</structname> to a #GDateWeekday.
 *
 * Returns: a #GDateWeekday
 **/
GDateWeekday
e_weekday_from_tm_wday (gint tm_wday)
{
	GDateWeekday weekday;

	switch (tm_wday) {
		case 0:
			weekday = G_DATE_SUNDAY;
			break;
		case 1:
			weekday = G_DATE_MONDAY;
			break;
		case 2:
			weekday = G_DATE_TUESDAY;
			break;
		case 3:
			weekday = G_DATE_WEDNESDAY;
			break;
		case 4:
			weekday = G_DATE_THURSDAY;
			break;
		case 5:
			weekday = G_DATE_FRIDAY;
			break;
		case 6:
			weekday = G_DATE_SATURDAY;
			break;
		default:
			g_return_val_if_reached (G_DATE_BAD_WEEKDAY);
	}

	return weekday;
}

/* Evolution Locks for crash recovery */
static const gchar *
get_lock_filename (void)
{
	static gchar *filename = NULL;

	if (G_UNLIKELY (filename == NULL))
		filename = g_build_filename (
			e_get_user_config_dir (), ".running", NULL);

	return filename;
}

gboolean
e_file_lock_create (void)
{
	const gchar *filename = get_lock_filename ();
	gboolean status = FALSE;
	FILE *file;

	file = g_fopen (filename, "w");
	if (file != NULL) {
		/* The lock file also serves as a PID file. */
		g_fprintf (
			file, "%" G_GINT64_FORMAT "\n",
			(gint64) getpid ());
		fclose (file);
		status = TRUE;
	} else {
		const gchar *errmsg = g_strerror (errno);
		g_warning ("Lock file creation failed: %s", errmsg);
	}

	return status;
}

void
e_file_lock_destroy (void)
{
	const gchar *filename = get_lock_filename ();

	if (g_unlink (filename) == -1) {
		const gchar *errmsg = g_strerror (errno);
		g_warning ("Lock file deletion failed: %s", errmsg);
	}
}

gboolean
e_file_lock_exists (void)
{
	const gchar *filename = get_lock_filename ();

	return g_file_test (filename, G_FILE_TEST_EXISTS);
}

/* Returns a PID stored in the lock file; 0 if no such file exists. */
GPid
e_file_lock_get_pid (void)
{
	const gchar *filename = get_lock_filename ();
	gchar *contents = NULL;
	GPid pid = (GPid) 0;
	gint64 n_int64;

	if (!g_file_get_contents (filename, &contents, NULL, NULL)) {
		return pid;
	}

	/* Try to extract an integer value from the string. */
	n_int64 = g_ascii_strtoll (contents, NULL, 10);
	if (n_int64 > 0 && n_int64 < G_MAXINT64) {
		/* XXX Probably not portable. */
		pid = (GPid) n_int64;
	}

	g_free (contents);

	return pid;
}

/**
 * e_util_guess_mime_type:
 * @filename: a local file name, or URI
 * @localfile: %TRUE to check the file content, FALSE to check only the name
 *
 * Tries to determine the MIME type for @filename.  Free the returned
 * string with g_free().
 *
 * Returns: the MIME type of @filename, or %NULL if the MIME type could
 *          not be determined
 **/
gchar *
e_util_guess_mime_type (const gchar *filename,
                        gboolean localfile)
{
	gchar *mime_type = NULL;

	g_return_val_if_fail (filename != NULL, NULL);

	if (localfile) {
		GFile *file;
		GFileInfo *fi;

		if (strstr (filename, "://"))
			file = g_file_new_for_uri (filename);
		else
			file = g_file_new_for_path (filename);

		fi = g_file_query_info (
			file, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
			G_FILE_QUERY_INFO_NONE, NULL, NULL);
		if (fi) {
			mime_type = g_content_type_get_mime_type (
				g_file_info_get_content_type (fi));
			g_object_unref (fi);
		}

		g_object_unref (file);
	}

	if (!mime_type) {
		/* file doesn't exists locally, thus guess based on the filename */
		gboolean uncertain = FALSE;
		gchar *content_type;

		content_type = g_content_type_guess (filename, NULL, 0, &uncertain);
		if (content_type) {
			mime_type = g_content_type_get_mime_type (content_type);
			g_free (content_type);
		}
	}

	return mime_type;
}

GSList *
e_util_get_category_filter_options (void)
{
	GSList *res = NULL;
	GList *clist, *l;

	clist = e_categories_dup_list ();
	for (l = clist; l; l = l->next) {
		const gchar *cname = l->data;
		struct _filter_option *fo;

		if (!e_categories_is_searchable (cname))
			continue;

		fo = g_new0 (struct _filter_option, 1);

		fo->title = g_strdup (cname);
		fo->value = g_strdup (cname);
		res = g_slist_prepend (res, fo);
	}

	g_list_free_full (clist, g_free);

	return g_slist_reverse (res);
}

/**
 * e_util_dup_searchable_categories:
 *
 * Returns a list of the searchable categories, with each item being a UTF-8
 * category name. The list should be freed with g_list_free() when done with it,
 * and the items should be freed with g_free(). Everything can be freed at once
 * using g_list_free_full().
 *
 * Returns: (transfer full) (element-type utf8): a list of searchable category
 * names; free with g_list_free_full()
 */
GList *
e_util_dup_searchable_categories (void)
{
	GList *res = NULL, *all_categories, *l;

	all_categories = e_categories_dup_list ();
	for (l = all_categories; l; l = l->next) {
		gchar *cname = l->data;

		/* Steal the string from e_categories_dup_list(). */
		if (e_categories_is_searchable (cname))
			res = g_list_prepend (res, (gpointer) cname);
		else
			g_free (cname);
	}

	/* NOTE: Do *not* free the items. They have been freed or stolen
	 * above. */
	g_list_free (all_categories);

	return g_list_reverse (res);
}
/**
 * e_util_get_open_source_job_info:
 * @extension_name: an extension name of the source
 * @source_display_name: an ESource's display name
 * @description: (out) (transfer-full): a description to use
 * @alert_ident: (out) (transfer-full): an alert ident to use on failure
 * @alert_arg_0: (out) (transfer-full): an alert argument 0 to use on failure
 *
 * Populates @description, @alert_ident and @alert_arg_0 to be used
 * to open an #ESource with extension @extension_name. The values
 * can be used for functions like e_alert_sink_submit_thread_job().
 *
 * If #TRUE is returned, then the caller is responsible to free
 * all @description, @alert_ident and @alert_arg_0 with g_free(),
 * when no longer needed.
 *
 * Returns: #TRUE, if the values for @description, @alert_ident and @alert_arg_0
 *     were set for the given @extension_name; when #FALSE is returned, then
 *     none of these out variables are changed.
 *
 * Since: 3.16
 **/
gboolean
e_util_get_open_source_job_info (const gchar *extension_name,
				 const gchar *source_display_name,
				 gchar **description,
				 gchar **alert_ident,
				 gchar **alert_arg_0)
{
	g_return_val_if_fail (extension_name != NULL, FALSE);
	g_return_val_if_fail (source_display_name != NULL, FALSE);
	g_return_val_if_fail (description != NULL, FALSE);
	g_return_val_if_fail (alert_ident != NULL, FALSE);
	g_return_val_if_fail (alert_arg_0 != NULL, FALSE);

	if (g_ascii_strcasecmp (extension_name, E_SOURCE_EXTENSION_CALENDAR) == 0) {
		*alert_ident = g_strdup ("calendar:failed-open-calendar");
		*description = g_strdup_printf (_("Opening calendar “%s”"), source_display_name);
	} else if (g_ascii_strcasecmp (extension_name, E_SOURCE_EXTENSION_MEMO_LIST) == 0) {
		*alert_ident = g_strdup ("calendar:failed-open-memos");
		*description = g_strdup_printf (_("Opening memo list “%s”"), source_display_name);
	} else if (g_ascii_strcasecmp (extension_name, E_SOURCE_EXTENSION_TASK_LIST) == 0) {
		*alert_ident = g_strdup ("calendar:failed-open-tasks");
		*description = g_strdup_printf (_("Opening task list “%s”"), source_display_name);
	} else if (g_ascii_strcasecmp (extension_name, E_SOURCE_EXTENSION_ADDRESS_BOOK) == 0) {
		*alert_ident = g_strdup ("addressbook:load-error");
		*description = g_strdup_printf (_("Opening address book “%s”"), source_display_name);
	} else {
		return FALSE;
	}

	*alert_arg_0 = g_strdup (source_display_name);

	return TRUE;
}

/**
 * e_util_propagate_open_source_job_error:
 * @job_data: an #EAlertSinkThreadJobData instance
 * @extension_name: what extension name had beeing opened
 * @local_error: (allow none): a #GError as obtained in a thread job; can be NULL for success
 * @error: (allow none): an output #GError, to which propagate the @local_error
 *
 * Propagates (and cosumes) the @local_error into the @error, eventually
 * changes alert_ident for the @job_data for well-known error codes,
 * where is available better error description.
 *
 * Since: 3.16
 **/
void
e_util_propagate_open_source_job_error (EAlertSinkThreadJobData *job_data,
					const gchar *extension_name,
					GError *local_error,
					GError **error)
{
	const gchar *alert_ident = NULL;

	g_return_if_fail (job_data != NULL);
	g_return_if_fail (extension_name != NULL);

	if (!local_error)
		return;

	if (!error) {
		g_error_free (local_error);
		return;
	}

	if (g_error_matches (local_error, E_CLIENT_ERROR, E_CLIENT_ERROR_REPOSITORY_OFFLINE)) {
		if (g_ascii_strcasecmp (extension_name, E_SOURCE_EXTENSION_CALENDAR) == 0) {
			alert_ident = "calendar:prompt-no-contents-offline-calendar";
		} else if (g_ascii_strcasecmp (extension_name, E_SOURCE_EXTENSION_MEMO_LIST) == 0) {
			alert_ident = "calendar:prompt-no-contents-offline-memos";
		} else if (g_ascii_strcasecmp (extension_name, E_SOURCE_EXTENSION_TASK_LIST) == 0) {
			alert_ident = "calendar:prompt-no-contents-offline-tasks";
		} else if (g_ascii_strcasecmp (extension_name, E_SOURCE_EXTENSION_ADDRESS_BOOK) == 0) {
		}
	}

	if (alert_ident)
		e_alert_sink_thread_job_set_alert_ident (job_data, alert_ident);

	g_propagate_error (error, local_error);
}

EClient *
e_util_open_client_sync (EAlertSinkThreadJobData *job_data,
			 EClientCache *client_cache,
			 const gchar *extension_name,
			 ESource *source,
			 guint32 wait_for_connected_seconds,
			 GCancellable *cancellable,
			 GError **error)
{
	gchar *description = NULL, *alert_ident = NULL, *alert_arg_0 = NULL;
	EClient *client = NULL;
	ESourceRegistry *registry;
	gchar *display_name;
	GError *local_error = NULL;

	registry = e_client_cache_ref_registry (client_cache);
	display_name = e_util_get_source_full_name (registry, source);
	g_clear_object (&registry);

	g_warn_if_fail (e_util_get_open_source_job_info (extension_name,
		display_name, &description, &alert_ident, &alert_arg_0));

	g_free (display_name);

	camel_operation_push_message (cancellable, "%s", description);

	client = e_client_cache_get_client_sync (client_cache, source, extension_name, wait_for_connected_seconds, cancellable, &local_error);

	camel_operation_pop_message (cancellable);

	if (!client) {
		e_alert_sink_thread_job_set_alert_ident (job_data, alert_ident);
		e_alert_sink_thread_job_set_alert_arg_0 (job_data, alert_arg_0);

		e_util_propagate_open_source_job_error (job_data, extension_name, local_error, error);
	}

	g_free (description);
	g_free (alert_ident);
	g_free (alert_arg_0);

	return client;
}

/**
 * e_binding_transform_rgba_to_string:
 * @binding: a #GBinding
 * @source_value: a #GValue of type #GDK_TYPE_RGBA
 * @target_value: a #GValue of type #G_TYPE_STRING
 * @not_used: not used
 *
 * Transforms a #GdkRGBA value to a color string specification.
 *
 * Returns: %TRUE always
 **/
gboolean
e_binding_transform_rgba_to_string (GBinding *binding,
                                    const GValue *source_value,
                                    GValue *target_value,
                                    gpointer not_used)
{
	const GdkRGBA *color;
	gchar *string;

	g_return_val_if_fail (G_IS_BINDING (binding), FALSE);

	color = g_value_get_boxed (source_value);
	if (!color) {
		g_value_set_string (target_value, "");
	} else {
		string = gdk_rgba_to_string (color);
		g_value_take_string (target_value, g_steal_pointer (&string));
	}

	return TRUE;
}

/**
 * e_binding_transform_string_to_rgba:
 * @binding: a #GBinding
 * @source_value: a #GValue of type #G_TYPE_STRING
 * @target_value: a #GValue of type #GDK_TYPE_RGBA
 * @not_used: not used
 *
 * Transforms a color string specification to a #GdkRGBA.
 *
 * Returns: %TRUE if color string specification was valid
 **/
gboolean
e_binding_transform_string_to_rgba (GBinding *binding,
                                    const GValue *source_value,
                                    GValue *target_value,
                                    gpointer not_used)
{
	GdkRGBA color;
	const gchar *string;

	g_return_val_if_fail (G_IS_BINDING (binding), FALSE);

	string = g_value_get_string (source_value);
	if (gdk_rgba_parse (&color, string)) {
		g_value_set_boxed (target_value, &color);
		return TRUE;
	}

	return FALSE;
}

/**
 * e_binding_transform_text_non_null:
 * @binding: a #GBinding
 * @source_value: a #GValue of type #G_TYPE_STRING
 * @target_value: a #GValue of type #G_TYPE_STRING
 * @user_data: custom user data, unused
 *
 * Transforms a text value to a text value which is never NULL;
 * an empty string is used instead of NULL.
 *
 * Returns: %TRUE on success
 **/
gboolean
e_binding_transform_text_non_null (GBinding *binding,
				   const GValue *source_value,
				   GValue *target_value,
				   gpointer user_data)
{
	const gchar *str;

	g_return_val_if_fail (G_IS_BINDING (binding), FALSE);
	g_return_val_if_fail (source_value != NULL, FALSE);
	g_return_val_if_fail (target_value != NULL, FALSE);

	str = g_value_get_string (source_value);
	if (!str)
		str = "";

	g_value_set_string (target_value, str);

	return TRUE;
}

/**
 * e_binding_transform_text_to_uri:
 * @binding: a #GBinding
 * @source_value: a #GValue of type #G_TYPE_STRING
 * @target_value: a #GValue of boxed #GUri
 * @not_used: not used
 *
 * Transforms a URI string into a #GUri. It expects the source
 * object to be an #ESourceExtension descendant, then it adds
 * also the user name from its #ESourceAuthentication extension.
 *
 * Returns: %TRUE
 *
 **/
gboolean
e_binding_transform_text_to_uri (GBinding *binding,
				 const GValue *source_value,
				 GValue *target_value,
				 gpointer not_used)
{
	GUri *uri;
	GObject *source_binding;
	const gchar *text;

	text = g_value_get_string (source_value);
	uri = g_uri_parse (text, SOUP_HTTP_URI_FLAGS, NULL);

	if (!uri)
		uri = g_uri_build (G_URI_FLAGS_NONE, "http", NULL, NULL, -1, "", NULL, NULL);

	source_binding = g_binding_dup_source (binding);

	if (E_IS_SOURCE_EXTENSION (source_binding)) {
		ESource *source = NULL;
		ESourceAuthentication *extension;
		const gchar *user;

		source = e_source_extension_ref_source (E_SOURCE_EXTENSION (source_binding));
		if (e_source_has_extension (source, E_SOURCE_EXTENSION_AUTHENTICATION)) {
			extension = e_source_get_extension (source, E_SOURCE_EXTENSION_AUTHENTICATION);
			user = e_source_authentication_get_user (extension);

			e_util_change_uri_component (&uri, SOUP_URI_USER, user);
		}

		g_clear_object (&source);
	}

	g_value_take_boxed (target_value, uri);
	g_clear_object (&source_binding);

	return TRUE;
}

/**
 * e_binding_transform_uri_to_text:
 * @binding: a #GBinding
 * @source_value: a #GValue of boxed #GUri
 * @target_value: a #GValue of type #G_TYPE_STRING
 * @not_used: not used
 *
 * Transforms a #GUri into a string.
 *
 * Returns: %TRUE
 *
 **/
gboolean
e_binding_transform_uri_to_text (GBinding *binding,
				 const GValue *source_value,
				 GValue *target_value,
				 gpointer not_used)
{
	GUri *uri;
	gchar *text;

	uri = g_value_get_boxed (source_value);

	if (g_uri_get_host (uri)) {
		text = g_uri_to_string_partial (uri, G_URI_HIDE_USERINFO | G_URI_HIDE_PASSWORD);
	} else {
		GObject *target;

		text = NULL;
		target = g_binding_dup_target (binding);
		g_object_get (target, g_binding_get_target_property (binding), &text, NULL);
		g_clear_object (&target);

		if (!text || !*text) {
			g_free (text);
			text = g_uri_to_string_partial (uri, G_URI_HIDE_USERINFO | G_URI_HIDE_PASSWORD);
		}
	}

	g_value_take_string (target_value, text);

	return TRUE;
}

/**
 * e_binding_bind_object_text_property:
 * @source: the source #GObject
 * @source_property: the text property on the source to bind
 * @target: the target #GObject
 * @target_property: the text property on the target to bind
 * @flags: flags to pass to e_binding_bind_property_full()
 *
 * Installs a new text property object binding, using e_binding_bind_property_full(),
 * with transform functions to make sure that a NULL pointer is not
 * passed in either way. Instead of NULL an empty string is used.
 *
 * Returns: the #GBinding instance representing the binding between the two #GObject instances;
 *    there applies the same rules to it as for the result of e_binding_bind_property_full().
 **/
GBinding *
e_binding_bind_object_text_property (gpointer source,
				     const gchar *source_property,
				     gpointer target,
				     const gchar *target_property,
				     GBindingFlags flags)
{
	GObjectClass *klass;
	GParamSpec *property;

	g_return_val_if_fail (G_IS_OBJECT (source), NULL);
	g_return_val_if_fail (source_property != NULL, NULL);
	g_return_val_if_fail (G_IS_OBJECT (target), NULL);
	g_return_val_if_fail (target_property != NULL, NULL);

	klass = G_OBJECT_GET_CLASS (source);
	property = g_object_class_find_property (klass, source_property);
	g_return_val_if_fail (property != NULL, NULL);
	g_return_val_if_fail (property->value_type == G_TYPE_STRING, NULL);

	klass = G_OBJECT_GET_CLASS (target);
	property = g_object_class_find_property (klass, target_property);
	g_return_val_if_fail (property != NULL, NULL);
	g_return_val_if_fail (property->value_type == G_TYPE_STRING, NULL);

	return e_binding_bind_property_full (source, source_property,
					     target, target_property,
					     flags,
					     e_binding_transform_text_non_null,
					     e_binding_transform_text_non_null,
					     NULL, NULL);
}

typedef struct _EConnectNotifyData {
	GConnectFlags flags;
	GValue *old_value;

	GCallback c_handler;
	gpointer user_data;
} EConnectNotifyData;

static EConnectNotifyData *
e_connect_notify_data_new (GCallback c_handler,
			   gpointer user_data,
			   guint32 connect_flags)
{
	EConnectNotifyData *connect_data;

	connect_data = g_new0 (EConnectNotifyData, 1);
	connect_data->flags = connect_flags;
	connect_data->c_handler = c_handler;
	connect_data->user_data = user_data;

	return connect_data;
}

static void
e_connect_notify_data_free (EConnectNotifyData *data)
{
	if (!data)
		return;

	if (data->old_value) {
		g_value_unset (data->old_value);
		g_free (data->old_value);
	}
	g_free (data);
}

static gboolean
e_value_equal (GValue *value1,
	       GValue *value2)
{
	if (value1 == value2)
		return TRUE;

	if (!value1 || !value2)
		return FALSE;

	#define testType(_uc,_lc) G_STMT_START { \
		if (G_VALUE_HOLDS_ ## _uc (value1)) \
			return g_value_get_ ## _lc (value1) == g_value_get_ ## _lc (value2); \
	} G_STMT_END

	testType (BOOLEAN, boolean);
	testType (BOXED, boxed);
	testType (CHAR, schar);
	testType (DOUBLE, double);
	testType (ENUM, enum);
	testType (FLAGS, flags);
	testType (FLOAT, float);
	testType (GTYPE, gtype);
	testType (INT, int);
	testType (INT64, int64);
	testType (LONG, long);
	testType (OBJECT, object);
	testType (POINTER, pointer);
	testType (UCHAR, uchar);
	testType (UINT, uint);
	testType (UINT64, uint64);
	testType (ULONG, ulong);

	#undef testType

	if (G_VALUE_HOLDS_PARAM (value1)) {
		GParamSpec *param1, *param2;

		param1 = g_value_get_param (value1);
		param2 = g_value_get_param (value2);

		return param1 && param2 &&
			g_strcmp0 (param1->name, param2->name) == 0 &&
			param1->flags == param2->flags &&
			param1->value_type == param2->value_type &&
			param1->owner_type == param2->owner_type;
	} else if (G_VALUE_HOLDS_STRING (value1)) {
		const gchar *string1, *string2;

		string1 = g_value_get_string (value1);
		string2 = g_value_get_string (value2);

		return g_strcmp0 (string1, string2) == 0;
	} else if (G_VALUE_HOLDS_VARIANT (value1)) {
		GVariant *variant1, *variant2;

		variant1 = g_value_get_variant (value1);
		variant2 = g_value_get_variant (value2);

		return variant1 == variant2 ||
			(variant1 && variant2 && g_variant_equal (variant1, variant2));
	}

	return FALSE;
}

static void
e_signal_connect_notify_cb (gpointer instance,
			    GParamSpec *param,
			    gpointer user_data)
{
	EConnectNotifyData *connect_data = user_data;
	GValue *value;

	g_return_if_fail (connect_data != NULL);

	value = g_new0 (GValue, 1);
	g_value_init (value, param->value_type);
	g_object_get_property (instance, param->name, value);

	if (!e_value_equal (connect_data->old_value, value)) {
		typedef void (* NotifyCBType) (gpointer instance, GParamSpec *param, gpointer user_data);
		NotifyCBType c_handler = (NotifyCBType) connect_data->c_handler;

		if (connect_data->old_value) {
			g_value_unset (connect_data->old_value);
			g_free (connect_data->old_value);
		}
		connect_data->old_value = value;

		if (connect_data->flags == G_CONNECT_SWAPPED) {
			c_handler (connect_data->user_data, param, instance);
		} else {
			c_handler (instance, param, connect_data->user_data);
		}
	} else {
		g_value_unset (value);
		g_free (value);
	}
}

/**
 * e_signal_connect_notify:
 *
 * This installs a special handler in front of @c_handler, which will
 * call the @c_handler only if the property value changed since the last
 * time it was checked. Due to this, these handlers cannot be disconnected
 * by by any of the g_signal_handlers_* functions, but only with the returned
 * handler ID. A convenient e_signal_disconnect_notify_handler() was added
 * to make it easier.
 **/
gulong
e_signal_connect_notify (gpointer instance,
			 const gchar *notify_name,
			 GCallback c_handler,
			 gpointer user_data)
{
	EConnectNotifyData *connect_data;

	g_return_val_if_fail (g_str_has_prefix (notify_name, "notify::"), 0);

	connect_data = e_connect_notify_data_new (c_handler, user_data, 0);

	return g_signal_connect_data (instance,
				      notify_name,
				      G_CALLBACK (e_signal_connect_notify_cb),
				      connect_data,
				      (GClosureNotify) e_connect_notify_data_free,
				      0);
}

/**
 * e_signal_connect_notify_after:
 *
 * This installs a special handler in front of @c_handler, which will
 * call the @c_handler only if the property value changed since the last
 * time it was checked. Due to this, these handlers cannot be disconnected
 * by by any of the g_signal_handlers_* functions, but only with the returned
 * handler ID. A convenient e_signal_disconnect_notify_handler() was added
 * to make it easier.
 **/
gulong
e_signal_connect_notify_after (gpointer instance,
			       const gchar *notify_name,
			       GCallback c_handler,
			       gpointer user_data)
{
	EConnectNotifyData *connect_data;

	g_return_val_if_fail (g_str_has_prefix (notify_name, "notify::"), 0);

	connect_data = e_connect_notify_data_new (c_handler, user_data, G_CONNECT_AFTER);

	return g_signal_connect_data (instance,
				      notify_name,
				      G_CALLBACK (e_signal_connect_notify_cb),
				      connect_data,
				      (GClosureNotify) e_connect_notify_data_free,
				      G_CONNECT_AFTER);
}

/**
 * e_signal_connect_notify_swapped:
 *
 * This installs a special handler in front of @c_handler, which will
 * call the @c_handler only if the property value changed since the last
 * time it was checked. Due to this, these handlers cannot be disconnected
 * by by any of the g_signal_handlers_* functions, but only with the returned
 * handler ID. A convenient e_signal_disconnect_notify_handler() was added
 * to make it easier.
 **/
gulong
e_signal_connect_notify_swapped (gpointer instance,
				 const gchar *notify_name,
				 GCallback c_handler,
				 gpointer user_data)
{
	EConnectNotifyData *connect_data;

	g_return_val_if_fail (g_str_has_prefix (notify_name, "notify::"), 0);

	connect_data = e_connect_notify_data_new (c_handler, user_data, G_CONNECT_SWAPPED);

	return g_signal_connect_data (instance,
				      notify_name,
				      G_CALLBACK (e_signal_connect_notify_cb),
				      connect_data,
				      (GClosureNotify) e_connect_notify_data_free,
				      0);
}

/**
 * e_signal_connect_notify_object:
 *
 * This installs a special handler in front of @c_handler, which will
 * call the @c_handler only if the property value changed since the last
 * time it was checked. Due to this, these handlers cannot be disconnected
 * by by any of the g_signal_handlers_* functions, but only with the returned
 * handler ID. A convenient e_signal_disconnect_notify_handler() was added
 * to make it easier.
 **/
gulong
e_signal_connect_notify_object (gpointer instance,
				const gchar *notify_name,
				GCallback c_handler,
				gpointer gobject,
				GConnectFlags connect_flags)
{
	EConnectNotifyData *connect_data;
	GClosure *closure;

	g_return_val_if_fail (g_str_has_prefix (notify_name, "notify::"), 0);

	if (!gobject) {
		if ((connect_flags & G_CONNECT_SWAPPED) != 0)
			return e_signal_connect_notify_swapped (instance, notify_name, c_handler, gobject);
		else if ((connect_flags & G_CONNECT_AFTER) != 0)
			e_signal_connect_notify_after (instance, notify_name, c_handler, gobject);
		else
			g_warn_if_fail (connect_flags == 0);

		return e_signal_connect_notify (instance, notify_name, c_handler, gobject);
	}

	g_return_val_if_fail (G_IS_OBJECT (gobject), 0);

	connect_data = e_connect_notify_data_new (c_handler, gobject, connect_flags & G_CONNECT_SWAPPED);
	closure = g_cclosure_new (
		G_CALLBACK (e_signal_connect_notify_cb),
		connect_data,
		(GClosureNotify) e_connect_notify_data_free);

	g_object_watch_closure (G_OBJECT (gobject), closure);

	return g_signal_connect_closure (instance,
					 notify_name,
					 closure,
					 connect_flags & G_CONNECT_AFTER);
}

/**
 * e_signal_disconnect_notify_handler:
 *
 * Convenient handler disconnect function to be used with
 * returned handler IDs from:
 *    e_signal_connect_notify()
 *    e_signal_connect_notify_after()
 *    e_signal_connect_notify_swapped()
 *    e_signal_connect_notify_object()
 * but not necessarily only with these functions.
 **/
void
e_signal_disconnect_notify_handler (gpointer instance,
				    gulong *handler_id)
{
	g_return_if_fail (instance != NULL);
	g_return_if_fail (handler_id != NULL);

	if (!*handler_id)
		return;

	g_signal_handler_disconnect (instance, *handler_id);
	*handler_id = 0;
}

static GMutex settings_hash_lock;
static GHashTable *settings_hash = NULL;

/**
 * e_util_ref_settings:
 * @schema_id: the id of the schema to reference settings for
 *
 * Either returns an existing referenced #GSettings object for the given @schema_id,
 * or creates a new one and remembers it for later use, to avoid having too many
 * #GSettings objects created for the same @schema_id.
 *
 * Returns: A #GSettings for the given @schema_id. The returned #GSettings object
 *   is referenced, thus free it with g_object_unref() when done with it.
 *
 * Since: 3.16
 **/
GSettings *
e_util_ref_settings (const gchar *schema_id)
{
	GSettings *settings;

	g_return_val_if_fail (schema_id != NULL, NULL);
	g_return_val_if_fail (*schema_id, NULL);

	g_mutex_lock (&settings_hash_lock);

	if (!settings_hash) {
		settings_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
	}

	settings = g_hash_table_lookup (settings_hash, schema_id);
	if (!settings) {
		settings = g_settings_new (schema_id);
		g_hash_table_insert (settings_hash, g_strdup (schema_id), settings);
	}

	if (settings)
		g_object_ref (settings);

	g_mutex_unlock (&settings_hash_lock);

	return settings;
}

/**
 * e_util_cleanup_settings:
 *
 * Frees all the memory taken by e_util_ref_settings().
 *
 * Since: 3.16
 **/
void
e_util_cleanup_settings (void)
{
	g_mutex_lock (&settings_hash_lock);

	g_clear_pointer (&settings_hash, g_hash_table_destroy);

	g_mutex_unlock (&settings_hash_lock);
}

/**
 * e_util_prompt_user:
 * @parent: parent window
 * @settings_schema: name of the settings schema where @promptkey belongs.
 * @promptkey: settings key to check if we should prompt the user or not.
 * @tag: e_alert tag.
 *
 * Convenience function to query the user with a Yes/No dialog and a
 * "Do not show this dialog again" checkbox. If the user checks that
 * checkbox, then @promptkey is set to %FALSE, otherwise it is set to
 * %TRUE.
 *
 * Returns %TRUE if the user clicks Yes or %FALSE otherwise.
 **/
gboolean
e_util_prompt_user (GtkWindow *parent,
                    const gchar *settings_schema,
                    const gchar *promptkey,
                    const gchar *tag,
                    ...)
{
	GtkWidget *dialog;
	GtkWidget *check = NULL;
	GtkWidget *container;
	va_list ap;
	gint button;
	GSettings *settings = NULL;
	EAlert *alert = NULL;

	if (promptkey) {
		settings = e_util_ref_settings (settings_schema);

		if (!g_settings_get_boolean (settings, promptkey)) {
			g_object_unref (settings);
			return TRUE;
		}
	}

	va_start (ap, tag);
	alert = e_alert_new_valist (tag, ap);
	va_end (ap);

	dialog = e_alert_dialog_new (parent, alert);
	g_object_unref (alert);

	container = e_alert_dialog_get_content_area (E_ALERT_DIALOG (dialog));

	if (promptkey) {
		check = gtk_check_button_new_with_mnemonic (
			_("_Do not show this message again"));
		gtk_box_pack_start (
			GTK_BOX (container), check, FALSE, FALSE, 0);
		gtk_widget_show (check);
	}

	button = gtk_dialog_run (GTK_DIALOG (dialog));
	if (promptkey)
		g_settings_set_boolean (
			settings, promptkey,
			!gtk_toggle_button_get_active (
				GTK_TOGGLE_BUTTON (check)));

	gtk_widget_destroy (dialog);

	g_clear_object (&settings);

	return button == GTK_RESPONSE_YES;
}

/**
 * e_util_is_running_gnome:
 *
 * Returns: Whether the current running desktop environment is GNOME.
 *
 * Since: 3.18
 **/
gboolean
e_util_is_running_gnome (void)
{
#ifdef G_OS_WIN32
	return FALSE;
#else
	static gint runs_gnome = -1;

	if (runs_gnome == -1) {
		const gchar *desktop;
		desktop = g_getenv ("XDG_CURRENT_DESKTOP");
		runs_gnome = 0;
		if (desktop != NULL) {
			gint ii;
			gchar **desktops = g_strsplit (desktop, ":", -1);
			for (ii = 0; desktops[ii]; ii++) {
				if (!g_ascii_strcasecmp (desktops[ii], "gnome")) {
					runs_gnome = 1;
					break;
				}
			}
			g_strfreev (desktops);
		}

		if (runs_gnome) {
			GDesktopAppInfo *app_info;

			app_info = g_desktop_app_info_new ("gnome-notifications-panel.desktop");
			if (!app_info) {
				runs_gnome = 0;
			}

			g_clear_object (&app_info);
		}
	}

	return runs_gnome != 0;
#endif
}

/**
 * e_util_is_running_flatpak:
 *
 * Returns: Whether running in Flatpak.
 *
 * Since: 3.32
 **/
gboolean
e_util_is_running_flatpak (void)
{
#ifdef G_OS_UNIX
	static gint is_flatpak = -1;

	if (is_flatpak == -1) {
		if (g_file_test ("/.flatpak-info", G_FILE_TEST_EXISTS) ||
		    g_getenv ("EVOLUTION_FLATPAK") != NULL) /* Only for debugging purposes */
			is_flatpak = 1;
		else
			is_flatpak = 0;
	}

	return is_flatpak == 1;
#else
	return FALSE;
#endif
}

/**
 * e_util_set_entry_issue_hint:
 * @entry: a #GtkEntry
 * @hint: (allow none): a hint to set, or %NULL to unset
 *
 * Sets a @hint on the secondary @entry icon about an entered value issue,
 * or unsets it, when the @hint is %NULL.
 *
 * Since: 3.20
 **/
void
e_util_set_entry_issue_hint (GtkWidget *entry,
			     const gchar *hint)
{
	GtkEntry *eentry;

	g_return_if_fail (GTK_IS_ENTRY (entry));

	eentry = GTK_ENTRY (entry);

	if (hint) {
		gtk_entry_set_icon_from_icon_name (eentry, GTK_ENTRY_ICON_SECONDARY, "dialog-warning");
		gtk_entry_set_icon_tooltip_text (eentry, GTK_ENTRY_ICON_SECONDARY, hint);
	} else {
		gtk_entry_set_icon_from_icon_name (eentry, GTK_ENTRY_ICON_SECONDARY, NULL);
		gtk_entry_set_icon_tooltip_text (eentry, GTK_ENTRY_ICON_SECONDARY, NULL);
	}
}

static GThread *main_thread = NULL;

void
e_util_init_main_thread (GThread *thread)
{
	g_return_if_fail (main_thread == NULL);

	main_thread = thread ? thread : g_thread_self ();
}

gboolean
e_util_is_main_thread (GThread *thread)
{
	return thread ? thread == main_thread : g_thread_self () == main_thread;
}

/**
 * e_util_save_image_from_clipboard:
 * @clipboard: a #GtkClipboard
 * @hint: (allow none): a hint to set, or %NULL to unset
 *
 * Saves the image from @clipboard to a temporary file and returns its URI.
 *
 * Since: 3.22
 **/
gchar *
e_util_save_image_from_clipboard (GtkClipboard *clipboard)
{
	GdkPixbuf *pixbuf = NULL;
	gchar *tmpl;
	gchar *filename = NULL;
	gchar *uri = NULL;
	GError *error = NULL;

	g_return_val_if_fail (GTK_IS_CLIPBOARD (clipboard), NULL);

	/* Extract the image data from the clipboard. */
	pixbuf = gtk_clipboard_wait_for_image (clipboard);
	g_return_val_if_fail (pixbuf != NULL, FALSE);

	tmpl = g_strconcat (_("Image"), "-XXXXXX.png", NULL);

	/* Reserve a temporary file. */
	filename = e_mktemp (tmpl);

	g_free (tmpl);

	if (filename == NULL) {
		g_set_error (
			&error, G_FILE_ERROR,
			g_file_error_from_errno (errno),
			"Could not create temporary file: %s",
			g_strerror (errno));
		goto exit;
	}

	/* Save the pixbuf as a temporary file in image/png format. */
	if (!gdk_pixbuf_save (pixbuf, filename, "png", &error, NULL))
		goto exit;

	/* Convert the filename to a URI. */
	uri = g_filename_to_uri (filename, NULL, &error);

 exit:
	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	g_object_unref (pixbuf);
	g_free (filename);

	return uri;
}

/**
 * e_util_save_file_chooser_folder:
 * @file_chooser: a #GtkFileChooser
 *
 * Saves the current folder of the @file_chooser, thus it could be used
 * by e_util_load_file_chooser_folder() to open it in the last chosen folder.
 *
 * Since: 3.24
 **/
void
e_util_save_file_chooser_folder (GtkFileChooser *file_chooser)
{
	GSettings *settings;
	gchar *uri;

	g_return_if_fail (GTK_IS_FILE_CHOOSER (file_chooser));

	uri = gtk_file_chooser_get_current_folder_uri (file_chooser);
	if (uri && g_str_has_prefix (uri, "file://")) {
		settings = e_util_ref_settings ("org.gnome.evolution.shell");
		g_settings_set_string (settings, "file-chooser-folder", uri);
		g_object_unref (settings);
	}

	g_free (uri);
}

/**
 * e_util_load_file_chooser_folder:
 * @file_chooser: a #GtkFileChooser
 *
 * Sets the current folder to the @file_chooser to the one previously saved
 * by e_util_save_file_chooser_folder(). The function does nothing if none
 * or invalid is saved.
 *
 * Since: 3.24
 **/
void
e_util_load_file_chooser_folder (GtkFileChooser *file_chooser)
{
	GSettings *settings;
	gchar *uri;

	g_return_if_fail (GTK_IS_FILE_CHOOSER (file_chooser));

	settings = e_util_ref_settings ("org.gnome.evolution.shell");
	uri = g_settings_get_string (settings, "file-chooser-folder");
	g_object_unref (settings);

	if (uri && g_str_has_prefix (uri, "file://")) {
		gchar *filename;

		filename = g_filename_from_uri (uri, NULL, NULL);
		if (filename && g_file_test (filename, G_FILE_TEST_IS_DIR))
			gtk_file_chooser_set_current_folder_uri (file_chooser, uri);

		g_free (filename);
	}

	g_free (uri);
}

/**
 * e_util_get_webkit_developer_mode_enabled:
 *
 * Returns: Whether WebKit developer mode is enabled. This is read only
 *    once, thus any changes in the GSettings property require restart
 *    of the Evolution.
 *
 * Since: 3.24
 **/
gboolean
e_util_get_webkit_developer_mode_enabled (void)
{
	static gchar enabled = -1;

	if (enabled == -1) {
		GSettings *settings;

		settings = e_util_ref_settings ("org.gnome.evolution.shell");
		enabled = g_settings_get_boolean (settings, "webkit-developer-mode") ? 1 : 0;
		g_clear_object (&settings);
	}

	return enabled != 0;
}

/**
 * e_util_next_uri_from_uri_list:
 * @uri_list: array of URIs separated by new lines
 * @len: (out): a length of the found URI
 * @list_len: (out): a length of the array
 *
 * Returns: A newly allocated string with found URI.
 *
 * Since: 3.26
 **/
gchar *
e_util_next_uri_from_uri_list (guchar **uri_list,
                               gint *len,
                               gint *list_len)
{
	guchar *uri, *begin;

	begin = *uri_list;
	*len = 0;
	while (**uri_list && **uri_list != '\n' && **uri_list != '\r' && *list_len) {
		(*uri_list) ++;
		(*len) ++;
		(*list_len) --;
	}

	uri = (guchar *) g_strndup ((gchar *) begin, *len);

	while ((!**uri_list || **uri_list == '\n' || **uri_list == '\r') && *list_len) {
		(*uri_list) ++;
		(*list_len) --;
	}

	return (gchar *) uri;
}

/**
 * e_util_resize_window_for_screen:
 * @window: a #GtkWindow
 * @window_width: the @window width without @children, or -1 to compute
 * @window_height: the @window height without @children, or -1 to compute
 * @children: (element-type GtkWidget): (nullable): a #GSList with children to calculate with
 *
 * Calculates the size of the @window considering preferred sizes of @children,
 * and shrinks the @window in case it won't be completely visible on the screen
 * it is assigned to.
 *
 * Since: 3.26
 **/
void
e_util_resize_window_for_screen (GtkWindow *window,
				 gint window_width,
				 gint window_height,
				 const GSList *children)
{
	gint width = -1, height = -1, content_width = -1, content_height = -1, current_width = -1, current_height = -1;
	GtkRequisition requisition;
	const GSList *link;

	g_return_if_fail (GTK_IS_WINDOW (window));

	gtk_window_get_default_size (window, &width, &height);
	if (width < 0 || height < 0) {
		gtk_widget_get_preferred_size (GTK_WIDGET (window), &requisition, NULL);

		width = requisition.width;
		height = requisition.height;
	}

	for (link = children; link; link = g_slist_next (link)) {
		GtkWidget *widget = link->data;

		if (GTK_IS_SCROLLED_WINDOW (widget))
			widget = gtk_bin_get_child (GTK_BIN (widget));

		if (GTK_IS_VIEWPORT (widget))
			widget = gtk_bin_get_child (GTK_BIN (widget));

		if (!GTK_IS_WIDGET (widget))
			continue;

		gtk_widget_get_preferred_size (widget, &requisition, NULL);

		if (requisition.width > content_width)
			content_width = requisition.width;
		if (requisition.height > content_height)
			content_height = requisition.height;

		widget = gtk_widget_get_parent (widget);
		if (GTK_IS_VIEWPORT (widget))
			widget = gtk_widget_get_parent (widget);

		if (GTK_IS_WIDGET (widget)) {
			gtk_widget_get_preferred_size (widget, &requisition, NULL);

			if (current_width == -1 || current_width < requisition.width)
				current_width = requisition.width;
			if (current_height == -1 || current_height < requisition.height)
				current_height = requisition.height;
		}
	}

	if (content_width > 0 && content_height > 0 && width > 0 && height > 0) {
		GdkDisplay *display;
		GdkMonitor *monitor;
		GdkRectangle monitor_area;
		gint x = 0, y = 0;

		display = gtk_widget_get_display (GTK_WIDGET (window));
		gtk_window_get_position (GTK_WINDOW (window), &x, &y);

		monitor = gdk_display_get_monitor_at_point (display, x, y);
		gdk_monitor_get_workarea (monitor, &monitor_area);

		/* When the children are packed inside the window then influence the window
		   size too, thus subtract it, if possible. */
		if (window_width < 0) {
			if (current_width > 0 && current_width < width)
				width -= current_width;
		} else {
			width = window_width;
		}

		if (window_height < 0) {
			if (current_height > 0 && current_height < height)
				height -= current_height;
		} else {
			height = window_height;
		}

		if (content_width > monitor_area.width - width)
			content_width = monitor_area.width - width;

		if (content_height > monitor_area.height - height)
			content_height = monitor_area.height - height;

		if (content_width > 0 && content_height > 0)
			gtk_window_set_default_size (GTK_WINDOW (window), width + content_width, height + content_height);
	}
}

/**
 * e_util_query_ldap_root_dse_sync:
 * @host: an LDAP server host name
 * @port: an LDAP server port
 * @security: an %ESourceLDAPSecurity to use for the connection
 * @out_root_dse: (out) (transfer full): NULL-terminated array of the server root DSE-s, or %NULL on error
 * @cancellable: optional #GCancellable object, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Queries an LDAP server identified by @host and @port for supported
 * search bases and returns them as a NULL-terminated array of strings
 * at @out_root_dse. It sets @out_root_dse to NULL on error.
 * Free the returned @out_root_dse with g_strfreev() when no longer needed.
 *
 * The function fails and sets @error to G_IO_ERROR_NOT_SUPPORTED when
 * Evolution had been compiled without LDAP support.
 *
 * Returns: Whether succeeded.
 *
 * Since: 3.28
 **/
gboolean
e_util_query_ldap_root_dse_sync (const gchar *host,
				 guint16 port,
				 ESourceLDAPSecurity security,
				 gchar ***out_root_dse,
				 GCancellable *cancellable,
				 GError **error)
{
#ifdef HAVE_LDAP
	G_LOCK_DEFINE_STATIC (ldap);
	LDAP *ldap = NULL;
	LDAPMessage *result = NULL;
	struct timeval timeout = { 0, };
	gchar **values = NULL, **root_dse;
	gint ldap_error;
	gint option;
	gint version;
	gint ii;
	const gchar *attrs[] = { "namingContexts", NULL };

	g_return_val_if_fail (host && *host, FALSE);
	g_return_val_if_fail (port > 0, FALSE);
	g_return_val_if_fail (out_root_dse != NULL, FALSE);

	*out_root_dse = NULL;

	timeout.tv_sec = 5;
	timeout.tv_usec = 0;

	G_LOCK (ldap);

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		goto exit;

	ldap = ldap_init (host, port);
	if (!ldap) {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
			_("This address book server might be unreachable or the server name may be misspelled or your network connection could be down."));
		goto exit;
	}

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		goto exit;

	version = LDAP_VERSION3;
	option = LDAP_OPT_PROTOCOL_VERSION;
	ldap_error = ldap_set_option (ldap, option, &version);
	if (ldap_error != LDAP_OPT_SUCCESS) {
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_INITIALIZED,
			_("Failed to set protocol version to LDAPv3 (%d): %s"), ldap_error,
			ldap_err2string (ldap_error) ? ldap_err2string (ldap_error) : _("Unknown error"));
		goto exit;
	}

	ldap_error = ldap_set_option (ldap, LDAP_OPT_NETWORK_TIMEOUT, &timeout);
	if (ldap_error != LDAP_OPT_SUCCESS) {
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_INITIALIZED,
			_("Failed to set connection timeout option (%d): %s"), ldap_error,
			ldap_err2string (ldap_error) ? ldap_err2string (ldap_error) : _("Unknown error"));
		goto exit;
	}

	ldap_error = ldap_set_option (ldap, LDAP_OPT_TIMEOUT, &timeout);
	if (ldap_error != LDAP_OPT_SUCCESS) {
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_INITIALIZED,
			_("Failed to set connection timeout option (%d): %s"), ldap_error,
			ldap_err2string (ldap_error) ? ldap_err2string (ldap_error) : _("Unknown error"));
		goto exit;
	}

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		goto exit;

	if (security == E_SOURCE_LDAP_SECURITY_LDAPS) {
#ifdef SUNLDAP
		if (ldap_error == LDAP_SUCCESS) {
			ldap_set_option (ldap, LDAP_OPT_RECONNECT, LDAP_OPT_ON );
		}
#else
#if defined (LDAP_OPT_X_TLS_HARD) && defined (LDAP_OPT_X_TLS)
		gint tls_level = LDAP_OPT_X_TLS_HARD;
		ldap_set_option (ldap, LDAP_OPT_X_TLS, &tls_level);

		/* setup this on the global option set */
		tls_level = LDAP_OPT_X_TLS_ALLOW;
		ldap_set_option (NULL, LDAP_OPT_X_TLS_REQUIRE_CERT, &tls_level);
#elif defined (G_OS_WIN32)
		ldap_set_option (ldap, LDAP_OPT_SSL, LDAP_OPT_ON);
#endif
#endif
	} else if (security == E_SOURCE_LDAP_SECURITY_STARTTLS) {
#ifdef SUNLDAP
		if (ldap_error == LDAP_SUCCESS) {
			ldap_set_option (ldap, LDAP_OPT_RECONNECT, LDAP_OPT_ON);
		}
#else
		ldap_error = ldap_start_tls_s (ldap, NULL, NULL);
#endif
		if (ldap_error != LDAP_SUCCESS) {
			g_set_error (error, G_IO_ERROR, G_IO_ERROR_CONNECTION_REFUSED,
				_("Failed to use STARTTLS (%d): %s"), ldap_error,
				ldap_err2string (ldap_error) ? ldap_err2string (ldap_error) : _("Unknown error"));
			goto exit;
		}
	}

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		goto exit;

	/* FIXME Use the user's actual authentication settings. */
	ldap_error = ldap_simple_bind_s (ldap, NULL, NULL);
	if (ldap_error != LDAP_SUCCESS) {
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
			_("Failed to authenticate with LDAP server (%d): %s"), ldap_error,
			ldap_err2string (ldap_error) ? ldap_err2string (ldap_error) : _("Unknown error"));
		goto exit;
	}

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		goto exit;

	ldap_error = ldap_search_ext_s (
		ldap, LDAP_ROOT_DSE, LDAP_SCOPE_BASE,
		"(objectclass=*)", (gchar **) attrs, 0,
		NULL, NULL, &timeout, LDAP_NO_LIMIT, &result);
	if (ldap_error != LDAP_SUCCESS) {
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
			_("This LDAP server may use an older version of LDAP, which does not support this functionality or it may be misconfigured. Ask your administrator for supported search bases.\n\nDetailed error (%d): %s"),
			ldap_error, ldap_err2string (ldap_error) ? ldap_err2string (ldap_error) : _("Unknown error"));
		goto exit;
	}

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		goto exit;

	values = ldap_get_values (ldap, result, "namingContexts");
	if (values == NULL || values[0] == NULL || *values[0] == '\0') {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
			_("This LDAP server may use an older version of LDAP, which does not support this functionality or it may be misconfigured. Ask your administrator for supported search bases."));
		goto exit;
	}

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		goto exit;

	root_dse = g_new0 (gchar *, g_strv_length (values) + 1);

	for (ii = 0; values[ii]; ii++) {
		root_dse[ii] = g_strdup (values[ii]);
	}

	root_dse[ii] = NULL;

	*out_root_dse = root_dse;

 exit:
	if (values)
		ldap_value_free (values);

	if (result)
		ldap_msgfree (result);

	if (ldap)
		ldap_unbind_s (ldap);

	G_UNLOCK (ldap);

	return *out_root_dse != NULL;

#else /* HAVE_LDAP */
	g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
		_("Evolution had not been compiled with LDAP support"));

	return FALSE;
#endif
}

G_LOCK_DEFINE_STATIC (global_tables_lock);
static GHashTable *pixbufs_table = NULL; /* gchar *filename ~> GdkPixbuf * */
static GHashTable *iso_639_table = NULL;
static GHashTable *iso_3166_table = NULL;

#ifdef HAVE_ISO_CODES

#define ISOCODESLOCALEDIR ISO_CODES_PREFIX "/share/locale"

#ifdef G_OS_WIN32
#ifdef DATADIR
#undef DATADIR
#endif
#include <shlobj.h>

static gchar *
_get_iso_codes_prefix (void)
{
	static gchar retval[1000];
	static gint beenhere = 0;
	gchar *temp_dir = 0;

	if (beenhere)
		return retval;

	if (!(temp_dir = g_win32_get_package_installation_directory_of_module (_e_get_dll_hmodule ()))) {
		strcpy (retval, ISO_CODES_PREFIX);
		return retval;
	}

	strcpy (retval, temp_dir);
	g_free (temp_dir);
	beenhere = 1;
	return retval;
}

static gchar *
_get_isocodeslocaledir (void)
{
	static gchar retval[1000];
	static gint beenhere = 0;

	if (beenhere)
		return retval;

	g_snprintf (retval, sizeof (retval), "%s\\share\\locale", _get_iso_codes_prefix ());
	beenhere = 1;
	return retval;
}

#undef ISO_CODES_PREFIX
#define ISO_CODES_PREFIX _get_iso_codes_prefix ()

#undef ISOCODESLOCALEDIR
#define ISOCODESLOCALEDIR _get_isocodeslocaledir ()

#endif

#define ISO_639_DOMAIN	"iso_639"
#define ISO_3166_DOMAIN	"iso_3166"

static void
iso_639_start_element (GMarkupParseContext *context,
                       const gchar *element_name,
                       const gchar **attribute_names,
                       const gchar **attribute_values,
                       gpointer data,
                       GError **error)
{
	GHashTable *hash_table = data;
	const gchar *iso_639_1_code = NULL;
	const gchar *iso_639_2_code = NULL;
	const gchar *name = NULL;
	const gchar *code = NULL;
	gint ii;

	if (g_strcmp0 (element_name, "iso_639_entry") != 0) {
		return;
	}

	for (ii = 0; attribute_names[ii] != NULL; ii++) {
		if (strcmp (attribute_names[ii], "name") == 0)
			name = attribute_values[ii];
		else if (strcmp (attribute_names[ii], "iso_639_1_code") == 0)
			iso_639_1_code = attribute_values[ii];
		else if (strcmp (attribute_names[ii], "iso_639_2T_code") == 0)
			iso_639_2_code = attribute_values[ii];
	}

	code = (iso_639_1_code != NULL) ? iso_639_1_code : iso_639_2_code;

	if (code != NULL && *code != '\0' && name != NULL && *name != '\0')
		g_hash_table_insert (
			hash_table, g_strdup (code),
			g_strdup (dgettext (ISO_639_DOMAIN, name)));
}

static void
iso_3166_start_element (GMarkupParseContext *context,
                        const gchar *element_name,
                        const gchar **attribute_names,
                        const gchar **attribute_values,
                        gpointer data,
                        GError **error)
{
	GHashTable *hash_table = data;
	const gchar *name = NULL;
	const gchar *code = NULL;
	gint ii;

	if (strcmp (element_name, "iso_3166_entry") != 0)
		return;

	for (ii = 0; attribute_names[ii] != NULL; ii++) {
		if (strcmp (attribute_names[ii], "name") == 0)
			name = attribute_values[ii];
		else if (strcmp (attribute_names[ii], "alpha_2_code") == 0)
			code = attribute_values[ii];
	}

	if (code != NULL && *code != '\0' && name != NULL && *name != '\0')
		g_hash_table_insert (
			hash_table, g_ascii_strdown (code, -1),
			g_strdup (dgettext (ISO_3166_DOMAIN, name)));
}

static GMarkupParser iso_639_parser = {
	iso_639_start_element,
	NULL, NULL, NULL, NULL
};

static GMarkupParser iso_3166_parser = {
	iso_3166_start_element,
	NULL, NULL, NULL, NULL
};

static void
iso_codes_parse (const GMarkupParser *parser,
                 const gchar *basename,
                 GHashTable *hash_table)
{
	GMappedFile *mapped_file;
	gchar *filename;
	GError *error = NULL;

	filename = g_build_filename (
		ISO_CODES_PREFIX, "share", "xml",
		"iso-codes", basename, NULL);
	mapped_file = g_mapped_file_new (filename, FALSE, &error);
	g_free (filename);

	if (mapped_file != NULL) {
		GMarkupParseContext *context;
		const gchar *contents;
		gsize length;

		context = g_markup_parse_context_new (
			parser, 0, hash_table, NULL);
		contents = g_mapped_file_get_contents (mapped_file);
		length = g_mapped_file_get_length (mapped_file);
		g_markup_parse_context_parse (
			context, contents, length, &error);
		g_markup_parse_context_free (context);
#if GLIB_CHECK_VERSION(2,21,3)
		g_mapped_file_unref (mapped_file);
#else
		g_mapped_file_free (mapped_file);
#endif
	}

	if (error != NULL) {
		g_warning ("%s: %s", basename, error->message);
		g_error_free (error);
	}
}

#endif /* HAVE_ISO_CODES */

/**
 * e_util_get_language_info:
 * @language_tag: Language tag to get its name for, like "en_US"
 * @out_language_name: (out) (nullable) (transfer full): Return location for the language name, or %NULL
 * @out_country_name: (out) (nullable) (transfer full): Return location for the country name, or %NULL
 *
 * Splits language tag into a localized language name and country name (the variant).
 * The @out_language_name is always filled when the function returns %TRUE, but
 * the @out_countr_name can be %NULL. That's for cases when the @language_tag
 * contains only the country part, like "en".
 *
 * The function returns %FALSE when it could not decode language name from
 * the given @language_tag. When either of the @out_language_name and @out_country_name
 * is non-NULL and the function returns %TRUE, then their respective values
 * should be freed with g_free(), when no longer needed.
 *
 * Returns: %TRUE, when could get at least language name from the @language_tag,
 *    %FALSE otherwise.
 *
 * Since: 3.32
 **/
gboolean
e_util_get_language_info (const gchar *language_tag,
			  gchar **out_language_name,
			  gchar **out_country_name)
{
	const gchar *iso_639_name;
	const gchar *iso_3166_name;
	gchar *lowercase;
	gchar **tokens;

	g_return_val_if_fail (language_tag != NULL, FALSE);

	if (out_language_name)
		*out_language_name = NULL;
	if (out_country_name)
		*out_country_name = NULL;

	/* Split language code into lowercase tokens. */
	lowercase = g_ascii_strdown (language_tag, -1);
	tokens = g_strsplit (lowercase, "_", -1);
	g_free (lowercase);

	g_return_val_if_fail (tokens != NULL, FALSE);

	if (!iso_639_table && !iso_3166_table) {
#if defined (ENABLE_NLS) && defined (HAVE_ISO_CODES)
		bindtextdomain (ISO_639_DOMAIN, ISOCODESLOCALEDIR);
		bind_textdomain_codeset (ISO_639_DOMAIN, "UTF-8");

		bindtextdomain (ISO_3166_DOMAIN, ISOCODESLOCALEDIR);
		bind_textdomain_codeset (ISO_3166_DOMAIN, "UTF-8");
#endif /* ENABLE_NLS && HAVE_ISO_CODES */

		iso_639_table = g_hash_table_new_full (
			(GHashFunc) g_str_hash,
			(GEqualFunc) g_str_equal,
			(GDestroyNotify) g_free,
			(GDestroyNotify) g_free);

		iso_3166_table = g_hash_table_new_full (
			(GHashFunc) g_str_hash,
			(GEqualFunc) g_str_equal,
			(GDestroyNotify) g_free,
			(GDestroyNotify) g_free);

#ifdef HAVE_ISO_CODES
		iso_codes_parse (
			&iso_639_parser, "iso_639.xml", iso_639_table);
		iso_codes_parse (
			&iso_3166_parser, "iso_3166.xml", iso_3166_table);
#endif /* HAVE_ISO_CODES */
	}

	iso_639_name = g_hash_table_lookup (iso_639_table, tokens[0]);

	if (!iso_639_name) {
		g_strfreev (tokens);
		return FALSE;
	}

	if (out_language_name)
		*out_language_name = g_strdup (iso_639_name);

	if (g_strv_length (tokens) < 2)
		goto exit;

	if (out_country_name) {
		iso_3166_name = g_hash_table_lookup (iso_3166_table, tokens[1]);

		if (iso_3166_name)
			*out_country_name = g_strdup (iso_3166_name);
		else
			*out_country_name = g_strdup (tokens[1]);
	}

 exit:
	if (out_language_name && *out_language_name) {
		gchar *ptr;

		/* When the language name has ';' then strip the string at it */
		ptr = strchr (*out_language_name, ';');
		if (ptr)
			*ptr = '\0';
	}

	if (out_country_name && *out_country_name) {
		gchar *ptr;

		/* When the country name has two or more ';' then strip the string at the second of them */
		ptr = strchr (*out_country_name, ';');
		if (ptr)
			ptr = strchr (ptr + 1, ';');
		if (ptr)
			*ptr = '\0';
	}

	g_strfreev (tokens);

	return TRUE;
}

/**
 * e_util_get_language_name:
 * @language_tag: Language tag to get its name for, like "en_US"
 *
 * Returns: (transfer full): Newly allocated string with localized language name
 *
 * Since: 3.32
 **/
gchar *
e_util_get_language_name (const gchar *language_tag)
{
	gchar *language_name = NULL, *country_name = NULL;
	gchar *res;

	g_return_val_if_fail (language_tag != NULL, NULL);

	if (!e_util_get_language_info (language_tag, &language_name, &country_name)) {
		return g_strdup_printf (
			/* Translators: %s is the language ISO code. */
			C_("language", "Unknown (%s)"), language_tag);
	}

	if (!country_name)
		return language_name;

	res = g_strdup_printf (
		/* Translators: The first %s is the language name, and the
		 * second is the country name. Example: "French (France)" */
		C_("language", "%s (%s)"), language_name, country_name);

	g_free (language_name);
	g_free (country_name);

	return res;
}

/**
 * e_misc_util_free_global_memory:
 *
 * Frees global memory allocated by evolution-util library.
 * This is usually called at the end of the application.
 *
 * Since: 3.32
 **/
void
e_misc_util_free_global_memory (void)
{
	G_LOCK (global_tables_lock);
	g_clear_pointer (&iso_639_table, g_hash_table_destroy);
	g_clear_pointer (&iso_3166_table, g_hash_table_destroy);
	g_clear_pointer (&pixbufs_table, g_hash_table_destroy);
	G_UNLOCK (global_tables_lock);

	e_util_cleanup_settings ();
	e_spell_checker_free_global_memory ();
	e_simple_async_result_free_global_memory ();
}

/**
 * e_misc_util_ref_pixbuf:
 * @filename: a pixbuf file name to load
 * @error: return location to store a #GError on failure, or %NULL
 *
 * Loads @filename as a #GdkPixbuf and cached it in case it's needed
 * again, without a need to load it repeatedly.
 *
 * Returns: (transfer full) (nullable): a #GdkPixbuf loaded from the @filename, or %NULL on error
 *
 * Since: 3.50
 **/
GdkPixbuf *
e_misc_util_ref_pixbuf (const gchar *filename,
			GError **error)
{
	GdkPixbuf *pixbuf;

	g_return_val_if_fail (filename != NULL, NULL);

	G_LOCK (global_tables_lock);

	if (!pixbufs_table)
		pixbufs_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

	pixbuf = g_hash_table_lookup (pixbufs_table, filename);
	if (pixbuf) {
		g_object_ref (pixbuf);
	} else {
		pixbuf = gdk_pixbuf_new_from_file (filename, error);

		if (pixbuf)
			g_hash_table_insert (pixbufs_table, g_strdup (filename), g_object_ref (pixbuf));
	}

	G_UNLOCK (global_tables_lock);

	return pixbuf;
}

/**
 * e_util_can_preview_filename:
 * @filename: (nullable): a file name to test
 *
 * Returns: Whether the @filename can be used to create a preview
 *   in GtkFileChooser and such widgets. For example directories,
 *   pipes and sockets cannot by used.
 *
 * Since: 3.32
 **/
gboolean
e_util_can_preview_filename (const gchar *filename)
{
	GStatBuf st;

	if (!filename || !*filename)
		return FALSE;

	return g_stat (filename, &st) == 0
		&& !S_ISDIR (st.st_mode)
		#ifdef S_ISFIFO
		&& !S_ISFIFO (st.st_mode)
		#endif
		#ifdef S_ISSOCK
		&& !S_ISSOCK (st.st_mode)
		#endif
		;
}

/**
 * e_util_markup_append_escaped:
 * @buffer: a #GString buffer to append escaped text to
 * @format: printf-like format of the string to append
 * @...: arguments for the format
 *
 * Appends escaped markup text into @buffer. This function is
 * similar to g_markup_printf_escaped(), except it appends
 * the escaped text into a #GString.
 *
 * Since: 3.36
 **/
void
e_util_markup_append_escaped (GString *buffer,
			      const gchar *format,
			      ...)
{
	va_list va;
	gchar *escaped;

	g_return_if_fail (buffer != NULL);
	g_return_if_fail (format != NULL);

	va_start (va, format);
	escaped = g_markup_vprintf_escaped (format, va);
	va_end (va);

	g_string_append (buffer, escaped);

	g_free (escaped);
}

/**
 * e_util_markup_append_escaped_text:
 * @buffer: a #GString buffer to append escaped text to
 * @text: a text to escape and append to the buffer
 *
 * Markup-escapes @text and appends it to @buffer.
 *
 * Since: 3.46
 **/
void
e_util_markup_append_escaped_text (GString *buffer,
				   const gchar *text)
{
	gchar *escaped;

	g_return_if_fail (buffer != NULL);

	if (!text || !*text)
		return;

	escaped = g_markup_escape_text (text, -1);

	g_string_append (buffer, escaped);

	g_free (escaped);
}

void
e_util_enum_supported_locales (void)
{
	GString *locale;
	gchar *previous_locale;
	gint ii, category = LC_ALL;

	#if defined(LC_MESSAGES)
	category = LC_MESSAGES;
	#endif

	previous_locale = g_strdup (setlocale (category, NULL));

	locale = g_string_sized_new (32);

	for (ii = 0; e_supported_locales[ii].code; ii++) {
		gchar *catalog_filename;

		catalog_filename = g_build_filename (EVOLUTION_LOCALEDIR, e_supported_locales[ii].code, "LC_MESSAGES", GETTEXT_PACKAGE ".mo", NULL);

		if (catalog_filename && g_file_test (catalog_filename, G_FILE_TEST_EXISTS)) {
			g_string_printf (locale, "%s.UTF-8", e_supported_locales[ii].locale);

			if (!setlocale (category, locale->str)) {
				e_supported_locales[ii].locale = NULL;
			}
		} else {
			e_supported_locales[ii].locale = NULL;
		}

		g_free (catalog_filename);
	}

	setlocale (category, previous_locale);

	g_string_free (locale, TRUE);
	g_free (previous_locale);
}

const ESupportedLocales *
e_util_get_supported_locales (void)
{
	return e_supported_locales;
}

gchar *
e_util_get_uri_tooltip (const gchar *uri)
{
	CamelInternetAddress *address;
	CamelURL *curl;
	const gchar *format = NULL;
	GString *message = NULL;
	gchar *who;

	if (!uri || !*uri)
		goto exit;

	if (g_str_has_prefix (uri, "mailto:"))
		format = _("Click to mail %s");
	else if (g_str_has_prefix (uri, "callto:") ||
		 g_str_has_prefix (uri, "h323:") ||
		 g_str_has_prefix (uri, "sip:") ||
		 g_str_has_prefix (uri, "tel:"))
		format = _("Click to call %s");
	else if (g_str_has_prefix (uri, "##"))
		message = g_string_new (_("Click to hide/unhide addresses"));
	else if (g_str_has_prefix (uri, "mail:")) {
		const gchar *fragment;
		GUri *guri;

		guri = g_uri_parse (uri, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);
		if (!guri)
			goto exit;

		message = g_string_new (NULL);
		fragment = g_uri_get_fragment (guri);

		if (fragment && *fragment)
			g_string_append_printf (message, _("Go to the section %s of the message"), fragment);
		else
			g_string_append (message, _("Go to the beginning of the message"));

		g_uri_unref (guri);
	} else {
		message = g_string_new (NULL);

		g_string_append_printf (message, _("Click to open %s"), uri);
	}

	if (!format)
		goto exit;

	/* XXX Use something other than Camel here.  Surely
	 *     there's other APIs around that can do this. */
	curl = camel_url_new (uri, NULL);
	address = camel_internet_address_new ();
	camel_address_decode (CAMEL_ADDRESS (address), curl->path);
	camel_internet_address_sanitize_ascii_domain (address);
	who = camel_address_format (CAMEL_ADDRESS (address));

	if (!who && g_str_has_prefix (uri, "mailto:") && curl->query && *curl->query) {
		GHashTable *query;

		query = soup_form_decode (curl->query);
		if (query) {
			const gchar *to = g_hash_table_lookup (query, "to");
			if (to && *to) {
				camel_address_decode (CAMEL_ADDRESS (address), to);
				camel_internet_address_sanitize_ascii_domain (address);
				who = camel_address_format (CAMEL_ADDRESS (address));
			}
			g_hash_table_destroy (query);
		}
	}

	g_object_unref (address);
	camel_url_free (curl);

	if (!who) {
		who = g_strdup (strchr (uri, ':') + 1);
		camel_url_decode (who);
	}

	message = g_string_new (NULL);

	g_string_append_printf (message, format, who);

	g_free (who);

 exit:

	if (!message)
		return NULL;

	/* This limits the chars that appear as in some
	   links the size of chars can extend out of the screen */
	if (g_utf8_strlen (message->str, -1) > 150) {
		gchar *pos;

		pos = g_utf8_offset_to_pointer (message->str, 150);
		g_string_truncate (message, pos - message->str);
		g_string_append (message , _("…"));
	}

	return g_string_free (message, FALSE);
}

void
e_util_ensure_scrolled_window_height (GtkScrolledWindow *scrolled_window)
{
	GtkWidget *toplevel;
	GdkDisplay *display;
	GdkMonitor *monitor;
	GdkRectangle workarea;
	gint toplevel_height, scw_height, require_scw_height = 0, max_height;

	g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));

	toplevel = gtk_widget_get_ancestor (GTK_WIDGET (scrolled_window), GTK_TYPE_WINDOW);
	if (!toplevel)
		return;

	scw_height = gtk_widget_get_allocated_height (GTK_WIDGET (scrolled_window));

	gtk_widget_get_preferred_height_for_width (gtk_bin_get_child (GTK_BIN (scrolled_window)),
		gtk_widget_get_allocated_width (GTK_WIDGET (scrolled_window)),
		&require_scw_height, NULL);

	if (scw_height >= require_scw_height) {
		if (require_scw_height > 0)
			gtk_scrolled_window_set_min_content_height (scrolled_window, require_scw_height);
		return;
	}

	if (!gtk_widget_get_window (toplevel))
		return;

	display = gtk_widget_get_display (toplevel);
	monitor = gdk_display_get_monitor_at_window (display, gtk_widget_get_window (toplevel));
	gdk_monitor_get_workarea (monitor, &workarea);

	/* can enlarge up to 4 / 5 monitor's work area height */
	max_height = workarea.height * 4 / 5;

	toplevel_height = gtk_widget_get_allocated_height (toplevel);
	if (toplevel_height + require_scw_height - scw_height > max_height)
		return;

	gtk_scrolled_window_set_min_content_height (scrolled_window, require_scw_height);
}

void
e_util_make_safe_filename (gchar *filename)
{
	const gchar *unsafe_chars = "/\\";
	GSettings *settings;
	gchar *pp, *ts, *illegal_chars;
	gunichar cc;

	g_return_if_fail (filename != NULL);

	settings = e_util_ref_settings ("org.gnome.evolution.shell");
	illegal_chars = g_settings_get_string (settings, "filename-illegal-chars");
	g_clear_object (&settings);

	pp = filename;

	while (pp && *pp) {
		cc = g_utf8_get_char (pp);
		ts = pp;
		pp = g_utf8_next_char (pp);

		if (!g_unichar_isprint (cc) ||
		    (cc < 0xff && (strchr (unsafe_chars, cc & 0xff) ||
		     (illegal_chars && *illegal_chars && strchr (illegal_chars, cc & 0xff))))) {
			while (ts < pp)
				*ts++ = '_';
		}
	}

	g_free (illegal_chars);
}

gboolean
e_util_setup_toolbar_icon_size (GtkToolbar *toolbar,
				GtkIconSize default_size)
{
	GSettings *settings;
	EToolbarIconSize icon_size;

	g_return_val_if_fail (GTK_IS_TOOLBAR (toolbar), FALSE);

	settings = e_util_ref_settings ("org.gnome.evolution.shell");
	icon_size = g_settings_get_enum (settings, "toolbar-icon-size");
	g_object_unref (settings);

	if (icon_size == E_TOOLBAR_ICON_SIZE_SMALL)
		gtk_toolbar_set_icon_size (toolbar, GTK_ICON_SIZE_SMALL_TOOLBAR);
	else if (icon_size == E_TOOLBAR_ICON_SIZE_LARGE)
		gtk_toolbar_set_icon_size (toolbar, GTK_ICON_SIZE_LARGE_TOOLBAR);
	else if (default_size != GTK_ICON_SIZE_INVALID && e_util_get_use_header_bar ())
		gtk_toolbar_set_icon_size (toolbar, default_size);

	return icon_size == E_TOOLBAR_ICON_SIZE_SMALL ||
	       icon_size == E_TOOLBAR_ICON_SIZE_LARGE;
}

gboolean
e_util_get_use_header_bar (void)
{
	static gchar use_header_bar = -1;

	if (use_header_bar == -1) {
		GSettings *settings;

		settings = e_util_ref_settings ("org.gnome.evolution.shell");
		use_header_bar = g_settings_get_boolean (settings, "use-header-bar") ? 1 : 0;
		g_object_unref (settings);
	}

	return use_header_bar != 0;
}

void
e_open_map_uri (GtkWindow *parent,
		const gchar *location)
{
	#define GOOGLE_MAP_PREFIX "https://maps.google.com?q="
	#define OPENSTREETMAP_PREFIX "https://www.openstreetmap.org/search?query="

	GSettings *settings;
	gboolean open_map_prefer_local;
	gchar *open_map_target;
	gchar *uri;
	const gchar *prefix = NULL;

	g_return_if_fail (location != NULL);

	settings = e_util_ref_settings ("org.gnome.evolution.addressbook");
	open_map_target = g_settings_get_string (settings, "open-map-target");
	open_map_prefer_local = g_settings_get_boolean (settings, "open-map-prefer-local");
	g_object_unref (settings);

	/* Cannot check what apps are installed in the system when running in a Flatpak sandbox */
	if (open_map_prefer_local && !e_util_is_running_flatpak ()) {
		GAppInfo *app_info;

		app_info = g_app_info_get_default_for_uri_scheme ("maps");
		if (app_info) {
			prefix = "maps:q=";
			g_object_unref (app_info);
		}
	}

	if (!prefix) {
		if (open_map_target && g_ascii_strcasecmp (open_map_target, "google") == 0) {
			prefix = GOOGLE_MAP_PREFIX;
		} else {
			prefix = OPENSTREETMAP_PREFIX;
		}
	}

	g_free (open_map_target);

	uri = g_strconcat (prefix, location, NULL);
	e_show_uri (parent, uri);
	g_free (uri);
}

static gboolean
e_util_links_similar (const gchar *href1,
		      const gchar *href2)
{
	gsize href1_len, href2_len;
	gboolean similar;

	if (!href1 || !*href1 || !href2 || !*href2)
		return FALSE;

	if (g_ascii_strcasecmp (href1, href2) == 0)
		return TRUE;

	href1_len = strlen (href1);
	href2_len = strlen (href2);

	similar = (href1_len + 1 == href2_len &&
		g_str_has_prefix (href2, href1) &&
		href2[href2_len - 1] == '/') ||
	       (href1_len == href2_len + 1 &&
		g_str_has_prefix (href1, href2) &&
		href1[href1_len - 1] == '/');

	if (!similar && (strchr (href1, '%') || strchr (href2, '%'))) {
		gchar *decoded_href1, *decoded_href2;

		decoded_href1 = g_uri_unescape_string (href1, NULL);
		decoded_href2 = g_uri_unescape_string (href2, NULL);

		if (decoded_href1 && decoded_href2) {
			similar = g_ascii_strcasecmp (decoded_href1, decoded_href2) == 0;

			if (!similar) {
				href1_len = strlen (decoded_href1);
				href2_len = strlen (decoded_href2);

				similar = (href1_len + 1 == href2_len &&
					g_str_has_prefix (decoded_href2, decoded_href1) &&
					decoded_href2[href2_len - 1] == '/') ||
				       (href1_len == href2_len + 1 &&
					g_str_has_prefix (decoded_href1, decoded_href2) &&
					decoded_href1[href1_len - 1] == '/');
			}
		}

		g_free (decoded_href1);
		g_free (decoded_href2);
	}

	return similar;
}

static gboolean
e_util_is_supported_scheme (const gchar *href,
			    guint *out_scheme_len)
{
	const gchar *schemes[] = { "http:", "https:" };
	guint ii;

	for (ii = 0; ii < G_N_ELEMENTS (schemes); ii++) {
		const gchar *scheme = schemes[ii];
		guint scheme_len = strlen (scheme);

		if (g_ascii_strncasecmp (href, scheme, scheme_len) == 0) {
			if (out_scheme_len)
				*out_scheme_len = scheme_len;

			return TRUE;
		}
	}

	return FALSE;
}

static const gchar *
e_util_skip_scheme (const gchar *href)
{
	guint scheme_len = 0;

	if (!href || !*href)
		return href;

	if (e_util_is_supported_scheme (href, &scheme_len)) {
		href += scheme_len;

		if (g_str_has_prefix (href, "//"))
			href += 2;

		return href;
	}

	return href;
}

/**
 * e_util_link_requires_reference:
 * @href: link href, aka URL
 * @text: link text
 *
 * Checks whether the link's @href and the @text differ in a way that
 * they require a reference when converting it from HTML to text. Some
 * protocols can be completely ignored.
 *
 * Returns: whether requires the reference
 *
 * Since: 3.52
 **/
gboolean
e_util_link_requires_reference (const gchar *href,
				const gchar *text)
{
	gboolean similar;

	if (!href || !*href || !text || !*text)
		return FALSE;

	if (!e_util_is_supported_scheme (href, NULL))
		return FALSE;

	similar = e_util_links_similar (href, text);

	if (!similar)
		similar = e_util_links_similar (e_util_skip_scheme (href), e_util_skip_scheme (text));

	return !similar;
}

/**
 * e_util_call_malloc_trim_limited:
 *
 * Calls e_util_call_malloc_trim(), but not more often than once per 30 minutes.
 *
 * Since: 3.52
 **/
void
e_util_call_malloc_trim_limited (void)
{
	static gint64 last_called = 0;
	gint64 now = g_get_real_time ();

	if (now >= last_called + (30 * 60 * G_USEC_PER_SEC)) {
		last_called = now;
		e_util_call_malloc_trim ();
	}
}

static gboolean
e_util_detach_menu_on_idle_cb (gpointer user_data)
{
	GtkMenu *menu = user_data;

	/* it can be NULL when the menu is closed by clicking on a menu item and
	   non-NULL when the menu is dismissed without picking any item */
	if (gtk_menu_get_attach_widget (menu))
		gtk_menu_detach (menu);

	return G_SOURCE_REMOVE;
}

static void
e_util_menu_deactivate_cb (GtkMenu *menu,
			   gpointer user_data)
{
	g_return_if_fail (GTK_IS_MENU (menu));

	g_signal_handlers_disconnect_by_func (menu, e_util_menu_deactivate_cb, user_data);

	g_idle_add_full (G_PRIORITY_LOW, e_util_detach_menu_on_idle_cb, g_object_ref (menu), g_object_unref);
}

/**
 * e_util_connect_menu_detach_after_deactivate:
 * @menu: a #GtkMenu
 *
 * Connects a signal handler on a "deactivate" signal of the @menu and
 * calls gtk_menu_detach() after the handler is invoked, which can cause
 * destroy of the @menu. The @menu should be already attached to a widget.
 *
 * As the #GAction-s are not executed immediately by the GTK, the detach can be
 * called only later, not in the deactivate signal handler. This function makes
 * it simpler and consistent to detach (and possibly free) the @menu after the user
 * dismisses it either by clicking elsewhere or by picking an item from it.
 *
 * Since: 3.56
 **/
void
e_util_connect_menu_detach_after_deactivate (GtkMenu *menu)
{
	g_return_if_fail (GTK_IS_MENU (menu));
	g_return_if_fail (gtk_menu_get_attach_widget (menu) != NULL);

	g_signal_connect (
		menu, "deactivate",
		G_CALLBACK (e_util_menu_deactivate_cb), NULL);
}

/**
 * e_util_ignore_accel_for_focused:
 * @focused: (nullable): a focused #GtkWidget, or %NULL
 *
 * Returns whether an accel key press should be ignored, due to the @focused
 * might use it. Returns %FALSE, when the @focused is %NULL.
 *
 * Returns: whether an accel key press should be ignored
 *
 * Since: 3.56
 **/
gboolean
e_util_ignore_accel_for_focused (GtkWidget *focused)
{
	if (!focused)
		return FALSE;

	if ((GTK_IS_ENTRY (focused) || GTK_IS_EDITABLE (focused) ||
	    (GTK_IS_TREE_VIEW (focused) && gtk_tree_view_get_search_column (GTK_TREE_VIEW (focused)) >= 0))) {
		GdkEvent *event;
		gboolean ignore = TRUE;

		event = gtk_get_current_event ();
		if (event) {
			GdkModifierType modifs = 0;
			guint keyval = 0;
			gboolean can_process;

			/* multi-key presses are always allowed;
			   the GDK_SHIFT_MASK can be used to type numbers and such,
			   thus do not add it to the list of the modifiers */
			can_process = gdk_event_get_state (event, &modifs) &&
				(modifs & (GDK_CONTROL_MASK | GDK_MOD1_MASK)) != 0;

			if (!can_process && gdk_event_get_keyval (event, &keyval)) {
				/* function keys are allowed (they are used as shortcuts in the mail view) */
				can_process = keyval == GDK_KEY_F1 || keyval == GDK_KEY_F2 || keyval == GDK_KEY_F3 || keyval == GDK_KEY_F4 ||
					keyval == GDK_KEY_F5 || keyval == GDK_KEY_F6 || keyval == GDK_KEY_F7 || keyval == GDK_KEY_F8 ||
					keyval == GDK_KEY_F9 || keyval == GDK_KEY_F10 || keyval == GDK_KEY_F11 || keyval == GDK_KEY_F12;
			}

			g_clear_pointer (&event, gdk_event_free);

			ignore = !can_process;
		}

		return ignore;
	}

	return FALSE;
}

/**
 * e_util_is_dark_theme:
 * @widget: a #GtkWidget
 *
 * Check whether the current color theme is a dark theme.
 * It's determined from the @widget's style context.
 *
 * Returns: whether the current color theme is the dark theme
 *
 * Since: 3.58
 **/
gboolean
e_util_is_dark_theme (GtkWidget *widget)
{
	GtkStyleContext *style_context;
	GdkRGBA rgba;
	gdouble brightness;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

	style_context = gtk_widget_get_style_context (widget);
	gtk_style_context_get_color (style_context, gtk_style_context_get_state (style_context), &rgba);

	brightness =
		(0.2109 * 255.0 * rgba.red) +
		(0.5870 * 255.0 * rgba.green) +
		(0.1021 * 255.0 * rgba.blue);

	return brightness > 140;
}
