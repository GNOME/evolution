/*
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
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

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
#include <libgnome/gnome-init.h>

#include <camel/camel-url.h>

#ifdef G_OS_WIN32
#include <windows.h>
#endif

#include <libedataserver/e-data-server-util.h>
#include <libedataserver/e-categories.h>
#include "filter/filter-option.h"
#include "e-util.h"
#include "e-util-private.h"

/**
 * e_get_user_data_dir:
 *
 * Returns the base directory for Evolution-specific user data.
 * The string is owned by Evolution and must not be modified or freed.
 *
 * Returns: base directory for user data
 **/
const gchar *
e_get_user_data_dir (void)
{
	static gchar *dirname = NULL;

	if (G_UNLIKELY (dirname == NULL))
		dirname = g_build_filename (
			g_get_home_dir (), ".evolution", NULL);

	return dirname;
}

/**
 * e_get_accels_filename:
 *
 * Returns the name of the user data file containing custom keyboard
 * accelerator specifications.
 *
 * Returns: filename for accelerator specifications
 **/
const gchar *
e_get_accels_filename (void)
{
	static gchar *filename = NULL;

	if (G_UNLIKELY (filename == NULL))
		filename = g_build_filename (
			gnome_user_dir_get (),
			"accels", PACKAGE, NULL);

	return filename;
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
	GdkScreen *screen = NULL;
	GError *error = NULL;
	gchar *decoded_uri;
	guint32 timestamp;

	g_return_if_fail (uri != NULL);

	timestamp = gtk_get_current_event_time ();

	if (parent != NULL)
		screen = gtk_widget_get_screen (GTK_WIDGET (parent));

	decoded_uri = g_strdup (uri);
	camel_url_decode (decoded_uri);

	if (gtk_show_uri (screen, decoded_uri, timestamp, &error))
		goto exit;

	dialog = gtk_message_dialog_new_with_markup (
		parent, GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
		"<big><b>%s</b></big>",
		_("Could not open the link."));

	gtk_message_dialog_format_secondary_text (
		GTK_MESSAGE_DIALOG (dialog), "%s", error->message);

	gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);
	g_error_free (error);

exit:
	g_free (decoded_uri);
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
	GString *uri;
	GtkWidget *dialog;
	GdkScreen *screen = NULL;
	GError *error = NULL;
	guint32 timestamp;

	uri = g_string_new ("ghelp:" PACKAGE);
	timestamp = gtk_get_current_event_time ();

	if (parent != NULL)
		screen = gtk_widget_get_screen (GTK_WIDGET (parent));

	if (link_id != NULL)
		g_string_append_printf (uri, "?%s", link_id);

	if (gtk_show_uri (screen, uri->str, timestamp, &error))
		goto exit;

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
}

/**
 * e_lookup_action:
 * @ui_manager: a #GtkUIManager
 * @action_name: the name of an action
 *
 * Returns the first #GtkAction named @action_name by traversing the
 * list of action groups in @ui_manager.  If no such action exists, the
 * function emits a critical warning before returning %NULL, since this
 * probably indicates a programming error and most code is not prepared
 * to deal with lookup failures.
 *
 * Returns: the first #GtkAction named @action_name
 **/
GtkAction *
e_lookup_action (GtkUIManager *ui_manager,
                 const gchar *action_name)
{
	GtkAction *action = NULL;
	GList *iter;

	g_return_val_if_fail (GTK_IS_UI_MANAGER (ui_manager), NULL);
	g_return_val_if_fail (action_name != NULL, NULL);

	iter = gtk_ui_manager_get_action_groups (ui_manager);

	while (iter != NULL) {
		GtkActionGroup *action_group = iter->data;

		action = gtk_action_group_get_action (
			action_group, action_name);
		if (action != NULL)
			return action;

		iter = g_list_next (iter);
	}

	g_critical ("%s: action `%s' not found", G_STRFUNC, action_name);

	return NULL;
}

/**
 * e_lookup_action_group:
 * @ui_manager: a #GtkUIManager
 * @group_name: the name of an action group
 *
 * Returns the #GtkActionGroup in @ui_manager named @group_name.  If no
 * such action group exists, the function emits a critical warnings before
 * returning %NULL, since this probably indicates a programming error and
 * most code is not prepared to deal with lookup failures.
 *
 * Returns: the #GtkActionGroup named @group_name
 **/
