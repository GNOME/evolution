/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Authors:
 *   Michael Zucchi <notzed@ximian.com>
 *   Jeffrey Stedfast <fejj@ximian.com>
 *   Dan Winship <danw@ximian.com>
 *
 * Copyright (C) 2000, 2003 Ximian, Inc.
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


#ifndef CAMEL_FILE_UTILS_H
#define CAMEL_FILE_UTILS_H 1

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <glib.h>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>

int camel_file_util_encode_fixed_int32 (FILE *out, gint32);
int camel_file_util_decode_fixed_int32 (FILE *in, gint32 *);
int camel_file_util_encode_uint32 (FILE *out, guint32);
int camel_file_util_decode_uint32 (FILE *in, guint32 *);
int camel_file_util_encode_time_t (FILE *out, time_t);
int camel_file_util_decode_time_t (FILE *in, time_t *);
int camel_file_util_encode_off_t (FILE *out, off_t);
int camel_file_util_decode_off_t (FILE *in, off_t *);
int camel_file_util_encode_size_t (FILE *out, size_t);
int camel_file_util_decode_size_t (FILE *in, size_t *);
int camel_file_util_encode_string (FILE *out, const char *);
int camel_file_util_decode_string (FILE *in, char **);

int camel_mkdir (const char *path, mode_t mode);
char *camel_file_util_safe_filename (const char *name);

ssize_t camel_read (int fd, char *buf, size_t n);
ssize_t camel_write (int fd, const char *buf, size_t n);

char *camel_file_util_savename(const char *filename);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_FILE_UTILS_H */
