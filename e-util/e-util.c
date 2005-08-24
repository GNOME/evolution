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

#include "e-i18n.h"
#include "e-util.h"
#include "e-util-private.h"

int
g_str_compare (const void *x, const void *y)
{
	if (x == NULL || y == NULL) {
		if (x == y)
			return 0;
		else
			return x ? -1 : 1;
	}
	
	return strcmp (x, y);
}

int
g_collate_compare (const void *x, const void *y)
{
	if (x == NULL || y == NULL) {
		if (x == y)
			return 0;
		else
			return x ? -1 : 1;
	}
	
	return g_utf8_collate (x, y);
}

int
g_int_compare (const void *x, const void *y)
{
	if (GPOINTER_TO_INT (x) < GPOINTER_TO_INT (y))
		return -1;
	else if (GPOINTER_TO_INT (x) == GPOINTER_TO_INT (y))
		return 0;
	else
		return 1;
}

char *
e_strdup_strip(const char *string)
{
	int i;
	int length = 0;
	int initial = 0;
	for ( i = 0; string[i]; i++ ) {
		if (initial == i && isspace((unsigned char) string[i])) {
			initial ++;
		}
		if (!isspace((unsigned char) string[i])) {
			length = i - initial + 1;
		}
	}
	return g_strndup(string + initial, length);
}

void
e_free_object_list (GList *list)
{
	GList *p;

	for (p = list; p != NULL; p = p->next)
		g_object_unref (p->data);

	g_list_free (list);
}

void
e_free_object_slist (GSList *list)
{
	GSList *p;

	for (p = list; p != NULL; p = p->next)
		g_object_unref (p->data);

	g_slist_free (list);
}

void
e_free_string_list (GList *list)
{
	GList *p;

	for (p = list; p != NULL; p = p->next)
		g_free (p->data);

	g_list_free (list);
}

void
e_free_string_slist (GSList *list)
{
	GSList *p;

	for (p = list; p != NULL; p = p->next)
		g_free (p->data);
	g_slist_free (list);
}

#define BUFF_SIZE 1024

char *
e_read_file(const char *filename)
{
	int fd;
	char buffer[BUFF_SIZE];
	GList *list = NULL, *list_iterator;
	GList *lengths = NULL, *lengths_iterator;
	int length = 0;
	int bytes;
	char *ret_val;

	fd = g_open(filename, O_RDONLY, 0);
	if (fd == -1)
		return NULL;
	bytes = read(fd, buffer, BUFF_SIZE);
	while (bytes) {
		if (bytes > 0) {
			char *temp = g_malloc(bytes);
			memcpy (temp, buffer, bytes);
			list = g_list_prepend(list, temp);
			lengths = g_list_prepend(lengths, GINT_TO_POINTER(bytes));
			length += bytes;
		} else {
			if (errno != EINTR) {
				close(fd);
				g_list_foreach(list, (GFunc) g_free, NULL);
				g_list_free(list);
				g_list_free(lengths);
				return NULL;
			}
		}
		bytes = read(fd, buffer, BUFF_SIZE);
	}
	ret_val = g_new(char, length + 1);
	ret_val[length] = 0;
	lengths_iterator = lengths;
	list_iterator = list;
	for ( ; list_iterator; list_iterator = list_iterator->next, lengths_iterator = lengths_iterator->next) {
		int this_length = GPOINTER_TO_INT(lengths_iterator->data);
		length -= this_length;
		memcpy(ret_val + length, list_iterator->data, this_length);
	}
	close(fd);
	g_list_foreach(list, (GFunc) g_free, NULL);
	g_list_free(list);
	g_list_free(lengths);
	return ret_val;
}

gint
e_write_file(const char *filename, const char *data, int flags)
{
	int fd;
	int length = strlen(data);
	int bytes;
	fd = g_open(filename, flags | O_WRONLY, 0666);
	if (fd == -1)
		return errno;
	while (length > 0) {
		bytes = write(fd, data, length);
		if (bytes > 0) {
			length -= bytes;
			data += bytes;
		} else {
			if (errno != EINTR && errno != EAGAIN) {
				int save_errno = errno;
				close(fd);
				return save_errno;
			}
		}
	}
	if (close(fd) != 0) {
		return errno;
	}
	return 0;
}