GtkActionGroup *
e_lookup_action_group (GtkUIManager *ui_manager,
                       const gchar *group_name)
{
	GList *iter;

	g_return_val_if_fail (GTK_IS_UI_MANAGER (ui_manager), NULL);
	g_return_val_if_fail (group_name != NULL, NULL);

	iter = gtk_ui_manager_get_action_groups (ui_manager);

	while (iter != NULL) {
		GtkActionGroup *action_group = iter->data;
		const gchar *name;

		name = gtk_action_group_get_name (action_group);
		if (strcmp (name, group_name) == 0)
			return action_group;

		iter = g_list_next (iter);
	}

	g_critical ("%s: action group `%s' not found", G_STRFUNC, group_name);

	return NULL;
}

/**
 * e_load_ui_definition:
 * @ui_manager: a #GtkUIManager
 * @basename: basename of the UI definition file
 *
 * Loads a UI definition into @ui_manager from Evolution's UI directory.
 * Failure here is fatal, since the application can't function without
 * its UI definitions.
 *
 * Returns: The merge ID for the merged UI.  The merge ID can be used to
 *          unmerge the UI with gtk_ui_manager_remove_ui().
 **/
guint
e_load_ui_definition (GtkUIManager *ui_manager,
                      const gchar *basename)
{
	gchar *filename;
	guint merge_id;
	GError *error = NULL;

	g_return_val_if_fail (GTK_IS_UI_MANAGER (ui_manager), 0);
	g_return_val_if_fail (basename != NULL, 0);

	filename = g_build_filename (EVOLUTION_UIDIR, basename, NULL);
	merge_id = gtk_ui_manager_add_ui_from_file (
		ui_manager, filename, &error);
	g_free (filename);

	if (error != NULL) {
		g_error ("%s: %s", basename, error->message);
		g_assert_not_reached ();
	}

	return merge_id;
}

/**
 * e_action_compare_by_label:
 * @action1: a #GtkAction
 * @action2: a #GtkAction
 *
 * Compares the labels for @action1 and @action2 using g_utf8_collate().
 *
 * Returns: &lt; 0 if @action1 compares before @action2, 0 if they
 *          compare equal, &gt; 0 if @action1 compares after @action2
 **/
gint
e_action_compare_by_label (GtkAction *action1,
                           GtkAction *action2)
{
	gchar *label1;
	gchar *label2;
	gint result;

	/* XXX This is horribly inefficient but will generally only be
	 *     used on short lists of actions during UI construction. */

	if (action1 == action2)
		return 0;

	g_object_get (action1, "label", &label1, NULL);
	g_object_get (action2, "label", &label2, NULL);

	result = g_utf8_collate (label1, label2);

	g_free (label1);
	g_free (label2);

	return result;
}

/**
 * e_action_group_remove_all_actions:
 * @action_group: a #GtkActionGroup
 *
 * Removes all actions from the action group.
 **/
void
e_action_group_remove_all_actions (GtkActionGroup *action_group)
{
	GList *list, *iter;

	/* XXX I've proposed this function for inclusion in GTK+.
         *     GtkActionGroup stores actions in an internal hash
         *     table and can do this more efficiently by calling
         *     g_hash_table_remove_all().
         *
         *     http://bugzilla.gnome.org/show_bug.cgi?id=550485 */

	g_return_if_fail (GTK_IS_ACTION_GROUP (action_group));

	list = gtk_action_group_list_actions (action_group);
	for (iter = list; iter != NULL; iter = iter->next)
		gtk_action_group_remove_action (action_group, iter->data);
	g_list_free (list);
}

/**
 * e_str_without_underscores:
 * @s: the string to strip underscores from.
 *
 * Strips underscores from a string in the same way @gtk_label_new_with_mnemonis does.
 * The returned string should be freed.
 */
gchar *
e_str_without_underscores (const gchar *s)
{
	gchar *new_string;
	const gchar *sp;
	gchar *dp;

	new_string = g_malloc (strlen (s) + 1);

	dp = new_string;
	for (sp = s; *sp != '\0'; sp ++) {
		if (*sp != '_') {
			*dp = *sp;
			dp ++;
		} else if (sp[1] == '_') {
			/* Translate "__" in "_".  */
			*dp = '_';
			dp ++;
			sp ++;
		}
	}
	*dp = 0;

	return new_string;
}

