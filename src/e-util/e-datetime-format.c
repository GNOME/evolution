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
 * Copyright (C) 1999-2009 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "e-datetime-format.h"

#include <libedataserver/libedataserver.h>

#include "e-misc-utils.h"

#define KEYS_GROUPNAME "formats"

#ifdef G_OS_WIN32
#ifdef localtime_r
#undef localtime_r
#endif
/* The localtime() in Microsoft's C library *is* thread-safe */
#define localtime_r(timep, result)  (localtime (timep) ? memcpy ((result), localtime (timep), sizeof (*(result))) : 0)
#endif

/**
 * e_datetime_format_dup_config_filename:
 *
 * Returns configuration file name for the date/time format.
 *
 * Returns: (transfer full): configuration file name for the date/time format
 *
 * Since: 3.50
 **/
gchar *
e_datetime_format_dup_config_filename (void)
{
	return g_build_filename (e_get_user_data_dir (), "datetime-formats.ini", NULL);
}

typedef struct _ChangeListerData {
	EDatetimeFormatChangedFunc func;
	gpointer user_data;
} ChangeListerData;

static GHashTable *key2fmt = NULL;
static GPtrArray *change_listeners = NULL; /* ChangeListerData */

/**
 * e_datetime_format_add_change_listener:
 * @func: (closure user_data) (scope forever): a change callback
 * @user_data: user data for the @func
 *
 * Adds a change listener, which calls @func with @user_data.
 * Remote it with e_datetime_format_remove_change_listener(),
 * when no longer needed.
 *
 * Since: 3.58
 **/
void
e_datetime_format_add_change_listener (EDatetimeFormatChangedFunc func,
				       gpointer user_data)
{
	ChangeListerData *data;

	if (!change_listeners)
		change_listeners = g_ptr_array_new_with_free_func (g_free);

	data = g_new0 (ChangeListerData, 1);
	data->func = func;
	data->user_data = user_data;

	g_ptr_array_add (change_listeners, data);
}

/**
 * e_datetime_format_remove_change_listener:
 * @func: (closure user_data) (scope call): a change callback
 * @user_data: user data for the @func
 *
 * Removes a listener previously added by e_datetime_format_add_change_listener().
 * It does nothing when no such listener exists. Both the @func and the @user_data
 * are compared when looking for the match.
 *
 * Since: 3.58
 **/
void
e_datetime_format_remove_change_listener (EDatetimeFormatChangedFunc func,
					  gpointer user_data)
{
	guint ii;

	if (!change_listeners)
		return;

	for (ii = 0; ii < change_listeners->len; ii++) {
		ChangeListerData *data = g_ptr_array_index (change_listeners, ii);

		if (data->func == func && data->user_data == user_data) {
			g_ptr_array_remove_index (change_listeners, ii);

			if (change_listeners->len == 0)
				g_clear_pointer (&change_listeners, g_ptr_array_unref);
			break;
		}
	}
}

/**
 * e_datetime_format_free_memory:
 *
 * Frees loaded configuration from the memory. The next call to the date/time
 * format functions will load the configuration again. This function should be
 * called from the same thread as the other date/time format functions, which
 * is usually the main/GUI thread.
 *
 * It also frees information about all the change listeners, if there were any.
 *
 * Since: 3.50
 **/
void
e_datetime_format_free_memory (void)
{
	g_clear_pointer (&key2fmt, g_hash_table_destroy);
	g_clear_pointer (&change_listeners, g_ptr_array_unref);
}

static GKeyFile *setup_keyfile = NULL; /* used on the combo */
static gint setup_keyfile_instances = 0;

static void
save_keyfile (GKeyFile *keyfile,
	      const gchar *key,
	      DTFormatKind kind)
{
	gchar *contents;
	gchar *filename;
	gsize length;
	GError *error = NULL;

	g_return_if_fail (keyfile != NULL);

	filename = e_datetime_format_dup_config_filename ();
	contents = g_key_file_to_data (keyfile, &length, NULL);

	g_file_set_contents (filename, contents, length, &error);

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	g_free (contents);
	g_free (filename);

	if (change_listeners) {
		const gchar *component, *part;
		gchar **split;
		guint ii;

		split = key ? g_strsplit (key, "-", -1) : NULL;
		component = split ? split[0] : NULL;
		part = split && split[0] && split[1] && split[2] ? split[1] : NULL;

		for (ii = 0; ii < change_listeners->len; ii++) {
			ChangeListerData *data = g_ptr_array_index (change_listeners, ii);

			data->func (component, part, kind, data->user_data);
		}

		g_strfreev (split);
	}
}

