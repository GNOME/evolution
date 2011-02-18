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

/**
 * SECTION: e-util
 * @include: e-util/e-util.h
 **/

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

#ifdef G_OS_WIN32
#include <windows.h>
#endif

#include <camel/camel.h>
#include <libedataserver/e-data-server-util.h>
#include <libedataserver/e-categories.h>
#include <libedataserver/e-source-list.h>

#include "filter/e-filter-option.h"

#include "e-util.h"
#include "e-util-private.h"

/**
 * e_get_gnome2_user_dir:
 *
 * Returns the base directory for user data, according to libgnome.
 * The directory can be overridden by setting the GNOME22_USER_DIR
 * environment variable.  The string is owned by Evolution and must
 * not be modified or freed.
 *
 * Returns: base directory for GNOME user data
 **/
const gchar *
e_get_gnome2_user_dir (void)
{
	static gchar *dirname = NULL;

#ifndef G_OS_WIN32
	if (G_UNLIKELY (dirname == NULL))
		dirname = g_strdup (g_getenv ("GNOME22_USER_DIR"));
#endif
    if (dirname == NULL)
		dirname = g_build_filename (
			g_get_home_dir (), ".gnome2", NULL);

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

	/* XXX The directory corresponds to gnome_user_accels_dir_get()
	 *     from libgnome.  Continue using this location until GNOME
	 *     decides on an XDG-compliant location.  Perhaps something
	 *     like $(XDG_CONFIG_DIR)/accels. */

	if (G_UNLIKELY (filename == NULL))
		filename = g_build_filename (
			e_get_gnome2_user_dir (),
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
	guint32 timestamp;

	g_return_if_fail (uri != NULL);

	timestamp = gtk_get_current_event_time ();

	if (parent != NULL)
		screen = gtk_widget_get_screen (GTK_WIDGET (parent));

	if (gtk_show_uri (screen, uri, timestamp, &error))
		return;

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

	g_critical ("%s: action '%s' not found", G_STRFUNC, action_name);

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

	g_critical ("%s: action group '%s' not found", G_STRFUNC, group_name);

	return NULL;
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
		g_assert_not_reached ();
	}
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
 * e_radio_action_get_current_action:
 * @radio_action: a #GtkRadioAction
 *
 * Returns the currently active member of the group to which @radio_action
 * belongs.
 *
 * Returns: the currently active group member
 **/
GtkRadioAction *
e_radio_action_get_current_action (GtkRadioAction *radio_action)
{
	GSList *group;
	gint current_value;

	g_return_val_if_fail (GTK_IS_RADIO_ACTION (radio_action), NULL);

	group = gtk_radio_action_get_group (radio_action);
	current_value = gtk_radio_action_get_current_value (radio_action);

	while (group != NULL) {
		gint value;

		radio_action = GTK_RADIO_ACTION (group->data);
		g_object_get (radio_action, "value", &value, NULL);

		if (value == current_value)
			return radio_action;

		group = g_slist_next (group);
	}

	return NULL;
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
 * e_type_traverse:
 * @parent_type: the root #GType to traverse from
 * @func: the function to call for each visited #GType
 * @user_data: user data to pass to the function
 *
 * Calls @func for all instantiable subtypes of @parent_type.
 *
 * This is often useful for extending functionality by way of #EModule.
 * A module may register a subtype of @parent_type in its e_module_load()
 * function.  Then later on the application will call e_type_traverse()
 * to instantiate all registered subtypes of @parent_type.
 **/
void
e_type_traverse (GType parent_type,
                 ETypeFunc func,
                 gpointer user_data)
{
	GType *children;
	guint n_children, ii;

	g_return_if_fail (func != NULL);

	children = g_type_children (parent_type, &n_children);

	for (ii = 0; ii < n_children; ii++) {
		GType type = children[ii];

		/* Recurse over the child's children. */
		e_type_traverse (type, func, user_data);

		/* Skip abstract types. */
		if (G_TYPE_IS_ABSTRACT (type))
			continue;

		func (type, user_data);
	}

	g_free (children);
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

/**
 * e_color_to_value:
 * @color: a #GdkColor
 *
 * Converts a #GdkColor to a 24-bit RGB color value.
 *
 * Returns: a 24-bit color value
 **/
guint32
e_color_to_value (GdkColor *color)
{
	guint16 red;
	guint16 green;
	guint16 blue;

	g_return_val_if_fail (color != NULL, 0);

	red = color->red >> 8;
	green = color->green >> 8;
	blue = color->blue >> 8;

	return (guint32) (((red << 16) | (green << 8) | blue) & 0xffffff);
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
		case 0:
			divider = epow10 (last_count);
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
e_strftime_fix_am_pm (gchar *str, gsize max, const gchar *fmt,
                      const struct tm *tm)
{
	gchar buf[10];
	gchar *sp;
	gchar *ffmt;
	gsize ret;

	if (strstr(fmt, "%p")==NULL && strstr(fmt, "%P")==NULL) {
		/* No AM/PM involved - can use the fmt string directly */
		ret=e_strftime (str, max, fmt, tm);
	} else {
		/* Get the AM/PM symbol from the locale */
		e_strftime (buf, 10, "%p", tm);

		if (buf[0]) {
			/* AM/PM have been defined in the locale
			 * so we can use the fmt string directly. */
			ret=e_strftime (str, max, fmt, tm);
		} else {
			/* No AM/PM defined by locale
			 * must change to 24 hour clock. */
			ffmt=g_strdup (fmt);
			for (sp=ffmt; (sp=strstr(sp, "%l")); sp++) {
				/* Maybe this should be 'k', but I have never
				 * seen a 24 clock actually use that format. */
				sp[1]='H';
			}
			for (sp=ffmt; (sp=strstr(sp, "%I")); sp++) {
				sp[1]='H';
			}
			ret=e_strftime (str, max, ffmt, tm);
			g_free (ffmt);
		}
	}

	return (ret);
}

gsize
e_utf8_strftime_fix_am_pm (gchar *str, gsize max, const gchar *fmt,
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
 * Returns: the gdouble value
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
 * Returns: the pointer to the buffer with the converted string
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

/* font options cache */
static gchar *fo_antialiasing = NULL;
static gchar *fo_hinting = NULL;
static gchar *fo_subpixel_order = NULL;
static GStaticMutex fo_lock = G_STATIC_MUTEX_INIT;

static void
fo_option_changed (GConfClient *client,
                   guint cnxn_id,
                   GConfEntry *entry,
                   gpointer user_data)
{
	const gchar *key;

	g_static_mutex_lock (&fo_lock);

	g_free (fo_antialiasing);
	key = "/desktop/gnome/font_rendering/antialiasing";
	fo_antialiasing = gconf_client_get_string (client, key, NULL);

	g_free (fo_hinting);
	key = "/desktop/gnome/font_rendering/hinting";
	fo_hinting = gconf_client_get_string (client, key, NULL);

	g_free (fo_subpixel_order);
	key = "/desktop/gnome/font_rendering/rgba_order";
	fo_subpixel_order = gconf_client_get_string (client, key, NULL);

	g_static_mutex_unlock (&fo_lock);
}

cairo_font_options_t *
get_font_options (void)
{
	static GConfClient *fo_gconf = NULL;
	cairo_font_options_t *font_options = cairo_font_options_create ();

	if (fo_gconf == NULL) {
		const gchar *key;

		fo_gconf = gconf_client_get_default ();

		key = "/desktop/gnome/font_rendering";
		gconf_client_add_dir (
			fo_gconf, key, GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

		key = "/desktop/gnome/font_rendering/antialiasing";
		gconf_client_notify_add (
			fo_gconf, key, fo_option_changed, NULL, NULL, NULL);

		key = "/desktop/gnome/font_rendering/hinting";
		gconf_client_notify_add (
			fo_gconf, key, fo_option_changed, NULL, NULL, NULL);

		key = "/desktop/gnome/font_rendering/rgba_order";
		gconf_client_notify_add (
			fo_gconf, key, fo_option_changed, NULL, NULL, NULL);

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

/**
 * e_util_guess_mime_type:
 * @filename: a local file name, or URI
 * @localfile: %TRUE to check the file content, FALSE to check only the name
 *
 * Tries to determine the MIME type for @filename.  Free the returned
 * string with g_free().
 *
 * Returns: the MIME type of @filename, or %NULL if the the MIME type could
 *          not be determined
 **/
gchar *
e_util_guess_mime_type (const gchar *filename, gboolean localfile)
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

/* XXX: Should e-util/ really depend on filter/ ?? */
GSList *
e_util_get_category_filter_options (void)
{
	GSList *res = NULL;
	GList *clist, *l;

	clist = e_categories_get_list ();
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

	g_list_free (clist);

	return g_slist_reverse (res);
}

/**
 * e_util_get_searchable_categories:
 *
 * Returns list of searchable categories only. The list should
 * be freed with g_list_free() when done with it, but the items
 * are internal strings, names of categories, which should not
 * be touched in other than read-only way, in other words the same
 * restrictions as for e_categories_get_list() applies here too.
 **/
GList *
e_util_get_searchable_categories (void)
{
	GList *res = NULL, *all_categories, *l;

	all_categories = e_categories_get_list ();
	for (l = all_categories; l; l = l->next) {
		const gchar *cname = l->data;

		if (e_categories_is_searchable (cname))
			res = g_list_prepend (res, (gpointer) cname);
	}

	g_list_free (all_categories);

	return g_list_reverse (res);
}

/**
 * e_util_set_source_combo_box_list:
 * @source_combo_box: an #ESourceComboBox
 * @source_gconf_path: GConf path with sources to use in an #ESourceList
 *
 * Sets an #ESourceList of a given GConf path to an #ESourceComboBox.
 **/
void
e_util_set_source_combo_box_list (GtkWidget *source_combo_box,
                                  const gchar *source_gconf_path)
{
	ESourceList *source_list;
	GConfClient *gconf_client;

	g_return_if_fail (source_combo_box != NULL);
	g_return_if_fail (source_gconf_path != NULL);

	gconf_client = gconf_client_get_default ();
	source_list = e_source_list_new_for_gconf (
		gconf_client, source_gconf_path);
	g_object_set (source_combo_box, "source-list", source_list, NULL);
	g_object_unref (source_list);
	g_object_unref (gconf_client);
}