gint
e_write_file_mkstemp(char *filename, const char *data)
{
	int fd;
	int length = strlen(data);
	int bytes;
	fd = g_mkstemp (filename);
	if (fd == -1)
		return errno;
	while (length > 0) {
		bytes = write(fd, data, length);
		if (bytes > 0) {
			length -= bytes;
			data += bytes;
		} else {
			if (errno != EINTR && errno != EAGAIN) {
				int save_errno = errno;
				close(fd);
				return save_errno;
			}
		}
	}
	if (close(fd) != 0) {
		return errno;
	}
	return 0;
}

/**
 * e_mkdir_hier:
 * @path: a directory path
 * @mode: a mode, as for mkdir(2)
 *
 * This creates the named directory with the given @mode, creating
 * any necessary intermediate directories (with the same @mode).
 *
 * Return value: 0 on success, -1 on error, in which case errno will
 * be set as for mkdir(2).
 **/
int
e_mkdir_hier(const char *path, mode_t mode)
{
	char *copy, *p;

	if (g_path_is_absolute (path)) {
		p = copy = g_strdup (path);
	} else {
		gchar *current_dir = g_get_current_dir();
		p = copy = g_build_filename (current_dir, path, NULL);
		g_free (current_dir);
	}

	p = (char *)g_path_skip_root (p);
	do {
		char *p1 = strchr (p, '/');
#ifdef G_OS_WIN32
		{
			char *p2 = strchr (p, '\\');
			if (p1 == NULL ||
			    (p2 != NULL && p2 < p1))
				p1 = p2;
		}
#endif
		p = p1;
		if (p)
			*p = '\0';
		if (!g_file_test (copy, G_FILE_TEST_IS_DIR)) {
			if (g_mkdir (copy, mode) == -1) {
				g_free (copy);
				return -1;
			}
		}
		if (p) {
			*p++ = '/';
		}
	} while (p);

	g_free (copy);
	return 0;
}

#if 0
char *
e_read_uri(const char *uri)
{
	GnomeVFSHandle *handle;
	GList *list = NULL, *list_iterator;
	GList *lengths = NULL, *lengths_iterator;
	gchar buffer[1025];
	gchar *ret_val;
	int length = 0;
	GnomeVFSFileSize bytes;

	gnome_vfs_open(&handle, uri, GNOME_VFS_OPEN_READ);
	
	gnome_vfs_read(handle, buffer, 1024, &bytes);
	while (bytes) {
		if (bytes) {
			char *temp = g_malloc(bytes);
			memcpy (temp, buffer, bytes);
			list = g_list_prepend(list, temp);
			lengths = g_list_prepend(lengths, GINT_TO_POINTER((gint) bytes));
			length += bytes;
		}
		gnome_vfs_read(handle, buffer, 1024, &bytes);
	}

	ret_val = g_new(char, length + 1);
	ret_val[length] = 0;
	lengths_iterator = lengths;
	list_iterator = list;
	for ( ; list_iterator; list_iterator = list_iterator->next, lengths_iterator = lengths_iterator->next) {
		int this_length = GPOINTER_TO_INT(lengths_iterator->data);
		length -= this_length;
		memcpy(ret_val + length, list_iterator->data, this_length);
	}
	gnome_vfs_close(handle);
	g_list_foreach(list, (GFunc) g_free, NULL);
	g_list_free(list);
	g_list_free(lengths);
	return ret_val;
}
#endif

/* Include build marshalers */

#include "e-util-marshal.h"

gchar**
e_strsplit (const gchar *string,
	    const gchar *delimiter,
	    gint         max_tokens)
{
  GSList *string_list = NULL, *slist;
  gchar **str_array, *s;
  guint i, n = 1;

  g_return_val_if_fail (string != NULL, NULL);
  g_return_val_if_fail (delimiter != NULL, NULL);

  if (max_tokens < 1)
    max_tokens = G_MAXINT;

  s = strstr (string, delimiter);
  if (s)
    {
      guint delimiter_len = strlen (delimiter);

      do
	{
	  guint len;
	  gchar *new_string;

	  len = s - string;
	  new_string = g_new (gchar, len + 1);
	  strncpy (new_string, string, len);
	  new_string[len] = 0;
	  string_list = g_slist_prepend (string_list, new_string);
	  n++;
	  string = s + delimiter_len;
	  s = strstr (string, delimiter);
	}
      while (--max_tokens && s);
    }

  n++;
  string_list = g_slist_prepend (string_list, g_strdup (string));

  str_array = g_new (gchar*, n);

  i = n - 1;

  str_array[i--] = NULL;
  for (slist = string_list; slist; slist = slist->next)
    str_array[i--] = slist->data;

  g_slist_free (string_list);

  return str_array;
}