static void
ensure_loaded (void)
{
	GKeyFile *keyfile;
	gchar *str, **keys;
	gint i;

	if (key2fmt)
		return;

	key2fmt = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	keyfile = g_key_file_new ();

	str = e_datetime_format_dup_config_filename ();
	g_key_file_load_from_file (keyfile, str, G_KEY_FILE_NONE, NULL);
	g_free (str);

	keys = g_key_file_get_keys (keyfile, KEYS_GROUPNAME, NULL, NULL);

	if (keys) {
		for (i = 0;  keys[i]; i++) {
			str = g_key_file_get_string (keyfile, KEYS_GROUPNAME, keys[i], NULL);
			if (str)
				g_hash_table_insert (key2fmt, g_strdup (keys[i]), str);
		}

		g_strfreev (keys);
	}

	g_key_file_free (keyfile);
}

static const gchar *
get_default_format (DTFormatKind kind,
                    const gchar *key)
{
	const gchar *res = NULL;

	ensure_loaded ();

	switch (kind) {
	case DTFormatKindDate:
		res = g_hash_table_lookup (key2fmt, "Default-Date");
		if (!res)
			res = "%x";
		break;
	case DTFormatKindTime:
		res = g_hash_table_lookup (key2fmt, "Default-Time");
		if (!res)
			res = "%X";
		break;
	case DTFormatKindDateTime:
		res = g_hash_table_lookup (key2fmt, "Default-DateTime");
		if (!res && key && g_str_has_prefix (key, "mail-table"))
			res = "%ad %H:%M";
		if (!res)
			res = "%x %X"; /* %c is also possible, but it doesn't play well with time zone identifiers */
		break;
	case DTFormatKindShortDate:
		res = g_hash_table_lookup (key2fmt, "Default-ShortDate");
		if (!res)
			res = "%A, %B %d";
		break;
	}

	if (!res)
		res = "%x %X";

	return res;
}

static const gchar *
get_format_internal (const gchar *key,
                     DTFormatKind kind)
{
	const gchar *res;

	ensure_loaded ();

	g_return_val_if_fail (key != NULL, NULL);
	g_return_val_if_fail (key2fmt != NULL, NULL);

	res = g_hash_table_lookup (key2fmt, key);
	if (!res)
		res = get_default_format (kind, key);

	return res;
}

static void
set_format_internal (const gchar *key,
                     const gchar *fmt,
                     GKeyFile *keyfile)
{
	ensure_loaded ();

	g_return_if_fail (key != NULL);
	g_return_if_fail (key2fmt != NULL);
	g_return_if_fail (keyfile != NULL);

	if (!fmt || !*fmt) {
		g_hash_table_remove (key2fmt, key);
		g_key_file_remove_key (keyfile, KEYS_GROUPNAME, key, NULL);
	} else {
		g_hash_table_insert (key2fmt, g_strdup (key), g_strdup (fmt));
		g_key_file_set_string (keyfile, KEYS_GROUPNAME, key, fmt);
	}
}