gint
e_str_compare (gconstpointer x, gconstpointer y)
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
e_str_case_compare (gconstpointer x, gconstpointer y)
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
e_collate_compare (gconstpointer x, gconstpointer y)
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
e_int_compare (gconstpointer x, gconstpointer y)
{
	gint nx = GPOINTER_TO_INT (x);
	gint ny = GPOINTER_TO_INT (y);

	return (nx == ny) ? 0 : (nx < ny) ? -1 : 1;
}

gboolean
e_write_file_uri (const gchar *filename, const gchar *data)
{
	gboolean res;
	gsize length;
	GFile *file;
	GOutputStream *stream;
	GError *error = NULL;

	g_return_val_if_fail (filename != NULL, FALSE);
	g_return_val_if_fail (data != NULL, FALSE);

	length = strlen (data);

	/* if it is uri, then create file for uri, otherwise for path */
	if (strstr (filename, "://"))
		file = g_file_new_for_uri (filename);
	else
		file = g_file_new_for_path (filename);

	if (!file) {
		g_warning ("Couldn't save item");
		return FALSE;
	}

	stream = G_OUTPUT_STREAM (g_file_replace (file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, &error));
	g_object_unref (file);

	if (!stream || error) {
		g_warning ("Couldn't save item%s%s", error ? ": " : "", error ? error->message : "");

		if (stream)
			g_object_unref (stream);

		if (error)
			g_error_free (error);

		return FALSE;
	}

	res = g_output_stream_write_all (stream, data, length, NULL, NULL, &error);

	if (error) {
		g_warning ("Couldn't save item: %s", error->message);
		g_clear_error (&error);
	}

	g_output_stream_close (stream, NULL, &error);
	g_object_unref (stream);

	if (error) {
		g_warning ("Couldn't close output stream: %s", error->message);
		g_error_free (error);
	}

	return res;
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

	locality = localeconv();
	grouping = locality->grouping;
	while (number) {
		gchar *group;
		switch (*grouping) {
		default:
			last_count = *grouping;
			grouping++;
		case 0:
			divider = epow10(last_count);
			if (number >= divider) {
				group = g_strdup_printf("%0*d", last_count, number % divider);
			} else {
				group = g_strdup_printf("%d", number % divider);
			}
			number /= divider;
			break;
		case CHAR_MAX:
			group = g_strdup_printf("%d", number);
			number = 0;
			break;
		}
		char_length += strlen(group);
		list = g_list_prepend(list, group);
		group_count ++;
	}

	if (list) {
		value = g_new(gchar, 1 + char_length + (group_count - 1) * strlen(locality->thousands_sep));

		iterator = list;
		value_iterator = value;

		strcpy(value_iterator, iterator->data);
		value_iterator += strlen(iterator->data);
		for (iterator = iterator->next; iterator; iterator = iterator->next) {
			strcpy(value_iterator, locality->thousands_sep);
			value_iterator += strlen(locality->thousands_sep);

			strcpy(value_iterator, iterator->data);
			value_iterator += strlen(iterator->data);
		}
		g_list_foreach (list, (GFunc) g_free, NULL);
		g_list_free (list);
		return value;
	} else {
		return g_strdup("0");
	}
}

/* Perform a binary search for key in base which has nmemb elements
   of size bytes each.  The comparisons are done by (*compare)().  */
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

/**
 * Function to do a last minute fixup of the AM/PM stuff if the locale
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
 **/

gsize
e_strftime_fix_am_pm (gchar *str, gsize max, const gchar *fmt,
                      const struct tm *tm)
{
	gchar buf[10];
	gchar *sp;
	gchar *ffmt;
	gsize ret;

	if (strstr(fmt, "%p")==NULL && strstr(fmt, "%P")==NULL) {
		/* No AM/PM involved - can use the fmt string directly */
		ret=e_strftime(str, max, fmt, tm);
	} else {
		/* Get the AM/PM symbol from the locale */
		e_strftime (buf, 10, "%p", tm);

		if (buf[0]) {
			/**
			 * AM/PM have been defined in the locale
			 * so we can use the fmt string directly
			 **/
			ret=e_strftime(str, max, fmt, tm);
		} else {
			/**
			 * No AM/PM defined by locale
			 * must change to 24 hour clock
			 **/
			ffmt=g_strdup(fmt);
			for (sp=ffmt; (sp=strstr(sp, "%l")); sp++) {
				/**
				 * Maybe this should be 'k', but I have never
				 * seen a 24 clock actually use that format
				 **/
				sp[1]='H';
			}
			for (sp=ffmt; (sp=strstr(sp, "%I")); sp++) {
				sp[1]='H';
			}
			ret=e_strftime(str, max, ffmt, tm);
			g_free(ffmt);
		}
	}

	return(ret);
}