gchar *
e_strstrcase (const gchar *haystack, const gchar *needle)
{
	/* find the needle in the haystack neglecting case */
	const gchar *ptr;
	guint len;

	g_return_val_if_fail (haystack != NULL, NULL);
	g_return_val_if_fail (needle != NULL, NULL);

	len = strlen(needle);
	if (len > strlen(haystack))
		return NULL;

	if (len == 0)
		return (gchar *) haystack;

	for (ptr = haystack; *(ptr + len - 1) != '\0'; ptr++)
		if (!g_strncasecmp (ptr, needle, len))
			return (gchar *) ptr;

	return NULL;
}

/* This only makes a filename safe for usage as a filename.  It still may have shell meta-characters in it. */
void
e_filename_make_safe (gchar *string)
{
	gchar *p, *ts;
	gunichar c;
	
	g_return_if_fail (string != NULL);
	p = string;

	while(p && *p) {
		c = g_utf8_get_char (p);
		ts = p;
		p = g_utf8_next_char (p);
		if (!g_unichar_isprint(c) || ( c < 0xff && strchr (" /'\"`&();|<>$%{}!", c&0xff ))) {
			while (ts<p) 	
				*ts++ = '_';
		}
	}
}

static gint
epow10 (gint number) {
	gint value;

	for (value = 1; number > 0; number --) {
		value *= 10;
	}
	return value;
}

gchar *
e_format_number (gint number)
{
	GList *iterator, *list = NULL;
	struct lconv *locality;
	gint char_length = 0;
	gint group_count = 0;
	guchar *grouping;
	int last_count = 3;
	int divider;
	char *value;
	char *value_iterator;

	locality = localeconv();
	grouping = locality->grouping;
	while (number) {
		char *group;
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
		value = g_new(char, 1 + char_length + (group_count - 1) * strlen(locality->thousands_sep));

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
		e_free_string_list (list);
		return value;
	} else {
		return g_strdup("0");
	}
}