static gchar *
format_relative_date (time_t tvalue,
                      time_t ttoday,
		      const gchar *prefer_date_fmt,
                      const struct tm *value,
                      const struct tm *today)
{
	gchar *res = g_strdup (prefer_date_fmt ? prefer_date_fmt : get_default_format (DTFormatKindDate, NULL));
	GDate now, val;
	gint diff;

	g_return_val_if_fail (value != NULL, res);
	g_return_val_if_fail (today != NULL, res);

	g_date_set_time_t (&now, ttoday);
	g_date_set_time_t (&val, tvalue);

	diff = g_date_get_julian (&now) - g_date_get_julian (&val);
	/* if it's more than a week, use the default date format */
	if (ABS (diff) > 7)
		return res;

	g_free (res);

	if (value->tm_year == today->tm_year &&
	    value->tm_mon == today->tm_mon &&
	    value->tm_mday == today->tm_mday) {
		res = g_strdup (_("Today"));
	} else {
		gboolean future = FALSE;

		if (diff < 0)
			future = TRUE;

		diff = ABS (diff);

		if (diff <= 1) {
			if (future)
				res = g_strdup (_("Tomorrow"));
			else
				res = g_strdup (_("Yesterday"));
		} else {
			if (future) {
				switch (g_date_get_weekday (&val)) {
				case 1:
					/* Translators: This is used for abbreviated days in the future.
				 *         You can use strftime modifiers here too, like "Next %a", to avoid
					 * repeated translation of the abbreviated day name. */
					res = g_strdup (C_ ("DateFmt", "Next Mon"));
					break;
				case 2:
					/* Translators: This is used for abbreviated days in the future.
				 *         You can use strftime modifiers here too, like "Next %a", to avoid
					 * repeated translation of the abbreviated day name. */
					res = g_strdup (C_ ("DateFmt", "Next Tue"));
					break;
				case 3:
					/* Translators: This is used for abbreviated days in the future.
				 *         You can use strftime modifiers here too, like "Next %a", to avoid
					 * repeated translation of the abbreviated day name. */
					res = g_strdup (C_ ("DateFmt", "Next Wed"));
					break;
				case 4:
					/* Translators: This is used for abbreviated days in the future.
				 *         You can use strftime modifiers here too, like "Next %a", to avoid
					 * repeated translation of the abbreviated day name. */
					res = g_strdup (C_ ("DateFmt", "Next Thu"));
					break;
				case 5:
					/* Translators: This is used for abbreviated days in the future.
				 *         You can use strftime modifiers here too, like "Next %a", to avoid
					 * repeated translation of the abbreviated day name. */
					res = g_strdup (C_ ("DateFmt", "Next Fri"));
					break;
				case 6:
					/* Translators: This is used for abbreviated days in the future.
				 *         You can use strftime modifiers here too, like "Next %a", to avoid
					 * repeated translation of the abbreviated day name. */
					res = g_strdup (C_ ("DateFmt", "Next Sat"));
					break;
				case 7:
					/* Translators: This is used for abbreviated days in the future.
				 *         You can use strftime modifiers here too, like "Next %a", to avoid
					 * repeated translation of the abbreviated day name. */
					res = g_strdup (C_ ("DateFmt", "Next Sun"));
					break;
				default:
					g_return_val_if_reached (NULL);
					break;
				}
			} else {
				res = g_strdup ("%a");
			}
		}
	}

	return res;
}

static void
format_internal (const gchar *key,
		 DTFormatKind kind,
		 time_t tvalue,
		 struct tm *tm_value,
		 gchar *buffer,
		 gint buffer_size)
{
	const gchar *fmt;
	GString *use_fmt = NULL;
	gint i, last = 0;
	struct tm today, value;
	time_t ttoday;

	if (!tm_value) {
		localtime_r (&tvalue, &value);
		tm_value = &value;
	} else {
		/* recalculate tvalue to local (system) timezone */
		tvalue = mktime (tm_value);
		localtime_r (&tvalue, &value);
	}

	fmt = get_format_internal (key, kind);
	for (i = 0; fmt[i]; i++) {
		if (fmt[i] == '%') {
			if (fmt[i + 1] == '%') {
				i++;
			} else if (fmt[i + 1] == 'a' && fmt[i + 2] == 'd' && (fmt[i + 3] == 0 || !g_ascii_isalpha (fmt[i + 3]))) {
				gchar *ad, *prefer_date_fmt = NULL;

				/* "%ad" for abbreviated date; it can be optionally extended
				   with preferred date format in [], like this: "%ad[%Y-%m-%d]" */
				if (!use_fmt) {
					use_fmt = g_string_new ("");

					ttoday = time (NULL);
					localtime_r (&ttoday, &today);
				}

				g_string_append_len (use_fmt, fmt + last, i - last);
				last = i + 3;
				i += 2;

				if (fmt[i + 1] == '[' && fmt[i + 2] != ']') {
					const gchar *end;

					end = strchr (fmt + i + 1, ']');
					if (end) {
						gint len = end - fmt - i - 1;

						prefer_date_fmt = g_strndup (fmt + i + 2, len - 1);

						/* Include the ending ']' */
						last += len + 1;
						i += len + 1;
					}
				}

				ad = format_relative_date (tvalue, ttoday, prefer_date_fmt, &value, &today);
				if (ad)
					g_string_append (use_fmt, ad);
				else if (g_ascii_isspace (fmt[i + 1]))
					i++;

				g_free (prefer_date_fmt);
				g_free (ad);
			}
		}
	}

	if (use_fmt && last < i) {
		g_string_append_len (use_fmt, fmt + last, i - last);
	}

	e_utf8_strftime_fix_am_pm (buffer, buffer_size, use_fmt ? use_fmt->str : fmt, tm_value);

	if (use_fmt)
		g_string_free (use_fmt, TRUE);

	g_strstrip (buffer);
}

