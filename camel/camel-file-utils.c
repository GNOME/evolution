/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Authors:
 *   Michael Zucchi <notzed@ximian.com>
 *   Dan Winship <danw@ximian.com>
 *
 * Copyright (C) 2000, 2001 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of version 2 of the GNU General Public 
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "camel-file-utils.h"
#include "camel-url.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#include <string.h>

#include <netinet/in.h>

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif

/**
 * camel_file_util_encode_uint32:
 * @out: file to output to
 * @value: value to output
 * 
 * Utility function to save an uint32 to a file.
 * 
 * Return value: 0 on success, -1 on error.
 **/
int
camel_file_util_encode_uint32 (FILE *out, guint32 value)
{
	int i;

	for (i = 28; i > 0; i -= 7) {
		if (value >= (1 << i)) {
			unsigned int c = (value >> i) & 0x7f;
			if (fputc (c, out) == -1)
				return -1;
		}
	}
	return fputc (value | 0x80, out);
}

/**
 * camel_file_util_decode_uint32:
 * @in: file to read from
 * @dest: pointer to a variable to store the value in
 * 
 * Retrieve an encoded uint32 from a file.
 * 
 * Return value: 0 on success, -1 on error.  @*dest will contain the
 * decoded value.
 **/
int
camel_file_util_decode_uint32 (FILE *in, guint32 *dest)
{
        guint32 value = 0;
	int v;

        /* until we get the last byte, keep decoding 7 bits at a time */
        while ( ((v = fgetc (in)) & 0x80) == 0 && v!=EOF) {
                value |= v;
                value <<= 7;
        }
	if (v == EOF) {
		*dest = value >> 7;
		return -1;
	}
	*dest = value | (v & 0x7f);

        return 0;
}

/**
 * camel_file_util_encode_fixed_int32:
 * @out: file to output to
 * @value: value to output
 * 
 * Encode a gint32, performing no compression, but converting
 * to network order.
 * 
 * Return value: 0 on success, -1 on error.
 **/
int
camel_file_util_encode_fixed_int32 (FILE *out, gint32 value)
{
	guint32 save;

	save = htonl (value);
	if (fwrite (&save, sizeof (save), 1, out) != 1)
		return -1;
	return 0;
}

/**
 * camel_file_util_decode_fixed_int32:
 * @in: file to read from
 * @dest: pointer to a variable to store the value in
 * 
 * Retrieve a gint32.
 * 
 * Return value: 0 on success, -1 on error.
 **/
int
camel_file_util_decode_fixed_int32 (FILE *in, gint32 *dest)
{
	guint32 save;

	if (fread (&save, sizeof (save), 1, in) == 1) {
		*dest = ntohl (save);
		return 0;
	} else {
		return -1;
	}
}

/**
 * camel_file_util_encode_time_t:
 * @out: file to output to
 * @value: value to output
 * 
 * Encode a time_t value to the file.
 * 
 * Return value: 0 on success, -1 on error.
 **/
int
camel_file_util_encode_time_t(FILE *out, time_t value)
{
	int i;

	for (i = sizeof (time_t) - 1; i >= 0; i--) {
		if (fputc((value >> (i * 8)) & 0xff, out) == -1)
			return -1;
	}
	return 0;
}

/**
 * camel_file_util_decode_time_t:
 * @in: file to read from
 * @dest: pointer to a variable to store the value in
 * 
 * Decode a time_t value.
 * 
 * Return value: 0 on success, -1 on error.
 **/
int
camel_file_util_decode_time_t (FILE *in, time_t *dest)
{
	time_t save = 0;
	int i = sizeof (time_t) - 1;
	int v = EOF;

        while (i >= 0 && (v = fgetc (in)) != EOF) {
		save |= ((time_t)v) << (i * 8);
		i--;
	}
	*dest = save;
	if (v == EOF)
		return -1;
	return 0;
}

/**
 * camel_file_util_encode_off_t:
 * @out: file to output to
 * @value: value to output
 * 
 * Encode an off_t type.
 * 
 * Return value: 0 on success, -1 on error.
 **/
int
camel_file_util_encode_off_t (FILE *out, off_t value)
{
	int i;

	for (i = sizeof (off_t) - 1; i >= 0; i--) {
		if (fputc ((value >> (i * 8)) & 0xff, out) == -1)
			return -1;
	}
	return 0;
}

/**
 * camel_file_util_decode_off_t:
 * @in: file to read from
 * @dest: pointer to a variable to put the value in
 * 
 * Decode an off_t type.
 * 
 * Return value: 0 on success, -1 on failure.
 **/
int
camel_file_util_decode_off_t (FILE *in, off_t *dest)
{
	off_t save = 0;
	int i = sizeof(off_t) - 1;
	int v = EOF;

        while (i >= 0 && (v = fgetc (in)) != EOF) {
		save |= ((off_t)v) << (i * 8);
		i--;
	}
	*dest = save;
	if (v == EOF)
		return -1;
	return 0;
}

/**
 * camel_file_util_encode_string:
 * @out: file to output to
 * @str: value to output
 * 
 * Encode a normal string and save it in the output file.
 * 
 * Return value: 0 on success, -1 on error.
 **/
int
camel_file_util_encode_string (FILE *out, const char *str)
{
	register int len;

	if (str == NULL)
		return camel_file_util_encode_uint32 (out, 1);
	
	if ((len = strlen (str)) > 65536)
		len = 65536;
	
	if (camel_file_util_encode_uint32 (out, len+1) == -1)
		return -1;
	if (len == 0 || fwrite (str, len, 1, out) == 1)
		return 0;
	return -1;
}

/**
 * camel_file_util_decode_string:
 * @in: file to read from
 * @str: pointer to a variable to store the value in
 * 
 * Decode a normal string from the input file.
 * 
 * Return value: 0 on success, -1 on error.
 **/
int
camel_file_util_decode_string (FILE *in, char **str)
{
	guint32 len;
	register char *ret;

	if (camel_file_util_decode_uint32 (in, &len) == -1) {
		*str = NULL;
		return -1;
	}

	len--;
	if (len > 65536) {
		*str = NULL;
		return -1;
	}

	ret = g_malloc (len+1);
	if (len > 0 && fread (ret, len, 1, in) != 1) {
		g_free (ret);
		*str = NULL;
		return -1;
	}

	ret[len] = 0;
	*str = ret;
	return 0;
}

/* Make a directory heirarchy.
   Always use full paths */
int
camel_file_util_mkdir(const char *path, mode_t mode)
{
	char *copy, *p;

	g_assert(path && path[0] == '/');

	p = copy = alloca(strlen(path)+1);
	strcpy(copy, path);
	do {
		p = strchr(p + 1, '/');
		if (p)
			*p = '\0';
		if (access(copy, F_OK) == -1) {
			if (mkdir(copy, mode) == -1)
				return -1;
		}
		if (p)
			*p = '/';
	} while (p);

	return 0;
}

char *
camel_file_util_safe_filename(const char *name)
{
	if (name == NULL)
		return NULL;
	
	return camel_url_encode(name, "/?()'*");
}
