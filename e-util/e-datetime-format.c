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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "e-datetime-format.h"

#include <libedataserver/libedataserver.h>

#include "e-misc-utils.h"

#define KEYS_FILENAME "datetime-formats.ini"
#define KEYS_GROUPNAME "formats"

#ifdef G_OS_WIN32
#ifdef localtime_r
#undef localtime_r
#endif
/* The localtime() in Microsoft's C library *is* thread-safe */
#define localtime_r(timep, result)  (localtime (timep) ? memcpy ((result), localtime (timep), sizeof (*(result))) : 0)
#endif

static GHashTable *key2fmt = NULL;

static GKeyFile *setup_keyfile = NULL; /* used on the combo */
static gint setup_keyfile_instances = 0;

static void
save_keyfile (GKeyFile *keyfile)
{
	gchar *contents;
	gchar *filename;
	gsize length;
	GError *error = NULL;

	g_return_if_fail (keyfile != NULL);

	filename = g_build_filename (e_get_user_data_dir (), KEYS_FILENAME, NULL);
	contents = g_key_file_to_data (keyfile, &length, NULL);

	g_file_set_contents (filename, contents, length, &error);

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	g_free (contents);
	g_free (filename);
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

	str = g_build_filename (e_get_user_data_dir (), KEYS_FILENAME, NULL);
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
                      const struct tm *value,
                      const struct tm *today)
{
	gchar *res = g_strdup (get_default_format (DTFormatKindDate, NULL));
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

static gchar *
format_internal (const gchar *key,
                 DTFormatKind kind,
                 time_t tvalue,
                 struct tm *tm_value)
{
	const gchar *fmt;
	gchar buff[129];
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
				gchar *ad;

				/* "%ad" for abbreviated date */
				if (!use_fmt) {
					use_fmt = g_string_new ("");

					ttoday = time (NULL);
					localtime_r (&ttoday, &today);
				}

				g_string_append_len (use_fmt, fmt + last, i - last);
				last = i + 3;
				i += 2;

				ad = format_relative_date (tvalue, ttoday, &value, &today);
				if (ad)
					g_string_append (use_fmt, ad);
				else if (g_ascii_isspace (fmt[i + 3]))
					i++;

				g_free (ad);
			}
		}
	}

	if (use_fmt && last < i) {
		g_string_append_len (use_fmt, fmt + last, i - last);
	}

	e_utf8_strftime_fix_am_pm (buff, sizeof (buff) - 1, use_fmt ? use_fmt->str : fmt, tm_value);

	if (use_fmt)
		g_string_free (use_fmt, TRUE);

	return g_strstrip (g_strdup (buff));
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
	gint i, idx = 0;
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
		if (i == 0) {
			gtk_combo_box_text_append_text (
				GTK_COMBO_BOX_TEXT (combo), _(items[i]));
		} else {
			gtk_combo_box_text_append_text (
				GTK_COMBO_BOX_TEXT (combo), items[i]);
			if (!idx && fmt && g_str_equal (fmt, items[i]))
				idx = i;
		}
	}

	if (idx == 0 && fmt && !g_str_equal (fmt, get_default_format (kind, key))) {
		gtk_combo_box_text_append_text (
			GTK_COMBO_BOX_TEXT (combo), fmt);
		idx = i;
	}

	gtk_combo_box_set_active ((GtkComboBox *) combo, idx);
}

static void
update_preview_widget (GtkWidget *combo)
{
	GtkWidget *preview;
	const gchar *key;
	gchar *value;
	time_t now;

	g_return_if_fail (GTK_IS_COMBO_BOX (combo));

	preview = g_object_get_data (G_OBJECT (combo), "preview-label");
	g_return_if_fail (preview != NULL);
	g_return_if_fail (GTK_IS_LABEL (preview));

	key = g_object_get_data (G_OBJECT (combo), "format-key");
	g_return_if_fail (key != NULL);

	time (&now);

	value = format_internal (key, GPOINTER_TO_INT (g_object_get_data (G_OBJECT (combo), "format-kind")), now, NULL);
	gtk_label_set_text (GTK_LABEL (preview), value ? value : "");
	g_free (value);
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
	save_keyfile (keyfile);
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
		save_keyfile (setup_keyfile);
		g_key_file_free (setup_keyfile);
		setup_keyfile = NULL;
	}
}

/**
 * e_datetime_format_add_setup_widget:
 * @table: Where to attach widgets. Requires 3 columns.
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
e_datetime_format_add_setup_widget (GtkWidget *table,
                                    gint row,
                                    const gchar *component,
                                    const gchar *part,
                                    DTFormatKind kind,
                                    const gchar *caption)
{
	GtkListStore *store;
	GtkWidget *label, *combo, *preview, *align;
	gchar *key;

	g_return_if_fail (table != NULL);
	g_return_if_fail (row >= 0);
	g_return_if_fail (component != NULL);
	g_return_if_fail (*component != 0);

	key = gen_key (component, part, kind);

	label = gtk_label_new_with_mnemonic (caption ? caption : _("Format:"));

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

	align = gtk_alignment_new (0.0, 0.5, 0.0, 0.0);
	gtk_container_add (GTK_CONTAINER (align), combo);

	gtk_table_attach ((GtkTable *) table, label, 0, 1, row, row + 1, 0, 0, 2, 0);
	gtk_table_attach ((GtkTable *) table, align, 1, 2, row, row + 1, 0, 0, 2, 0);

	preview = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (preview), 0.0, 0.5);
	gtk_label_set_ellipsize (GTK_LABEL (preview), PANGO_ELLIPSIZE_END);
	gtk_table_attach ((GtkTable *) table, preview, 2, 3, row, row + 1, GTK_EXPAND | GTK_FILL, 0, 2, 0);

	if (!setup_keyfile) {
		gchar *filename;

		filename = g_build_filename (e_get_user_data_dir (), KEYS_FILENAME, NULL);
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

	gtk_widget_show_all (table);
}

gchar *
e_datetime_format_format (const gchar *component,
                          const gchar *part,
                          DTFormatKind kind,
                          time_t value)
{
	gchar *key, *res;

	g_return_val_if_fail (component != NULL, NULL);
	g_return_val_if_fail (*component != 0, NULL);

	key = gen_key (component, part, kind);
	g_return_val_if_fail (key != NULL, NULL);

	res = format_internal (key, kind, value, NULL);

	g_free (key);

	return res;
}

gchar *
e_datetime_format_format_tm (const gchar *component,
                             const gchar *part,
                             DTFormatKind kind,
                             struct tm *tm_time)
{
	gchar *key, *res;

	g_return_val_if_fail (component != NULL, NULL);
	g_return_val_if_fail (*component != 0, NULL);
	g_return_val_if_fail (tm_time != NULL, NULL);

	key = gen_key (component, part, kind);
	g_return_val_if_fail (key != NULL, NULL);

	res = format_internal (key, kind, 0, tm_time);

	g_free (key);

	return res;
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