static void
fill_combo_formats (GtkWidget *combo,
                    const gchar *key,
                    DTFormatKind kind)
{
	const gchar *date_items[] = {
		N_ ("Use locale default"),
		"%m/%d/%y",	/* American style */
		"%m/%d/%Y",	/* American style, full year */
		"%d.%m.%y",	/* non-American style */
		"%d.%m.%Y",	/* non-American style, full year */
		"%ad",		/* abbreviated date, like "Today" */
		NULL
	};

	const gchar *time_items[] = {
		N_ ("Use locale default"),
		"%I:%M:%S %p",	/* 12hours style */
		"%I:%M %p",	/* 12hours style, without seconds */
		"%H:%M:%S",	/* 24hours style */
		"%H:%M",	/* 24hours style, without seconds */
		NULL
	};

	const gchar *datetime_items[] = {
		N_ ("Use locale default"),
		"%m/%d/%y %I:%M:%S %p",	/* American style */
		"%m/%d/%Y %I:%M:%S %p",	/* American style, full year */
		"%m/%d/%y %I:%M %p",	/* American style, without seconds */
		"%m/%d/%Y %I:%M %p",	/* American style, without seconds, full year */
		"%ad %I:%M:%S %p",	/* %ad is an abbreviated date, like "Today" */
		"%ad %I:%M %p",		/* %ad is an abbreviated date, like "Today", without seconds */
		"%d.%m.%y %H:%M:%S",	/* non-American style */
		"%d.%m.%Y %H:%M:%S",	/* non-American style, full year */
		"%d.%m.%y %H:%M",	/* non-American style, without seconds */
		"%d.%m.%Y %H:%M",	/* non-American style, without seconds, full year */
		"%ad %H:%M:%S",
		"%ad %H:%M",		/* without seconds */
		NULL
	};

	const gchar *shortdate_items[] = {
		"%A, %B %d",
		"%A, %d %B",
		"%a, %b %d",
		"%a, %d %b",
		NULL
	};

	const gchar **items = NULL;
	gint i, idx = 0, max_len = 0;
	const gchar *fmt;

	g_return_if_fail (GTK_IS_COMBO_BOX (combo));

	switch (kind) {
	case DTFormatKindDate:
		items = date_items;
		break;
	case DTFormatKindTime:
		items = time_items;
		break;
	case DTFormatKindDateTime:
		items = datetime_items;
		break;
	case DTFormatKindShortDate:
		items = shortdate_items;
		break;
	}

	g_return_if_fail (items != NULL);

	fmt = get_format_internal (key, kind);

	for (i = 0; items[i]; i++) {
		gint len;

		if (i == 0) {
			gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _(items[i]));
			len = g_utf8_strlen (_(items[i]), -1);
		} else {
			gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), items[i]);
			len = g_utf8_strlen (items[i], -1);

			if (!idx && fmt && g_str_equal (fmt, items[i]))
				idx = i;
		}

		if (len > max_len)
			max_len = len;
	}

	if (idx == 0 && fmt && !g_str_equal (fmt, get_default_format (kind, key))) {
		gint len;

		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), fmt);
		idx = i;

		len = g_utf8_strlen (fmt, -1);
		if (len > max_len)
			max_len = len;
	}

	gtk_combo_box_set_active ((GtkComboBox *) combo, idx);

	if (max_len > 10) {
		GtkWidget *widget;

		widget = gtk_bin_get_child (GTK_BIN (combo));
		if (GTK_IS_ENTRY (widget))
			gtk_entry_set_width_chars (GTK_ENTRY (widget), max_len + 1);
	}
}

static void
update_preview_widget (GtkWidget *combo)
{
	GtkWidget *preview;
	const gchar *key;
	gchar buffer[129];
	time_t now;

	g_return_if_fail (GTK_IS_COMBO_BOX (combo));

	preview = g_object_get_data (G_OBJECT (combo), "preview-label");
	g_return_if_fail (preview != NULL);
	g_return_if_fail (GTK_IS_LABEL (preview));

	key = g_object_get_data (G_OBJECT (combo), "format-key");
	g_return_if_fail (key != NULL);

	time (&now);

	format_internal (key, GPOINTER_TO_INT (g_object_get_data (G_OBJECT (combo), "format-kind")), now, NULL, buffer, sizeof (buffer));
	gtk_label_set_text (GTK_LABEL (preview), buffer);
}