gsize
e_utf8_strftime_fix_am_pm (gchar *str, gsize max, const gchar *fmt,
                           const struct tm *tm)
{
	gsize sz, ret;
	gchar *locale_fmt, *buf;

	locale_fmt = g_locale_from_utf8(fmt, -1, NULL, &sz, NULL);
	if (!locale_fmt)
		return 0;

	ret = e_strftime_fix_am_pm(str, max, locale_fmt, tm);
	if (!ret) {
		g_free (locale_fmt);
		return 0;
	}

	buf = g_locale_to_utf8(str, ret, NULL, &sz, NULL);
	if (!buf) {
		g_free (locale_fmt);
		return 0;
	}

	if (sz >= max) {
		gchar *tmp = buf + max - 1;
		tmp = g_utf8_find_prev_char(buf, tmp);
		if (tmp)
			sz = tmp - buf;
		else
			sz = 0;
	}
	memcpy(str, buf, sz);
	str[sz] = '\0';
	g_free(locale_fmt);
	g_free(buf);
	return sz;
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
 * Return value: the gdouble value.
 **/
gdouble
e_flexible_strtod (const gchar *nptr, gchar **endptr)
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
	while (isspace ((guchar)*p))
		p++;

	/* Skip leading optional sign */
	if (*p == '+' || *p == '-')
		p++;

	if (p[0] == '0' &&
	    (p[1] == 'x' || p[1] == 'X')) {
		p += 2;
		/* HEX - find the (optional) decimal point */

		while (isxdigit ((guchar)*p))
			p++;

		if (*p == '.') {
			decimal_point_pos = p++;

			while (isxdigit ((guchar)*p))
				p++;

			if (*p == 'p' || *p == 'P')
				p++;
			if (*p == '+' || *p == '-')
				p++;
			while (isdigit ((guchar)*p))
				p++;
			end = p;
		} else if (strncmp (p, decimal_point, decimal_point_len) == 0) {
			return strtod (nptr, endptr);
		}
	} else {
		while (isdigit ((guchar)*p))
			p++;

		if (*p == '.') {
			decimal_point_pos = p++;

			while (isdigit ((guchar)*p))
				p++;

			if (*p == 'e' || *p == 'E')
				p++;
			if (*p == '+' || *p == '-')
				p++;
			while (isdigit ((guchar)*p))
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
			fail_pos = (gchar *)nptr + (fail_pos - copy) - (decimal_point_len - 1);
		else
			fail_pos = (gchar *)nptr + (fail_pos - copy);
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
 * Return value: The pointer to the buffer with the converted string.
 **/
gchar *
e_ascii_dtostr (gchar *buffer, gint buf_len, const gchar *format, gdouble d)
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

		while (isdigit ((guchar)*p))
			p++;

		if (strncmp (p, decimal_point, decimal_point_len) == 0) {
			*p = '.';
			p++;
			if (decimal_point_len > 1) {
				rest_len = strlen (p + (decimal_point_len-1));
				memmove (p, p + (decimal_point_len-1),
					 rest_len);
				p[rest_len] = 0;
			}
		}
	}

	return buffer;
}

gchar *
e_strdup_append_strings (gchar *first_string, ...)
{
	gchar *buffer;
	gchar *current;
	gint length;
	va_list args1;
	va_list args2;
	gchar *v_string;
	gint v_int;

	va_start (args1, first_string);
	G_VA_COPY (args2, args1);

	length = 0;

	v_string = first_string;
	while (v_string) {
		v_int = va_arg (args1, gint);
		if (v_int >= 0)
			length += v_int;
		else
			length += strlen (v_string);
		v_string = va_arg (args1, gchar *);
	}

	buffer  = g_new (gchar, length + 1);
	current = buffer;

	v_string = first_string;
	while (v_string) {
		v_int = va_arg (args2, gint);
		if (v_int < 0) {
			gint i;
			for (i = 0; v_string[i]; i++) {
				*(current++) = v_string[i];
			}
		} else {
			gint i;
			for (i = 0; v_string[i] && i < v_int; i++) {
				*(current++) = v_string[i];
			}
		}
		v_string = va_arg (args2, gchar *);
	}
	*(current++) = 0;

	va_end (args1);
	va_end (args2);

	return buffer;
}

/* font options cache */
static gchar *fo_antialiasing = NULL, *fo_hinting = NULL, *fo_subpixel_order = NULL;
static GStaticMutex fo_lock = G_STATIC_MUTEX_INIT;

static void
fo_option_changed (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
	#define update_value(key,variable)	\
		g_free (variable);		\
		variable = gconf_client_get_string (client, "/desktop/gnome/font_rendering/" key, NULL);

	g_static_mutex_lock (&fo_lock);
	update_value ("antialiasing", fo_antialiasing);
	update_value ("hinting", fo_hinting);
	update_value ("rgba_order", fo_subpixel_order);
	g_static_mutex_unlock (&fo_lock);

	#undef update_value
}

cairo_font_options_t *
get_font_options (void)
{
	static GConfClient *fo_gconf = NULL;
	cairo_font_options_t *font_options = cairo_font_options_create ();

	if (fo_gconf == NULL) {
		fo_gconf = gconf_client_get_default ();

		gconf_client_add_dir (fo_gconf, "/desktop/gnome/font_rendering", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
		gconf_client_notify_add (fo_gconf, "/desktop/gnome/font_rendering/antialiasing", fo_option_changed, NULL, NULL, NULL);
		gconf_client_notify_add (fo_gconf, "/desktop/gnome/font_rendering/hinting", fo_option_changed, NULL, NULL, NULL);
		gconf_client_notify_add (fo_gconf, "/desktop/gnome/font_rendering/rgba_order", fo_option_changed, NULL, NULL, NULL);

		fo_option_changed (fo_gconf, 0, NULL, NULL);
	}

	g_static_mutex_lock (&fo_lock);

	/* Antialiasing */
	if (fo_antialiasing == NULL)
		cairo_font_options_set_antialias (font_options, CAIRO_ANTIALIAS_DEFAULT);
	else if (strcmp (fo_antialiasing, "grayscale") == 0)
		cairo_font_options_set_antialias (font_options, CAIRO_ANTIALIAS_GRAY);
	else if (strcmp (fo_antialiasing, "rgba") == 0)
		cairo_font_options_set_antialias (font_options, CAIRO_ANTIALIAS_SUBPIXEL);
	else if (strcmp (fo_antialiasing, "none") == 0)
		cairo_font_options_set_antialias (font_options, CAIRO_ANTIALIAS_NONE);
	else
		cairo_font_options_set_antialias (font_options, CAIRO_ANTIALIAS_DEFAULT);

	if (fo_hinting == NULL)
		cairo_font_options_set_hint_style (font_options, CAIRO_HINT_STYLE_DEFAULT);
	else if (strcmp (fo_hinting, "full") == 0)
		cairo_font_options_set_hint_style (font_options, CAIRO_HINT_STYLE_FULL);
	else if (strcmp (fo_hinting, "medium") == 0)
		cairo_font_options_set_hint_style (font_options, CAIRO_HINT_STYLE_MEDIUM);
	else if (strcmp (fo_hinting, "slight") == 0)
		cairo_font_options_set_hint_style (font_options, CAIRO_HINT_STYLE_SLIGHT);
	else if (strcmp (fo_hinting, "none") == 0)
		cairo_font_options_set_hint_style (font_options, CAIRO_HINT_STYLE_NONE);
	else
		cairo_font_options_set_hint_style (font_options, CAIRO_HINT_STYLE_DEFAULT);

	if (fo_subpixel_order == NULL)
		cairo_font_options_set_subpixel_order (font_options, CAIRO_SUBPIXEL_ORDER_DEFAULT);
	else if (strcmp (fo_subpixel_order, "rgb") == 0)
		cairo_font_options_set_subpixel_order (font_options, CAIRO_SUBPIXEL_ORDER_RGB);
	else if (strcmp (fo_subpixel_order, "bgr") == 0)
		cairo_font_options_set_subpixel_order (font_options, CAIRO_SUBPIXEL_ORDER_BGR);
	else if (strcmp (fo_subpixel_order, "vrgb") == 0)
		cairo_font_options_set_subpixel_order (font_options, CAIRO_SUBPIXEL_ORDER_VRGB);
	else if (strcmp (fo_subpixel_order, "vbgr") == 0)
		cairo_font_options_set_subpixel_order (font_options, CAIRO_SUBPIXEL_ORDER_VBGR);
	else
		cairo_font_options_set_subpixel_order (font_options, CAIRO_SUBPIXEL_ORDER_DEFAULT);

	g_static_mutex_unlock (&fo_lock);

	return font_options;
}

/**
 * e_file_update_save_path:
 * @uri: URI to store
 * @free: If TRUE, free uri
 *
 * Save the save_dir path for evolution.  If free is TRUE, uri gets freed when
 * done.  Genearally, this should be called with the output of
 * gtk_file_chooser_get_current_folder_uri()  The URI must be a path URI, not a
 * file URI.
 **/
void
e_file_update_save_path (gchar *uri, gboolean free)
{
	GConfClient *gconf = gconf_client_get_default();
	GError *error = NULL;

	gconf_client_set_string(gconf, "/apps/evolution/mail/save_dir", uri, &error);
	if (error != NULL) {
		g_warning("%s (%s) %s", G_STRLOC, G_STRFUNC, error->message);
		g_clear_error(&error);
	}
	g_object_unref(gconf);
	if (free)
		g_free(uri);
}

/**
 * e_file_get_save_path:
 *
 * Return the save_dir path for evolution.  If there isn't a save_dir, returns
 * the users home directory.  Returns an allocated URI that should be freed by
 * the caller.
 **/
gchar *
e_file_get_save_path (void)
{
	GConfClient *gconf = gconf_client_get_default();
	GError *error = NULL;
	gchar *uri;

	uri = gconf_client_get_string(gconf, "/apps/evolution/mail/save_dir", &error);
	if (error != NULL) {
		g_warning("%s (%s) %s", G_STRLOC, G_STRFUNC, error->message);
		g_clear_error(&error);
	}
	g_object_unref(gconf);

	if (uri == NULL) {
		GFile *file;

		file = g_file_new_for_path (g_get_home_dir ());
		if (file) {
			uri = g_file_get_uri (file);
			g_object_unref (file);
		}
	}

	return (uri);
}

/* Evolution Locks for crash recovery */

#define LOCK_FILE ".running"

static const gchar *
get_lock_filename (void)
{
	static gchar *filename = NULL;

	if (G_UNLIKELY (filename == NULL))
		filename = g_build_filename (e_get_user_data_dir (), LOCK_FILE, NULL);

	return filename;
}

gboolean
e_file_lock_create ()
{
	const gchar *fname = get_lock_filename ();
	gboolean status = FALSE;

	gint fd = g_creat (fname, S_IRUSR|S_IWUSR);
	if (fd == -1) {
		g_warning ("Lock file '%s' creation failed, error %d\n", fname, errno);
	} else {
		status = TRUE;
		close (fd);
	}

	return status;
}

void
e_file_lock_destroy ()
{
	const gchar *fname = get_lock_filename ();

	if (g_unlink (fname) == -1) {
		g_warning ("Lock destroy: failed to unlink file '%s'!",fname);
	}
}

gboolean
e_file_lock_exists ()
{
	const gchar *fname = get_lock_filename ();

	return g_file_test (fname, G_FILE_TEST_EXISTS);
}

/**
 * e_util_guess_mime_type:
 * @filename: it's a local file name, or URI.
 * @localfile: set to TRUE if can check the local file content, FALSE to check only based on the filename itself.
 * Returns: NULL or newly allocated string with a mime_type of the given file. Free with g_free.
 *
 * Guesses mime_type for the given filename.
 **/
gchar *
e_util_guess_mime_type (const gchar *filename, gboolean localfile)
{
	gchar *mime_type = NULL;

	g_return_val_if_fail (filename != NULL, NULL);

	if (localfile) {
		GFile *file;

		if (strstr (filename, "://"))
			file = g_file_new_for_uri (filename);
		else
			file = g_file_new_for_path (filename);

		if (file) {
			GFileInfo *fi;

			fi = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE, G_FILE_QUERY_INFO_NONE, NULL, NULL);
			if (fi) {
				mime_type = g_content_type_get_mime_type (g_file_info_get_content_type (fi));

				g_object_unref (fi);
			}

			g_object_unref (file);
		}
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

/**
 * e_util_filename_to_uri:
 * @filename: local file name.
 * Returns: either newly allocated string or NULL. Free with g_free.
 *
 * Converts local file name to URI.
 **/
gchar *
e_util_filename_to_uri (const gchar *filename)
{
	GFile *file;
	gchar *uri = NULL;

	g_return_val_if_fail (filename != NULL, NULL);

	file = g_file_new_for_path (filename);

	if (file) {
		uri = g_file_get_uri (file);
		g_object_unref (file);
	}

	return uri;
}

/**
 * e_util_uri_to_filename:
 * @uri: uri.
 * Returns: either newly allocated string or NULL. Free with g_free.
 *
 * Converts URI to local file name. NULL indicates no such local file name exists.
 **/
gchar *
e_util_uri_to_filename (const gchar *uri)
{
	GFile *file;
	gchar *filename = NULL;

	g_return_val_if_fail (uri != NULL, NULL);

	file = g_file_new_for_uri (uri);

	if (file) {
		filename = g_file_get_path (file);
		g_object_unref (file);
	}

	return filename;
}

/**
 * e_util_read_file:
 * @filename: File name to read.
 * @filename_is_uri: Whether the file name is URI, if not, then it's a local path.
 * @buffer: Read content or the file. Should not be NULL. Returned value should be freed with g_free.
 * @read: Number of actually read bytes. Should not be NULL.
 * @error: Here will be returned an error from reading operations. Can be NULL. Not every time is set when returned FALSE.
 * Returns: Whether was reading successful or not.
 *
 * Reads synchronously content of the file, to which is pointed either by path or by URI.
 * Mount point should be already mounted when calling this function.
 **/
gboolean
e_util_read_file (const gchar *filename, gboolean filename_is_uri, gchar **buffer, gsize *read, GError **error)
{
	GFile *file;
	GFileInfo *info;
	GError *err = NULL;
	gboolean res = FALSE;

	g_return_val_if_fail (filename != NULL, FALSE);
	g_return_val_if_fail (buffer != NULL, FALSE);
	g_return_val_if_fail (read != NULL, FALSE);

	*buffer = NULL;
	*read = 0;

	if (filename_is_uri)
		file = g_file_new_for_uri (filename);
	else
		file = g_file_new_for_path (filename);

	g_return_val_if_fail (file != NULL, FALSE);

	info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_SIZE, G_FILE_QUERY_INFO_NONE, NULL, &err);

	if (!err && info) {
		guint64 sz;
		gchar *buff;

		sz = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_STANDARD_SIZE);
		buff = g_malloc (sizeof (gchar) * sz);

		if (buff) {
			GInputStream *stream;

			stream = G_INPUT_STREAM (g_file_read (file, NULL, &err));

			if (!err && stream) {
				res = g_input_stream_read_all (stream, buff, sz, read, NULL, &err);

				if (err)
					res = FALSE;

				if (res)
					*buffer = buff;
				else
					g_free (buff);
			}

			if (stream)
				g_object_unref (stream);
		}
	}

	if (info)
		g_object_unref (info);

	g_object_unref (file);

	if (err) {
		if (error)
			*error = err;
		else
			g_error_free (err);
	}

	return res;
}

GSList *
e_util_get_category_filter_options (void)
{
	GSList *res = NULL;
	GList *clist, *l;

	clist = e_categories_get_list ();
	for (l = clist; l; l = l->next) {
		const gchar *cname = l->data;
		struct _filter_option *fo = g_new0 (struct _filter_option, 1);

		fo->title = g_strdup (cname);
		fo->value = g_strdup (cname);
		res = g_slist_prepend (res, fo);
	}

	g_list_free (clist);

	return g_slist_reverse (res);
}

static gpointer
e_camel_object_copy (gpointer camel_object)
{
	if (CAMEL_IS_OBJECT (camel_object))
		camel_object_ref (camel_object);

	return camel_object;
}

static void
e_camel_object_free (gpointer camel_object)
{
	if (CAMEL_IS_OBJECT (camel_object))
		camel_object_unref (camel_object);
}

GType
e_camel_object_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
		type = g_boxed_type_register_static (
			"ECamelObject",
			(GBoxedCopyFunc) e_camel_object_copy,
			(GBoxedFreeFunc) e_camel_object_free);

	return type;
}
