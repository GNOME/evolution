/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-xml-utils.c
 * Copyright (C) 2000  Helix Code, Inc.
 * Author: Chris Lahey <clahey@helixcode.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <glib.h>
#include <gtk/gtkobject.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>

#include "e-util.h"

int
g_str_compare(const void *x, const void *y)
{
  return strcmp(x, y);
}

int
g_int_compare(const void *x, const void *y)
{
  if ( GPOINTER_TO_INT(x) < GPOINTER_TO_INT(y) )
    return -1;
  else if ( GPOINTER_TO_INT(x) == GPOINTER_TO_INT(y) )
    return 0;
  else
    return 1;
}

char *
e_strdup_strip(char *string)
{
	int i;
	int length = 0;
	int initial = 0;
	for ( i = 0; string[i]; i++ ) {
		if (initial == i && isspace(string[i])) {
			initial ++;
		}
		if (!isspace(string[i])) {
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
		gtk_object_unref (GTK_OBJECT (p->data));

	g_list_free (list);
}

void
e_free_string_list (GList *list)
{
	GList *p;

	for (p = list; p != NULL; p = p->next)
		g_free (p->data);

	g_list_free (list);
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

	fd = open(filename, O_RDONLY);
	if (fd == -1)
		return NULL;
	bytes = read(fd, buffer, BUFF_SIZE);
	while (bytes) {
		if (bytes > 0) {
			list = g_list_prepend(list, g_strndup(buffer, bytes));
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
	fd = open(filename, flags, 0666);
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
	close(fd);
	return 0;
}