static void
format_combo_changed_cb (GtkWidget *combo,
                         gpointer user_data)
{
	const gchar *key;
	DTFormatKind kind;
	GKeyFile *keyfile;

	g_return_if_fail (GTK_IS_COMBO_BOX (combo));

	key = g_object_get_data (G_OBJECT (combo), "format-key");
	g_return_if_fail (key != NULL);

	kind = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (combo), "format-kind"));
	keyfile = g_object_get_data (G_OBJECT (combo), "setup-key-file");

	if (kind != DTFormatKindShortDate && gtk_combo_box_get_active (GTK_COMBO_BOX (combo)) == 0) {
		/* use locale default */
		set_format_internal (key, NULL, keyfile);
	} else {
		gchar *text;

		text = gtk_combo_box_text_get_active_text (
			GTK_COMBO_BOX_TEXT (combo));
		set_format_internal (key, text, keyfile);
		g_free (text);
	}

	update_preview_widget (combo);

	/* save on every change only because 'unref_setup_keyfile' is never called :(
	 * how about in kill - bonobo? */
	save_keyfile (keyfile, key, kind);
}

static gchar *
gen_key (const gchar *component,
         const gchar *part,
         DTFormatKind kind)
{
	const gchar *kind_str = NULL;

	g_return_val_if_fail (component != NULL, NULL);
	g_return_val_if_fail (*component != 0, NULL);

	switch (kind) {
	case DTFormatKindDate:
		kind_str = "Date";
		break;
	case DTFormatKindTime:
		kind_str = "Time";
		break;
	case DTFormatKindDateTime:
		kind_str = "DateTime";
		break;
	case DTFormatKindShortDate:
		kind_str = "ShortDate";
		break;
	}

	return g_strconcat (component, (part && *part) ? "-" : "", part && *part ? part : "", "-", kind_str, NULL);
}

static void
unref_setup_keyfile (gpointer ptr)
{
	g_return_if_fail (ptr == setup_keyfile);
	g_return_if_fail (setup_keyfile != NULL);
	g_return_if_fail (setup_keyfile_instances > 0);

	/* this is never called */
	setup_keyfile_instances--;
	if (setup_keyfile_instances == 0) {
		save_keyfile (setup_keyfile, NULL, 0);
		g_key_file_free (setup_keyfile);
		setup_keyfile = NULL;
	}
}

/**
 * e_datetime_format_add_setup_widget:
 * @grid: Where to attach widgets. Requires 3 columns.
 * @row: On which row to attach.
 * @component: Component identifier for the format. Cannot be empty nor NULL.
 * @part: Part in the component, can be NULL or empty string.
 * @kind: Kind of the format for the component/part.
 * @caption: Caption for the widget, can be NULL, then the "Format:" is used.
 *
 * Adds a setup widget for a component and part. The table should have 3 columns.
 * All the work related to loading and saving the value is done automatically,
 * on user's changes.
 **/
void
e_datetime_format_add_setup_widget (GtkGrid *grid,
                                    gint row,
                                    const gchar *component,
                                    const gchar *part,
                                    DTFormatKind kind,
                                    const gchar *caption)
{
	GtkListStore *store;
	GtkWidget *label, *combo, *preview;
	gchar *key;

	g_return_if_fail (GTK_IS_GRID (grid));
	g_return_if_fail (row >= 0);
	g_return_if_fail (component != NULL);
	g_return_if_fail (*component != 0);

	key = gen_key (component, part, kind);

	label = gtk_label_new_with_mnemonic (caption ? caption : _("Format:"));
	gtk_label_set_xalign (GTK_LABEL (label), 1.0);

	store = gtk_list_store_new (1, G_TYPE_STRING);
	combo = g_object_new (
		GTK_TYPE_COMBO_BOX_TEXT,
		"model", store,
		"has-entry", TRUE,
		"entry-text-column", 0,
		NULL);
	g_object_unref (store);

	fill_combo_formats (combo, key, kind);
	gtk_label_set_mnemonic_widget ((GtkLabel *) label, combo);

	gtk_grid_attach (grid, label, 0, row, 1, 1);
	gtk_grid_attach (grid, combo, 1, row, 1, 1);

	preview = gtk_label_new ("");
	gtk_label_set_xalign (GTK_LABEL (preview), 0);
	gtk_label_set_ellipsize (GTK_LABEL (preview), PANGO_ELLIPSIZE_END);
	gtk_grid_attach (grid, preview, 2, row, 1, 1);

	if (!setup_keyfile) {
		gchar *filename;

		filename = e_datetime_format_dup_config_filename ();
		setup_keyfile = g_key_file_new ();
		g_key_file_load_from_file (setup_keyfile, filename, G_KEY_FILE_NONE, NULL);
		g_free (filename);

		setup_keyfile_instances = 1;
	} else {
		setup_keyfile_instances++;
	}

	g_object_set_data (G_OBJECT (combo), "preview-label", preview);
	g_object_set_data (G_OBJECT (combo), "format-kind", GINT_TO_POINTER (kind));
	g_object_set_data_full (G_OBJECT (combo), "format-key", key, g_free);
	g_object_set_data_full (G_OBJECT (combo), "setup-key-file", setup_keyfile, unref_setup_keyfile);
	g_signal_connect (
		combo, "changed",
		G_CALLBACK (format_combo_changed_cb), NULL);

	update_preview_widget (combo);

	gtk_widget_show_all (GTK_WIDGET (grid));
}

