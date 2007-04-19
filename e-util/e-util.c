/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-util.c
 * Copyright 2000, 2001, Ximian, Inc.
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
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
#include <libgnome/gnome-util.h>
#include <libgnomevfs/gnome-vfs.h>

#ifdef G_OS_WIN32
#include <windows.h>
#endif

#include <libedataserver/e-data-server-util.h>
#include "e-util.h"
#include "e-util-private.h"

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
	if (x == NULL || y == NULL) {
		if (x == y)
			return 0;
		else
			return x ? -1 : 1;
	}
	
	return g_utf8_collate (g_utf8_casefold (x, -1), g_utf8_casefold (y, -1));
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

gint
e_write_file_uri (const gchar *filename, const gchar *data)
{
	guint64 length = strlen(data);
	guint64 bytes;
	GnomeVFSResult result;
	GnomeVFSHandle *handle = NULL;

	result = gnome_vfs_create (&handle, filename, GNOME_VFS_OPEN_WRITE, FALSE, 0755);
	if (result != GNOME_VFS_OK) {
		g_warning ("Couldn't save item");
		return 1;
	}
	
	while (length > 0) {
		gnome_vfs_write(handle, data, length, &bytes);
		if (bytes > 0) {
			length -= bytes;
			data += bytes;
		} else {
			gnome_vfs_close(handle);
			return 1;
		}
	}

	gnome_vfs_close (handle);
	return 0;
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

	g_assert (decimal_point_len != 0);

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

	g_assert (decimal_point_len != 0);

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
get_font_options ()
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

	gconf_client_set_string(gconf, "/apps/evolution/mail/save_dir", uri, NULL);
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
	gchar *uri;

	uri = gconf_client_get_string(gconf, "/apps/evolution/mail/save_dir", NULL);
	g_object_unref(gconf);

	if (uri == NULL)
		uri = gnome_vfs_get_uri_from_local_path(g_get_home_dir());
	return (uri);
}