static gchar *
do_format_number_as_float (double number)
{
	GList *iterator, *list = NULL;
	struct lconv *locality;
	gint char_length = 0;
	gint group_count = 0;
	guchar *grouping;
	int last_count = 3;
	int divider;
	char *value;
	char *value_iterator;
	double fractional;

	locality = localeconv();
	grouping = locality->grouping;
	while (number >= 1.0) {
		char *group;
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
				group = g_strdup_printf("%d", (int) fractional);
			}
			break;
		case CHAR_MAX:
			divider = epow10(last_count);
			number /= divider;
			fractional = modf (number, &number);
			fractional *= divider;
			fractional = floor (fractional);

			while (number >= 1.0) {
				group = g_strdup_printf("%0*d", last_count, (int) fractional);

				char_length += strlen(group);
				list = g_list_prepend(list, group);
				group_count ++;

				divider = epow10(last_count);
				number /= divider;
				fractional = modf (number, &number);
				fractional *= divider;
				fractional = floor (fractional);
			}

			group = g_strdup_printf("%d", (int) fractional);
			break;
		}
		char_length += strlen(group);
		list = g_list_prepend(list, group);
		group_count ++;
	}

	if (list) {
		value = g_new(char, 1 + char_length + (group_count - 1) * strlen(locality->thousands_sep));

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
		e_free_string_list (list);
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
	str_intpart = do_format_number_as_float ((double) int_part);

	if (!strcmp(locality->mon_decimal_point, "")) {
		decimal_point = ".";
	}
	else {
		decimal_point = locality->mon_decimal_point;
	}

	fraction = (int) ((number - int_part) * 100);

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

gboolean
e_create_directory (gchar *directory)
{
	gint ret_val = e_mkdir_hier (directory, 0777);
	if (ret_val == -1)
		return FALSE;
	else
		return TRUE;
}


/* Perform a binary search for key in base which has nmemb elements
   of size bytes each.  The comparisons are done by (*compare)().  */
void      e_bsearch                                                        (const void       *key,
									    const void       *base,
									    size_t            nmemb,
									    size_t            size,
									    ESortCompareFunc  compare,
									    gpointer          closure,
									    size_t	     *start,
									    size_t	     *end)
{
	size_t l, u, idx;
	const void *p;
	int comparison;
	if (!(start || end))
		return;

	l = 0;
	u = nmemb;
	while (l < u) {
		idx = (l + u) / 2;
		p = (void *) (((const char *) base) + (idx * size));
		comparison = (*compare) (key, p, closure);
		if (comparison < 0)
			u = idx;
		else if (comparison > 0)
			l = idx + 1;
		else {
			size_t lsave, usave;
			lsave = l;
			usave = u;
			if (start) {
				while (l < u) {
					idx = (l + u) / 2;
					p = (void *) (((const char *) base) + (idx * size));
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
					p = (void *) (((const char *) base) + (idx * size));
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

static gpointer closure_closure;
static ESortCompareFunc compare_closure;

static int
qsort_callback(const void *data1, const void *data2)
{
	return (*compare_closure) (data1, data2, closure_closure);
}

/* Forget it.  We're just going to use qsort.  I lost the need for a stable sort. */
void
e_sort (void             *base,
	size_t            nmemb,
	size_t            size,
	ESortCompareFunc  compare,
	gpointer          closure)
{
	closure_closure = closure;
	compare_closure = compare;
	qsort(base, nmemb, size, qsort_callback);
#if 0
	void *base_copy;
	int i;
	base_copy = g_malloc(nmemb * size);

	for (i = 0; i < nmemb; i++) {
		int position;
		e_bsearch(base + (i * size), base_copy, i, size, compare, closure, NULL, &position);
		memmove(base_copy + (position + 1) * size, base_copy + position * size, (i - position) * size);
		memcpy(base_copy + position * size, base + i * size, size);
	}
	memcpy(base, base_copy, nmemb * size);
	g_free(base_copy);
#endif
}

size_t e_strftime(char *s, size_t max, const char *fmt, const struct tm *tm)
{
#ifdef HAVE_LKSTRFTIME
	return strftime(s, max, fmt, tm);
#else
	char *c, *ffmt, *ff;
	size_t ret;

	ffmt = g_strdup(fmt);
	ff = ffmt;
	while ((c = strstr(ff, "%l")) != NULL) {
		c[1] = 'I';
		ff = c;
	}

	ff = ffmt;
	while ((c = strstr(ff, "%k")) != NULL) {
		c[1] = 'H';
		ff = c;
	}

	ret = strftime(s, max, ffmt, tm);
	g_free(ffmt);
	return ret;
#endif
}

size_t 
e_utf8_strftime(char *s, size_t max, const char *fmt, const struct tm *tm)
{
	size_t sz, ret;
	char *locale_fmt, *buf;

	locale_fmt = g_locale_from_utf8(fmt, -1, NULL, &sz, NULL);
	if (!locale_fmt)
		return 0;

	ret = e_strftime(s, max, locale_fmt, tm);
	if (!ret) {
		g_free (locale_fmt);
		return 0;
	}

	buf = g_locale_to_utf8(s, ret, NULL, &sz, NULL);
	if (!buf) {
		g_free (locale_fmt);
		return 0;
	}

	if (sz >= max) {
		char *tmp = buf + max - 1;
		tmp = g_utf8_find_prev_char(buf, tmp);
		if (tmp)
			sz = tmp - buf;
		else
			sz = 0;
	}
	memcpy(s, buf, sz);
	s[sz] = '\0';
	g_free(locale_fmt);
	g_free(buf);
	return sz;
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

size_t e_strftime_fix_am_pm(char *s, size_t max, const char *fmt, const struct tm *tm)
{
	char buf[10];
	char *sp;
	char *ffmt;
	size_t ret;

	if (strstr(fmt, "%p")==NULL && strstr(fmt, "%P")==NULL) {
		/* No AM/PM involved - can use the fmt string directly */
		ret=e_strftime(s, max, fmt, tm);
	} else {
		/* Get the AM/PM symbol from the locale */
		e_strftime (buf, 10, "%p", tm);

		if (buf[0]) {
			/**
			 * AM/PM have been defined in the locale
			 * so we can use the fmt string directly
			 **/
			ret=e_strftime(s, max, fmt, tm);
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
			ret=e_strftime(s, max, ffmt, tm);
			g_free(ffmt);
		}
	}
	return(ret);
}

size_t 
e_utf8_strftime_fix_am_pm(char *s, size_t max, const char *fmt, const struct tm *tm)
{
	size_t sz, ret;
	char *locale_fmt, *buf;

	locale_fmt = g_locale_from_utf8(fmt, -1, NULL, &sz, NULL);
	if (!locale_fmt)
		return 0;

	ret = e_strftime_fix_am_pm(s, max, locale_fmt, tm);
	if (!ret) {
		g_free (locale_fmt);
		return 0;
	}

	buf = g_locale_to_utf8(s, ret, NULL, &sz, NULL);
	if (!buf) {
		g_free (locale_fmt);
		return 0;
	}

	if (sz >= max) {
		char *tmp = buf + max - 1;
		tmp = g_utf8_find_prev_char(buf, tmp);
		if (tmp)
			sz = tmp - buf;
		else
			sz = 0;
	}
	memcpy(s, buf, sz);
	s[sz] = '\0';
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
e_flexible_strtod (const gchar *nptr,
		   gchar      **endptr)
{
	gchar *fail_pos;
	gdouble val;
	struct lconv *locale_data;
	const char *decimal_point;
	int decimal_point_len;
	const char *p, *decimal_point_pos;
	const char *end = NULL; /* Silence gcc */
	char *copy, *c;

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
			fail_pos = (char *)nptr + (fail_pos - copy) - (decimal_point_len - 1);
		else
			fail_pos = (char *)nptr + (fail_pos - copy);
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
e_ascii_dtostr (gchar       *buffer,
		gint         buf_len,
		const gchar *format,
		gdouble      d)
{
	struct lconv *locale_data;
	const char *decimal_point;
	int decimal_point_len;
	gchar *p;
	int rest_len;
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
	char *v_string;
	int v_int;

	va_start (args1, first_string);
	G_VA_COPY (args2, args1);

	length = 0;

	v_string = first_string;
	while (v_string) {
		v_int = va_arg (args1, int);
		if (v_int >= 0)
			length += v_int;
		else
			length += strlen (v_string);
		v_string = va_arg (args1, char *);
	}

	buffer  = g_new (char, length + 1);
	current = buffer;

	v_string = first_string;
	while (v_string) {
		v_int = va_arg (args2, int);
		if (v_int < 0) {
			int i;
			for (i = 0; v_string[i]; i++) {
				*(current++) = v_string[i];
			}
		} else {
			int i;
			for (i = 0; v_string[i] && i < v_int; i++) {
				*(current++) = v_string[i];
			}
		}
		v_string = va_arg (args2, char *);
	}
	*(current++) = 0;

	va_end (args1);
	va_end (args2);

	return buffer;
}

gchar **
e_strdupv (const gchar **str_array)
{
	if (str_array) {
		gint i;
		gchar **retval;

		i = 0;
		while (str_array[i])
			i++;
          
		retval = g_new (gchar*, i + 1);

		i = 0;
		while (str_array[i]) {
			retval[i] = g_strdup (str_array[i]);
			i++;
		}
		retval[i] = NULL;

		return retval;
	} else {
		return NULL;
	}
}

char *
e_gettext (const char *msgid)
{
	static gboolean initialized = FALSE;

	if (!initialized) {
		bindtextdomain (E_I18N_DOMAIN, EVOLUTION_LOCALEDIR);
		bind_textdomain_codeset (E_I18N_DOMAIN, "UTF-8");
		initialized = TRUE;
	}        

	return dgettext (E_I18N_DOMAIN, msgid);
}