gchar *
e_datetime_format_format (const gchar *component,
                          const gchar *part,
                          DTFormatKind kind,
                          time_t value)
{
	gchar buffer[129];

	g_return_val_if_fail (component != NULL, NULL);
	g_return_val_if_fail (*component != 0, NULL);

	e_datetime_format_format_inline (component, part, kind, value, buffer, sizeof (buffer));

	return g_strdup (buffer);
}

void
e_datetime_format_format_inline (const gchar *component,
				 const gchar *part,
				 DTFormatKind kind,
				 time_t value,
				 gchar *buffer,
				 gint buffer_size)
{
	gchar *key;

	g_return_if_fail (component != NULL);
	g_return_if_fail (*component != 0);
	g_return_if_fail (buffer != NULL);
	g_return_if_fail (buffer_size > 0);

	key = gen_key (component, part, kind);
	g_return_if_fail (key != NULL);

	format_internal (key, kind, value, NULL, buffer, buffer_size - 1);

	g_free (key);

	buffer[buffer_size - 1] = '\0';
}

gchar *
e_datetime_format_format_tm (const gchar *component,
                             const gchar *part,
                             DTFormatKind kind,
                             struct tm *tm_time)
{
	gchar buffer[129];

	g_return_val_if_fail (component != NULL, NULL);
	g_return_val_if_fail (*component != 0, NULL);
	g_return_val_if_fail (tm_time != NULL, NULL);

	e_datetime_format_format_tm_inline (component, part, kind, tm_time, buffer, sizeof (buffer));

	return g_strdup (buffer);
}

void
e_datetime_format_format_tm_inline (const gchar *component,
				    const gchar *part,
				    DTFormatKind kind,
				    struct tm *tm_time,
				    gchar *buffer,
				    gint buffer_size)
{
	gchar *key;

	g_return_if_fail (component != NULL);
	g_return_if_fail (*component != 0);
	g_return_if_fail (tm_time != NULL);
	g_return_if_fail (buffer != NULL);
	g_return_if_fail (buffer_size > 0);

	key = gen_key (component, part, kind);
	g_return_if_fail (key != NULL);

	format_internal (key, kind, 0, tm_time, buffer, buffer_size - 1);

	g_free (key);

	buffer[buffer_size - 1] = '\0';
}

gboolean
e_datetime_format_includes_day_name (const gchar *component,
                                     const gchar *part,
                                     DTFormatKind kind)
{
	gchar *key;
	const gchar *fmt;
	gboolean res;

	g_return_val_if_fail (component != NULL, FALSE);
	g_return_val_if_fail (*component != 0, FALSE);

	key = gen_key (component, part, kind);
	g_return_val_if_fail (key != NULL, FALSE);

	fmt = get_format_internal (key, kind);

	res = fmt && (strstr (fmt, "%a") != NULL || strstr (fmt, "%A") != NULL);

	g_free (key);

	return res;
}

const gchar *
e_datetime_format_get_format (const gchar *component,
			      const gchar *part,
			      DTFormatKind kind)
{
	gchar *key;
	const gchar *fmt;

	g_return_val_if_fail (component != NULL, NULL);
	g_return_val_if_fail (*component != 0, NULL);

	key = gen_key (component, part, kind);
	g_return_val_if_fail (key != NULL, NULL);

	fmt = get_format_internal (key, kind);

	g_free (key);

	return (fmt && !*fmt) ? NULL : fmt;
}
