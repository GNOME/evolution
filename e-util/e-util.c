/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-util.c
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
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

#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <gio/gio.h>
#include <libgnome/gnome-util.h>

#ifdef G_OS_WIN32
#include <windows.h>
#endif

#include <libedataserver/e-data-server-util.h>
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
 * e_str_without_underscores:
 * @s: the string to strip underscores from.
 *
 * Strips underscores from a string in the same way @gtk_label_new_with_mnemonis does.
 * The returned string should be freed.
 */
char *
e_str_without_underscores (const char *s)
{
	char *new_string;
	const char *sp;
	char *dp;

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

/* Include build marshalers */

#include "e-util-marshal.h"

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

static gchar *
do_format_number_as_float (gdouble number)
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
	gdouble fractional;

	locality = localeconv();
	grouping = locality->grouping;
	while (number >= 1.0) {
		gchar *group;
		switch (*grouping) {
		default:
			last_count = *grouping;
			grouping++;
			/* Fall through */
		case 0:
			divider = epow10(last_count);
			number /= divider;
			fractional = modf (number, &number);
			fractional *= divider;
			fractional = floor (fractional);

			if (number >= 1.0) {
				group = g_strdup_printf("%0*d", last_count, (int) fractional);
			} else {
				group = g_strdup_printf("%d", (gint) fractional);
			}
			break;
		case CHAR_MAX:
			divider = epow10(last_count);
			number /= divider;
			fractional = modf (number, &number);
			fractional *= divider;
			fractional = floor (fractional);

			while (number >= 1.0) {
				group = g_strdup_printf("%0*d", last_count, (gint) fractional);

				char_length += strlen(group);
				list = g_list_prepend(list, group);
				group_count ++;

				divider = epow10(last_count);
				number /= divider;
				fractional = modf (number, &number);
				fractional *= divider;
				fractional = floor (fractional);
			}

			group = g_strdup_printf("%d", (gint) fractional);
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

gchar *
e_format_number_float (gfloat number)
{
	gfloat          int_part;
	gint            fraction;
	struct lconv   *locality;
	gchar          *str_intpart;
	gchar          *decimal_point;
	gchar          *str_fraction;
	gchar          *value;

	locality = localeconv();

	int_part = floor (number);
	str_intpart = do_format_number_as_float ((gdouble) int_part);

	if (!strcmp(locality->mon_decimal_point, "")) {
		decimal_point = ".";
	}
	else {
		decimal_point = locality->mon_decimal_point;
	}

	fraction = (gint) ((number - int_part) * 100);

	if (fraction == 0) {
		str_fraction = g_strdup ("00");
	} else {
		str_fraction = g_strdup_printf ("%02d", fraction);
	}

	value = g_strconcat (str_intpart, decimal_point, str_fraction, NULL);

	g_free (str_intpart);
	g_free (str_fraction);

	return value;
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

cairo_font_options_t *
get_font_options (void)
{
	gchar *antialiasing, *hinting, *subpixel_order;
	GConfClient *gconf = gconf_client_get_default ();
	cairo_font_options_t *font_options = cairo_font_options_create ();

	/* Antialiasing */
	antialiasing = gconf_client_get_string (gconf,
			"/desktop/gnome/font_rendering/antialiasing", NULL);
	if (antialiasing == NULL)
		cairo_font_options_set_antialias (font_options, CAIRO_ANTIALIAS_DEFAULT);
	else {
		if (strcmp (antialiasing, "grayscale") == 0)
			cairo_font_options_set_antialias (font_options, CAIRO_ANTIALIAS_GRAY);
		else if (strcmp (antialiasing, "rgba") == 0)
			cairo_font_options_set_antialias (font_options, CAIRO_ANTIALIAS_SUBPIXEL);
		else if (strcmp (antialiasing, "none") == 0)
			cairo_font_options_set_antialias (font_options, CAIRO_ANTIALIAS_NONE);
		else
			cairo_font_options_set_antialias (font_options, CAIRO_ANTIALIAS_DEFAULT);
	}
	hinting = gconf_client_get_string (gconf,
			"/desktop/gnome/font_rendering/hinting", NULL);
	if (hinting == NULL)
		cairo_font_options_set_hint_style (font_options, CAIRO_HINT_STYLE_DEFAULT);
	else {
		if (strcmp (hinting, "full") == 0)
			cairo_font_options_set_hint_style (font_options, CAIRO_HINT_STYLE_FULL);
		else if (strcmp (hinting, "medium") == 0)
			cairo_font_options_set_hint_style (font_options, CAIRO_HINT_STYLE_MEDIUM);
		else if (strcmp (hinting, "slight") == 0)
			cairo_font_options_set_hint_style (font_options, CAIRO_HINT_STYLE_SLIGHT);
		else if (strcmp (hinting, "none") == 0)
			cairo_font_options_set_hint_style (font_options, CAIRO_HINT_STYLE_NONE);
		else
			cairo_font_options_set_hint_style (font_options, CAIRO_HINT_STYLE_DEFAULT);
	}
	subpixel_order = gconf_client_get_string (gconf,
			"/desktop/gnome/font_rendering/rgba_order", NULL);
	if (subpixel_order == NULL)
		cairo_font_options_set_subpixel_order (font_options, CAIRO_SUBPIXEL_ORDER_DEFAULT);
	else {
		if (strcmp (subpixel_order, "rgb") == 0)
			cairo_font_options_set_subpixel_order (font_options, CAIRO_SUBPIXEL_ORDER_RGB);
		else if (strcmp (subpixel_order, "bgr") == 0)
			cairo_font_options_set_subpixel_order (font_options, CAIRO_SUBPIXEL_ORDER_BGR);
		else if (strcmp (subpixel_order, "vrgb") == 0)
			cairo_font_options_set_subpixel_order (font_options, CAIRO_SUBPIXEL_ORDER_VRGB);
		else if (strcmp (subpixel_order, "vbgr") == 0)
			cairo_font_options_set_subpixel_order (font_options, CAIRO_SUBPIXEL_ORDER_VBGR);
		else
			cairo_font_options_set_subpixel_order (font_options, CAIRO_SUBPIXEL_ORDER_DEFAULT);
	}
	g_free (antialiasing);
	g_free (hinting);
	g_free (subpixel_order);
	g_object_unref (gconf);
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
	const char *fname = get_lock_filename ();
	gboolean status = FALSE;

	int fd = g_creat (fname, S_IRUSR|S_IWUSR);
	if (fd == -1){
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
	const char *fname = get_lock_filename ();

	if (g_unlink (fname) == -1){
		g_warning ("Lock destroy: failed to unlink file '%s'!",fname);
	}
}

gboolean 
e_file_lock_exists ()
{
	const char *fname = get_lock_filename ();

	return g_file_test (fname, G_FILE_TEST_EXISTS);
}

/**
 * e_util_guess_mime_type:
 * @filename: it's a local file name, or URI.
 * Returns: NULL or newly allocated string with a mime_type of the given file. Free with g_free.
 *
 * Guesses mime_type for the given file_name.
 **/
char *
e_util_guess_mime_type (const char *filename)
{
	GFile *file;
	GFileInfo *fi;
	char *mime_type;

	g_return_val_if_fail (filename != NULL, NULL);

	if (strstr (filename, "://"))
		file = g_file_new_for_uri (filename);
	else
		file = g_file_new_for_path (filename);

	if (!file)
		return NULL;

	fi = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE, G_FILE_QUERY_INFO_NONE, NULL, NULL);
	if (!fi) {
		g_object_unref (file);
		return NULL;
	}

	mime_type = g_content_type_get_mime_type (g_file_info_get_content_type (fi));

	g_object_unref (fi);
	g_object_unref (file);

	return mime_type;
}

/**
 * e_util_filename_to_uri:
 * @filename: local file name.
 * Returns: either newly allocated string or NULL. Free with g_free.
 *
 * Converts local file name to URI.
 **/
char *
e_util_filename_to_uri (const char *filename)
{
	GFile *file;
	char *uri = NULL;

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
char *
e_util_uri_to_filename (const char *uri)
{
	GFile *file;
	char *filename = NULL;

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
e_util_read_file (const char *filename, gboolean filename_is_uri, char **buffer, gsize *read, GError **error)
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
		char *buff;

		sz = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_STANDARD_SIZE);
		buff = g_malloc (sizeof (char) * sz);

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

